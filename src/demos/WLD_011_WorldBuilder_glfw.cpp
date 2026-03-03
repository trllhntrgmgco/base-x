/**
 * WLD-011: Holodeck World Builder — Procedural VR Environment Creator
 * Procedural world generation with real-time editing and DRO optimization.
 *
 * Controls:
 *   WASD: Move camera / navigate
 *   Mouse: Look around / select objects
 *   LMB: Place object at cursor
 *   RMB: Remove object at cursor
 *   1-5: Select biome (Plains, Forest, Mountains, Desert, Ocean)
 *   T: Toggle terrain edit mode
 *   G: Generate new world seed
 *   +/-: Adjust brush size
 *   Tab: Cycle object palette (trees, rocks, buildings, paths)
 *   F: Toggle fog/atmosphere
 *   L: Toggle lighting mode (day/night/custom)
 *   P: Toggle physics simulation
 *   R: Reset world
 *   ESC: Quit
 */

#include <GLFW/glfw3.h>
#include <BaseX/DRO.h>
#include <BaseX/GLText.h>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <random>
#include <algorithm>
#include <string>

static constexpr float PI = 3.14159265f;

// ══════════════════════════════════════════════════════════════════════
// GRAPHICS QUALITY SETTINGS
// ══════════════════════════════════════════════════════════════════════

static int g_qualityLevel = 2;  // 0=Low, 1=Medium, 2=High, 3=Ultra
static bool g_enableShadows = true;
static bool g_enableReflections = true;
static bool g_enableClouds = true;
static bool g_enableAO = true;
static float g_sunAngle = 0.8f;  // Sun position (radians from horizon)
static float g_sunAzimuth = 0.3f;

// Sun/Moon direction (computed from angles)
static float g_lightDir[3] = {0.5f, 0.8f, 0.3f};

// ══════════════════════════════════════════════════════════════════════
// HELPERS
// ══════════════════════════════════════════════════════════════════════

static void perspectiveGL(double fov, double asp, double zn, double zf) {
    double f = 1.0 / tan(fov * PI / 360.0);
    double m[16] = {};
    m[0]  = f / asp;  m[5]  = f;
    m[10] = (zf + zn) / (zn - zf); m[11] = -1;
    m[14] = 2 * zf * zn / (zn - zf);
    glMultMatrixd(m);
}

static void lookAtGL(float ex, float ey, float ez, float cx, float cy, float cz) {
    float fx = cx - ex, fy = cy - ey, fz = cz - ez;
    float fl = sqrtf(fx*fx + fy*fy + fz*fz);
    fx /= fl; fy /= fl; fz /= fl;
    float sx = -fz, sy = 0.0f, sz = fx;
    float sl = sqrtf(sx*sx + sy*sy + sz*sz);
    if (sl > 0) { sx /= sl; sy /= sl; sz /= sl; }
    float ux = sy*fz - sz*fy, uy = sz*fx - sx*fz, uz = sx*fy - sy*fx;
    float m[16] = {sx, ux, -fx, 0, sy, uy, -fy, 0, sz, uz, -fz, 0, 0, 0, 0, 1};
    glMultMatrixf(m);
    glTranslatef(-ex, -ey, -ez);
}

// Normalize a 3D vector
static void normalize3(float* v) {
    float len = sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    if (len > 0.0001f) { v[0] /= len; v[1] /= len; v[2] /= len; }
}

// Compute cross product
static void cross3(const float* a, const float* b, float* out) {
    out[0] = a[1]*b[2] - a[2]*b[1];
    out[1] = a[2]*b[0] - a[0]*b[2];
    out[2] = a[0]*b[1] - a[1]*b[0];
}

// Compute dot product
static float dot3(const float* a, const float* b) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

// Phong lighting calculation
static void computeLighting(const float* normal, const float* baseColor, float* outColor) {
    float ambient = 0.25f;
    float diffuse = std::max(0.0f, dot3(normal, g_lightDir));

    // Soften shadows
    diffuse = diffuse * 0.7f + 0.3f;

    float intensity = ambient + diffuse * 0.75f;
    intensity = std::min(1.0f, intensity);

    outColor[0] = baseColor[0] * intensity;
    outColor[1] = baseColor[1] * intensity;
    outColor[2] = baseColor[2] * intensity;
}

// Set GL color with lighting
static void glColorLit(const float* normal, float r, float g, float b) {
    float base[3] = {r, g, b};
    float lit[3];
    computeLighting(normal, base, lit);
    glColor3fv(lit);
}

// Forward declaration - implemented after globals
static void updateSunDirection();

// ══════════════════════════════════════════════════════════════════════
// BIOME DEFINITIONS
// ══════════════════════════════════════════════════════════════════════

enum class Biome : int {
    PLAINS = 0,
    FOREST,
    MOUNTAINS,
    DESERT,
    OCEAN,
    COUNT
};

struct BiomeDef {
    const char* name;
    float baseHeight;
    float heightVariance;
    float vegetationDensity;
    float waterLevel;
    float color[3];       // Base terrain color
    float fogColor[3];
    float fogDensity;
};

static const BiomeDef BIOMES[] = {
    {"Plains",    0.0f, 0.1f, 0.2f, -1.0f, {0.3f, 0.6f, 0.2f}, {0.7f, 0.8f, 0.9f}, 0.01f},
    {"Forest",    0.1f, 0.3f, 0.9f, -0.5f, {0.1f, 0.4f, 0.1f}, {0.5f, 0.6f, 0.5f}, 0.03f},
    {"Mountains", 0.5f, 1.0f, 0.1f, -0.8f, {0.5f, 0.5f, 0.5f}, {0.8f, 0.8f, 0.9f}, 0.02f},
    {"Desert",    0.0f, 0.2f, 0.02f,-2.0f, {0.9f, 0.8f, 0.5f}, {0.9f, 0.85f,0.7f}, 0.005f},
    {"Ocean",    -0.5f, 0.05f,0.0f,  0.0f, {0.1f, 0.3f, 0.6f}, {0.6f, 0.7f, 0.9f}, 0.04f},
};

// ══════════════════════════════════════════════════════════════════════
// PLACEABLE OBJECTS
// ══════════════════════════════════════════════════════════════════════

