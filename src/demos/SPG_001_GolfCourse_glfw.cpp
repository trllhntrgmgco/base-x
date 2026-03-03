/**
 * SPG-001: Procedural Golf Course — 9-Hole Championship
 * A complete golf simulation with procedurally generated holes.
 *
 * Features:
 *   - 18 unique procedurally generated holes (Par 3, 4, 5)
 *   - Realistic ball physics with spin, wind, and terrain effects
 *   - Multiple terrain types: fairway, rough, bunker, green, water
 *   - Dynamic wind system
 *   - Full scoring system (strokes, par, birdie, bogey)
 *   - Ball trajectory preview
 *   - Camera modes: follow ball, overhead, free
 *
 * Phase D — Static Daytime Sky:
 *   Multi-band gradient sky (zenith→upper→lower→horizon), frozen at midday.
 *   Sun disc + dual-layer corona at fixed high position.
 *   8 light drifting cloud puffs for ambience.
 *   getGolfSkyColors() provides matching fog/clear color everywhere.
 *   No day/night cycle — always bright afternoon.
 *
 * Phase E — Tree Wind & Better Shadows:
 *   Trees sway with g_windSpeed/g_windAngle (trunk lean + layered foliage).
 *   Per-tree phase variation; upper foliage layers sway more.
 *   Canopy edge ripple via animated sinf modulation.
 *   Sun-directional tree shadows (offset by g_slx/g_slz), alpha gradient.
 *   Ball shadow stretches and fades when airborne, offset by sun dir.
 *
 * Phase F — Ball Effects & Juice:
 *   Particle system: burst-on-event + per-frame update/render.
 *   Flight trail: fading white dots behind airborne ball.
 *   Landing puff: grass/dirt particles on first bounce.
 *   Bunker spray: sandy particles when hitting from sand.
 *   Water splash: blue-white burst on water hazard.
 *   Swing kick: small dirt burst at ball origin on swing.
 *   Rolling dust: subtle trail on non-green terrain.
 *   Power glow: aim arrow pulses & thickens at high power.
 *
 * Smooth Terrain Transitions:
 *   Noise-perturbed edges: fairway, green, bunker, water boundaries
 *     use fractalNoise to create organic, irregular shapes.
 *   Transition blend overlay: alpha-blended texture quads drawn at
 *     terrain type boundaries, weighted by neighbor count,
 *     creating smooth gradients between fairway/rough/green/bunker.
 *   Height blending: edge cells get smoothly interpolated heights.
 *
 * Controls:
 *   Mouse: Aim direction
 *   LMB Hold + Drag: Set power (pull back to increase)
 *   Space: Swing / Hit ball
 *   1-9: Jump to hole 1-9
 *   0: Jump to hole 10
 *   Q: Jump to hole 11
 *   W: Jump to hole 12
 *   E: Jump to hole 13
 *   R: Jump to hole 14
 *   T: Jump to hole 15
 *   Y: Jump to hole 16
 *   U: Jump to hole 17
 *   I: Jump to hole 18
 *   C: Cycle camera mode (Follow/Overhead/Free)
 *   V: Toggle trajectory preview
 *   R: Reset ball to tee
 *   Tab: View scorecard
 *   N: Next hole (after sinking putt)
 *   F: Toggle free camera (WASD to move)
 *   ESC: Quit
 */

#include <GLFW/glfw3.h>
#ifdef _WIN32
#include <GL/glext.h>
#endif
#include <BaseX/DRO.h>
#include <BaseX/GLText.h>
#include <cmath>
#include <cstdio>
#include <vector>
#include <random>
#include <algorithm>
#include <string>

static constexpr float PI = 3.14159265f;

// ══════════════════════════════════════════════════════════════════════
// GL EXTENSION LOADING
// ══════════════════════════════════════════════════════════════════════

static PFNGLGENBUFFERSPROC    glGenBuffers    = nullptr;
static PFNGLBINDBUFFERPROC    glBindBuffer    = nullptr;
static PFNGLBUFFERDATAPROC    glBufferData    = nullptr;
static PFNGLBUFFERSUBDATAPROC glBufferSubData = nullptr;
static PFNGLDELETEBUFFERSPROC glDeleteBuffers = nullptr;

// Shader extensions (Phase A)
static PFNGLCREATESHADERPROC      glCreateShader     = nullptr;
static PFNGLSHADERSOURCEPROC      glShaderSource     = nullptr;
static PFNGLCOMPILESHADERPROC     glCompileShader    = nullptr;
static PFNGLGETSHADERIVPROC       glGetShaderiv      = nullptr;
static PFNGLGETSHADERINFOLOGPROC  glGetShaderInfoLog = nullptr;
static PFNGLCREATEPROGRAMPROC     glCreateProgram    = nullptr;
static PFNGLATTACHSHADERPROC      glAttachShader     = nullptr;
static PFNGLLINKPROGRAMPROC       glLinkProgram      = nullptr;
static PFNGLGETPROGRAMIVPROC      glGetProgramiv     = nullptr;
static PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog= nullptr;
static PFNGLUSEPROGRAMPROC        glUseProgram       = nullptr;
static PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation= nullptr;
static PFNGLUNIFORM1FPROC        glUniform1f        = nullptr;
static PFNGLUNIFORM1IPROC        glUniform1i        = nullptr;
static PFNGLUNIFORM3FPROC        glUniform3f        = nullptr;
static PFNGLDELETESHADERPROC     glDeleteShader     = nullptr;
static PFNGLDELETEPROGRAMPROC    glDeleteProgram    = nullptr;

static bool g_hasShaders = false;
static float g_wallTime = 0.0f;

static bool loadGLExtensions() {
    glGenBuffers    = (PFNGLGENBUFFERSPROC)   glfwGetProcAddress("glGenBuffers");
    glBindBuffer    = (PFNGLBINDBUFFERPROC)   glfwGetProcAddress("glBindBuffer");
    glBufferData    = (PFNGLBUFFERDATAPROC)   glfwGetProcAddress("glBufferData");
    glBufferSubData = (PFNGLBUFFERSUBDATAPROC)glfwGetProcAddress("glBufferSubData");
    glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)glfwGetProcAddress("glDeleteBuffers");
    glCreateShader     = (PFNGLCREATESHADERPROC)     glfwGetProcAddress("glCreateShader");
    glShaderSource     = (PFNGLSHADERSOURCEPROC)     glfwGetProcAddress("glShaderSource");
    glCompileShader    = (PFNGLCOMPILESHADERPROC)    glfwGetProcAddress("glCompileShader");
    glGetShaderiv      = (PFNGLGETSHADERIVPROC)      glfwGetProcAddress("glGetShaderiv");
    glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC) glfwGetProcAddress("glGetShaderInfoLog");
    glCreateProgram    = (PFNGLCREATEPROGRAMPROC)    glfwGetProcAddress("glCreateProgram");
    glAttachShader     = (PFNGLATTACHSHADERPROC)     glfwGetProcAddress("glAttachShader");
    glLinkProgram      = (PFNGLLINKPROGRAMPROC)      glfwGetProcAddress("glLinkProgram");
    glGetProgramiv     = (PFNGLGETPROGRAMIVPROC)     glfwGetProcAddress("glGetProgramiv");
    glGetProgramInfoLog= (PFNGLGETPROGRAMINFOLOGPROC)glfwGetProcAddress("glGetProgramInfoLog");
    glUseProgram       = (PFNGLUSEPROGRAMPROC)       glfwGetProcAddress("glUseProgram");
    glGetUniformLocation=(PFNGLGETUNIFORMLOCATIONPROC)glfwGetProcAddress("glGetUniformLocation");
    glUniform1f        = (PFNGLUNIFORM1FPROC)        glfwGetProcAddress("glUniform1f");
    glUniform1i        = (PFNGLUNIFORM1IPROC)        glfwGetProcAddress("glUniform1i");
    glUniform3f        = (PFNGLUNIFORM3FPROC)        glfwGetProcAddress("glUniform3f");
    glDeleteShader     = (PFNGLDELETESHADERPROC)     glfwGetProcAddress("glDeleteShader");
    glDeleteProgram    = (PFNGLDELETEPROGRAMPROC)    glfwGetProcAddress("glDeleteProgram");
    g_hasShaders = glCreateShader && glCreateProgram && glUseProgram;
    return glGenBuffers && glBindBuffer && glBufferData && glDeleteBuffers;
}

// ══════════════════════════════════════════════════════════════════════
// TERRAIN VBO
// ══════════════════════════════════════════════════════════════════════

struct TerrainVertex {
    float x, y, z;     // position
    float nx, ny, nz;  // normal
    float u, v;         // texcoord
};

static const int NUM_TERRAIN_TYPES = 7;
static const int NUM_LOD_RINGS = 5;
static GLuint g_terrainVBO[NUM_TERRAIN_TYPES][NUM_LOD_RINGS] = {};
static int    g_terrainVertCount[NUM_TERRAIN_TYPES][NUM_LOD_RINGS] = {};
static int    g_lastCamIX = -9999, g_lastCamIZ = -9999;

struct LODRing { int radius; int step; };
static const LODRing g_lodRings[NUM_LOD_RINGS] = { {12, 1}, {40, 1}, {100, 2}, {200, 4}, {320, 4} };
static const float g_texScale = 2.0f;

static float g_slx, g_sly, g_slz;

static void initTerrainSun() {
    const float sunX = 0.4f, sunY = 0.8f, sunZ = 0.3f;
    float len = sqrtf(sunX*sunX + sunY*sunY + sunZ*sunZ);
    g_slx = sunX/len; g_sly = sunY/len; g_slz = sunZ/len;
}

// buildTerrainVBOs is defined after getVertexNormal
static void buildTerrainVBOs(int camIX, int camIZ);

// ══════════════════════════════════════════════════════════════════════
// PROCEDURAL TEXTURES
// ══════════════════════════════════════════════════════════════════════

static const int TEX_SIZE = 512;
static GLuint g_textures[7] = {};

static void putPixel(unsigned char* data, int x, int y, int r, int g, int b) {
    int idx = (y * TEX_SIZE + x) * 3;
    data[idx]   = (unsigned char)std::max(0, std::min(255, r));
    data[idx+1] = (unsigned char)std::max(0, std::min(255, g));
    data[idx+2] = (unsigned char)std::max(0, std::min(255, b));
}

static float texNoise(float x, float y, int octaves) {
    float total = 0, amp = 1.0f, freq = 1.0f, maxVal = 0;
    for (int i = 0; i < octaves; i++) {
        float fx = floorf(x*freq), fy = floorf(y*freq);
        float dx = x*freq - fx, dy = y*freq - fy;
        int ix = (int)fx * 1619 + (int)fy * 31337;
        auto h = [](int n) -> float {
            n = (n << 13) ^ n;
            return 1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f;
        };
        float v00 = h(ix), v10 = h(ix+1619), v01 = h(ix+31337), v11 = h(ix+1619+31337);
        float sx = dx*dx*(3-2*dx), sy = dy*dy*(3-2*dy);
        float v = v00 + sx*(v10-v00) + sy*(v01-v00) + sx*sy*(v00-v10-v01+v11);
        total += v * amp;
        maxVal += amp;
        amp *= 0.5f;
        freq *= 2.0f;
    }
    return total / maxVal;
}

static GLuint makeTexture(unsigned char* data) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, TEX_SIZE, TEX_SIZE, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    // Build mipmaps for smooth LOD transitions
    int level = 0, w = TEX_SIZE, h = TEX_SIZE;
    while (w > 1 || h > 1) {
        level++;
        int pw = w, ph = h;
        w = std::max(1, w / 2);
        h = std::max(1, h / 2);
        std::vector<unsigned char> mip(w * h * 3);
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int sx = x * 2, sy = y * 2;
                for (int c = 0; c < 3; c++) {
                    int sum = data[(sy*pw + sx)*3 + c];
                    if (sx+1 < pw) sum += data[(sy*pw + sx+1)*3 + c]; else sum += sum;
                    if (sy+1 < ph) sum += data[((sy+1)*pw + sx)*3 + c]; else sum += sum;
                    if (sx+1 < pw && sy+1 < ph) sum += data[((sy+1)*pw + sx+1)*3 + c]; else sum += sum;
                    mip[(y*w + x)*3 + c] = (unsigned char)(sum / 4);
                }
            }
        }
        glTexImage2D(GL_TEXTURE_2D, level, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, mip.data());
        // Copy mip for next level
        if (w * h * 3 <= (int)sizeof(unsigned char) * TEX_SIZE * TEX_SIZE * 3)
            memcpy(data, mip.data(), w * h * 3);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // Anisotropic filtering for crisp terrain at oblique viewing angles
    float maxAniso = 1.0f;
    glGetFloatv(0x84FF, &maxAniso);  // GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
    if (maxAniso > 1.0f)
        glTexParameterf(GL_TEXTURE_2D, 0x84FE, maxAniso);  // GL_TEXTURE_MAX_ANISOTROPY_EXT
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

static void generateTextures() {
    std::vector<unsigned char> data(TEX_SIZE * TEX_SIZE * 3);
    unsigned char* d = data.data();

    // TEE — manicured short grass with mowing stripes
    for (int y = 0; y < TEX_SIZE; y++) {
        for (int x = 0; x < TEX_SIZE; x++) {
            float n1 = texNoise(x*0.05f, y*0.05f, 4) * 12.0f;
            float n2 = texNoise(x*0.15f+50, y*0.15f+50, 3) * 5.0f;
            float stripe = sinf(y * 0.12f) * 4.0f;
            putPixel(d, x, y, 68+(int)(n1+stripe), 145+(int)(n1*1.5f+n2+stripe), 52+(int)(n1*0.5f));
        }
    }
    g_textures[0] = makeTexture(d);

    // FAIRWAY — lush striped grass with blade detail
    for (int y = 0; y < TEX_SIZE; y++) {
        for (int x = 0; x < TEX_SIZE; x++) {
            float n1 = texNoise(x*0.04f, y*0.04f, 5) * 10.0f;
            float n2 = texNoise(x*0.2f+99, y*0.2f+99, 3) * 6.0f;
            float stripe = sinf(y * 0.09f + x * 0.015f) * 5.0f;
            float blade = texNoise(x*0.8f, y*0.8f, 2) * 3.0f;
            putPixel(d, x, y, 60+(int)(n1+stripe), 155+(int)(n1*1.6f+n2+stripe+blade), 44+(int)(n1*0.4f));
        }
    }
    g_textures[1] = makeTexture(d);

    // ROUGH — thick clumpy dark grass
    for (int y = 0; y < TEX_SIZE; y++) {
        for (int x = 0; x < TEX_SIZE; x++) {
            float n1 = texNoise(x*0.06f, y*0.06f, 5) * 18.0f;
            float clump = texNoise(x*0.12f+30, y*0.12f+30, 4) * 12.0f;
            float blade = texNoise(x*0.5f, y*0.5f, 2) * 5.0f;
            putPixel(d, x, y, 42+(int)(n1+clump*0.3f), 105+(int)(n1+clump+blade), 30+(int)(n1*0.5f));
        }
    }
    g_textures[2] = makeTexture(d);

    // BUNKER — sand with pebble and grain detail
    for (int y = 0; y < TEX_SIZE; y++) {
        for (int x = 0; x < TEX_SIZE; x++) {
            float n1 = texNoise(x*0.05f, y*0.05f, 4) * 15.0f;
            float grain = texNoise(x*0.3f+70, y*0.3f+70, 3) * 8.0f;
            float pebble = texNoise(x*0.8f+20, y*0.8f+20, 2);
            float p = (pebble > 0.6f) ? 12.0f : 0.0f;
            putPixel(d, x, y, 218+(int)(n1+grain+p), 205+(int)(n1*0.85f+grain+p*0.8f), 150+(int)(n1*0.6f+p*0.5f));
        }
    }
    g_textures[3] = makeTexture(d);

    // GREEN — very fine smooth grass with subtle grain
    for (int y = 0; y < TEX_SIZE; y++) {
        for (int x = 0; x < TEX_SIZE; x++) {
            float n1 = texNoise(x*0.03f, y*0.03f, 5) * 6.0f;
            float n2 = texNoise(x*0.12f+40, y*0.12f+40, 3) * 3.0f;
            float stripe = sinf(y * 0.15f + x * 0.008f) * 2.5f;
            putPixel(d, x, y, 50+(int)(n1+stripe), 172+(int)(n1*1.8f+n2+stripe), 58+(int)(n1+stripe*0.5f));
        }
    }
    g_textures[4] = makeTexture(d);

    // WATER — deep blue with caustic ripple patterns
    for (int y = 0; y < TEX_SIZE; y++) {
        for (int x = 0; x < TEX_SIZE; x++) {
            float n1 = texNoise(x*0.04f, y*0.04f, 4) * 8.0f;
            float ripple = sinf(x*0.15f + cosf(y*0.12f)*2.0f) * cosf(y*0.15f + sinf(x*0.1f)*2.0f) * 10.0f;
            float caustic = std::max(0.0f, texNoise(x*0.08f+10, y*0.08f+10, 3)) * 15.0f;
            putPixel(d, x, y, 20+(int)(n1*0.2f+caustic*0.3f), 65+(int)(n1*0.4f+ripple*0.3f+caustic*0.4f), 145+(int)(n1+ripple+caustic*0.6f));
        }
    }
    g_textures[5] = makeTexture(d);

    // OOB — earth with stones and dead grass patches
    for (int y = 0; y < TEX_SIZE; y++) {
        for (int x = 0; x < TEX_SIZE; x++) {
            float n1 = texNoise(x*0.05f, y*0.05f, 5) * 16.0f;
            float stone = texNoise(x*0.3f+55, y*0.3f+55, 3);
            float s = (stone > 0.5f) ? 15.0f : 0.0f;
            float grass = std::max(0.0f, texNoise(x*0.08f+80, y*0.08f+80, 3)) * 8.0f;
            putPixel(d, x, y, 85+(int)(n1+s), 72+(int)(n1*0.7f+s*0.8f+grass), 50+(int)(n1*0.5f+s*0.3f));
        }
    }
    g_textures[6] = makeTexture(d);
}