enum class ObjectType : int {
    TREE_OAK = 0,
    TREE_PINE,
    TREE_PALM,
    ROCK_SMALL,
    ROCK_LARGE,
    BUILDING_HUT,
    BUILDING_TOWER,
    PATH_STONE,
    WATER_WELL,
    FENCE,
    COUNT
};

struct ObjectDef {
    const char* name;
    float scale;
    float color[3];
    bool hasPhysics;
    int lodLevels;
};

static const ObjectDef OBJECTS[] = {
    {"Oak Tree",     1.0f, {0.2f, 0.5f, 0.1f}, true,  4},
    {"Pine Tree",    1.2f, {0.1f, 0.4f, 0.15f},true,  4},
    {"Palm Tree",    1.5f, {0.3f, 0.6f, 0.2f}, true,  3},
    {"Small Rock",   0.3f, {0.5f, 0.5f, 0.5f}, true,  2},
    {"Large Rock",   1.0f, {0.4f, 0.4f, 0.45f},true,  3},
    {"Hut",          1.0f, {0.6f, 0.4f, 0.2f}, true,  3},
    {"Tower",        2.0f, {0.7f, 0.7f, 0.7f}, true,  4},
    {"Stone Path",   0.1f, {0.6f, 0.6f, 0.55f},false, 2},
    {"Water Well",   0.8f, {0.5f, 0.5f, 0.6f}, true,  2},
    {"Fence",        0.5f, {0.5f, 0.35f,0.2f}, true,  2},
};

// ══════════════════════════════════════════════════════════════════════
// WORLD OBJECT INSTANCE
// ══════════════════════════════════════════════════════════════════════

struct WorldObject {
    ObjectType type;
    float x, y, z;
    float rotation;
    float scale;
    int lodLevel;
    bool selected;
};

// ══════════════════════════════════════════════════════════════════════
// TERRAIN CHUNK (DRO-optimized)
// ══════════════════════════════════════════════════════════════════════

struct TerrainChunk {
    static constexpr int SIZE = 16;
    float heights[SIZE][SIZE];
    Biome biome;
    int chunkX, chunkZ;
    int lodLevel;
    bool dirty;
};

// ══════════════════════════════════════════════════════════════════════
// WORLD STATE
// ══════════════════════════════════════════════════════════════════════

static constexpr int WORLD_CHUNKS = 8;
static constexpr int WORLD_SIZE = WORLD_CHUNKS * TerrainChunk::SIZE;

static std::vector<TerrainChunk> g_chunks;
static std::vector<WorldObject> g_objects;
static BaseX::DROManager g_dro;

// Camera state
static float g_camX = WORLD_SIZE / 2.0f;
static float g_camY = 10.0f;
static float g_camZ = WORLD_SIZE / 2.0f;
static float g_camYaw = 0.0f;
static float g_camPitch = -0.3f;
static float g_camSpeed = 10.0f;

// Editor state
static Biome g_currentBiome = Biome::PLAINS;
static ObjectType g_currentObject = ObjectType::TREE_OAK;
static int g_objectPalette = 0;
static float g_brushSize = 3.0f;
static bool g_terrainEditMode = true;
static bool g_fogEnabled = true;
static bool g_physicsEnabled = true;
static int g_lightingMode = 0;  // 0=day, 1=night, 2=custom
static float g_timeOfDay = 0.5f; // 0-1

// Input state
static bool g_keys[512] = {};
static double g_mouseX = 0, g_mouseY = 0;
static bool g_mouseCaptured = false;
static bool g_lmbDown = false, g_rmbDown = false;

// Random
static std::mt19937 g_rng(42);
static std::uniform_real_distribution<float> g_r01(0, 1);

// ══════════════════════════════════════════════════════════════════════
// LIGHTING FUNCTIONS (need globals)
// ══════════════════════════════════════════════════════════════════════

// Update sun direction based on time of day
static void updateSunDirection() {
    if (g_lightingMode == 1) {  // Night - moonlight from above
        g_lightDir[0] = 0.2f;
        g_lightDir[1] = 0.9f;
        g_lightDir[2] = 0.1f;
    } else {
        g_lightDir[0] = cosf(g_sunAzimuth) * cosf(g_sunAngle);
        g_lightDir[1] = sinf(g_sunAngle);
        g_lightDir[2] = sinf(g_sunAzimuth) * cosf(g_sunAngle);
    }
    normalize3(g_lightDir);
}

// ══════════════════════════════════════════════════════════════════════
// NOISE FUNCTIONS (Simplex-like for procedural generation)
// ══════════════════════════════════════════════════════════════════════

static float hash2D(float x, float z) {
    int ix = (int)floorf(x) * 1619;
    int iz = (int)floorf(z) * 31337;
    int n = ix + iz;
    n = (n << 13) ^ n;
    return 1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f;
}

static float smoothNoise2D(float x, float z) {
    float fx = floorf(x), fz = floorf(z);
    float dx = x - fx, dz = z - fz;
    
    float v00 = hash2D(fx, fz);
    float v10 = hash2D(fx + 1, fz);
    float v01 = hash2D(fx, fz + 1);
    float v11 = hash2D(fx + 1, fz + 1);
    
    // Smooth interpolation
    float sx = dx * dx * (3 - 2 * dx);
    float sz = dz * dz * (3 - 2 * dz);
    
    float i0 = v00 + sx * (v10 - v00);
    float i1 = v01 + sx * (v11 - v01);
    return i0 + sz * (i1 - i0);
}

static float fractalNoise(float x, float z, int octaves, float persistence) {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;
    
    for (int i = 0; i < octaves; i++) {
        total += smoothNoise2D(x * frequency, z * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }
    return total / maxValue;
}

// ══════════════════════════════════════════════════════════════════════
// TERRAIN GENERATION
// ══════════════════════════════════════════════════════════════════════

static void generateChunk(TerrainChunk& chunk, uint32_t seed) {
    const BiomeDef& biome = BIOMES[(int)chunk.biome];
    
    for (int z = 0; z < TerrainChunk::SIZE; z++) {
        for (int x = 0; x < TerrainChunk::SIZE; x++) {
            float worldX = chunk.chunkX * TerrainChunk::SIZE + x;
            float worldZ = chunk.chunkZ * TerrainChunk::SIZE + z;
            
            // Multi-octave noise for natural terrain
            float noise = fractalNoise(worldX * 0.02f + seed, worldZ * 0.02f + seed, 4, 0.5f);
            
            chunk.heights[x][z] = biome.baseHeight + noise * biome.heightVariance * 5.0f;
            
            // Clamp water areas
            if (chunk.heights[x][z] < biome.waterLevel) {
                chunk.heights[x][z] = biome.waterLevel;
            }
        }
    }
    chunk.dirty = false;
}

static void generateWorld(uint32_t seed) {
    g_chunks.clear();
    g_chunks.resize(WORLD_CHUNKS * WORLD_CHUNKS);
    
    for (int cz = 0; cz < WORLD_CHUNKS; cz++) {
        for (int cx = 0; cx < WORLD_CHUNKS; cx++) {
            TerrainChunk& chunk = g_chunks[cz * WORLD_CHUNKS + cx];
            chunk.chunkX = cx;
            chunk.chunkZ = cz;
            chunk.biome = g_currentBiome;
            chunk.lodLevel = 0;
            chunk.dirty = true;
            generateChunk(chunk, seed);
        }
    }
    
    // Populate with objects based on biome
    g_objects.clear();
    const BiomeDef& biome = BIOMES[(int)g_currentBiome];
    
    int numObjects = (int)(WORLD_SIZE * WORLD_SIZE * biome.vegetationDensity * 0.01f);
    for (int i = 0; i < numObjects; i++) {
        WorldObject obj;
        obj.x = g_r01(g_rng) * WORLD_SIZE;
        obj.z = g_r01(g_rng) * WORLD_SIZE;
        
        // Get terrain height at position
        int cx = (int)obj.x / TerrainChunk::SIZE;
        int cz = (int)obj.z / TerrainChunk::SIZE;
        if (cx >= 0 && cx < WORLD_CHUNKS && cz >= 0 && cz < WORLD_CHUNKS) {
            TerrainChunk& chunk = g_chunks[cz * WORLD_CHUNKS + cx];
            int lx = (int)obj.x % TerrainChunk::SIZE;
            int lz = (int)obj.z % TerrainChunk::SIZE;
            obj.y = chunk.heights[lx][lz];
        } else {
            obj.y = 0;
        }
        
        // Select appropriate object type for biome
        if (g_currentBiome == Biome::FOREST) {
            obj.type = g_r01(g_rng) > 0.3f ? ObjectType::TREE_OAK : ObjectType::TREE_PINE;
        } else if (g_currentBiome == Biome::DESERT) {
            obj.type = g_r01(g_rng) > 0.5f ? ObjectType::ROCK_SMALL : ObjectType::TREE_PALM;
        } else if (g_currentBiome == Biome::MOUNTAINS) {
            obj.type = g_r01(g_rng) > 0.7f ? ObjectType::TREE_PINE : ObjectType::ROCK_LARGE;
        } else {
            obj.type = ObjectType::TREE_OAK;
        }
        
        obj.rotation = g_r01(g_rng) * 2 * PI;
        obj.scale = 0.8f + g_r01(g_rng) * 0.4f;
        obj.lodLevel = 0;
        obj.selected = false;
        
        g_objects.push_back(obj);
    }
}

// ══════════════════════════════════════════════════════════════════════
// DRO LOD MANAGEMENT
// ══════════════════════════════════════════════════════════════════════

static void updateLODs() {
    // Update chunk LODs based on distance from camera
    for (auto& chunk : g_chunks) {
        float chunkCenterX = (chunk.chunkX + 0.5f) * TerrainChunk::SIZE;
        float chunkCenterZ = (chunk.chunkZ + 0.5f) * TerrainChunk::SIZE;
        float dx = chunkCenterX - g_camX;
        float dz = chunkCenterZ - g_camZ;
        float dist = sqrtf(dx*dx + dz*dz);
        
        // Use DRO to determine appropriate precision level
        BaseX::RadixBase radix = g_dro.getOptimalRadix(dist);
        
        if (dist < 20.0f) chunk.lodLevel = 0;
        else if (dist < 50.0f) chunk.lodLevel = 1;
        else if (dist < 100.0f) chunk.lodLevel = 2;
        else chunk.lodLevel = 3;
    }
    
    // Update object LODs
    for (auto& obj : g_objects) {
        float dx = obj.x - g_camX;
        float dy = obj.y - g_camY;
        float dz = obj.z - g_camZ;
        float dist = sqrtf(dx*dx + dy*dy + dz*dz);
        
        const ObjectDef& def = OBJECTS[(int)obj.type];
        if (dist < 15.0f) obj.lodLevel = 0;
        else if (dist < 40.0f) obj.lodLevel = std::min(1, def.lodLevels - 1);
        else if (dist < 80.0f) obj.lodLevel = std::min(2, def.lodLevels - 1);
        else obj.lodLevel = def.lodLevels - 1;
    }
}

// ══════════════════════════════════════════════════════════════════════
// RENDERING - ENHANCED GRAPHICS
// ══════════════════════════════════════════════════════════════════════

// Compute terrain normal at a point
static void getTerrainNormal(const TerrainChunk& chunk, int x, int z, float* normal) {
    float h = chunk.heights[x][z];
    float hL = (x > 0) ? chunk.heights[x-1][z] : h;
    float hR = (x < TerrainChunk::SIZE-1) ? chunk.heights[x+1][z] : h;
    float hD = (z > 0) ? chunk.heights[x][z-1] : h;
    float hU = (z < TerrainChunk::SIZE-1) ? chunk.heights[x][z+1] : h;

    normal[0] = hL - hR;
    normal[1] = 2.0f;
    normal[2] = hD - hU;
    normalize3(normal);
}

static void renderTerrain() {
    for (const auto& chunk : g_chunks) {
        const BiomeDef& biome = BIOMES[(int)chunk.biome];
        int step = 1 << chunk.lodLevel;

        glBegin(GL_TRIANGLES);
        for (int z = 0; z < TerrainChunk::SIZE - step; z += step) {
            for (int x = 0; x < TerrainChunk::SIZE - step; x += step) {
                float wx = chunk.chunkX * TerrainChunk::SIZE + x;
                float wz = chunk.chunkZ * TerrainChunk::SIZE + z;

                int x1 = std::min(x + step, TerrainChunk::SIZE - 1);
                int z1 = std::min(z + step, TerrainChunk::SIZE - 1);

                float h00 = chunk.heights[x][z];
                float h10 = chunk.heights[x1][z];
                float h01 = chunk.heights[x][z1];
                float h11 = chunk.heights[x1][z1];

                // Enhanced color with height-based variation and slope coloring
                auto setVertexWithLighting = [&](int lx, int lz, float h, float vx, float vz) {
                    float normal[3];
                    getTerrainNormal(chunk, std::min(lx, TerrainChunk::SIZE-1), 
                                     std::min(lz, TerrainChunk::SIZE-1), normal);

                    // Height-based coloring
                    float t = (h - biome.baseHeight) / (biome.heightVariance * 5.0f + 0.1f);
                    t = std::max(0.0f, std::min(1.0f, t * 0.5f + 0.5f));

                    // Slope-based coloring (steeper = more rock/dirt)
                    float slope = 1.0f - normal[1];  // 0 = flat, 1 = vertical

                    float r = biome.color[0];
                    float g = biome.color[1];
                    float b = biome.color[2];

                    // Snow on high peaks (mountains biome)
                    if (h > 3.0f && g_currentBiome == Biome::MOUNTAINS) {
                        float snow = std::min(1.0f, (h - 3.0f) * 0.5f);
                        r = r * (1-snow) + 0.95f * snow;
                        g = g * (1-snow) + 0.95f * snow;
                        b = b * (1-snow) + 0.98f * snow;
                    }

                    // Rocky areas on steep slopes
                    if (slope > 0.3f) {
                        float rock = std::min(1.0f, (slope - 0.3f) * 2.0f);
                        r = r * (1-rock) + 0.45f * rock;
                        g = g * (1-rock) + 0.42f * rock;
                        b = b * (1-rock) + 0.40f * rock;
                    }

                    // Apply lighting
                    r *= t; g *= t; b *= t;
                    glColorLit(normal, r, g, b);
                    glVertex3f(vx, h, vz);
                };

                // Triangle 1
                setVertexWithLighting(x, z, h00, wx, wz);
                setVertexWithLighting(x1, z, h10, wx + step, wz);
                setVertexWithLighting(x, z1, h01, wx, wz + step);

                // Triangle 2
                setVertexWithLighting(x1, z, h10, wx + step, wz);
                setVertexWithLighting(x1, z1, h11, wx + step, wz + step);
                setVertexWithLighting(x, z1, h01, wx, wz + step);
            }
        }
        glEnd();
    }
}

static void renderTree(float x, float y, float z, float scale, float rot, int lod) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(rot * 180.0f / PI, 0, 1, 0);
    glScalef(scale, scale, scale);

    int segments = std::max(6, 16 - lod * 4);
    float trunkH = 1.5f;
    float trunkR = 0.12f;

    // Trunk with bark texture simulation
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= segments; i++) {
        float a = (float)i / segments * 2 * PI;
        float cx = cosf(a) * trunkR;
        float cz = sinf(a) * trunkR;

        // Compute normal for trunk
        float normal[3] = {cosf(a), 0.0f, sinf(a)};

        // Bark color variation
        float barkVar = 0.9f + 0.2f * sinf(a * 5.0f);
        glColorLit(normal, 0.35f * barkVar, 0.22f * barkVar, 0.08f * barkVar);
        glVertex3f(cx, 0, cz);

        glColorLit(normal, 0.38f * barkVar, 0.24f * barkVar, 0.1f * barkVar);
        glVertex3f(cx * 0.6f, trunkH, cz * 0.6f);
    }
    glEnd();

    // Multiple layers of foliage for fuller look
    int foliageLayers = std::max(2, 4 - lod);
    float baseRadius = 1.2f;
    float topY = trunkH + 3.0f;

    for (int layer = 0; layer < foliageLayers; layer++) {
        float layerT = (float)layer / foliageLayers;
        float layerY = trunkH + layerT * 2.5f;
        float layerR = baseRadius * (1.0f - layerT * 0.7f);
        float nextY = trunkH + (layerT + 1.0f/foliageLayers) * 2.5f;
        float nextR = baseRadius * (1.0f - (layerT + 1.0f/foliageLayers) * 0.7f);
        if (layer == foliageLayers - 1) { nextR = 0.0f; nextY = topY; }

        glBegin(GL_TRIANGLE_STRIP);
        for (int i = 0; i <= segments; i++) {
            float a = (float)i / segments * 2 * PI;

            // Slight irregularity for natural look
            float wobble = 1.0f + 0.15f * sinf(a * 3.0f + layer);
            float r1 = layerR * wobble;
            float r2 = nextR * wobble;

            // Foliage normal pointing outward and up
            float normal[3] = {cosf(a) * 0.5f, 0.7f, sinf(a) * 0.5f};
            normalize3(normal);

            // Color variation (darker inside, lighter outside)
            float shade = 0.85f + 0.15f * layerT;
            float leafVar = 0.9f + 0.2f * sinf(a * 7.0f);
            glColorLit(normal, 0.15f * shade * leafVar, 0.45f * shade * leafVar, 0.1f * shade * leafVar);
            glVertex3f(cosf(a) * r1, layerY, sinf(a) * r1);

            glColorLit(normal, 0.18f * shade * leafVar, 0.5f * shade * leafVar, 0.12f * shade * leafVar);
            glVertex3f(cosf(a) * r2, nextY, sinf(a) * r2);
        }
        glEnd();
    }

    glPopMatrix();
}