// ══════════════════════════════════════════════════════════════════════
// TERRAIN TYPES
// ══════════════════════════════════════════════════════════════════════

enum class TerrainType {
    TEE,
    FAIRWAY,
    ROUGH,
    BUNKER,
    GREEN,
    WATER,
    OUT_OF_BOUNDS
};

struct TerrainProperties {
    const char* name;
    float friction;      // Ball friction multiplier
    float bounce;        // Ball bounce multiplier
    float speedPenalty;  // Speed reduction on roll
    float color[3];
};

static const TerrainProperties TERRAIN_PROPS[] = {
    {"Tee",       0.02f, 0.3f, 0.98f, {0.30f, 0.58f, 0.22f}},
    {"Fairway",   0.03f, 0.25f, 0.97f, {0.28f, 0.62f, 0.20f}},
    {"Rough",     0.08f, 0.15f, 0.90f, {0.22f, 0.42f, 0.14f}},
    {"Bunker",    0.15f, 0.1f, 0.80f, {0.92f, 0.85f, 0.62f}},
    {"Green",     0.015f, 0.2f, 0.99f, {0.22f, 0.68f, 0.25f}},
    {"Water",     1.0f, 0.0f, 0.0f, {0.15f, 0.35f, 0.65f}},
    {"OOB",       1.0f, 0.0f, 0.0f, {0.35f, 0.30f, 0.22f}},
};

// ══════════════════════════════════════════════════════════════════════
// HOLE DEFINITION
// ══════════════════════════════════════════════════════════════════════

struct Hazard {
    TerrainType type;
    float x, z;
    float radius;
};

struct Tree {
    float x, z;
    float height;
};

struct Hole {
    int number;
    int par;
    float length;           // Yards (scaled)
    
    // Tee position
    float teeX, teeZ;
    
    // Hole/flag position
    float holeX, holeZ;
    
    // Fairway control points for shape
    std::vector<float> fairwayPath;  // x,z pairs
    float fairwayWidth;
    
    // Green
    float greenRadius;
    float greenSlope;       // Slope direction/magnitude
    float greenSlopeAngle;
    
    // Hazards
    std::vector<Hazard> hazards;
    
    // Trees
    std::vector<Tree> trees;
};

// ══════════════════════════════════════════════════════════════════════
// BALL STATE
// ══════════════════════════════════════════════════════════════════════

struct Ball {
    float x, y, z;
    float vx, vy, vz;
    float spin;             // Backspin/topspin
    bool inFlight;
    bool inHole;
    bool inWater;
    bool outOfBounds;
    TerrainType terrain;
};

// ══════════════════════════════════════════════════════════════════════
// GAME STATE
// ══════════════════════════════════════════════════════════════════════

static std::vector<Hole> g_holes;
static Ball g_ball;
static int g_currentHole = 0;
static int g_strokes[18] = {0};
static int g_totalStrokes = 0;

// Swing state
static float g_aimAngle = 0.0f;
static float g_power = 0.0f;
static float g_maxPower = 50.0f;
static bool g_isAiming = false;
static bool g_showTrajectory = true;
static float g_swingTimer = 0.0f;

// Wind
static float g_windSpeed = 0.0f;
static float g_windAngle = 0.0f;

// Camera
enum class CameraMode { FOLLOW, OVERHEAD, FREE };
static CameraMode g_cameraMode = CameraMode::FOLLOW;
static float g_camX, g_camY, g_camZ;
static float g_camYaw = 0.0f, g_camPitch = -0.3f;
static float g_camDist = 15.0f;
static float g_camAngle = 0.0f;
static float g_camOffsetX = 0.0f;
static float g_camOffsetZ = 0.0f;

// UI
static bool g_showScorecard = false;
static bool g_showContours = true;
static bool g_holeComplete = false;
static float g_holeCompleteTimer = 0.0f;

// Input
static bool g_keys[512] = {};
static bool g_mouseDown = false;
static double g_mouseX, g_mouseY;
static double g_dragStartY;
static bool g_freeCam = false;

// Random
static std::mt19937 g_rng(12345);
static std::uniform_real_distribution<float> g_rand(0.0f, 1.0f);

// DRO Manager for precision
static BaseX::DROManager g_droManager;

// Terrain heightmap
static constexpr int TERRAIN_SIZE = 1024;
static std::vector<std::vector<float>> g_heightMap;
static std::vector<std::vector<TerrainType>> g_terrainMap;
static std::vector<std::vector<float>> g_blendMap;  // 0=interior, 1=boundary edge

// ══════════════════════════════════════════════════════════════════════
// UTILITY FUNCTIONS
// ══════════════════════════════════════════════════════════════════════

static float distance2D(float x1, float z1, float x2, float z2) {
    float dx = x2 - x1, dz = z2 - z1;
    return sqrtf(dx*dx + dz*dz);
}

// Smooth vertex normal from surrounding heights
static void getVertexNormal(int x, int z, float& nx, float& ny, float& nz) {
    int x0 = std::max(0, x-1), x1 = std::min(TERRAIN_SIZE-1, x+1);
    int z0 = std::max(0, z-1), z1 = std::min(TERRAIN_SIZE-1, z+1);
    float hL = g_heightMap[z][x0], hR = g_heightMap[z][x1];
    float hD = g_heightMap[z0][x], hU = g_heightMap[z1][x];
    nx = hL - hR;
    ny = 2.0f;
    nz = hD - hU;
    float len = sqrtf(nx*nx + ny*ny + nz*nz);
    if (len > 0) { nx /= len; ny /= len; nz /= len; }
}

// Forward declaration for hash2D (used by sky clouds, defined below)
static float hash2D(float x, float z);

// ══════════════════════════════════════════════════════════════════════
// SKY SYSTEM (Phase D: Static Daytime Sky — frozen midday)
// ══════════════════════════════════════════════════════════════════════

static void getGolfSkyColors(float& fogR, float& fogG, float& fogB) {
    fogR = 0.62f; fogG = 0.76f; fogB = 0.90f;
}

static void renderSky() {
    glDisable(GL_FOG);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);  glPushMatrix(); glLoadIdentity();
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Frozen midday: sun high, full daylight
    float zR = 0.18f, zG = 0.32f, zB = 0.72f;   // zenith
    float uR = 0.32f, uG = 0.48f, uB = 0.82f;   // upper
    float lR = 0.52f, lG = 0.65f, lB = 0.88f;   // lower
    float hR = 0.65f, hG = 0.76f, hB = 0.90f;   // horizon

    // 3 gradient strips: horizon→lower→upper→zenith
    glBegin(GL_QUADS);
    glColor3f(hR,hG,hB); glVertex2f(-1,-1); glVertex2f(1,-1);
    glColor3f(lR,lG,lB); glVertex2f(1,-0.2f); glVertex2f(-1,-0.2f);
    glColor3f(lR,lG,lB); glVertex2f(-1,-0.2f); glVertex2f(1,-0.2f);
    glColor3f(uR,uG,uB); glVertex2f(1,0.4f); glVertex2f(-1,0.4f);
    glColor3f(uR,uG,uB); glVertex2f(-1,0.4f); glVertex2f(1,0.4f);
    glColor3f(zR,zG,zB); glVertex2f(1,1); glVertex2f(-1,1);
    glEnd();

    // Sun disc + corona — fixed high position (midday)
    float ssx = 0.15f, ssy = 0.65f;
    // Outer corona
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(1.f, 0.97f, 0.85f, 0.20f);
    glVertex2f(ssx, ssy);
    glColor4f(1.f, 0.95f, 0.7f, 0.0f);
    for(int i=0;i<=20;i++){float a=i/20.f*2*PI;glVertex2f(ssx+cosf(a)*0.12f,ssy+sinf(a)*0.12f);}
    glEnd();
    // Inner corona
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(1.f, 0.98f, 0.88f, 0.40f);
    glVertex2f(ssx, ssy);
    glColor4f(1.f, 0.96f, 0.75f, 0.0f);
    for(int i=0;i<=16;i++){float a=i/16.f*2*PI;glVertex2f(ssx+cosf(a)*0.07f,ssy+sinf(a)*0.07f);}
    glEnd();
    // Sun disc
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(1.f, 0.98f, 0.90f, 1.f);
    glVertex2f(ssx, ssy);
    for(int i=0;i<=16;i++){float a=i/16.f*2*PI;glVertex2f(ssx+cosf(a)*0.035f,ssy+sinf(a)*0.035f);}
    glEnd();

    // Light drifting clouds (8 puffs, gentle drift)
    float drift = g_wallTime * 0.006f;
    for(int i=0;i<8;i++){
        float cx = hash2D((float)i * 13.7f, 3.1f) * 2.f - 1.f;
        float cy = 0.15f + hash2D(5.5f, (float)i * 9.3f) * 0.45f;
        float cw = 0.12f + hash2D((float)i * 2.1f, 7.7f) * 0.18f;
        float ch = 0.018f + hash2D((float)i * 4.4f, 1.3f) * 0.022f;
        cx = fmodf(cx + drift + 3.f, 2.f) - 1.f;
        glBegin(GL_TRIANGLE_FAN);
        glColor4f(1.f, 1.f, 1.f, 0.18f);
        glVertex2f(cx, cy);
        glColor4f(1.f, 1.f, 1.f, 0.0f);
        for(int j=0;j<=12;j++){
            float a=j/12.f*2*PI;
            float rx = cw * (0.8f + 0.2f * sinf(a * 3.f + (float)i));
            float ry = ch * (0.7f + 0.3f * cosf(a * 2.f + (float)i * 0.5f));
            glVertex2f(cx + cosf(a) * rx, cy + sinf(a) * ry);
        }
        glEnd();
    }

    glDisable(GL_BLEND);
    glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
}