static void renderRock(float x, float y, float z, float scale, float rot, int lod, bool large) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(rot * 180.0f / PI, 0, 1, 0);
    glScalef(scale, scale * (large ? 1.0f : 0.5f), scale);
    
    glColor3f(0.5f, 0.5f, 0.5f);
    
    int segments = std::max(4, 8 - lod * 2);
    float r = large ? 0.8f : 0.3f;
    
    // Simple rock as irregular sphere
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < segments; i++) {
        for (int j = 0; j < segments; j++) {
            float a1 = (float)i / segments * PI;
            float a2 = (float)(i + 1) / segments * PI;
            float b1 = (float)j / segments * 2 * PI;
            float b2 = (float)(j + 1) / segments * 2 * PI;
            
            auto vertex = [&](float a, float b) {
                float rx = sinf(a) * cosf(b) * r * (0.8f + 0.4f * hash2D(a, b));
                float ry = cosf(a) * r * 0.7f;
                float rz = sinf(a) * sinf(b) * r * (0.8f + 0.4f * hash2D(b, a));
                glVertex3f(rx, ry + r * 0.5f, rz);
            };
            
            vertex(a1, b1); vertex(a2, b1); vertex(a1, b2);
            vertex(a2, b1); vertex(a2, b2); vertex(a1, b2);
        }
    }
    glEnd();
    
    glPopMatrix();
}

static void renderBuilding(float x, float y, float z, float scale, float rot, int lod, bool tower) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(rot * 180.0f / PI, 0, 1, 0);
    glScalef(scale, scale, scale);
    
    float w = tower ? 1.0f : 2.0f;
    float h = tower ? 4.0f : 1.5f;
    float d = tower ? 1.0f : 2.0f;
    
    // Walls
    glColor3f(0.7f, 0.65f, 0.55f);
    glBegin(GL_QUADS);
    // Front
    glVertex3f(-w/2, 0, d/2); glVertex3f(w/2, 0, d/2);
    glVertex3f(w/2, h, d/2); glVertex3f(-w/2, h, d/2);
    // Back
    glVertex3f(w/2, 0, -d/2); glVertex3f(-w/2, 0, -d/2);
    glVertex3f(-w/2, h, -d/2); glVertex3f(w/2, h, -d/2);
    // Left
    glVertex3f(-w/2, 0, -d/2); glVertex3f(-w/2, 0, d/2);
    glVertex3f(-w/2, h, d/2); glVertex3f(-w/2, h, -d/2);
    // Right
    glVertex3f(w/2, 0, d/2); glVertex3f(w/2, 0, -d/2);
    glVertex3f(w/2, h, -d/2); glVertex3f(w/2, h, d/2);
    glEnd();
    
    // Roof
    glColor3f(0.5f, 0.3f, 0.2f);
    if (tower) {
        // Pointed roof
        glBegin(GL_TRIANGLES);
        glVertex3f(0, h + 1.5f, 0);
        glVertex3f(-w/2, h, d/2); glVertex3f(w/2, h, d/2);
        glVertex3f(0, h + 1.5f, 0);
        glVertex3f(w/2, h, d/2); glVertex3f(w/2, h, -d/2);
        glVertex3f(0, h + 1.5f, 0);
        glVertex3f(w/2, h, -d/2); glVertex3f(-w/2, h, -d/2);
        glVertex3f(0, h + 1.5f, 0);
        glVertex3f(-w/2, h, -d/2); glVertex3f(-w/2, h, d/2);
        glEnd();
    } else {
        // Flat roof
        glBegin(GL_QUADS);
        glVertex3f(-w/2, h, -d/2); glVertex3f(-w/2, h, d/2);
        glVertex3f(w/2, h, d/2); glVertex3f(w/2, h, -d/2);
        glEnd();
    }
    
    glPopMatrix();
}

static void renderObjects() {
    for (const auto& obj : g_objects) {
        const ObjectDef& def = OBJECTS[(int)obj.type];
        
        switch (obj.type) {
            case ObjectType::TREE_OAK:
            case ObjectType::TREE_PINE:
            case ObjectType::TREE_PALM:
                renderTree(obj.x, obj.y, obj.z, obj.scale * def.scale, obj.rotation, obj.lodLevel);
                break;
            case ObjectType::ROCK_SMALL:
                renderRock(obj.x, obj.y, obj.z, obj.scale * def.scale, obj.rotation, obj.lodLevel, false);
                break;
            case ObjectType::ROCK_LARGE:
                renderRock(obj.x, obj.y, obj.z, obj.scale * def.scale, obj.rotation, obj.lodLevel, true);
                break;
            case ObjectType::BUILDING_HUT:
                renderBuilding(obj.x, obj.y, obj.z, obj.scale * def.scale, obj.rotation, obj.lodLevel, false);
                break;
            case ObjectType::BUILDING_TOWER:
                renderBuilding(obj.x, obj.y, obj.z, obj.scale * def.scale, obj.rotation, obj.lodLevel, true);
                break;
            default:
                break;
        }
        
        // Selection indicator
        if (obj.selected) {
            glColor3f(1, 1, 0);
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < 16; i++) {
                float a = i / 16.0f * 2 * PI;
                glVertex3f(obj.x + cosf(a) * 1.5f, obj.y + 0.1f, obj.z + sinf(a) * 1.5f);
            }
            glEnd();
        }
    }
}

static void renderWater() {
    const BiomeDef& biome = BIOMES[(int)g_currentBiome];
    if (biome.waterLevel < -1.5f) return;

    static float waveTime = 0.0f;
    waveTime += 0.016f;  // Approximate 60fps

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Render water as a grid for wave effect
    int waterRes = g_qualityLevel >= 2 ? 32 : 16;
    float cellSize = (float)WORLD_SIZE / waterRes;

    glBegin(GL_TRIANGLES);
    for (int z = 0; z < waterRes; z++) {
        for (int x = 0; x < waterRes; x++) {
            float wx = x * cellSize;
            float wz = z * cellSize;

            // Wave function
            auto getWaveHeight = [&](float px, float pz) {
                float wave1 = sinf(px * 0.3f + waveTime * 2.0f) * 0.05f;
                float wave2 = sinf(pz * 0.4f + waveTime * 1.5f) * 0.04f;
                float wave3 = sinf((px + pz) * 0.2f + waveTime) * 0.03f;
                return biome.waterLevel + wave1 + wave2 + wave3;
            };

            auto getWaterColor = [&](float px, float pz) {
                // Fresnel-like effect: deeper water is darker
                float depth = biome.waterLevel - (-2.0f);  // Approximate depth
                float fresnel = 0.6f + 0.4f * sinf(px * 0.1f + pz * 0.1f + waveTime);

                // Day/night water color
                if (g_lightingMode == 1) {  // Night
                    glColor4f(0.05f * fresnel, 0.1f * fresnel, 0.2f * fresnel, 0.85f);
                } else {
                    // Specular highlight simulation
                    float specX = (px - g_camX) / WORLD_SIZE;
                    float specZ = (pz - g_camZ) / WORLD_SIZE;
                    float spec = std::max(0.0f, 1.0f - sqrtf(specX*specX + specZ*specZ) * 2.0f);
                    spec = powf(spec, 4.0f) * 0.3f;

                    glColor4f(0.1f + spec, 0.35f * fresnel + spec, 0.6f * fresnel + spec, 0.7f);
                }
            };

            float h00 = getWaveHeight(wx, wz);
            float h10 = getWaveHeight(wx + cellSize, wz);
            float h01 = getWaveHeight(wx, wz + cellSize);
            float h11 = getWaveHeight(wx + cellSize, wz + cellSize);

            // Triangle 1
            getWaterColor(wx, wz); glVertex3f(wx, h00, wz);
            getWaterColor(wx + cellSize, wz); glVertex3f(wx + cellSize, h10, wz);
            getWaterColor(wx, wz + cellSize); glVertex3f(wx, h01, wz + cellSize);

            // Triangle 2
            getWaterColor(wx + cellSize, wz); glVertex3f(wx + cellSize, h10, wz);
            getWaterColor(wx + cellSize, wz + cellSize); glVertex3f(wx + cellSize, h11, wz + cellSize);
            getWaterColor(wx, wz + cellSize); glVertex3f(wx, h01, wz + cellSize);
        }
    }
    glEnd();

    glDisable(GL_BLEND);
}