static float hash2D(float x, float z) {
    int ix = (int)floorf(x) * 1619 + (int)floorf(z) * 31337;
    ix = (ix << 13) ^ ix;
    return 1.0f - ((ix * (ix * ix * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f;
}

static float smoothNoise(float x, float z) {
    float fx = floorf(x), fz = floorf(z);
    float dx = x - fx, dz = z - fz;
    float v00 = hash2D(fx, fz), v10 = hash2D(fx+1, fz);
    float v01 = hash2D(fx, fz+1), v11 = hash2D(fx+1, fz+1);
    float sx = dx*dx*(3-2*dx), sz = dz*dz*(3-2*dz);
    return v00 + sx*(v10-v00) + sz*(v01-v00) + sx*sz*(v00-v10-v01+v11);
}

static float fractalNoise(float x, float z, int octaves) {
    float total = 0, amp = 1, freq = 1, maxVal = 0;
    for (int i = 0; i < octaves; i++) {
        total += smoothNoise(x*freq, z*freq) * amp;
        maxVal += amp;
        amp *= 0.5f;
        freq *= 2.0f;
    }
    return total / maxVal;
}

// ══════════════════════════════════════════════════════════════════════
// COURSE GENERATION
// ══════════════════════════════════════════════════════════════════════

static void generateHole(Hole& hole, int number, float startX, float startZ, float courseDir) {
    hole.number = number;

    // Determine par and length based on hole number
    if (number == 3 || number == 6 || number == 8 || number == 12 || number == 15 || number == 17) {
        hole.par = 3;
        hole.length = 120 + g_rand(g_rng) * 60;  // 120-180 yards
    } else if (number == 2 || number == 5 || number == 10 || number == 14 || number == 18) {
        hole.par = 5;
        hole.length = 450 + g_rand(g_rng) * 100;  // 450-550 yards
    } else {
        hole.par = 4;
        hole.length = 280 + g_rand(g_rng) * 120;  // 280-400 yards
    }

    // Scale length to world units (1 yard = 0.3 units)
    float scaledLength = hole.length * 0.3f;

    // Tee position
    hole.teeX = startX;
    hole.teeZ = startZ;

    // Hole direction: courseDir controls forward/backward, alternate left/right
    float sway = ((number % 2 == 0) ? -0.3f : 0.3f) + (g_rand(g_rng) - 0.5f) * 0.4f;
    float baseAngle = courseDir + sway;

    // Compute hole position
    hole.holeX = hole.teeX + sinf(baseAngle) * scaledLength;
    hole.holeZ = hole.teeZ + cosf(baseAngle) * scaledLength;

    // Clamp hole position within terrain bounds (with margin)
    float margin = 40.0f;
    hole.holeX = std::max(margin, std::min((float)TERRAIN_SIZE - margin, hole.holeX));
    hole.holeZ = std::max(margin, std::min((float)TERRAIN_SIZE - margin, hole.holeZ));

    // Recalculate actual length after clamping
    float actualDist = distance2D(hole.teeX, hole.teeZ, hole.holeX, hole.holeZ);
    float holeAngle = atan2f(hole.holeX - hole.teeX, hole.holeZ - hole.teeZ);
    
    // Fairway path (curved for longer holes)
    hole.fairwayPath.clear();
    hole.fairwayPath.push_back(hole.teeX);
    hole.fairwayPath.push_back(hole.teeZ);
    
    if (hole.par >= 4) {
        // Add mid-point with curve
        float midT = 0.5f;
        float curve = (g_rand(g_rng) - 0.5f) * actualDist * 0.15f;
        float midX = hole.teeX + sinf(holeAngle) * actualDist * midT + cosf(holeAngle) * curve;
        float midZ = hole.teeZ + cosf(holeAngle) * actualDist * midT - sinf(holeAngle) * curve;
        hole.fairwayPath.push_back(midX);
        hole.fairwayPath.push_back(midZ);
    }
    
    hole.fairwayPath.push_back(hole.holeX);
    hole.fairwayPath.push_back(hole.holeZ);
    
    hole.fairwayWidth = 12.0f + g_rand(g_rng) * 6.0f;
    
    // Green
    hole.greenRadius = 8.0f + g_rand(g_rng) * 4.0f;
    hole.greenSlope = 0.01f + g_rand(g_rng) * 0.02f;
    hole.greenSlopeAngle = g_rand(g_rng) * 2 * PI;
    
    // Hazards
    hole.hazards.clear();
    int numHazards = 1 + (int)(g_rand(g_rng) * 3);
    for (int i = 0; i < numHazards; i++) {
        Hazard h;
        h.type = (g_rand(g_rng) < 0.6f) ? TerrainType::BUNKER : TerrainType::WATER;
        
        // Place hazard along fairway
        float t = 0.3f + g_rand(g_rng) * 0.5f;
        float pathX = hole.teeX + (hole.holeX - hole.teeX) * t;
        float pathZ = hole.teeZ + (hole.holeZ - hole.teeZ) * t;
        float offset = (g_rand(g_rng) - 0.5f) * hole.fairwayWidth * 2.5f;
        h.x = pathX + cosf(holeAngle) * offset;
        h.z = pathZ - sinf(holeAngle) * offset;
        h.radius = 4.0f + g_rand(g_rng) * 5.0f;
        
        hole.hazards.push_back(h);
    }
    
    // Greenside bunkers
    for (int i = 0; i < 2; i++) {
        if (g_rand(g_rng) < 0.7f) {
            Hazard h;
            h.type = TerrainType::BUNKER;
            float angle = g_rand(g_rng) * 2 * PI;
            h.x = hole.holeX + cosf(angle) * (hole.greenRadius + 4.0f);
            h.z = hole.holeZ + sinf(angle) * (hole.greenRadius + 4.0f);
            h.radius = 3.0f + g_rand(g_rng) * 2.0f;
            hole.hazards.push_back(h);
        }
    }
    
    // Trees (reject any that land on green, tee, or fairway)
    hole.trees.clear();
    if (number == 8 || number == 9) return;  // Holes 8 & 9: no trees
    int numTrees = 10 + (int)(g_rand(g_rng) * 20);
    int treeAttempts = 0;
    while ((int)hole.trees.size() < numTrees && treeAttempts < numTrees * 4) {
        treeAttempts++;
        Tree tree;
        float t = g_rand(g_rng);
        float pathX = hole.teeX + (hole.holeX - hole.teeX) * t;
        float pathZ = hole.teeZ + (hole.holeZ - hole.teeZ) * t;
        float offset = (g_rand(g_rng) < 0.5f ? -1 : 1) * (hole.fairwayWidth + 5.0f + g_rand(g_rng) * 15.0f);
        tree.x = pathX + cosf(holeAngle) * offset;
        tree.z = pathZ - sinf(holeAngle) * offset;
        tree.height = 4.0f + g_rand(g_rng) * 4.0f;
        // Reject trees too close to green
        if (distance2D(tree.x, tree.z, hole.holeX, hole.holeZ) < hole.greenRadius + 5.0f) continue;
        // Reject trees too close to tee
        if (distance2D(tree.x, tree.z, hole.teeX, hole.teeZ) < 8.0f) continue;
        hole.trees.push_back(tree);
    }
}

static const int NUM_HOLES = 18;

static void generateCourse() {
    g_holes.clear();
    g_holes.resize(NUM_HOLES);

    // Front 9 on the left side of the map, back 9 on the right
    float startX = TERRAIN_SIZE * 0.35f;
    float startZ = 60.0f;

    for (int i = 0; i < NUM_HOLES; i++) {
        float courseDir = (i < 9) ? 0.0f : PI;

        generateHole(g_holes[i], i + 1, startX, startZ, courseDir);

        // Gap to next tee: scale with hole length so short holes don't bunch
        float holeLen = distance2D(g_holes[i].teeX, g_holes[i].teeZ,
                                    g_holes[i].holeX, g_holes[i].holeZ);
        float gapForward = 25.0f + g_rand(g_rng) * 15.0f;
        float gapSide = (g_rand(g_rng) - 0.5f) * 15.0f;

        if (i == 8) {
            // Turn point: back 9 starts offset to the right, heading back
            startX = TERRAIN_SIZE * 0.65f;
            startZ = g_holes[i].holeZ - gapForward;
        } else if (i < 9) {
            startX = g_holes[i].holeX + gapSide;
            startZ = g_holes[i].holeZ + gapForward;
        } else {
            startX = g_holes[i].holeX + gapSide;
            startZ = g_holes[i].holeZ - gapForward;
        }

        float margin = 80.0f;
        startX = std::max(margin, std::min((float)TERRAIN_SIZE - margin, startX));
        startZ = std::max(margin, std::min((float)TERRAIN_SIZE - margin, startZ));
    }
}

static void generateTerrainMaps() {
    g_heightMap.resize(TERRAIN_SIZE);
    g_terrainMap.resize(TERRAIN_SIZE);
    
    for (int z = 0; z < TERRAIN_SIZE; z++) {
        g_heightMap[z].resize(TERRAIN_SIZE);
        g_terrainMap[z].resize(TERRAIN_SIZE);
        
        for (int x = 0; x < TERRAIN_SIZE; x++) {
            // Base height from noise
            float h = fractalNoise(x * 0.02f, z * 0.02f, 3) * 3.0f;
            g_heightMap[z][x] = h;
            g_terrainMap[z][x] = TerrainType::ROUGH;  // Default
        }
    }
    
    // Pass 1: Paint fairways and hazards for ALL holes
    for (const auto& hole : g_holes) {
        // Paint fairway along path
        for (size_t i = 0; i < hole.fairwayPath.size() - 2; i += 2) {
            float x1 = hole.fairwayPath[i], z1 = hole.fairwayPath[i + 1];
            float x2 = hole.fairwayPath[i + 2], z2 = hole.fairwayPath[i + 3];

            for (float t = 0; t < 1.0f; t += 0.02f) {
                float px = x1 + (x2 - x1) * t;
                float pz = z1 + (z2 - z1) * t;

                // Paint fairway area
                for (int dz = -15; dz <= 15; dz++) {
                    for (int dx = -15; dx <= 15; dx++) {
                        int ix = (int)px + dx;
                        int iz = (int)pz + dz;
                        if (ix >= 0 && ix < TERRAIN_SIZE && iz >= 0 && iz < TERRAIN_SIZE) {
                            float dist = sqrtf((float)(dx*dx + dz*dz));
                            // Slight mow-line undulation (very subtle)
                            float edgeNoise = fractalNoise(ix * 0.03f + 500.f, iz * 0.03f + 500.f, 2) * 1.0f;
                            if (dist < hole.fairwayWidth + edgeNoise) {
                                g_terrainMap[iz][ix] = TerrainType::FAIRWAY;
                                float edgeDist = (hole.fairwayWidth + edgeNoise) - dist;
                                float blend = std::min(1.f, edgeDist / 2.f);
                                g_heightMap[iz][ix] *= (1.f - 0.7f * blend);
                            }
                        }
                    }
                }
            }
        }

        // Paint hazards
        for (const auto& haz : hole.hazards) {
            int searchR = (int)haz.radius + 4;
            for (int dz = -searchR; dz <= searchR; dz++) {
                for (int dx = -searchR; dx <= searchR; dx++) {
                    int ix = (int)haz.x + dx;
                    int iz = (int)haz.z + dz;
                    if (ix >= 0 && ix < TERRAIN_SIZE && iz >= 0 && iz < TERRAIN_SIZE) {
                        float dist = sqrtf((float)(dx*dx + dz*dz));
                        // Slight lip shape, mostly clean edge
                        float hazNoise = fractalNoise(ix * 0.04f + 300.f, iz * 0.04f + 300.f, 2) * 0.5f;
                        if (dist < haz.radius + hazNoise) {
                            g_terrainMap[iz][ix] = haz.type;
                            float edgeDist = (haz.radius + hazNoise) - dist;
                            float blend = std::min(1.f, edgeDist / 1.5f);
                            if (haz.type == TerrainType::BUNKER) {
                                g_heightMap[iz][ix] -= 0.3f * blend;
                            } else if (haz.type == TerrainType::WATER) {
                                g_heightMap[iz][ix] = g_heightMap[iz][ix] * (1.f - blend) + (-0.5f) * blend;
                            }
                        }
                    }
                }
            }
        }
    }

    // Pass 2: Paint greens (with fringe collar) and tees for ALL holes
    // Greens/tees always win, even over adjacent holes' fairways.
    for (const auto& hole : g_holes) {
        // Paint fringe collar (fairway ring around green — 2 cells wide)
        int fringeSearchR = (int)hole.greenRadius + 4;
        for (int dz = -fringeSearchR; dz <= fringeSearchR; dz++) {
            for (int dx = -fringeSearchR; dx <= fringeSearchR; dx++) {
                int ix = (int)hole.holeX + dx;
                int iz = (int)hole.holeZ + dz;
                if (ix >= 0 && ix < TERRAIN_SIZE && iz >= 0 && iz < TERRAIN_SIZE) {
                    float dist = sqrtf((float)(dx*dx + dz*dz));
                    if (dist >= hole.greenRadius && dist < hole.greenRadius + 2.5f) {
                        if (g_terrainMap[iz][ix] == TerrainType::ROUGH)
                            g_terrainMap[iz][ix] = TerrainType::FAIRWAY;
                    }
                }
            }
        }
        // Paint green — clean circle, no noise (manicured edge)
        int greenSearchR = (int)hole.greenRadius + 1;
        for (int dz = -greenSearchR; dz <= greenSearchR; dz++) {
            for (int dx = -greenSearchR; dx <= greenSearchR; dx++) {
                int ix = (int)hole.holeX + dx;
                int iz = (int)hole.holeZ + dz;
                if (ix >= 0 && ix < TERRAIN_SIZE && iz >= 0 && iz < TERRAIN_SIZE) {
                    float dist = sqrtf((float)(dx*dx + dz*dz));
                    if (dist < hole.greenRadius) {
                        g_terrainMap[iz][ix] = TerrainType::GREEN;
                        float slopeX = cosf(hole.greenSlopeAngle);
                        float slopeZ = sinf(hole.greenSlopeAngle);
                        g_heightMap[iz][ix] = 0.05f + (dx * slopeX + dz * slopeZ) * hole.greenSlope;
                    }
                }
            }
        }

        // Paint tee box — clean rectangle, no noise (5x3 cells)
        for (int dz = -3; dz <= 3; dz++) {
            for (int dx = -5; dx <= 5; dx++) {
                int ix = (int)hole.teeX + dx;
                int iz = (int)hole.teeZ + dz;
                if (ix >= 0 && ix < TERRAIN_SIZE && iz >= 0 && iz < TERRAIN_SIZE) {
                    g_terrainMap[iz][ix] = TerrainType::TEE;
                    g_heightMap[iz][ix] = 0.1f;
                }
            }
        }
    }

    // Pass 3: Build blend weight map — tight 2-cell anti-alias radius.
    // Keeps borders crisp like real mow lines; just smooths the 1-cell staircase.
    static constexpr int BLEND_RADIUS = 2;
    g_blendMap.resize(TERRAIN_SIZE);
    for (int z = 0; z < TERRAIN_SIZE; z++) {
        g_blendMap[z].assign(TERRAIN_SIZE, 0.f);
    }
    for (int z = 1; z < TERRAIN_SIZE - 1; z++) {
        for (int x = 1; x < TERRAIN_SIZE - 1; x++) {
            TerrainType myType = g_terrainMap[z][x];
            float minDist = (float)(BLEND_RADIUS + 1);
            for (int dz2 = -BLEND_RADIUS; dz2 <= BLEND_RADIUS; dz2++) {
                for (int dx2 = -BLEND_RADIUS; dx2 <= BLEND_RADIUS; dx2++) {
                    int nx = x + dx2, nz = z + dz2;
                    if (nx < 0 || nx >= TERRAIN_SIZE || nz < 0 || nz >= TERRAIN_SIZE) continue;
                    if (g_terrainMap[nz][nx] != myType) {
                        float d = sqrtf((float)(dx2*dx2 + dz2*dz2));
                        if (d < minDist) minDist = d;
                    }
                }
            }
            if (minDist <= (float)BLEND_RADIUS) {
                float w = 1.f - minDist / (float)BLEND_RADIUS;
                g_blendMap[z][x] = w;
            }
        }
    }
    // Single light 3x3 blur — just anti-aliases the cell staircase, keeps edges sharp
    {
        std::vector<std::vector<float>> tmp(TERRAIN_SIZE, std::vector<float>(TERRAIN_SIZE, 0.f));
        for (int z = 1; z < TERRAIN_SIZE - 1; z++) {
            for (int x = 1; x < TERRAIN_SIZE - 1; x++) {
                float sum = 0;
                for (int dz2 = -1; dz2 <= 1; dz2++)
                    for (int dx2 = -1; dx2 <= 1; dx2++)
                        sum += g_blendMap[z+dz2][x+dx2];
                tmp[z][x] = sum / 9.f;
            }
        }
        g_blendMap = tmp;
    }
}

// ══════════════════════════════════════════════════════════════════════
// TERRAIN QUERIES
// ══════════════════════════════════════════════════════════════════════

static float getHeight(float x, float z) {
    int ix = (int)x, iz = (int)z;
    if (ix < 0 || ix >= TERRAIN_SIZE - 1 || iz < 0 || iz >= TERRAIN_SIZE - 1) return 0;
    float fx = x - ix, fz = z - iz;
    float h00 = g_heightMap[iz][ix];
    float h10 = g_heightMap[iz][ix + 1];
    float h01 = g_heightMap[iz + 1][ix];
    float h11 = g_heightMap[iz + 1][ix + 1];
    return h00*(1-fx)*(1-fz) + h10*fx*(1-fz) + h01*(1-fx)*fz + h11*fx*fz;
}

static TerrainType getTerrain(float x, float z) {
    int ix = (int)x, iz = (int)z;
    if (ix < 0 || ix >= TERRAIN_SIZE || iz < 0 || iz >= TERRAIN_SIZE) 
        return TerrainType::OUT_OF_BOUNDS;
    return g_terrainMap[iz][ix];
}

// Blended terrain properties for smooth roll physics at transition zones
static TerrainProperties getBlendedTerrainProps(float x, float z) {
    int ix = (int)x, iz = (int)z;
    if (ix < 1 || ix >= TERRAIN_SIZE - 1 || iz < 1 || iz >= TERRAIN_SIZE - 1)
        return TERRAIN_PROPS[(int)TerrainType::ROUGH];
    TerrainType myType = g_terrainMap[iz][ix];
    const TerrainProperties& myProps = TERRAIN_PROPS[(int)myType];
    float bw = g_blendMap[iz][ix];
    if (bw < 0.01f) return myProps;
    // Find dominant neighbor type
    int bestType = (int)myType;
    int bestCount = 0;
    for (int dz = -2; dz <= 2; dz++)
        for (int dx = -2; dx <= 2; dx++) {
            int nx = ix + dx, nz = iz + dz;
            if (nx < 0 || nx >= TERRAIN_SIZE || nz < 0 || nz >= TERRAIN_SIZE) continue;
            int nt = (int)g_terrainMap[nz][nx];
            if (nt != (int)myType) {
                int cnt = 0;
                for (int dz2 = -2; dz2 <= 2; dz2++)
                    for (int dx2 = -2; dx2 <= 2; dx2++) {
                        int nx2 = ix+dx2, nz2 = iz+dz2;
                        if (nx2 >= 0 && nx2 < TERRAIN_SIZE && nz2 >= 0 && nz2 < TERRAIN_SIZE)
                            if ((int)g_terrainMap[nz2][nx2] == nt) cnt++;
                    }
                if (cnt > bestCount) { bestCount = cnt; bestType = nt; }
            }
        }
    if (bestType == (int)myType) return myProps;
    const TerrainProperties& nbrProps = TERRAIN_PROPS[bestType];
    float t = bw * bw * (3.f - 2.f * bw);  // smoothstep
    TerrainProperties blended;
    blended.name = myProps.name;
    blended.friction = myProps.friction * (1.f - t) + nbrProps.friction * t;
    blended.bounce = myProps.bounce * (1.f - t) + nbrProps.bounce * t;
    blended.speedPenalty = myProps.speedPenalty * (1.f - t) + nbrProps.speedPenalty * t;
    blended.color[0] = myProps.color[0]; blended.color[1] = myProps.color[1]; blended.color[2] = myProps.color[2];
    return blended;
}

// ══════════════════════════════════════════════════════════════════════
// BALL PHYSICS
// ══════════════════════════════════════════════════════════════════════

static float g_dropTimer = 0.0f;
static bool g_dropping = false;
static bool g_ballWasMoving = false;

// ══════════════════════════════════════════════════════════════════════
// PARTICLE SYSTEM (Phase F: Ball Effects & Juice)
// ══════════════════════════════════════════════════════════════════════

struct Particle {
    float x,y,z, vx,vy,vz;
    float life, maxLife;
    float r,g,b,a;
    float size;
};

static std::vector<Particle> g_particles;
static constexpr int MAX_PARTICLES = 200;

// Trail ring buffer for flight trail
static constexpr int TRAIL_LEN = 40;
static float g_trail[TRAIL_LEN][3];
static int g_trailHead = 0, g_trailCount = 0;
static float g_trailTimer = 0.f;

static void spawnParticles(float x, float y, float z,
                           int count, float speed, float spread,
                           float r, float g, float b, float life, float sz) {
    for (int i = 0; i < count && (int)g_particles.size() < MAX_PARTICLES; i++) {
        Particle p;
        p.x = x; p.y = y; p.z = z;
        float a1 = g_rand(g_rng) * 2 * PI;
        float a2 = g_rand(g_rng) * spread;
        float sp = speed * (0.5f + g_rand(g_rng) * 0.5f);
        p.vx = cosf(a1) * sinf(a2) * sp;
        p.vy = cosf(a2) * sp + speed * 0.3f;
        p.vz = sinf(a1) * sinf(a2) * sp;
        p.life = life * (0.7f + g_rand(g_rng) * 0.3f);
        p.maxLife = p.life;
        p.r = r + (g_rand(g_rng) - 0.5f) * 0.1f;
        p.g = g + (g_rand(g_rng) - 0.5f) * 0.1f;
        p.b = b + (g_rand(g_rng) - 0.5f) * 0.1f;
        p.a = 1.f; p.size = sz;
        g_particles.push_back(p);
    }
}

static void updateParticles(float dt) {
    for (auto& p : g_particles) {
        p.vy -= 12.f * dt; // gravity
        p.x += p.vx * dt; p.y += p.vy * dt; p.z += p.vz * dt;
        p.life -= dt;
        p.a = std::max(0.f, p.life / p.maxLife);
    }
    g_particles.erase(std::remove_if(g_particles.begin(), g_particles.end(),
        [](const Particle& p) { return p.life <= 0; }), g_particles.end());
}

static void renderParticles() {
    if (g_particles.empty()) return;
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);
    for (const auto& p : g_particles) {
        glColor4f(p.r, p.g, p.b, p.a * 0.8f);
        float s = p.size * (0.5f + p.a * 0.5f);
        glBegin(GL_TRIANGLE_FAN);
        glVertex3f(p.x, p.y, p.z);
        for (int i = 0; i <= 6; i++) {
            float a = i / 6.f * 2 * PI;
            glVertex3f(p.x + cosf(a) * s, p.y + sinf(a) * s * 0.6f, p.z + sinf(a) * s);
        }
        glEnd();
    }
    glDisable(GL_BLEND);
}

static void renderFlightTrail() {
    if (g_trailCount < 2) return;
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < g_trailCount; i++) {
        int idx = (g_trailHead - g_trailCount + i + TRAIL_LEN) % TRAIL_LEN;
        float t = (float)i / g_trailCount;
        glColor4f(1.f, 1.f, 1.f, t * 0.4f);
        glVertex3f(g_trail[idx][0], g_trail[idx][1], g_trail[idx][2]);
    }
    glEnd();
    // Dots along trail
    glPointSize(3.f);
    glBegin(GL_POINTS);
    for (int i = 0; i < g_trailCount; i += 2) {
        int idx = (g_trailHead - g_trailCount + i + TRAIL_LEN) % TRAIL_LEN;
        float t = (float)i / g_trailCount;
        glColor4f(1.f, 1.f, 0.9f, t * 0.5f);
        glVertex3f(g_trail[idx][0], g_trail[idx][1], g_trail[idx][2]);
    }
    glEnd();
    glPointSize(1.f);
    glDisable(GL_BLEND);
}

static void renderRollingDust() {
    if (g_ball.inHole || g_ball.inFlight) return;
    float spd = sqrtf(g_ball.vx*g_ball.vx + g_ball.vz*g_ball.vz);
    if (spd < 1.5f) return;
    if (g_ball.terrain == TerrainType::GREEN || g_ball.terrain == TerrainType::WATER) return;
    float alpha = std::min(0.25f, (spd - 1.5f) * 0.04f);
    float gy = getHeight(g_ball.x, g_ball.z) + 0.01f;
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // Small fading puff behind ball
    float bx = g_ball.x - g_ball.vx * 0.05f;
    float bz = g_ball.z - g_ball.vz * 0.05f;
    float dustR = (g_ball.terrain == TerrainType::BUNKER) ? 0.85f : 0.55f;
    float dustG = (g_ball.terrain == TerrainType::BUNKER) ? 0.75f : 0.50f;
    float dustB = (g_ball.terrain == TerrainType::BUNKER) ? 0.45f : 0.35f;
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(dustR, dustG, dustB, alpha);
    glVertex3f(bx, gy + 0.03f, bz);
    glColor4f(dustR, dustG, dustB, 0.f);
    for (int i = 0; i <= 8; i++) {
        float a = i / 8.f * 2 * PI;
        float r = 0.08f + spd * 0.01f;
        glVertex3f(bx + cosf(a) * r, gy + 0.03f, bz + sinf(a) * r);
    }
    glEnd();
    glDisable(GL_BLEND);
}

static void resetBall() {
    const Hole& hole = g_holes[g_currentHole];
    g_ball.x = hole.teeX;
    g_ball.z = hole.teeZ;
    g_ball.y = getHeight(g_ball.x, g_ball.z) + 0.02f;
    g_ball.vx = g_ball.vy = g_ball.vz = 0;
    g_ball.spin = 0;
    g_ball.inFlight = false;
    g_ball.inHole = false;
    g_ball.inWater = false;
    g_ball.outOfBounds = false;
    g_ball.terrain = TerrainType::TEE;
    g_holeComplete = false;
    g_dropping = false;
    g_dropTimer = 0.0f;
    g_ballWasMoving = false;

    // Reset aim toward hole
    g_aimAngle = atan2f(hole.holeX - hole.teeX, hole.holeZ - hole.teeZ);

    // Reset camera orbit
    g_camDist = 15.0f;
    g_camOffsetX = 0.0f;  // Orbit angle offset
}

static void hitBall(float power, float angle) {
    if (g_ball.inHole) return;
    
    g_strokes[g_currentHole]++;
    g_totalStrokes++;
    
    // Club selection affects trajectory (simplified)
    float launchAngle = 0.4f;  // ~23 degrees default
    TerrainType terrain = getTerrain(g_ball.x, g_ball.z);
    
    // Adjust for terrain
    const TerrainProperties& props = TERRAIN_PROPS[(int)terrain];
    float terrainPenalty = 1.0f - props.friction * 2.0f;
    terrainPenalty = std::max(0.5f, terrainPenalty);
    
    // Putting on green
    if (terrain == TerrainType::GREEN && power < 15.0f) {
        launchAngle = 0.02f;  // Very low for putting
    }
    
    float speed = power * terrainPenalty;
    
    g_ball.vx = sinf(angle) * cosf(launchAngle) * speed;
    g_ball.vz = cosf(angle) * cosf(launchAngle) * speed;
    g_ball.vy = sinf(launchAngle) * speed;
    g_ball.spin = 0.5f;  // Backspin
    g_ball.inFlight = true;
    g_ballWasMoving = true;

    g_swingTimer = 0.5f;

    // Phase F: Swing kick particles
    float kickR = (terrain == TerrainType::BUNKER) ? 0.85f : 0.45f;
    float kickG = (terrain == TerrainType::BUNKER) ? 0.75f : 0.55f;
    float kickB = (terrain == TerrainType::BUNKER) ? 0.45f : 0.30f;
    int kickN = (terrain == TerrainType::BUNKER) ? 18 : 10;
    spawnParticles(g_ball.x, g_ball.y, g_ball.z, kickN, 3.f, 1.2f, kickR, kickG, kickB, 0.6f, 0.04f);
    // Reset trail
    g_trailHead = 0; g_trailCount = 0; g_trailTimer = 0.f;
}

static void updateBall(float dt) {
    if (g_ball.inHole) return;

    const Hole& hole = g_holes[g_currentHole];

    float distToHole = distance2D(g_ball.x, g_ball.z, hole.holeX, hole.holeZ);
    float ballSpeed = sqrtf(g_ball.vx*g_ball.vx + g_ball.vz*g_ball.vz);

    // Ball dropping into hole animation
    if (g_dropping) {
        g_dropTimer += dt;
        float t = std::min(g_dropTimer / 0.6f, 1.0f);
        // Pull toward center and sink
        g_ball.x += (hole.holeX - g_ball.x) * dt * 15.0f;
        g_ball.z += (hole.holeZ - g_ball.z) * dt * 15.0f;
        float groundY = getHeight(hole.holeX, hole.holeZ);
        g_ball.y = groundY + 0.02f - t * 0.15f;  // Sink below ground
        g_ball.vx = g_ball.vy = g_ball.vz = 0;
        if (g_dropTimer > 0.6f) {
            g_ball.inHole = true;
            g_ball.x = hole.holeX;
            g_ball.z = hole.holeZ;
            g_ball.y = groundY - 0.15f;
            g_holeComplete = true;
            g_dropping = false;
            g_dropTimer = 0.0f;
        }
        return;
    }

    // Hole cup radius (gameplay-scaled: ~1.5 feet at 0.3 units/yard)
    const float cupRadius = 0.15f;
    // Attraction zone (~3 feet around the cup)
    const float attractRadius = 0.5f;

    // Hole attraction: subtle gravity pull when ball is very close and rolling
    if (!g_ball.inFlight && distToHole < attractRadius) {
        float pullFactor = (attractRadius - distToHole) / attractRadius;
        float attractStrength = pullFactor * pullFactor * 12.0f;
        float dirX = (hole.holeX - g_ball.x) / std::max(distToHole, 0.001f);
        float dirZ = (hole.holeZ - g_ball.z) / std::max(distToHole, 0.001f);
        g_ball.vx += dirX * attractStrength * dt;
        g_ball.vz += dirZ * attractStrength * dt;
    }

    // Ball drops in: within cup and speed is catchable
    if (distToHole < cupRadius && !g_ball.inFlight) {
        if (ballSpeed < 4.0f) {
            // Ball caught by the cup
            g_dropping = true;
            g_dropTimer = 0.0f;
            g_ball.vx = g_ball.vy = g_ball.vz = 0;
            return;
        } else {
            // Lip-out: too fast, ball skips over
            g_ball.vx *= 0.6f;
            g_ball.vz *= 0.6f;
        }
    }
    
    float groundY = getHeight(g_ball.x, g_ball.z);
    TerrainType terrain = getTerrain(g_ball.x, g_ball.z);
    g_ball.terrain = terrain;
    
    if (terrain == TerrainType::WATER) {
        // Phase F: Water splash
        spawnParticles(g_ball.x, g_ball.y + 0.1f, g_ball.z, 20, 4.f, 1.0f, 0.5f, 0.7f, 0.95f, 0.7f, 0.05f);
        spawnParticles(g_ball.x, g_ball.y + 0.05f, g_ball.z, 8, 2.f, 0.6f, 0.9f, 0.95f, 1.f, 0.5f, 0.03f);
        g_ball.inWater = true;
        g_ball.vx = g_ball.vy = g_ball.vz = 0;
        // Penalty stroke and reset
        g_strokes[g_currentHole]++;
        g_totalStrokes++;
        resetBall();
        return;
    }
    
    if (terrain == TerrainType::OUT_OF_BOUNDS) {
        g_ball.outOfBounds = true;
        g_strokes[g_currentHole]++;
        g_totalStrokes++;
        resetBall();
        return;
    }
    
    const TerrainProperties& props = TERRAIN_PROPS[(int)terrain];
    TerrainProperties blended = getBlendedTerrainProps(g_ball.x, g_ball.z);

    if (g_ball.inFlight) {
        // Gravity
        g_ball.vy -= 30.0f * dt;

        // Wind
        g_ball.vx += g_windSpeed * cosf(g_windAngle) * dt * 0.5f;
        g_ball.vz += g_windSpeed * sinf(g_windAngle) * dt * 0.5f;

        // Air resistance
        float drag = 0.01f;
        g_ball.vx *= (1.0f - drag);
        g_ball.vy *= (1.0f - drag);
        g_ball.vz *= (1.0f - drag);

        // Move
        g_ball.x += g_ball.vx * dt;
        g_ball.y += g_ball.vy * dt;
        g_ball.z += g_ball.vz * dt;

        // Phase F: Record flight trail
        g_trailTimer += dt;
        if (g_trailTimer >= 0.03f) {
            g_trailTimer = 0.f;
            g_trail[g_trailHead][0] = g_ball.x;
            g_trail[g_trailHead][1] = g_ball.y;
            g_trail[g_trailHead][2] = g_ball.z;
            g_trailHead = (g_trailHead + 1) % TRAIL_LEN;
            if (g_trailCount < TRAIL_LEN) g_trailCount++;
        }
        
        // Ground collision
        if (g_ball.y <= groundY + 0.02f) {
            g_ball.y = groundY + 0.02f;
            
            if (g_ball.vy < -1.0f) {
                // Bounce — use blended properties at transition zones
                g_ball.vy = -g_ball.vy * blended.bounce;
                g_ball.vx *= (0.7f + blended.speedPenalty * 0.1f);
                g_ball.vz *= (0.7f + blended.speedPenalty * 0.1f);

                // Spin effect
                g_ball.vz -= g_ball.spin * 2.0f;
                g_ball.spin *= 0.5f;

                // Phase F: Landing puff
                float pR = (terrain == TerrainType::BUNKER) ? 0.85f : 0.4f;
                float pG = (terrain == TerrainType::BUNKER) ? 0.75f : 0.5f;
                float pB = (terrain == TerrainType::BUNKER) ? 0.45f : 0.25f;
                int pN = (terrain == TerrainType::BUNKER) ? 14 : 8;
                spawnParticles(g_ball.x, groundY + 0.05f, g_ball.z, pN, 2.5f, 1.3f, pR, pG, pB, 0.5f, 0.035f);
            } else {
                // Start rolling
                g_ball.inFlight = false;
                g_ball.vy = 0;
                // Phase F: Clear trail on landing
                g_trailCount = 0; g_trailHead = 0;
            }
        }
    } else {
        // Rolling
        g_ball.y = groundY + 0.02f;
        
        // Terrain slope affects roll
        float hL = getHeight(g_ball.x - 0.1f, g_ball.z);
        float hR = getHeight(g_ball.x + 0.1f, g_ball.z);
        float hU = getHeight(g_ball.x, g_ball.z - 0.1f);
        float hD = getHeight(g_ball.x, g_ball.z + 0.1f);
        
        float slopeX = (hR - hL) * 5.0f;
        float slopeZ = (hD - hU) * 5.0f;
        
        g_ball.vx += slopeX * dt * 10.0f;
        g_ball.vz += slopeZ * dt * 10.0f;
        
        // Friction — blended at transition zones so roll matches visual
        g_ball.vx *= blended.speedPenalty;
        g_ball.vz *= blended.speedPenalty;
        
        // Move
        g_ball.x += g_ball.vx * dt;
        g_ball.z += g_ball.vz * dt;
        
        // Stop if slow enough
        if (ballSpeed < 0.1f) {
            g_ball.vx = g_ball.vz = 0;
            // Ball just came to rest — re-aim at hole and reset camera
            if (g_ballWasMoving) {
                g_ballWasMoving = false;
                g_aimAngle = atan2f(hole.holeX - g_ball.x, hole.holeZ - g_ball.z);
                g_camOffsetX = 0.0f;  // Reset orbit behind ball
            }
        } else {
            g_ballWasMoving = true;
        }
    }
}

// ══════════════════════════════════════════════════════════════════════
// RENDERING
// ══════════════════════════════════════════════════════════════════════

static void perspectiveGL(double fov, double asp, double zn, double zf) {
    double f = 1.0 / tan(fov * PI / 360.0);
    double m[16] = {};
    m[0] = f / asp; m[5] = f;
    m[10] = (zf + zn) / (zn - zf); m[11] = -1;
    m[14] = 2 * zf * zn / (zn - zf);
    glMultMatrixd(m);
}

static void lookAtGL(float ex, float ey, float ez, float cx, float cy, float cz) {
    float fx = cx - ex, fy = cy - ey, fz = cz - ez;
    float fl = sqrtf(fx*fx + fy*fy + fz*fz);
    fx /= fl; fy /= fl; fz /= fl;
    float sx = -fz, sy = 0, sz = fx;
    float sl = sqrtf(sx*sx + sz*sz);
    if (sl > 0) { sx /= sl; sz /= sl; }
    float ux = sy*fz - sz*fy, uy = sz*fx - sx*fz, uz = sx*fy - sy*fx;
    float m[16] = {sx, ux, -fx, 0, sy, uy, -fy, 0, sz, uz, -fz, 0, 0, 0, 0, 1};
    glMultMatrixf(m);
    glTranslatef(-ex, -ey, -ez);
}

static void buildTerrainVBOs(int camIX, int camIZ) {
    int ddx = camIX - g_lastCamIX, ddz = camIZ - g_lastCamIZ;
    if (ddx*ddx + ddz*ddz < 4) return;
    g_lastCamIX = camIX;
    g_lastCamIZ = camIZ;
    for (int terrType = 0; terrType < NUM_TERRAIN_TYPES; terrType++) {
        for (int ring = 0; ring < NUM_LOD_RINGS; ring++) {
            int outerR = g_lodRings[ring].radius;
            int innerR = (ring > 0) ? g_lodRings[ring - 1].radius : 0;
            int step = g_lodRings[ring].step;
            int minX = std::max(0, ((camIX - outerR) / step) * step);
            int maxX = std::min(TERRAIN_SIZE - step - 1, camIX + outerR);
            int minZ = std::max(0, ((camIZ - outerR) / step) * step);
            int maxZ = std::min(TERRAIN_SIZE - step - 1, camIZ + outerR);
            int maxTiles = ((maxX - minX) / step + 1) * ((maxZ - minZ) / step + 1);
            std::vector<TerrainVertex> verts;
            verts.reserve(maxTiles * 6);
            for (int z = minZ; z < maxZ; z += step) {
                for (int x = minX; x < maxX; x += step) {
                    TerrainType t = g_terrainMap[z][x];
                    if ((int)t != terrType) continue;
                    if (innerR > 0) {
                        int dx2 = x + step/2 - camIX;
                        int dz2 = z + step/2 - camIZ;
                        if (dx2 > -innerR && dx2 < innerR && dz2 > -innerR && dz2 < innerR) continue;
                    }
                    float h00 = g_heightMap[z][x];
                    float h10 = g_heightMap[z][x + step];
                    float h01 = g_heightMap[z + step][x];
                    float h11 = g_heightMap[z + step][x + step];
                    float u0 = x / g_texScale, u1 = (x + step) / g_texScale;
                    float v0 = z / g_texScale, v1 = (z + step) / g_texScale;
                    float nx00,ny00,nz00, nx10,ny10,nz10, nx01,ny01,nz01, nx11,ny11,nz11;
                    getVertexNormal(x, z, nx00, ny00, nz00);
                    getVertexNormal(std::min(x+step,TERRAIN_SIZE-1), z, nx10, ny10, nz10);
                    getVertexNormal(x, std::min(z+step,TERRAIN_SIZE-1), nx01, ny01, nz01);
                    getVertexNormal(std::min(x+step,TERRAIN_SIZE-1), std::min(z+step,TERRAIN_SIZE-1), nx11, ny11, nz11);
                    verts.push_back({(float)x,        h00, (float)z,        nx00,ny00,nz00, u0, v0});
                    verts.push_back({(float)(x+step), h10, (float)z,        nx10,ny10,nz10, u1, v0});
                    verts.push_back({(float)x,        h01, (float)(z+step), nx01,ny01,nz01, u0, v1});
                    verts.push_back({(float)(x+step), h10, (float)z,        nx10,ny10,nz10, u1, v0});
                    verts.push_back({(float)(x+step), h11, (float)(z+step), nx11,ny11,nz11, u1, v1});
                    verts.push_back({(float)x,        h01, (float)(z+step), nx01,ny01,nz01, u0, v1});
                }
            }
            g_terrainVertCount[terrType][ring] = (int)verts.size();
            if (!g_terrainVBO[terrType][ring])
                glGenBuffers(1, &g_terrainVBO[terrType][ring]);
            glBindBuffer(GL_ARRAY_BUFFER, g_terrainVBO[terrType][ring]);
            glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(TerrainVertex), verts.data(), GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
    }
}

// ══════════════════════════════════════════════════════════════════════
// GLSL SHADER PIPELINE (Phase A: Per-Pixel Terrain + Animated Water)
// ══════════════════════════════════════════════════════════════════════

static GLuint g_golfTerrainProg=0, g_golfWaterProg=0;
static GLint g_gtLocSun,g_gtLocAmb,g_gtLocFogD,g_gtLocFogC,g_gtLocTex;
static GLint g_gwLocSun,g_gwLocAmb,g_gwLocFogD,g_gwLocFogC,g_gwLocTex,g_gwLocTime;

static GLuint golfCompileShader(GLenum type,const char*src){
    GLuint s=glCreateShader(type);
    glShaderSource(s,1,&src,nullptr);glCompileShader(s);
    GLint ok;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[512];glGetShaderInfoLog(s,512,nullptr,log);fprintf(stderr,"Golf shader: %s\n",log);}
    return s;
}
static GLuint golfBuildProgram(const char*vs,const char*fs){
    GLuint v=golfCompileShader(GL_VERTEX_SHADER,vs),f=golfCompileShader(GL_FRAGMENT_SHADER,fs);
    GLuint p=glCreateProgram();
    glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);
    GLint ok;glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){char log[512];glGetProgramInfoLog(p,512,nullptr,log);fprintf(stderr,"Golf link: %s\n",log);}
    glDeleteShader(v);glDeleteShader(f);return p;
}