static void renderSky() {
    static float cloudTime = 0.0f;
    cloudTime += 0.002f;

    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Sky gradient based on time of day
    float topColor[3], midColor[3], bottomColor[3];
    float sunGlow[3] = {0, 0, 0};

    if (g_lightingMode == 1) {  // Night
        topColor[0] = 0.01f; topColor[1] = 0.01f; topColor[2] = 0.05f;
        midColor[0] = 0.02f; midColor[1] = 0.03f; midColor[2] = 0.08f;
        bottomColor[0] = 0.05f; bottomColor[1] = 0.05f; bottomColor[2] = 0.12f;
    } else {  // Day
        // Sun position affects colors
        float sunHeight = sinf(g_sunAngle);

        // Zenith blue
        topColor[0] = 0.25f; topColor[1] = 0.45f; topColor[2] = 0.85f;
        // Mid sky
        midColor[0] = 0.45f + sunHeight * 0.1f;
        midColor[1] = 0.65f + sunHeight * 0.1f;
        midColor[2] = 0.92f;
        // Horizon (warm near sun)
        bottomColor[0] = 0.7f + sunHeight * 0.2f;
        bottomColor[1] = 0.75f + sunHeight * 0.15f;
        bottomColor[2] = 0.85f;

        // Sun glow color
        sunGlow[0] = 1.0f; sunGlow[1] = 0.9f; sunGlow[2] = 0.7f;
    }

    // Multi-band sky gradient
    glBegin(GL_QUADS);
    // Bottom band (horizon)
    glColor3fv(bottomColor);
    glVertex2f(-1, -1); glVertex2f(1, -1);
    glColor3f((bottomColor[0] + midColor[0]) * 0.5f,
              (bottomColor[1] + midColor[1]) * 0.5f,
              (bottomColor[2] + midColor[2]) * 0.5f);
    glVertex2f(1, -0.3f); glVertex2f(-1, -0.3f);

    // Middle band
    glColor3f((bottomColor[0] + midColor[0]) * 0.5f,
              (bottomColor[1] + midColor[1]) * 0.5f,
              (bottomColor[2] + midColor[2]) * 0.5f);
    glVertex2f(-1, -0.3f); glVertex2f(1, -0.3f);
    glColor3fv(midColor);
    glVertex2f(1, 0.3f); glVertex2f(-1, 0.3f);

    // Top band (zenith)
    glColor3fv(midColor);
    glVertex2f(-1, 0.3f); glVertex2f(1, 0.3f);
    glColor3fv(topColor);
    glVertex2f(1, 1); glVertex2f(-1, 1);
    glEnd();

    // Render sun/moon
    if (g_lightingMode == 0) {
        // Sun
        float sunScreenX = g_lightDir[0] * 0.7f;
        float sunScreenY = g_lightDir[1] * 0.8f - 0.1f;

        if (sunScreenY > -0.5f) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);

            // Sun glow
            int sunSegs = 32;
            glBegin(GL_TRIANGLE_FAN);
            glColor4f(1.0f, 0.95f, 0.8f, 1.0f);
            glVertex2f(sunScreenX, sunScreenY);
            glColor4f(1.0f, 0.7f, 0.3f, 0.0f);
            for (int i = 0; i <= sunSegs; i++) {
                float a = (float)i / sunSegs * 2 * PI;
                glVertex2f(sunScreenX + cosf(a) * 0.15f, sunScreenY + sinf(a) * 0.15f);
            }
            glEnd();

            // Sun disc
            glBegin(GL_TRIANGLE_FAN);
            glColor4f(1.0f, 1.0f, 0.95f, 1.0f);
            glVertex2f(sunScreenX, sunScreenY);
            for (int i = 0; i <= sunSegs; i++) {
                float a = (float)i / sunSegs * 2 * PI;
                glVertex2f(sunScreenX + cosf(a) * 0.04f, sunScreenY + sinf(a) * 0.04f);
            }
            glEnd();

            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_BLEND);
        }
    } else {
        // Moon
        float moonX = 0.3f, moonY = 0.5f;
        glEnable(GL_BLEND);

        int moonSegs = 24;
        glBegin(GL_TRIANGLE_FAN);
        glColor4f(0.9f, 0.9f, 0.95f, 1.0f);
        glVertex2f(moonX, moonY);
        glColor4f(0.7f, 0.7f, 0.8f, 0.8f);
        for (int i = 0; i <= moonSegs; i++) {
            float a = (float)i / moonSegs * 2 * PI;
            glVertex2f(moonX + cosf(a) * 0.05f, moonY + sinf(a) * 0.05f);
        }
        glEnd();

        // Stars
        glPointSize(2.0f);
        glBegin(GL_POINTS);
        for (int i = 0; i < 100; i++) {
            float sx = hash2D(i * 0.1f, 0.0f);
            float sy = hash2D(0.0f, i * 0.1f) * 0.5f + 0.2f;
            float twinkle = 0.5f + 0.5f * sinf(cloudTime * 10.0f + i);
            glColor3f(twinkle, twinkle, twinkle);
            glVertex2f(sx, sy);
        }
        glEnd();

        glDisable(GL_BLEND);
    }

    // Clouds (day only)
    if (g_lightingMode == 0 && g_enableClouds) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Simple procedural clouds
        for (int c = 0; c < 12; c++) {
            float cx = fmodf(hash2D(c, 0) + cloudTime * 0.1f, 2.0f) - 1.0f;
            float cy = 0.3f + hash2D(0, c) * 0.4f;
            float cw = 0.15f + hash2D(c, c) * 0.1f;
            float ch = 0.03f + hash2D(c + 1, c) * 0.02f;

            float cloudAlpha = 0.4f + hash2D(c, c + 1) * 0.2f;

            // Cloud puff (ellipse)
            glBegin(GL_TRIANGLE_FAN);
            glColor4f(1.0f, 1.0f, 1.0f, cloudAlpha);
            glVertex2f(cx, cy);
            glColor4f(1.0f, 1.0f, 1.0f, 0.0f);
            for (int i = 0; i <= 16; i++) {
                float a = (float)i / 16 * 2 * PI;
                glVertex2f(cx + cosf(a) * cw, cy + sinf(a) * ch);
            }
            glEnd();
        }

        glDisable(GL_BLEND);
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
}