static const char* GOLF_TERRAIN_VS=R"(
#version 120
varying vec3 v_norm,v_wpos;varying vec2 v_uv;varying float v_fogD;
void main(){
    gl_Position=gl_ModelViewProjectionMatrix*gl_Vertex;
    v_norm=normalize(gl_NormalMatrix*gl_Normal);
    v_wpos=gl_Vertex.xyz;
    v_uv=gl_MultiTexCoord0.xy;
    v_fogD=length((gl_ModelViewMatrix*gl_Vertex).xyz);
})";

static const char* GOLF_TERRAIN_FS=R"(
#version 120
uniform sampler2D u_tex;uniform vec3 u_sun;uniform float u_amb,u_fogD;uniform vec3 u_fogC;
varying vec3 v_norm,v_wpos;varying vec2 v_uv;varying float v_fogD;
void main(){
    vec4 tc=texture2D(u_tex,v_uv);
    vec3 n=normalize(v_norm);
    float diff=max(dot(n,u_sun),0.);
    float light=u_amb+diff*(1.-u_amb*.3);
    vec3 h=normalize(u_sun+vec3(0,1,0));
    float spec=pow(max(dot(n,h),0.),32.)*.1;
    vec3 c=tc.rgb*light+vec3(1.)*spec;
    float fd=u_fogD*v_fogD;
    float f=clamp(exp(-fd*fd),0.,1.);
    gl_FragColor=vec4(mix(u_fogC,c,f),1.);
})";

static const char* GOLF_WATER_VS=R"(
#version 120
uniform float u_time;
varying vec3 v_wpos;varying vec2 v_uv;varying float v_fogD;
void main(){
    vec3 p=gl_Vertex.xyz;
    float t=u_time;
    p.y+=sin(p.x*.8+t*1.8)*.04+sin(p.z*.6+t*1.4)*.035+sin((p.x+p.z)*.5+t*2.2)*.02;
    gl_Position=gl_ModelViewProjectionMatrix*vec4(p,1.);
    v_wpos=p;
    v_uv=gl_MultiTexCoord0.xy;
    v_fogD=length((gl_ModelViewMatrix*vec4(p,1.)).xyz);
})";

static const char* GOLF_WATER_FS=R"(
#version 120
uniform sampler2D u_tex;uniform vec3 u_sun;uniform float u_amb,u_fogD,u_time;uniform vec3 u_fogC;
varying vec3 v_wpos;varying vec2 v_uv;varying float v_fogD;
void main(){
    float t=u_time;
    float dx=cos(v_wpos.x*.5+t*2.)*.06+cos((v_wpos.x+v_wpos.z)*.3+t*2.5)*.03;
    float dz=cos(v_wpos.z*.7+t*1.5)*.06;
    vec3 n=normalize(vec3(-dx,1.,-dz));
    vec2 uv=v_uv+vec2(sin(t*.3+v_wpos.z*.1)*.03,cos(t*.2+v_wpos.x*.1)*.03);
    vec4 tc=texture2D(u_tex,uv);
    float diff=max(dot(n,u_sun),0.);
    vec3 refl=reflect(-u_sun,n);
    float spec=pow(max(dot(refl,vec3(0,1,0)),0.),96.);
    float caustic=abs(sin(v_wpos.x*2.+t*1.5)*sin(v_wpos.z*2.3+t*1.2))*.08;
    vec3 deep=vec3(.08,.22,.45);vec3 shallow=vec3(.15,.4,.55);
    float depthT=clamp(sin(v_wpos.x*.1+v_wpos.z*.1)*.5+.5,0.,1.);
    vec3 wc=mix(deep,shallow,depthT);
    vec3 c=mix(wc,tc.rgb,.3)*(u_amb+diff*.6)+vec3(1.)*spec*.7+vec3(caustic);
    float fd=u_fogD*v_fogD;
    float f=clamp(exp(-fd*fd),0.,1.);
    gl_FragColor=vec4(mix(u_fogC,c,f),.65);
})";

static void initGolfShaders(){
    if(!g_hasShaders)return;
    g_golfTerrainProg=golfBuildProgram(GOLF_TERRAIN_VS,GOLF_TERRAIN_FS);
    g_gtLocTex=glGetUniformLocation(g_golfTerrainProg,"u_tex");
    g_gtLocSun=glGetUniformLocation(g_golfTerrainProg,"u_sun");
    g_gtLocAmb=glGetUniformLocation(g_golfTerrainProg,"u_amb");
    g_gtLocFogD=glGetUniformLocation(g_golfTerrainProg,"u_fogD");
    g_gtLocFogC=glGetUniformLocation(g_golfTerrainProg,"u_fogC");
    g_golfWaterProg=golfBuildProgram(GOLF_WATER_VS,GOLF_WATER_FS);
    g_gwLocTex=glGetUniformLocation(g_golfWaterProg,"u_tex");
    g_gwLocSun=glGetUniformLocation(g_golfWaterProg,"u_sun");
    g_gwLocAmb=glGetUniformLocation(g_golfWaterProg,"u_amb");
    g_gwLocFogD=glGetUniformLocation(g_golfWaterProg,"u_fogD");
    g_gwLocFogC=glGetUniformLocation(g_golfWaterProg,"u_fogC");
    g_gwLocTime=glGetUniformLocation(g_golfWaterProg,"u_time");
}

static void renderTerrain() {
    int camIX = (int)g_camX, camIZ = (int)g_camZ;
    buildTerrainVBOs(camIX, camIZ);

    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    bool useShader = g_hasShaders && g_golfTerrainProg;

    if (useShader) {
        glUseProgram(g_golfTerrainProg);
        glUniform1i(g_gtLocTex, 0);
        glUniform3f(g_gtLocSun, g_slx, g_sly, g_slz);
        glUniform1f(g_gtLocAmb, 0.4f);
        glUniform1f(g_gtLocFogD, 0.003f);
        { float fr,fg,fb; getGolfSkyColors(fr,fg,fb); glUniform3f(g_gtLocFogC, fr,fg,fb); }
    } else {
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glEnable(GL_LIGHTING); glEnable(GL_LIGHT0);
        float lp[4]={g_slx,g_sly,g_slz,0.f}; glLightfv(GL_LIGHT0,GL_POSITION,lp);
        float am[4]={0.4f,0.4f,0.4f,1.f}; glLightfv(GL_LIGHT0,GL_AMBIENT,am);
        float df[4]={0.6f,0.6f,0.6f,1.f}; glLightfv(GL_LIGHT0,GL_DIFFUSE,df);
        glEnable(GL_COLOR_MATERIAL); glColor3f(1,1,1);
    }

    // Render non-water terrain
    for (int terrType = 0; terrType < NUM_TERRAIN_TYPES; terrType++) {
        if (terrType == (int)TerrainType::WATER) continue;
        glBindTexture(GL_TEXTURE_2D, g_textures[terrType]);
        for (int ring = 0; ring < NUM_LOD_RINGS; ring++) {
            int count = g_terrainVertCount[terrType][ring];
            if (count == 0) continue;
            glBindBuffer(GL_ARRAY_BUFFER, g_terrainVBO[terrType][ring]);
            glVertexPointer(3, GL_FLOAT, sizeof(TerrainVertex), (void*)0);
            glNormalPointer(GL_FLOAT, sizeof(TerrainVertex), (void*)(3 * sizeof(float)));
            glTexCoordPointer(2, GL_FLOAT, sizeof(TerrainVertex), (void*)(6 * sizeof(float)));
            glDrawArrays(GL_TRIANGLES, 0, count);
        }
    }

    // Render water with animation shader
    if (useShader) {
        glUseProgram(g_golfWaterProg);
        glUniform1i(g_gwLocTex, 0);
        glUniform3f(g_gwLocSun, g_slx, g_sly, g_slz);
        glUniform1f(g_gwLocAmb, 0.4f);
        glUniform1f(g_gwLocFogD, 0.003f);
        { float fr,fg,fb; getGolfSkyColors(fr,fg,fb); glUniform3f(g_gwLocFogC, fr,fg,fb); }
        glUniform1f(g_gwLocTime, g_wallTime);
    }
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, g_textures[(int)TerrainType::WATER]);
    for (int ring = 0; ring < NUM_LOD_RINGS; ring++) {
        int count = g_terrainVertCount[(int)TerrainType::WATER][ring];
        if (count == 0) continue;
        glBindBuffer(GL_ARRAY_BUFFER, g_terrainVBO[(int)TerrainType::WATER][ring]);
        glVertexPointer(3, GL_FLOAT, sizeof(TerrainVertex), (void*)0);
        glNormalPointer(GL_FLOAT, sizeof(TerrainVertex), (void*)(3 * sizeof(float)));
        glTexCoordPointer(2, GL_FLOAT, sizeof(TerrainVertex), (void*)(6 * sizeof(float)));
        glDrawArrays(GL_TRIANGLES, 0, count);
    }
    glDisable(GL_BLEND);

    if (useShader) glUseProgram(0);
    else { glDisable(GL_COLOR_MATERIAL); glDisable(GL_LIGHTING); }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
}

// ══════════════════════════════════════════════════════════════════════
// TERRAIN TRANSITION BLEND OVERLAY (smooth texture boundaries)
// ══════════════════════════════════════════════════════════════════════