static void renderUI(int winW, int winH) {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, winW, 0, winH, -1, 1);  // Note: Y flipped for gltext
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);

    // Background panel
    glColor4f(0, 0, 0, 0.6f);
    glEnable(GL_BLEND);
    glBegin(GL_QUADS);
    glVertex2f(10, winH - 10); glVertex2f(320, winH - 10);
    glVertex2f(320, winH - 250); glVertex2f(10, winH - 250);
    glEnd();

    // Text info
    glColor3f(1, 1, 1);
    char buf[256];
    float y = winH - 25.0f;

    gltext::draw("=== WORLD BUILDER ===", 15, y); y -= 18;
    snprintf(buf, sizeof(buf), "Biome: %s (1-5)", BIOMES[(int)g_currentBiome].name);
    gltext::draw(buf, 15, y); y -= 15;
    snprintf(buf, sizeof(buf), "Mode: %s (T)", g_terrainEditMode ? "Terrain" : "Objects");
    gltext::draw(buf, 15, y); y -= 15;
    snprintf(buf, sizeof(buf), "Object: %s (Tab)", OBJECTS[(int)g_currentObject].name);
    gltext::draw(buf, 15, y); y -= 15;
    snprintf(buf, sizeof(buf), "Brush: %.1f (+/-)", g_brushSize);
    gltext::draw(buf, 15, y); y -= 15;
    snprintf(buf, sizeof(buf), "Objects: %zu", g_objects.size());
    gltext::draw(buf, 15, y); y -= 18;

    // Graphics settings
    glColor3f(0.8f, 0.9f, 1.0f);
    gltext::draw("--- GRAPHICS ---", 15, y); y -= 15;
    glColor3f(1, 1, 1);
    const char* qualityNames[] = {"Low", "Medium", "High", "Ultra"};
    snprintf(buf, sizeof(buf), "Quality: %s (Q)", qualityNames[g_qualityLevel]);
    gltext::draw(buf, 15, y); y -= 15;
    snprintf(buf, sizeof(buf), "Fog: %s (F)", g_fogEnabled ? "ON" : "OFF");
    gltext::draw(buf, 15, y); y -= 15;
    snprintf(buf, sizeof(buf), "Clouds: %s (C)", g_enableClouds ? "ON" : "OFF");
    gltext::draw(buf, 15, y); y -= 15;
    snprintf(buf, sizeof(buf), "Lighting: %s (L)", g_lightingMode == 0 ? "Day" : "Night");
    gltext::draw(buf, 15, y); y -= 15;

    // DRO metrics
    auto metrics = g_dro.getMetrics();
    snprintf(buf, sizeof(buf), "DRO Radix: Base-%d", (int)metrics.currentRadix);
    gltext::draw(buf, 15, y); y -= 15;

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// ══════════════════════════════════════════════════════════════════════
// INPUT HANDLING
// ══════════════════════════════════════════════════════════════════════

static void keyCallback(GLFWwindow* w, int key, int /*scancode*/, int action, int /*mods*/) {
    if (action == GLFW_PRESS) {
        g_keys[key] = true;
        
        if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, GLFW_TRUE);
        if (key == GLFW_KEY_R) generateWorld((uint32_t)g_rng());
        if (key == GLFW_KEY_G) generateWorld((uint32_t)g_rng());
        if (key == GLFW_KEY_T) g_terrainEditMode = !g_terrainEditMode;
        if (key == GLFW_KEY_F) g_fogEnabled = !g_fogEnabled;
        if (key == GLFW_KEY_L) g_lightingMode = (g_lightingMode + 1) % 2;
        if (key == GLFW_KEY_P) g_physicsEnabled = !g_physicsEnabled;
        if (key == GLFW_KEY_C) g_enableClouds = !g_enableClouds;
        if (key == GLFW_KEY_Q) g_qualityLevel = (g_qualityLevel + 1) % 4;
        
        // Biome selection
        if (key >= GLFW_KEY_1 && key <= GLFW_KEY_5) {
            g_currentBiome = (Biome)(key - GLFW_KEY_1);
            generateWorld((uint32_t)g_rng());
        }
        
        // Object palette
        if (key == GLFW_KEY_TAB) {
            g_currentObject = (ObjectType)(((int)g_currentObject + 1) % (int)ObjectType::COUNT);
        }
        
        // Brush size
        if (key == GLFW_KEY_EQUAL || key == GLFW_KEY_KP_ADD) g_brushSize = std::min(10.0f, g_brushSize + 0.5f);
        if (key == GLFW_KEY_MINUS || key == GLFW_KEY_KP_SUBTRACT) g_brushSize = std::max(1.0f, g_brushSize - 0.5f);
        
    } else if (action == GLFW_RELEASE) {
        g_keys[key] = false;
    }
}

static void mouseButtonCallback(GLFWwindow* w, int button, int action, int /*mods*/) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        g_lmbDown = (action == GLFW_PRESS);
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        g_rmbDown = (action == GLFW_PRESS);
        if (action == GLFW_PRESS) {
            g_mouseCaptured = true;
            glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        } else {
            g_mouseCaptured = false;
            glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
}