static void renderTerrainTransitions() {
    int range = 60;
    int cx = (int)g_camX, cz = (int)g_camZ;
    int minX = std::max(1, cx - range), maxX = std::min(TERRAIN_SIZE - 2, cx + range);
    int minZ = std::max(1, cz - range), maxZ = std::min(TERRAIN_SIZE - 2, cz + range);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    if (g_hasShaders) glUseProgram(0);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    // Narrow texture blend — only the immediate 1-2 border cells get feathered
    for (int nbrType = 0; nbrType < NUM_TERRAIN_TYPES; nbrType++) {
        if (nbrType == (int)TerrainType::WATER) continue;
        glBindTexture(GL_TEXTURE_2D, g_textures[nbrType]);
        glBegin(GL_TRIANGLES);
        for (int z = minZ; z < maxZ; z++) {
            for (int x = minX; x < maxX; x++) {
                int myType = (int)g_terrainMap[z][x];
                if (myType == nbrType) continue;
                float bw00 = g_blendMap[z][x];
                float bw10 = g_blendMap[z][x+1];
                float bw01 = g_blendMap[z+1][x];
                float bw11 = g_blendMap[z+1][x+1];
                if (bw00 < 0.02f && bw10 < 0.02f && bw01 < 0.02f && bw11 < 0.02f) continue;
                // Check immediate neighbors only (tight radius)
                bool hasNbr = false;
                for (int dz2 = -2; dz2 <= 2 && !hasNbr; dz2++)
                    for (int dx2 = -2; dx2 <= 2 && !hasNbr; dx2++) {
                        int nx2 = x+dx2, nz2 = z+dz2;
                        if (nx2 >= 0 && nx2 < TERRAIN_SIZE && nz2 >= 0 && nz2 < TERRAIN_SIZE)
                            if ((int)g_terrainMap[nz2][nx2] == nbrType) hasNbr = true;
                    }
                if (!hasNbr) continue;
                // Thin feathered edge — alpha ramps quickly, keeps border defined
                float a00 = std::min(1.f, bw00 * 1.8f) * 0.5f;
                float a10 = std::min(1.f, bw10 * 1.8f) * 0.5f;
                float a01 = std::min(1.f, bw01 * 1.8f) * 0.5f;
                float a11 = std::min(1.f, bw11 * 1.8f) * 0.5f;
                float yOff = 0.005f;
                float h00 = g_heightMap[z][x] + yOff;
                float h10 = g_heightMap[z][x+1] + yOff;
                float h01 = g_heightMap[z+1][x] + yOff;
                float h11 = g_heightMap[z+1][x+1] + yOff;
                float u0 = x / g_texScale, u1 = (x+1) / g_texScale;
                float v0 = z / g_texScale, v1 = (z+1) / g_texScale;
                glColor4f(1,1,1,a00); glTexCoord2f(u0,v0); glVertex3f((float)x, h00, (float)z);
                glColor4f(1,1,1,a10); glTexCoord2f(u1,v0); glVertex3f((float)(x+1), h10, (float)z);
                glColor4f(1,1,1,a01); glTexCoord2f(u0,v1); glVertex3f((float)x, h01, (float)(z+1));
                glColor4f(1,1,1,a10); glTexCoord2f(u1,v0); glVertex3f((float)(x+1), h10, (float)z);
                glColor4f(1,1,1,a11); glTexCoord2f(u1,v1); glVertex3f((float)(x+1), h11, (float)(z+1));
                glColor4f(1,1,1,a01); glTexCoord2f(u0,v1); glVertex3f((float)x, h01, (float)(z+1));
            }
        }
        glEnd();
    }

    // Render crisp border lines at green, bunker, and tee perimeters.
    // 4x subdivided edges — only draw actual boundary edges where type changes,
    // with height sampled at each sub-point for terrain-following precision.
    glDisable(GL_TEXTURE_2D);
    glLineWidth(2.0f);
    static constexpr int BORDER_SUB = 4;  // subdivisions per cell edge
    glBegin(GL_LINES);
    for (int z = minZ; z < maxZ; z++) {
        for (int x = minX; x < maxX; x++) {
            TerrainType myType = g_terrainMap[z][x];
            if (myType != TerrainType::GREEN && myType != TerrainType::BUNKER && myType != TerrainType::TEE) continue;
            if (myType == TerrainType::GREEN)
                glColor4f(0.10f, 0.32f, 0.08f, 0.8f);
            else if (myType == TerrainType::BUNKER)
                glColor4f(0.55f, 0.42f, 0.20f, 0.7f);
            else
                glColor4f(0.18f, 0.42f, 0.12f, 0.6f);
            // Check each of the 4 edges; draw subdivided line only where neighbor differs
            // Edge 0: top (z side), Edge 1: bottom (z+1 side), Edge 2: left (x side), Edge 3: right (x+1 side)
            struct { int dx,dz; float x0,z0,x1,z1; } edges[4] = {
                {0,-1, (float)x,(float)z,       (float)(x+1),(float)z},       // top
                {0, 1, (float)x,(float)(z+1),   (float)(x+1),(float)(z+1)},   // bottom
                {-1,0, (float)x,(float)z,       (float)x,(float)(z+1)},       // left
                {1, 0, (float)(x+1),(float)z,   (float)(x+1),(float)(z+1)},   // right
            };
            for (int e = 0; e < 4; e++) {
                int nx = x + edges[e].dx, nz = z + edges[e].dz;
                if (nx < 0 || nx >= TERRAIN_SIZE || nz < 0 || nz >= TERRAIN_SIZE) continue;
                if (g_terrainMap[nz][nx] == myType) continue;
                // Draw this edge subdivided into BORDER_SUB segments
                float ex0 = edges[e].x0, ez0 = edges[e].z0;
                float ex1 = edges[e].x1, ez1 = edges[e].z1;
                for (int s = 0; s < BORDER_SUB; s++) {
                    float t0 = (float)s / BORDER_SUB;
                    float t1 = (float)(s + 1) / BORDER_SUB;
                    float px0 = ex0 + (ex1 - ex0) * t0, pz0 = ez0 + (ez1 - ez0) * t0;
                    float px1 = ex0 + (ex1 - ex0) * t1, pz1 = ez0 + (ez1 - ez0) * t1;
                    float hy0 = getHeight(px0, pz0) + 0.015f;
                    float hy1 = getHeight(px1, pz1) + 0.015f;
                    glVertex3f(px0, hy0, pz0);
                    glVertex3f(px1, hy1, pz1);
                }
            }
        }
    }
    glEnd();
    glLineWidth(1.0f);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
    glColor4f(1,1,1,1);
}

static void renderHole(const Hole& h) {
    float holeY = getHeight(h.holeX, h.holeZ);
    float teeY = getHeight(h.teeX, h.teeZ);

    // Distance check — only render details for nearby holes
    float distToCam = distance2D(g_camX, g_camZ, h.holeX, h.holeZ);
    float distTeeToC = distance2D(g_camX, g_camZ, h.teeX, h.teeZ);
    float minDist = std::min(distToCam, distTeeToC);
    if (minDist > 200.0f) return;

    // ── Tee box markers ──
    if (distTeeToC < 100.0f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        // Tee box outline (matches 5x3 painted area)
        glColor4f(1.0f, 1.0f, 1.0f, 0.6f);
        glLineWidth(2.0f);
        glBegin(GL_LINE_LOOP);
        glVertex3f(h.teeX - 5.0f, teeY + 0.02f, h.teeZ - 3.0f);
        glVertex3f(h.teeX + 5.0f, teeY + 0.02f, h.teeZ - 3.0f);
        glVertex3f(h.teeX + 5.0f, teeY + 0.02f, h.teeZ + 3.0f);
        glVertex3f(h.teeX - 5.0f, teeY + 0.02f, h.teeZ + 3.0f);
        glEnd();
        // Tee markers (two small colored blocks at front edge)
        glColor4f(1.0f, 0.2f, 0.2f, 0.9f);
        for (float side = -4.2f; side <= 4.2f; side += 8.4f) {
            glBegin(GL_QUADS);
            glVertex3f(h.teeX + side - 0.1f, teeY + 0.01f, h.teeZ - 2.5f);
            glVertex3f(h.teeX + side + 0.1f, teeY + 0.01f, h.teeZ - 2.5f);
            glVertex3f(h.teeX + side + 0.1f, teeY + 0.15f, h.teeZ - 2.5f);
            glVertex3f(h.teeX + side - 0.1f, teeY + 0.15f, h.teeZ - 2.5f);
            glEnd();
        }
        glDisable(GL_BLEND);
        glLineWidth(1.0f);
    }

    // ── Flag pole (3D cylinder) ──
    glColor3f(0.85f, 0.85f, 0.85f);
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= 8; i++) {
        float a = i / 8.0f * 2 * PI;
        glVertex3f(h.holeX + cosf(a) * 0.025f, holeY, h.holeZ + sinf(a) * 0.025f);
        glVertex3f(h.holeX + cosf(a) * 0.02f, holeY + 3.0f, h.holeZ + sinf(a) * 0.02f);
    }
    glEnd();

    // Flag (waving effect)
    float wave = sinf((float)glfwGetTime() * 3.0f) * 0.15f;
    glColor3f(1.0f, 0.15f, 0.1f);
    glBegin(GL_TRIANGLES);
    glVertex3f(h.holeX, holeY + 3.0f, h.holeZ);
    glVertex3f(h.holeX, holeY + 2.4f, h.holeZ);
    glVertex3f(h.holeX + 1.0f + wave, holeY + 2.7f, h.holeZ + wave * 0.5f);
    glEnd();
    // Flag shadow side
    glColor3f(0.8f, 0.1f, 0.08f);
    glBegin(GL_TRIANGLES);
    glVertex3f(h.holeX, holeY + 2.4f, h.holeZ);
    glVertex3f(h.holeX + 1.0f + wave, holeY + 2.7f, h.holeZ + wave * 0.5f);
    glVertex3f(h.holeX + 0.5f + wave * 0.5f, holeY + 2.35f, h.holeZ + wave * 0.25f);
    glEnd();

    // Hole number on flag area (just a visual indicator)

    // ── Animated water hazard overlays ──
    for (const auto& haz : h.hazards) {
        if (haz.type != TerrainType::WATER) continue;
        float hd = distance2D(g_camX, g_camZ, haz.x, haz.z);
        if (hd > 120.0f) continue;
        float wy = getHeight(haz.x, haz.z) + 0.06f;
        int segs = (hd < 40.0f) ? 24 : 14;
        float r = haz.radius;
        // Shore foam ring
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.85f, 0.92f, 0.95f, 0.25f + 0.08f * sinf(g_wallTime * 2.0f));
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < segs; i++) {
            float a = i / (float)segs * 2 * PI;
            float wobble = 0.92f + 0.08f * sinf(a * 3.0f + g_wallTime * 1.5f);
            glVertex3f(haz.x + cosf(a) * r * wobble, wy + 0.01f, haz.z + sinf(a) * r * wobble);
        }
        glEnd();
        // Inner ripple rings
        for (int ring = 1; ring <= 2; ring++) {
            float rr = r * (0.35f + ring * 0.2f);
            float phase = g_wallTime * (1.5f + ring * 0.5f);
            float alpha = 0.12f + 0.06f * sinf(phase);
            glColor4f(0.6f, 0.8f, 0.95f, alpha);
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < segs; i++) {
                float a = i / (float)segs * 2 * PI;
                float rw = rr * (0.95f + 0.05f * sinf(a * 5.0f + phase));
                float dy = sinf(a * 3.0f + phase) * 0.02f;
                glVertex3f(haz.x + cosf(a) * rw, wy + dy, haz.z + sinf(a) * rw);
            }
            glEnd();
        }
        // Specular highlight disc
        float sunRefl = std::max(0.0f, g_sly) * 0.3f;
        if (sunRefl > 0.02f) {
            glColor4f(1.0f, 0.98f, 0.9f, sunRefl * (0.5f + 0.3f * sinf(g_wallTime * 3.0f)));
            float sr = r * 0.25f;
            float sx = haz.x + sinf(g_wallTime * 0.3f) * r * 0.15f;
            float sz = haz.z + cosf(g_wallTime * 0.4f) * r * 0.15f;
            glBegin(GL_TRIANGLE_FAN);
            glVertex3f(sx, wy + 0.02f, sz);
            for (int i = 0; i <= 10; i++) {
                float a = i / 10.0f * 2 * PI;
                glVertex3f(sx + cosf(a) * sr, wy + 0.02f, sz + sinf(a) * sr * 0.7f);
            }
            glEnd();
        }
        glDisable(GL_BLEND);
    }

    // ── Hole cup ──
    glColor3f(0.02f, 0.02f, 0.02f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(h.holeX, holeY - 0.04f, h.holeZ);
    for (int i = 0; i <= 28; i++) {
        float a = i / 28.0f * 2 * PI;
        glVertex3f(h.holeX + cosf(a) * 0.15f, holeY - 0.04f, h.holeZ + sinf(a) * 0.15f);
    }
    glEnd();

    // White rim
    glColor3f(1.0f, 1.0f, 1.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 28; i++) {
        float a = i / 28.0f * 2 * PI;
        glVertex3f(h.holeX + cosf(a) * 0.17f, holeY + 0.002f, h.holeZ + sinf(a) * 0.17f);
    }
    glEnd();
    glLineWidth(1.0f);

    // ── Green contour lines & slope arrow (Phase C) ──
    if (g_showContours && distToCam < 50.0f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        float gr = h.greenRadius;
        float slopeX = cosf(h.greenSlopeAngle);
        float slopeZ = sinf(h.greenSlopeAngle);
        float slopeMag = h.greenSlope;

        // Contour iso-height lines across the green using marching-squares-lite
        int numContours = 5;
        float hMin = -gr * slopeMag;
        float hMax =  gr * slopeMag;
        float hRange = hMax - hMin;
        if (hRange < 0.001f) hRange = 0.001f;
        int gridRes = (distToCam < 20.0f) ? 32 : 18;
        float step = gr * 2.0f / gridRes;

        for (int ci = 1; ci < numContours; ci++) {
            float isoH = hMin + hRange * ci / numContours;
            // Color: blue for low (downhill), white mid, red for high (uphill)
            float t = (float)ci / numContours;
            float cr, cg, cb;
            if (t < 0.5f) { float s = t * 2.0f; cr = 0.2f + 0.6f * s; cg = 0.4f + 0.5f * s; cb = 0.9f - 0.3f * s; }
            else { float s = (t - 0.5f) * 2.0f; cr = 0.8f + 0.2f * s; cg = 0.9f - 0.5f * s; cb = 0.6f - 0.4f * s; }
            glColor4f(cr, cg, cb, 0.55f);
            glLineWidth(1.5f);
            glBegin(GL_LINES);
            for (int gz = 0; gz < gridRes; gz++) {
                for (int gx = 0; gx < gridRes; gx++) {
                    float lx0 = -gr + gx * step, lz0 = -gr + gz * step;
                    float lx1 = lx0 + step, lz1 = lz0 + step;
                    // Heights at four corners (green slope model)
                    float h00 = (lx0 * slopeX + lz0 * slopeZ) * slopeMag;
                    float h10 = (lx1 * slopeX + lz0 * slopeZ) * slopeMag;
                    float h01 = (lx0 * slopeX + lz1 * slopeZ) * slopeMag;
                    float h11 = (lx1 * slopeX + lz1 * slopeZ) * slopeMag;
                    // For each edge, check if iso crosses
                    auto emitEdge = [&](float ax, float az, float ah, float bx, float bz, float bh) {
                        if ((ah - isoH) * (bh - isoH) < 0.0f) {
                            float ft = (isoH - ah) / (bh - ah);
                            float px = ax + ft * (bx - ax);
                            float pz = az + ft * (bz - az);
                            float d2 = px * px + pz * pz;
                            if (d2 < gr * gr) {
                                float wy = getHeight(h.holeX + px, h.holeZ + pz) + 0.015f;
                                glVertex3f(h.holeX + px, wy, h.holeZ + pz);
                            }
                        }
                    };
                    // Bottom edge
                    emitEdge(lx0, lz0, h00, lx1, lz0, h10);
                    // Top edge
                    emitEdge(lx0, lz1, h01, lx1, lz1, h11);
                    // Left edge
                    emitEdge(lx0, lz0, h00, lx0, lz1, h01);
                    // Right edge
                    emitEdge(lx1, lz0, h10, lx1, lz1, h11);
                }
            }
            glEnd();
        }
        glLineWidth(1.0f);

        // Slope direction arrow — shows downhill break direction
        float arrowLen = gr * 0.5f;
        float ax = h.holeX + slopeX * arrowLen;
        float az = h.holeZ + slopeZ * arrowLen;
        float ay = getHeight(ax, az) + 0.025f;
        float baseY = getHeight(h.holeX, h.holeZ) + 0.025f;
        // Arrow shaft
        glColor4f(1.0f, 1.0f, 0.2f, 0.7f);
        glLineWidth(2.5f);
        glBegin(GL_LINES);
        glVertex3f(h.holeX, baseY, h.holeZ);
        glVertex3f(ax, ay, az);
        glEnd();
        // Arrowhead
        float headLen = arrowLen * 0.25f;
        float perpX = -slopeZ, perpZ = slopeX;
        float tipX = ax, tipZ = az;
        float tipY = ay;
        glBegin(GL_TRIANGLES);
        glColor4f(1.0f, 0.9f, 0.1f, 0.8f);
        glVertex3f(tipX, tipY, tipZ);
        glVertex3f(tipX - slopeX * headLen + perpX * headLen * 0.4f, tipY, tipZ - slopeZ * headLen + perpZ * headLen * 0.4f);
        glVertex3f(tipX - slopeX * headLen - perpX * headLen * 0.4f, tipY, tipZ - slopeZ * headLen - perpZ * headLen * 0.4f);
        glEnd();
        glLineWidth(1.0f);

        // Green edge ring (subtle boundary highlight)
        glColor4f(0.9f, 1.0f, 0.9f, 0.2f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 36; i++) {
            float a = i / 36.0f * 2 * PI;
            float ex = h.holeX + cosf(a) * gr;
            float ez = h.holeZ + sinf(a) * gr;
            float ey = getHeight(ex, ez) + 0.01f;
            glVertex3f(ex, ey, ez);
        }
        glEnd();

        glDisable(GL_BLEND);
    }

    // ── Trees (LOD-driven foliage, wind sway, directional shadow) ──
    for (const auto& tree : h.trees) {
        float tdist = distance2D(g_camX, g_camZ, tree.x, tree.z);
        if (tdist > 150.0f) continue;

        float ty = getHeight(tree.x, tree.z);
        float th = tree.height;
        float trunkR = 0.12f + th * 0.02f;
        int shadowSegs = (tdist < 25.0f) ? 14 : 8;
        int trunkSegs  = (tdist < 25.0f) ? 12 : 8;

        // Wind sway (Phase E)
        float windStr = g_windSpeed * 0.012f;
        float treePhase = tree.x * 0.7f + tree.z * 0.5f; // per-tree variation
        float swayX = sinf(g_wallTime * 1.8f + treePhase) * windStr * cosf(g_windAngle);
        float swayZ = sinf(g_wallTime * 1.8f + treePhase) * windStr * sinf(g_windAngle);
        float swayX2 = swayX + sinf(g_wallTime * 3.1f + treePhase * 1.3f) * windStr * 0.3f;
        float swayZ2 = swayZ + cosf(g_wallTime * 2.7f + treePhase * 0.9f) * windStr * 0.3f;

        // Sun-directional ground shadow (Phase E)
        float shOff = th * 0.4f; // shadow offset proportional to height
        float shCX = tree.x - g_slx * shOff;
        float shCZ = tree.z - g_slz * shOff;
        float shRX = th * 0.38f, shRZ = th * 0.28f;
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBegin(GL_TRIANGLE_FAN);
        glColor4f(0.0f, 0.08f, 0.0f, 0.28f);
        glVertex3f(shCX + swayX * 0.5f, ty + 0.005f, shCZ + swayZ * 0.5f);
        glColor4f(0.0f, 0.08f, 0.0f, 0.0f);
        for (int i = 0; i <= shadowSegs; i++) {
            float a = i / (float)shadowSegs * 2 * PI;
            glVertex3f(shCX + swayX * 0.5f + cosf(a) * shRX, ty + 0.005f, shCZ + swayZ * 0.5f + sinf(a) * shRZ);
        }
        glEnd();
        glDisable(GL_BLEND);

        // Trunk (top sways slightly)
        glColor3f(0.45f, 0.28f, 0.12f);
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= trunkSegs; i++) {
            float a = i / (float)trunkSegs * 2 * PI;
            float shade = 0.8f + 0.2f * cosf(a);
            glColor3f(0.45f * shade, 0.28f * shade, 0.12f * shade);
            glVertex3f(tree.x + cosf(a) * trunkR, ty, tree.z + sinf(a) * trunkR);
            glVertex3f(tree.x + swayX * 0.3f + cosf(a) * trunkR * 0.6f, ty + th * 0.35f, tree.z + swayZ * 0.3f + sinf(a) * trunkR * 0.6f);
        }
        glEnd();

        // Layered foliage with wind sway (LOD-scaled)
        int foliageLayers = (tdist < 20.0f) ? 4 : (tdist < 60.0f) ? 3 : 2;
        int foliageSegs   = (tdist < 20.0f) ? 14 : (tdist < 60.0f) ? 10 : 8;
        for (int layer = 0; layer < foliageLayers; layer++) {
            float layerT = (float)layer / foliageLayers;
            float lsx = swayX + swayX2 * layerT; // upper layers sway more
            float lsz = swayZ + swayZ2 * layerT;
            float layerY = ty + th * (0.3f + layer * 0.22f);
            float layerR = th * (0.48f - layer * 0.09f);
            float green = 0.35f + layer * 0.07f;
            float dark = 0.08f + layer * 0.025f;

            glBegin(GL_TRIANGLE_FAN);
            glColor3f(dark, green + 0.05f, dark);
            glVertex3f(tree.x + lsx, layerY + th * 0.2f, tree.z + lsz);
            for (int i = 0; i <= foliageSegs; i++) {
                float a = i / (float)foliageSegs * 2 * PI;
                float r = layerR * (0.85f + 0.15f * sinf(a * 3.0f + (float)layer + g_wallTime * 0.5f));
                float shade = 0.7f + 0.3f * std::max(0.0f, cosf(a - 0.5f));
                glColor3f(dark * shade, green * shade, dark * shade);
                glVertex3f(tree.x + lsx + cosf(a) * r, layerY, tree.z + lsz + sinf(a) * r);
            }
            glEnd();
        }
    }
}

static void renderBall() {
    if (g_ball.inHole) return;

    float dx = g_camX - g_ball.x;
    float dz = g_camZ - g_ball.z;
    float distance = sqrtf(dx*dx + dz*dz);

    int segs = (distance < 10.0f) ? 16 : (distance < 40.0f) ? 10 : 6;
    float r = 0.04f;  // Slightly bigger for visibility

    // Ball shadow on ground (sun-directional, Phase E)
    float groundY = getHeight(g_ball.x, g_ball.z);
    float ballH = g_ball.y - groundY;
    float shScale = 1.0f + ballH * 0.15f; // shadow grows when airborne
    float shAlpha = std::max(0.05f, 0.35f - ballH * 0.012f); // fades with height
    float shOffX = -g_slx * ballH * 0.3f;
    float shOffZ = -g_slz * ballH * 0.3f;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(0.0f, 0.05f, 0.0f, shAlpha);
    glVertex3f(g_ball.x + shOffX, groundY + 0.003f, g_ball.z + shOffZ);
    glColor4f(0.0f, 0.05f, 0.0f, 0.0f);
    for (int i = 0; i <= 12; i++) {
        float a = i / 12.0f * 2 * PI;
        glVertex3f(g_ball.x + shOffX + cosf(a) * r * 2.0f * shScale, groundY + 0.003f, g_ball.z + shOffZ + sinf(a) * r * 1.5f * shScale);
    }
    glEnd();
    glDisable(GL_BLEND);

    // Ball sphere with shading
    glPushMatrix();
    glTranslatef(g_ball.x, g_ball.y, g_ball.z);

    for (int i = 0; i < segs; i++) {
        float a1 = i * PI / segs;
        float a2 = (i + 1) * PI / segs;
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= segs * 2; j++) {
            float b = j * PI / segs;
            // Simple shading: top = bright, sides = dimmer
            for (int k = 0; k < 2; k++) {
                float aa = (k == 0) ? a1 : a2;
                float nx = sinf(aa) * cosf(b);
                float ny = cosf(aa);
                float nz = sinf(aa) * sinf(b);
                float light = 0.55f + 0.45f * std::max(0.0f, 0.4f*nx + 0.8f*ny + 0.3f*nz);
                glColor3f(light, light, light);
                glVertex3f(nx * r, ny * r, nz * r);
            }
        }
        glEnd();
    }

    glPopMatrix();
}

static void renderAimArrow() {
    if (g_ball.inHole || g_ball.inFlight || sqrtf(g_ball.vx*g_ball.vx + g_ball.vz*g_ball.vz) > 0.5f) return;

    float arrowLen = 2.0f + g_power * 0.1f;
    float ax = g_ball.x + sinf(g_aimAngle) * arrowLen;
    float az = g_ball.z + cosf(g_aimAngle) * arrowLen;
    float ay = getHeight(ax, az) + 0.1f;

    // Power indicator color + Phase F glow pulse
    float powerRatio = g_power / g_maxPower;
    float pulse = 0.5f + 0.5f * sinf(g_wallTime * 6.f);
    float glow = powerRatio * pulse * 0.3f;
    glColor3f(std::min(1.f, powerRatio + glow), std::max(0.f, 1.0f - powerRatio - glow * 0.5f), glow * 0.5f);

    // Thicker line at high power (draw multiple offset lines)
    glBegin(GL_LINES);
    glVertex3f(g_ball.x, g_ball.y + 0.05f, g_ball.z);
    glVertex3f(ax, ay, az);
    glEnd();
    if (powerRatio > 0.4f) {
        float off = 0.02f + powerRatio * 0.02f;
        glColor4f(std::min(1.f, powerRatio + glow), std::max(0.f, 1.f - powerRatio), 0.f, 0.4f);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBegin(GL_LINES);
        glVertex3f(g_ball.x + off, g_ball.y + 0.05f, g_ball.z);
        glVertex3f(ax + off, ay, az);
        glVertex3f(g_ball.x - off, g_ball.y + 0.05f, g_ball.z);
        glVertex3f(ax - off, ay, az);
        glEnd();
        glDisable(GL_BLEND);
    }

    // Arrow head
    float hr = std::min(1.f, powerRatio + glow);
    float hg = std::max(0.f, 1.0f - powerRatio - glow * 0.5f);
    glColor3f(hr, hg, glow * 0.5f);
    glBegin(GL_TRIANGLES);
    glVertex3f(ax, ay, az);
    glVertex3f(ax + sinf(g_aimAngle + PI + 0.3f) * 0.3f, ay, az + cosf(g_aimAngle + PI + 0.3f) * 0.3f);
    glVertex3f(ax + sinf(g_aimAngle + PI - 0.3f) * 0.3f, ay, az + cosf(g_aimAngle + PI - 0.3f) * 0.3f);
    glEnd();
}

static void renderTrajectory() {
    if (!g_showTrajectory || g_ball.inHole || g_ball.inFlight) return;
    if (g_power < 1.0f) return;
    
    // Simulate trajectory
    float x = g_ball.x, y = g_ball.y, z = g_ball.z;
    float vx = sinf(g_aimAngle) * cosf(0.4f) * g_power;
    float vy = sinf(0.4f) * g_power;
    float vz = cosf(g_aimAngle) * cosf(0.4f) * g_power;
    
    glColor4f(1, 1, 0, 0.5f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_LINE_STRIP);
    
    for (int i = 0; i < 100; i++) {
        glVertex3f(x, y, z);
        
        vy -= 30.0f * 0.02f;
        x += vx * 0.02f;
        y += vy * 0.02f;
        z += vz * 0.02f;
        
        float groundY = getHeight(x, z);
        if (y <= groundY) break;
    }
    
    glEnd();
    glDisable(GL_BLEND);
}