static void cursorCallback(GLFWwindow* /*w*/, double x, double y) {
    static double lastX = x, lastY = y;
    double dx = x - lastX, dy = y - lastY;
    lastX = x; lastY = y;
    
    if (g_mouseCaptured) {
        g_camYaw -= (float)dx * 0.003f;
        g_camPitch -= (float)dy * 0.003f;
        g_camPitch = std::max(-1.5f, std::min(1.5f, g_camPitch));
    }
    
    g_mouseX = x;
    g_mouseY = y;
}

static void scrollCallback(GLFWwindow* /*w*/, double /*xoff*/, double yoff) {
    g_camSpeed = std::max(2.0f, std::min(50.0f, g_camSpeed + (float)yoff * 2.0f));
}

// ══════════════════════════════════════════════════════════════════════
// UPDATE
// ══════════════════════════════════════════════════════════════════════

static void update(float dt) {
    // Update sun/lighting direction
    updateSunDirection();

    // Slowly animate sun position for dynamic lighting
    if (g_lightingMode == 0) {
        g_sunAzimuth += dt * 0.02f;  // Slow sun movement
        if (g_sunAzimuth > 2 * PI) g_sunAzimuth -= 2 * PI;
    }

    // Camera movement
    float fx = -sinf(g_camYaw) * cosf(g_camPitch);
    float fy = sinf(g_camPitch);
    float fz = -cosf(g_camYaw) * cosf(g_camPitch);
    float rx = cosf(g_camYaw);
    float rz = -sinf(g_camYaw);

    float speed = g_camSpeed * dt;
    if (g_keys[GLFW_KEY_W]) { g_camX += fx * speed; g_camY += fy * speed; g_camZ += fz * speed; }
    if (g_keys[GLFW_KEY_S]) { g_camX -= fx * speed; g_camY -= fy * speed; g_camZ -= fz * speed; }
    if (g_keys[GLFW_KEY_A]) { g_camX -= rx * speed; g_camZ -= rz * speed; }
    if (g_keys[GLFW_KEY_D]) { g_camX += rx * speed; g_camZ += rz * speed; }
    if (g_keys[GLFW_KEY_SPACE]) g_camY += speed;
    if (g_keys[GLFW_KEY_LEFT_SHIFT]) g_camY -= speed;

    // Keep camera in bounds
    g_camX = std::max(0.0f, std::min((float)WORLD_SIZE, g_camX));
    g_camZ = std::max(0.0f, std::min((float)WORLD_SIZE, g_camZ));
    g_camY = std::max(1.0f, g_camY);
    
    // Update LODs
    updateLODs();
    
    // Place object on LMB
    if (g_lmbDown && !g_terrainEditMode) {
        // Simple placement at camera forward position
        float placeX = g_camX - sinf(g_camYaw) * 5.0f;
        float placeZ = g_camZ - cosf(g_camYaw) * 5.0f;
        float placeY = 0.0f;
        
        // Get terrain height
        int cx = (int)placeX / TerrainChunk::SIZE;
        int cz = (int)placeZ / TerrainChunk::SIZE;
        if (cx >= 0 && cx < WORLD_CHUNKS && cz >= 0 && cz < WORLD_CHUNKS) {
            TerrainChunk& chunk = g_chunks[cz * WORLD_CHUNKS + cx];
            int lx = std::max(0, std::min(TerrainChunk::SIZE - 1, (int)placeX % TerrainChunk::SIZE));
            int lz = std::max(0, std::min(TerrainChunk::SIZE - 1, (int)placeZ % TerrainChunk::SIZE));
            placeY = chunk.heights[lx][lz];
        }
        
        WorldObject obj;
        obj.type = g_currentObject;
        obj.x = placeX;
        obj.y = placeY;
        obj.z = placeZ;
        obj.rotation = g_r01(g_rng) * 2 * PI;
        obj.scale = 1.0f;
        obj.lodLevel = 0;
        obj.selected = false;
        g_objects.push_back(obj);
        
        g_lmbDown = false;  // Prevent rapid placement
    }
}

// ══════════════════════════════════════════════════════════════════════
// MAIN
// ══════════════════════════════════════════════════════════════════════

int main() {
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    // Request multisampling for antialiasing
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "WLD-011: Holodeck World Builder", nullptr, nullptr);
    if (!window) {
        fprintf(stderr, "Failed to create window\n");
        glfwTerminate();
        return 1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorCallback);
    glfwSetScrollCallback(window, scrollCallback);
    
    // Initialize DRO with VR profile for precision management
    g_dro.initializeVRProfile();

    // Initialize sun direction
    updateSunDirection();

    // Generate initial world
    generateWorld(42);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Enable multisampling if available (GL_MULTISAMPLE = 0x809D)
    glEnable(0x809D);

    // Enable smooth shading
    glShadeModel(GL_SMOOTH);

    // Enable line antialiasing
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    
    double lastTime = glfwGetTime();
    
    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;
        
        glfwPollEvents();
        update(dt);
        
        int winW, winH;
        glfwGetFramebufferSize(window, &winW, &winH);
        glViewport(0, 0, winW, winH);
        
        // Clear with fog color
        const BiomeDef& biome = BIOMES[(int)g_currentBiome];
        if (g_lightingMode == 1) {
            glClearColor(0.02f, 0.02f, 0.1f, 1.0f);
        } else {
            glClearColor(biome.fogColor[0], biome.fogColor[1], biome.fogColor[2], 1.0f);
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Fog
        if (g_fogEnabled) {
            glEnable(GL_FOG);
            glFogi(GL_FOG_MODE, GL_EXP);
            float fogColor[4] = {biome.fogColor[0], biome.fogColor[1], biome.fogColor[2], 1.0f};
            glFogfv(GL_FOG_COLOR, fogColor);
            glFogf(GL_FOG_DENSITY, biome.fogDensity);
        } else {
            glDisable(GL_FOG);
        }
        
        // Setup camera
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        perspectiveGL(60.0, (double)winW / winH, 0.1, 500.0);
        
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        float lookX = g_camX - sinf(g_camYaw) * cosf(g_camPitch);
        float lookY = g_camY + sinf(g_camPitch);
        float lookZ = g_camZ - cosf(g_camYaw) * cosf(g_camPitch);
        lookAtGL(g_camX, g_camY, g_camZ, lookX, lookY, lookZ);
        
        // Render scene
        renderSky();
        renderTerrain();
        renderWater();
        renderObjects();
        renderUI(winW, winH);
        
        glfwSwapBuffers(window);
    }
    
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