static void renderWind() {
    // Wind indicator in corner
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    float wx = 0.85f, wy = 0.8f;
    float windLen = g_windSpeed * 0.02f;
    
    glColor3f(0.5f, 0.7f, 1.0f);
    glBegin(GL_LINES);
    glVertex2f(wx, wy);
    glVertex2f(wx + cosf(g_windAngle) * windLen, wy + sinf(g_windAngle) * windLen);
    glEnd();
    
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

static void renderUI(int winW, int winH) {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, winW, 0, winH, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    
    char buf[128];
    const Hole& hole = g_holes[g_currentHole];
    
    // Hole info
    float panelH = 120.0f;
    if (g_showContours && g_ball.terrain == TerrainType::GREEN) panelH = 155.0f;
    glColor4f(0, 0, 0, 0.6f);
    glEnable(GL_BLEND);
    glBegin(GL_QUADS);
    glVertex2f(10, winH - 10); glVertex2f(200, winH - 10);
    glVertex2f(200, winH - panelH); glVertex2f(10, winH - panelH);
    glEnd();
    
    glColor3f(1, 1, 1);
    float y = winH - 25.0f;
    
    snprintf(buf, sizeof(buf), "HOLE %d  Par %d", hole.number, hole.par);
    gltext::draw(buf, 15, y); y -= 18;
    
    snprintf(buf, sizeof(buf), "%d yards", (int)hole.length);
    gltext::draw(buf, 15, y); y -= 15;
    
    snprintf(buf, sizeof(buf), "Strokes: %d", g_strokes[g_currentHole]);
    gltext::draw(buf, 15, y); y -= 15;
    
    // Score relative to par
    int scoreToPar = g_strokes[g_currentHole] - hole.par;
    if (g_strokes[g_currentHole] > 0) {
        if (scoreToPar < 0) {
            glColor3f(0.3f, 1.0f, 0.3f);
            snprintf(buf, sizeof(buf), "%d under par", -scoreToPar);
        } else if (scoreToPar > 0) {
            glColor3f(1.0f, 0.5f, 0.3f);
            snprintf(buf, sizeof(buf), "%d over par", scoreToPar);
        } else {
            glColor3f(1, 1, 1);
            snprintf(buf, sizeof(buf), "Even par");
        }
        gltext::draw(buf, 15, y); y -= 15;
    }
    
    // Green reading info (Phase C)
    if (g_showContours && g_ball.terrain == TerrainType::GREEN) {
        float slopePct = hole.greenSlope * 100.0f;
        float slopeDeg = hole.greenSlopeAngle * 180.0f / PI;
        // Compass direction
        const char* dirs[] = {"N","NE","E","SE","S","SW","W","NW"};
        int di = ((int)(slopeDeg + 22.5f) % 360) / 45;
        if (di < 0) di += 8;
        const char* dir = dirs[di % 8];
        glColor3f(0.6f, 1.0f, 0.6f);
        snprintf(buf, sizeof(buf), "Break: %.1f%% %s", slopePct, dir);
        gltext::draw(buf, 15, y); y -= 15;
        snprintf(buf, sizeof(buf), "G: Contours %s", g_showContours ? "ON" : "OFF");
        glColor3f(0.5f, 0.8f, 0.5f);
        gltext::draw(buf, 15, y); y -= 15;
    }

    // Power meter
    if (g_isAiming) {
        glColor3f(0.3f, 0.3f, 0.3f);
        glBegin(GL_QUADS);
        glVertex2f(winW/2 - 100, 30);
        glVertex2f(winW/2 + 100, 30);
        glVertex2f(winW/2 + 100, 50);
        glVertex2f(winW/2 - 100, 50);
        glEnd();
        
        float powerRatio = g_power / g_maxPower;
        glColor3f(powerRatio, 1.0f - powerRatio, 0.0f);
        glBegin(GL_QUADS);
        glVertex2f(winW/2 - 98, 32);
        glVertex2f(winW/2 - 98 + 196 * powerRatio, 32);
        glVertex2f(winW/2 - 98 + 196 * powerRatio, 48);
        glVertex2f(winW/2 - 98, 48);
        glEnd();
        
        glColor3f(1, 1, 1);
        snprintf(buf, sizeof(buf), "Power: %.0f%%", powerRatio * 100);
        gltext::draw(buf, winW/2 - 40, 55);
    }
    
    // Wind indicator
    glColor3f(0.7f, 0.85f, 1.0f);
    snprintf(buf, sizeof(buf), "Wind: %.1f mph", g_windSpeed);
    gltext::draw(buf, winW - 120, winH - 25);
    
    // Terrain indicator
    const TerrainProperties& tProps = TERRAIN_PROPS[(int)g_ball.terrain];
    snprintf(buf, sizeof(buf), "Lie: %s", tProps.name);
    gltext::draw(buf, winW - 120, winH - 45);
    
    // Distance to hole
    float distYards = distance2D(g_ball.x, g_ball.z, hole.holeX, hole.holeZ) / 0.3f;
    snprintf(buf, sizeof(buf), "To hole: %d yds", (int)distYards);
    gltext::draw(buf, winW - 120, winH - 65);
    
    // DRO Radix
    BaseX::RadixBase radix = g_droManager.getOptimalRadix(distYards);
    snprintf(buf, sizeof(buf), "DRO Radix: %d", (int)radix);
    gltext::draw(buf, winW - 120, winH - 85);
    
    // LOD Level
    int lod = (distYards > 50.0f) ? 1 : 2;
    snprintf(buf, sizeof(buf), "LOD Level: %d", lod);
    gltext::draw(buf, winW - 120, winH - 105);
    
    // Hole complete message
    if (g_holeComplete) {
        int score = g_strokes[g_currentHole] - hole.par;
        const char* scoreName = "Par";
        if (g_strokes[g_currentHole] == 1) scoreName = "HOLE IN ONE!";
        else if (score == -3) scoreName = "Albatross!";
        else if (score == -2) scoreName = "Eagle!";
        else if (score == -1) scoreName = "Birdie!";
        else if (score == 1) scoreName = "Bogey";
        else if (score == 2) scoreName = "Double Bogey";
        else if (score >= 3) scoreName = "Triple+ Bogey";
        
        glColor3f(1, 1, 0);
        gltext::drawCentered(scoreName, (float)winW/2, (float)winH/2 + 20, 3.0f);
        gltext::drawCentered("Press N for next hole", (float)winW/2, (float)winH/2 - 20, 1.5f);
    }
    
    // Scorecard
    if (g_showScorecard) {
        glColor4f(0, 0, 0, 0.85f);
        glBegin(GL_QUADS);
        glVertex2f(winW/2 - 250, winH/2 + 220);
        glVertex2f(winW/2 + 250, winH/2 + 220);
        glVertex2f(winW/2 + 250, winH/2 - 220);
        glVertex2f(winW/2 - 250, winH/2 - 220);
        glEnd();

        glColor3f(1, 1, 1);
        gltext::drawCentered("SCORECARD", (float)winW/2, (float)winH/2 + 200, 2.0f);

        int totalPar = 0;
        // Front 9
        glColor3f(0.8f, 0.8f, 0.3f);
        gltext::draw("FRONT 9", winW/2.0f - 220, winH/2.0f + 175);
        y = winH/2.0f + 155;
        for (int i = 0; i < 9 && i < NUM_HOLES; i++) {
            totalPar += g_holes[i].par;
            int score = g_strokes[i];
            if (score > 0) {
                int diff = score - g_holes[i].par;
                if (diff < 0) glColor3f(0.3f, 1.0f, 0.3f);
                else if (diff > 0) glColor3f(1.0f, 0.5f, 0.3f);
                else glColor3f(1, 1, 1);
            } else {
                glColor3f(0.5f, 0.5f, 0.5f);
            }
            snprintf(buf, sizeof(buf), "%2d. Par %d: %s", 
                    i + 1, g_holes[i].par, score > 0 ? std::to_string(score).c_str() : "-");
            gltext::draw(buf, winW/2.0f - 220, y);
            y -= 17;
        }
        // Back 9
        if (NUM_HOLES > 9) {
            glColor3f(0.8f, 0.8f, 0.3f);
            gltext::draw("BACK 9", winW/2.0f + 20, winH/2.0f + 175);
            y = winH/2.0f + 155;
            for (int i = 9; i < NUM_HOLES; i++) {
                totalPar += g_holes[i].par;
                int score = g_strokes[i];
                if (score > 0) {
                    int diff = score - g_holes[i].par;
                    if (diff < 0) glColor3f(0.3f, 1.0f, 0.3f);
                    else if (diff > 0) glColor3f(1.0f, 0.5f, 0.3f);
                    else glColor3f(1, 1, 1);
                } else {
                    glColor3f(0.5f, 0.5f, 0.5f);
                }
                snprintf(buf, sizeof(buf), "%2d. Par %d: %s", 
                        i + 1, g_holes[i].par, score > 0 ? std::to_string(score).c_str() : "-");
                gltext::draw(buf, winW/2.0f + 20, y);
                y -= 17;
            }
        }

        glColor3f(1, 1, 0);
        snprintf(buf, sizeof(buf), "Total: %d (Par %d)", g_totalStrokes, totalPar);
        gltext::drawCentered(buf, (float)winW/2, winH/2.0f - 195, 1.5f);
    }
    
    // Controls hint
    glColor3f(0.7f, 0.7f, 0.7f);
    gltext::draw("LMB: Aim/Power  Space: Swing  Tab: Score  G: Contours  Left/Right: Orbit  Up/Down: Zoom", 10, 15);
    
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// ══════════════════════════════════════════════════════════════════════
// INPUT
// ══════════════════════════════════════════════════════════════════════

static void keyCallback(GLFWwindow* w, int key, int, int action, int mods) {
    if (action == GLFW_PRESS) {
        g_keys[key] = true;
        
        if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, GLFW_TRUE);
        if (key == GLFW_KEY_R) resetBall();
        if (key == GLFW_KEY_TAB) g_showScorecard = !g_showScorecard;
        if (key == GLFW_KEY_V) g_showTrajectory = !g_showTrajectory;
        if (key == GLFW_KEY_G) g_showContours = !g_showContours;
        if (key == GLFW_KEY_F) g_freeCam = !g_freeCam;
        
        if (key == GLFW_KEY_C) {
            g_cameraMode = (CameraMode)(((int)g_cameraMode + 1) % 3);
        }
        
        // Camera controls for FOLLOW mode
        if (g_cameraMode == CameraMode::FOLLOW) {
            if (key == GLFW_KEY_HOME) {
                g_camOffsetX = 0.0f;  // Reset orbit angle
                g_camDist = 15.0f;    // Reset zoom
            }
        }
        
        // Swing
        if (key == GLFW_KEY_SPACE && !g_ball.inFlight && !g_ball.inHole) {
            float ballSpeed = sqrtf(g_ball.vx*g_ball.vx + g_ball.vz*g_ball.vz);
            if (ballSpeed < 0.5f && g_power > 1.0f) {
                hitBall(g_power, g_aimAngle);
                g_power = 0;
                g_isAiming = false;
            }
        }
        
        // Next hole
        if (key == GLFW_KEY_N && g_holeComplete) {
            g_currentHole = (g_currentHole + 1) % NUM_HOLES;
            resetBall();
        }
        
        // Jump to hole (1-9, unmodified)
        if (!(mods & GLFW_MOD_SHIFT) && key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
            int h = key - GLFW_KEY_1;
            if (h < NUM_HOLES) { g_currentHole = h; resetBall(); }
        }
        // Jump to holes 10-18 (Shift+0 through Shift+8)
        if (mods & GLFW_MOD_SHIFT) {
            if (key == GLFW_KEY_0 && NUM_HOLES > 9)  { g_currentHole = 9;  resetBall(); }
            if (key == GLFW_KEY_1 && NUM_HOLES > 10) { g_currentHole = 10; resetBall(); }
            if (key == GLFW_KEY_2 && NUM_HOLES > 11) { g_currentHole = 11; resetBall(); }
            if (key == GLFW_KEY_3 && NUM_HOLES > 12) { g_currentHole = 12; resetBall(); }
            if (key == GLFW_KEY_4 && NUM_HOLES > 13) { g_currentHole = 13; resetBall(); }
            if (key == GLFW_KEY_5 && NUM_HOLES > 14) { g_currentHole = 14; resetBall(); }
            if (key == GLFW_KEY_6 && NUM_HOLES > 15) { g_currentHole = 15; resetBall(); }
            if (key == GLFW_KEY_7 && NUM_HOLES > 16) { g_currentHole = 16; resetBall(); }
            if (key == GLFW_KEY_8 && NUM_HOLES > 17) { g_currentHole = 17; resetBall(); }
        }
        
    } else if (action == GLFW_RELEASE) {
        g_keys[key] = false;
    }
}

static void mouseButtonCallback(GLFWwindow* win, int button, int action, int) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            g_mouseDown = true;
            g_isAiming = true;
            g_dragStartY = g_mouseY;
        } else {
            g_mouseDown = false;
        }
    }
    // Middle mouse button centers aim on hole
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS) {
        const Hole& hole = g_holes[g_currentHole];
        g_aimAngle = atan2f(hole.holeX - g_ball.x, hole.holeZ - g_ball.z);
    }
}

static void cursorCallback(GLFWwindow* win, double x, double y) {
    static double lastX = x, lastY = y;
    static bool initialized = false;
    if (!initialized) { lastX = x; lastY = y; initialized = true; }

    if (g_freeCam) {
        g_camYaw -= (float)(x - lastX) * 0.003f;
        g_camPitch -= (float)(y - lastY) * 0.003f;
        g_camPitch = std::max(-1.5f, std::min(1.5f, g_camPitch));
    }

    // Aiming — only rotate aim when mouse button is held
    if (g_isAiming && g_mouseDown && !g_ball.inFlight && !g_ball.inHole) {
        // Horizontal drag rotates aim
        g_aimAngle -= (float)(x - lastX) * 0.003f;

        // Vertical drag sets power
        float dragDist = (float)(y - g_dragStartY);
        g_power = std::max(0.0f, std::min(g_maxPower, dragDist * 0.2f));
    }

    lastX = x;
    lastY = y;
    g_mouseX = x;
    g_mouseY = y;
}

static void scrollCallback(GLFWwindow*, double, double yoff) {
    g_camDist = std::max(5.0f, std::min(50.0f, g_camDist - (float)yoff * 2.0f));
}

// ══════════════════════════════════════════════════════════════════════
// UPDATE
// ══════════════════════════════════════════════════════════════════════

static void update(float dt) {
    // Update wind slowly
    g_windAngle += (g_rand(g_rng) - 0.5f) * dt * 0.2f;
    g_windSpeed += (g_rand(g_rng) - 0.5f) * dt * 2.0f;
    g_windSpeed = std::max(0.0f, std::min(15.0f, g_windSpeed));
    
    // Update ball
    updateBall(dt);
    
    // Camera orbit and zoom with arrow keys (held down)
    if (g_cameraMode == CameraMode::FOLLOW) {
        if (g_keys[GLFW_KEY_LEFT])  g_camOffsetX -= 1.5f * dt;  // Orbit left
        if (g_keys[GLFW_KEY_RIGHT]) g_camOffsetX += 1.5f * dt;  // Orbit right
        if (g_keys[GLFW_KEY_UP])    g_camDist = std::max(3.0f, g_camDist - 15.0f * dt);  // Zoom in
        if (g_keys[GLFW_KEY_DOWN])  g_camDist = std::min(60.0f, g_camDist + 15.0f * dt); // Zoom out
    }

    // Hole complete timer
    if (g_holeComplete) {
        g_holeCompleteTimer += dt;
        if (g_holeCompleteTimer > 2.0f) {
            g_currentHole++;
            if (g_currentHole >= NUM_HOLES) {
                g_currentHole = 0;
            }
            resetBall();
            g_holeComplete = false;
            g_holeCompleteTimer = 0.0f;
        }
    }
}

// ══════════════════════════════════════════════════════════════════════
// MAIN
// ══════════════════════════════════════════════════════════════════════

int main() {
    if (!glfwInit()) return 1;
    
    glfwWindowHint(GLFW_SAMPLES, 8);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "SPG-001: Procedural Golf - 18 Holes", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorCallback);
    glfwSetScrollCallback(window, scrollCallback);
    
    // Generate course
    generateCourse();
    generateTerrainMaps();

    // Load GL extensions for VBOs
    if (!loadGLExtensions()) {
        fprintf(stderr, "Failed to load GL VBO extensions\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // Generate procedural textures (needs GL context)
    generateTextures();

    // Initialize terrain lighting
    initTerrainSun();

    // Initialize GLSL shaders (Phase A)
    initGolfShaders();

    // Initialize DRO
    g_droManager.initializeVRProfile();
    
    // Initialize wind
    g_windSpeed = 3.0f + g_rand(g_rng) * 5.0f;
    g_windAngle = g_rand(g_rng) * 2 * PI;
    
    // Initialize ball (resetBall sets camera behind ball toward hole)
    resetBall();
    
    glEnable(GL_DEPTH_TEST);
    glEnable(0x809D);  // GL_MULTISAMPLE
    
    double lastTime = glfwGetTime();
    
    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = std::min(0.05f, (float)(now - lastTime));
        lastTime = now;
        g_wallTime = (float)now;

        glfwPollEvents();
        update(dt);
        updateParticles(dt);
        
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        
        // Sky color (Phase D)
        float fogR,fogG,fogB; getGolfSkyColors(fogR,fogG,fogB);
        glClearColor(fogR, fogG, fogB, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Camera
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        perspectiveGL(60.0, (double)w / h, 0.1, 1000.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        
        float lookX, lookY, lookZ;
        switch (g_cameraMode) {
            case CameraMode::FOLLOW: {
                // Compute base angle from ball toward hole
                const Hole& curHole = g_holes[g_currentHole];
                float toHoleX = curHole.holeX - g_ball.x;
                float toHoleZ = curHole.holeZ - g_ball.z;
                float baseAngle = atan2f(toHoleX, toHoleZ);
                // Camera orbits behind ball; g_camOffsetX rotates around ball
                float camAngle = baseAngle + PI + g_camOffsetX;
                g_camX = g_ball.x + sinf(camAngle) * g_camDist;
                g_camZ = g_ball.z + cosf(camAngle) * g_camDist;
                g_camY = g_ball.y + g_camDist * 0.4f;
                // Ensure camera is above terrain
                float terrainY = getHeight(g_camX, g_camZ);
                if (g_camY < terrainY + 1.0f) g_camY = terrainY + 1.0f;
                lookX = g_ball.x;
                lookY = g_ball.y;
                lookZ = g_ball.z;
                break;
            }
            case CameraMode::OVERHEAD: {
                lookX = g_camX;
                lookY = 0;
                lookZ = g_camZ;
                break;
            }
            case CameraMode::FREE: {
                lookX = g_camX - sinf(g_camYaw) * cosf(g_camPitch);
                lookY = g_camY + sinf(g_camPitch);
                lookZ = g_camZ - cosf(g_camYaw) * cosf(g_camPitch);
                break;
            }
        }
        
        lookAtGL(g_camX, g_camY, g_camZ, lookX, lookY, lookZ);
        
        // Atmosphere
        glEnable(GL_FOG);
        glFogi(GL_FOG_MODE, GL_EXP2);
        float fogColor[4] = {fogR, fogG, fogB, 1.0f};
        glFogfv(GL_FOG_COLOR, fogColor);
        glFogf(GL_FOG_DENSITY, 0.003f);
        
        // Render
        renderSky();
        glEnable(GL_FOG);
        renderTerrain();
        renderTerrainTransitions();
        for (const auto& hole : g_holes) {
            renderHole(hole);
        }
        renderBall();
        renderFlightTrail();
        renderRollingDust();
        renderParticles();
        renderAimArrow();
        renderTrajectory();
        renderUI(w, h);
        
        glfwSwapBuffers(window);
    }
    
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
