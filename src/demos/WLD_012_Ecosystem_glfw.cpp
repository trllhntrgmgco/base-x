/**
 * WLD-012: Living Ecosystem - Tile-Based Procedural World
 *
 * Upgraded ecosystem: 12 creature types, 8x8 tile grid (1 acre per tile),
 * DRO precision scaling, frozen tile state, soft tether, multi-LOD terrain.
 *
 * Tile System:
 *   Each tile = 64x64 world units (~1 acre). 8x8 grid = 512x512 world.
 *   Active tile: full LOD B-64, creatures + plants simulated.
 *   Adjacent 8: medium LOD B-32, plants rendered, no creatures.
 *   Ring 2: low LOD B-16, terrain only.   Distant: silhouette B-8.
 *   Frozen tile cache: state preserved on exit, restored on re-entry.
 *
 * Phase D — Dynamic Sky & Sun:
 *   Multi-band gradient sky (zenith→upper→lower→horizon).
 *   Sunrise/sunset golden-hour color with twilight glow.
 *   Sun disc + dual-layer corona; warm tint at low angles.
 *   Moon with glow + craters on the opposite arc.
 *   80 procedural stars with twinkle, fade with daylight.
 *   12 drifting cloud puffs tied to weather cloudCover/wind/rain.
 *   getSkyColors() provides matching fog/clear color everywhere.
 *
 * Phase E — Tree Wind & Better Shadows:
 *   Trees sway with weather wind (trunk lean + canopy offset).
 *   Per-tree phase variation prevents uniform motion.
 *   Canopy edge ripple via animated sinf modulation.
 *   Sun-directional contact shadows with alpha gradient falloff.
 *   Creature shadows offset by g_lightDir, stretch with height.
 *
 * Creatures (12): Rabbit, Deer, Wolf, Eagle, Fox, Bear, Boar, Frog,
 *   Owl (nocturnal), Snake (ambush), Butterfly (passive), Fish (aquatic)
 *
 * Controls:
 *   WASD: Move camera        Mouse+RMB: Look around
 *   1-9,0,-,=: Select type   Space: Spawn at cursor
 *   T: Time speed            G: Cycle weather
 *   N: Day/night cycle       P: Pause
 *   H: Toggle HUD            F: Follow mode
 *   Tab: Select next         R: Reset ecosystem    ESC: Quit
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

// ══════════════════════════════════════════════════════════════════════
// GL EXTENSION LOADING (Phase 3: Shader Pipeline)
// ══════════════════════════════════════════════════════════════════════

#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW 0x88E4
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#endif
typedef ptrdiff_t GLsizeiptr;
typedef char GLchar;

#define GL_FN(ret,name,...) typedef ret(APIENTRY*PFN_##name)(__VA_ARGS__);static PFN_##name gl_##name=nullptr
GL_FN(void,GenBuffers,GLsizei,GLuint*);GL_FN(void,DeleteBuffers,GLsizei,const GLuint*);
GL_FN(void,BindBuffer,GLenum,GLuint);GL_FN(void,BufferData,GLenum,GLsizeiptr,const void*,GLenum);
GL_FN(GLuint,CreateShader,GLenum);GL_FN(void,ShaderSource,GLuint,GLsizei,const GLchar**,const GLint*);
GL_FN(void,CompileShader,GLuint);GL_FN(void,GetShaderiv,GLuint,GLenum,GLint*);
GL_FN(void,GetShaderInfoLog,GLuint,GLsizei,GLsizei*,GLchar*);
GL_FN(GLuint,CreateProgram);GL_FN(void,AttachShader,GLuint,GLuint);
GL_FN(void,LinkProgram,GLuint);GL_FN(void,GetProgramiv,GLuint,GLenum,GLint*);
GL_FN(void,GetProgramInfoLog,GLuint,GLsizei,GLsizei*,GLchar*);
GL_FN(void,UseProgram,GLuint);GL_FN(GLint,GetUniformLocation,GLuint,const GLchar*);
GL_FN(void,Uniform1f,GLint,GLfloat);GL_FN(void,Uniform3f,GLint,GLfloat,GLfloat,GLfloat);
GL_FN(void,DeleteShader,GLuint);GL_FN(void,DeleteProgram,GLuint);
#undef GL_FN

static bool g_hasShaders=false;
static float g_wallTime=0;

static void loadGLExtensions(){
    #define LD(n) gl_##n=(PFN_##n)glfwGetProcAddress("gl" #n)
    LD(GenBuffers);LD(DeleteBuffers);LD(BindBuffer);LD(BufferData);
    LD(CreateShader);LD(ShaderSource);LD(CompileShader);LD(GetShaderiv);LD(GetShaderInfoLog);
    LD(CreateProgram);LD(AttachShader);LD(LinkProgram);LD(GetProgramiv);LD(GetProgramInfoLog);
    LD(UseProgram);LD(GetUniformLocation);LD(Uniform1f);LD(Uniform3f);
    LD(DeleteShader);LD(DeleteProgram);
    #undef LD
    g_hasShaders=gl_CreateShader&&gl_CreateProgram&&gl_GenBuffers;
}

static constexpr float PI = 3.14159265f;

struct Creature;
static float getTerrainHeight(float x, float z);
static void activateTile(int newTX, int newTZ);
static void getSkyColors(float& fogR, float& fogG, float& fogB);

// ══════════════════════════════════════════════════════════════════════
// CONSTANTS
// ══════════════════════════════════════════════════════════════════════

static constexpr int TILE_SIZE      = 64;
static constexpr int GRID_TILES     = 8;
static constexpr int WORLD_SIZE     = TILE_SIZE * GRID_TILES;  // 512
static constexpr int MAX_CREATURES  = 60;
static constexpr int MAX_PLANTS     = 120;
static constexpr float WATER_LEVEL  = 1.5f;
static constexpr float TETHER_MARGIN= 8.0f;

// ══════════════════════════════════════════════════════════════════════
// WEATHER SYSTEM
// ══════════════════════════════════════════════════════════════════════

enum class Weather { CLEAR, CLOUDY, RAIN, STORM };
static const char* WEATHER_NAMES[] = {"Clear","Cloudy","Rain","Storm"};

struct WeatherState {
    Weather current = Weather::CLEAR;
    float cloudCover = 0.0f, rainIntensity = 0.0f;
    float windStrength = 0.0f, windAngle = 0.0f, lightLevel = 1.0f;
};

// ══════════════════════════════════════════════════════════════════════
// TIME SYSTEM
// ══════════════════════════════════════════════════════════════════════

struct TimeState {
    float dayTime = 0.25f, dayLength = 120.0f, timeScale = 1.0f;
    int day = 1; bool paused = false;
    bool isDaytime() const { return dayTime > 0.2f && dayTime < 0.8f; }
    float getSunAngle() const { return (dayTime - 0.25f) * 2 * PI; }
};
// ══════════════════════════════════════════════════════════════════════
// CREATURE TYPES (12)
// ══════════════════════════════════════════════════════════════════════

enum class CreatureType {
    RABBIT=0, DEER, WOLF, EAGLE, FOX, BEAR, BOAR, FROG,
    OWL, SNAKE, BUTTERFLY, FISH, COUNT
};

struct CreatureStats {
    const char* name;
    float size, maxSpeed, maxEnergy, hungerRate;
    float sightRange, attackRange, attackDamage, reproductionEnergy;
    float color[3];
    bool isPredator, canFly;
    bool isNocturnal, isAmbush, isWaterBound, isPassive, isDefensive, isOmnivore;
};

static const CreatureStats CREATURE_STATS[] = {
 {"Rabbit",   0.3f, 8.f, 100.f,0.5f, 15.f,0.5f, 0.f, 80.f, {.6f,.5f,.4f},      false,false,false,false,false,false,false,false},
 {"Deer",     1.0f, 6.f, 150.f,0.3f, 25.f,0.8f, 0.f,120.f, {.5f,.35f,.2f},      false,false,false,false,false,false,false,false},
 {"Wolf",     0.8f, 7.f, 120.f,0.4f, 30.f,1.0f,25.f,100.f, {.4f,.4f,.45f},      true, false,false,false,false,false,false,false},
 {"Eagle",    0.5f,12.f,  80.f,0.6f, 50.f,1.5f,20.f, 70.f, {.3f,.25f,.2f},      true, true, false,false,false,false,false,false},
 {"Fox",      0.5f, 9.f,  90.f,0.45f,20.f,0.8f,15.f, 75.f, {.85f,.45f,.15f},    true, false,false,false,false,false,false,false},
 {"Bear",     1.5f, 4.f, 200.f,0.25f,20.f,1.5f,40.f,160.f, {.45f,.3f,.15f},     true, false,false,false,false,false,false,true},
 {"Boar",     0.9f, 5.5f,130.f,0.35f,18.f,1.0f,10.f,110.f, {.3f,.25f,.2f},      false,false,false,false,false,false,true, false},
 {"Frog",     0.15f,6.f,  40.f,0.3f,  8.f,0.3f, 0.f, 30.f, {.2f,.6f,.15f},      false,false,false,false,false,false,false,false},
 {"Owl",      0.4f,10.f,  70.f,0.5f, 40.f,1.2f,18.f, 65.f, {.45f,.4f,.3f},      true, true, true, false,false,false,false,false},
 {"Snake",    0.25f,3.f,  60.f,0.2f,  6.f,0.5f,22.f, 50.f, {.35f,.4f,.2f},      true, false,false,true, false,false,false,false},
 {"Butterfly",0.1f, 4.f,  30.f,0.1f,  5.f,0.0f, 0.f, 20.f, {.8f,.5f,.9f},       false,false,false,false,false,true, false,false},
 {"Fish",     0.3f, 5.f,  50.f,0.15f,10.f,0.0f, 0.f, 40.f, {.6f,.65f,.7f},      false,false,false,false,true, false,false,false},
};

static constexpr int NUM_CREATURE_TYPES = (int)CreatureType::COUNT;

// Predator-prey lookup
static bool canHunt(CreatureType hunter, CreatureType prey) {
    switch (hunter) {
    case CreatureType::WOLF:  return prey==CreatureType::RABBIT||prey==CreatureType::DEER||prey==CreatureType::FOX||prey==CreatureType::BOAR;
    case CreatureType::EAGLE: return prey==CreatureType::RABBIT||prey==CreatureType::FROG||prey==CreatureType::SNAKE||prey==CreatureType::FISH;
    case CreatureType::FOX:   return prey==CreatureType::RABBIT||prey==CreatureType::FROG;
    case CreatureType::BEAR:  return prey==CreatureType::RABBIT||prey==CreatureType::DEER||prey==CreatureType::FOX||prey==CreatureType::BOAR||prey==CreatureType::SNAKE||prey==CreatureType::FISH;
    case CreatureType::OWL:   return prey==CreatureType::RABBIT||prey==CreatureType::FROG;
    case CreatureType::SNAKE: return prey==CreatureType::RABBIT||prey==CreatureType::FROG;
    default: return false;
    }
}

// ══════════════════════════════════════════════════════════════════════
// AI STATES + CREATURE STRUCT
// ══════════════════════════════════════════════════════════════════════

enum class AIState { IDLE, WANDERING, FLEEING, HUNTING, EATING, RESTING, DEAD };

struct Creature {
    CreatureType type;
    float x,y,z, vx,vy,vz, rotation;
    float energy, health, age;
    AIState state; int targetId; float stateTimer;
    bool selected; int lodLevel;
    float walkCycle, wingAngle;
    bool isAlive() const { return state!=AIState::DEAD && health>0; }
};

// ══════════════════════════════════════════════════════════════════════
// PLANT STRUCT
// ══════════════════════════════════════════════════════════════════════

enum class PlantType { GRASS, BUSH, TREE };
struct Plant {
    PlantType type; float x,y,z, growth, nutrition, maxNutrition; bool eaten;
};
// ══════════════════════════════════════════════════════════════════════
// TILE SYSTEM
// ══════════════════════════════════════════════════════════════════════

struct TileSnapshot {
    std::vector<Creature> creatures;
    std::vector<Plant> plants;
    bool valid = false;
};

static TileSnapshot g_tileCache[GRID_TILES][GRID_TILES];
static int g_activeTX = -1, g_activeTZ = -1;

static const char* getBiomeName(float avgH) {
    if (avgH < WATER_LEVEL) return "Wetland";
    if (avgH < 3.0f) return "Lowland";
    if (avgH < 5.5f) return "Forest";
    return "Highland";
}

// ══════════════════════════════════════════════════════════════════════
// GLOBAL STATE
// ══════════════════════════════════════════════════════════════════════

static std::vector<Creature> g_creatures;
static std::vector<Plant> g_plants;
static std::vector<std::vector<float>> g_terrain;
static BaseX::DROManager g_dro;
static WeatherState g_weather;
static TimeState g_time;

static float g_camX = WORLD_SIZE/2.f, g_camY = 20.f, g_camZ = WORLD_SIZE/2.f;
static float g_camYaw = 0.f, g_camPitch = -0.5f, g_camSpeed = 20.f;

static CreatureType g_selectedType = CreatureType::RABBIT;
static int g_selectedCreature = -1;
static bool g_followMode = false, g_showHUD = true, g_dayNightCycle = true;

static bool g_keys[512] = {};
static bool g_mouseCaptured = false;
static double g_mouseX = 0, g_mouseY = 0;

static std::mt19937 g_rng(42);
static std::uniform_real_distribution<float> g_rand(0.0f, 1.0f);

static float g_lightDir[3] = {0.5f,0.8f,0.3f};
static float g_ambientLight = 0.3f;

// ══════════════════════════════════════════════════════════════════════
// UTILITY FUNCTIONS
// ══════════════════════════════════════════════════════════════════════

static void normalize3(float* v) {
    float len = sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    if (len>0.0001f){v[0]/=len;v[1]/=len;v[2]/=len;}
}
static float dot3(const float*a,const float*b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
static float distance2D(float x1,float z1,float x2,float z2){float dx=x2-x1,dz=z2-z1;return sqrtf(dx*dx+dz*dz);}

static float hash2D(float x, float z) {
    int ix=(int)floorf(x)*1619, iz=(int)floorf(z)*31337, n=ix+iz;
    n=(n<<13)^n;
    return 1.0f-((n*(n*n*15731+789221)+1376312589)&0x7fffffff)/1073741824.0f;
}
static float smoothNoise2D(float x,float z){
    float fx=floorf(x),fz=floorf(z),dx=x-fx,dz=z-fz;
    float v00=hash2D(fx,fz),v10=hash2D(fx+1,fz),v01=hash2D(fx,fz+1),v11=hash2D(fx+1,fz+1);
    float sx=dx*dx*(3-2*dx),sz=dz*dz*(3-2*dz);
    return v00+sx*(v10-v00)+sz*(v01-v00)+sx*sz*(v00-v10-v01+v11);
}
static float fractalNoise(float x,float z,int oct){
    float total=0,amp=1,freq=1,mx=0;
    for(int i=0;i<oct;i++){total+=smoothNoise2D(x*freq,z*freq)*amp;mx+=amp;amp*=0.5f;freq*=2.f;}
    return total/mx;
}

// ══════════════════════════════════════════════════════════════════════
// TERRAIN
// ══════════════════════════════════════════════════════════════════════

static void generateTerrain() {
    g_terrain.resize(WORLD_SIZE+1);
    for(int z=0;z<=WORLD_SIZE;z++){
        g_terrain[z].resize(WORLD_SIZE+1);
        for(int x=0;x<=WORLD_SIZE;x++)
            g_terrain[z][x]=fractalNoise(x*0.02f,z*0.02f,5)*8.0f;
    }
}

static float getTerrainHeight(float x,float z){
    int ix=(int)x,iz=(int)z;
    if(ix<0||ix>=WORLD_SIZE||iz<0||iz>=WORLD_SIZE)return 0;
    float fx=x-ix,fz=z-iz;
    float h00=g_terrain[iz][ix],h10=g_terrain[iz][ix+1];
    float h01=g_terrain[iz+1][ix],h11=g_terrain[iz+1][ix+1];
    return h00*(1-fx)*(1-fz)+h10*fx*(1-fz)+h01*(1-fx)*fz+h11*fx*fz;
}

static float getTileAvgHeight(int tx,int tz){
    float sum=0; int cnt=0;
    int x0=tx*TILE_SIZE,z0=tz*TILE_SIZE;
    for(int z=z0;z<z0+TILE_SIZE;z+=8)
        for(int x=x0;x<x0+TILE_SIZE;x+=8){sum+=g_terrain[z][x];cnt++;}
    return cnt>0?sum/cnt:0;
}

// ══════════════════════════════════════════════════════════════════════
// TERRAIN MESH (Phase 1: Vertex Arrays + GL Lighting)
// ══════════════════════════════════════════════════════════════════════

struct TerrainVertex {
    float px, py, pz;   // position
    float nx, ny, nz;   // normal
    float r, g, b;      // color
};

struct TileMesh {
    std::vector<TerrainVertex> verts;
    std::vector<unsigned short> indices;
    int step = 0;
    bool built = false;
    GLuint vbo = 0, ibo = 0;
};

static TileMesh g_tileMeshes[GRID_TILES][GRID_TILES];
static float g_fps = 0.f;

static int lodStepForDist(int dist) {
    if (dist <= 1) return 1;
    if (dist <= 3) return 2;
    return 4;
}

static void stitchTileEdge(std::vector<TerrainVertex>& verts, int startIdx, int stride, int count, int ratio) {
    if (ratio <= 1) return;
    for (int i = 0; i < count; i++) {
        int rem = i % ratio;
        if (rem == 0) continue;
        int lo = (i / ratio) * ratio;
        int hi = std::min(lo + ratio, count - 1);
        float t = (float)rem / (float)ratio;
        auto& v = verts[startIdx + i * stride];
        const auto& vlo = verts[startIdx + lo * stride];
        const auto& vhi = verts[startIdx + hi * stride];
        v.py = vlo.py * (1.f - t) + vhi.py * t;
        v.nx = vlo.nx * (1.f - t) + vhi.nx * t;
        v.ny = vlo.ny * (1.f - t) + vhi.ny * t;
        v.nz = vlo.nz * (1.f - t) + vhi.nz * t;
        float len = sqrtf(v.nx*v.nx + v.ny*v.ny + v.nz*v.nz);
        if (len > 0.0001f) { v.nx /= len; v.ny /= len; v.nz /= len; }
    }
}

static void buildTileMesh(int tx, int tz, int step) {
    auto& m = g_tileMeshes[tz][tx];
    if (m.built && m.step == step) return;
    int x0 = tx * TILE_SIZE, z0 = tz * TILE_SIZE;
    int cols = TILE_SIZE / step + 1;
    int rows = TILE_SIZE / step + 1;

    m.verts.resize(rows * cols);
    for (int iz = 0; iz < rows; iz++) {
        for (int ix = 0; ix < cols; ix++) {
            int wx = x0 + ix * step;
            int wz = z0 + iz * step;
            float h = g_terrain[wz][wx];

            // Per-vertex normal from finite differences
            int wxL = std::max(0, wx - step), wxR = std::min(WORLD_SIZE, wx + step);
            int wzD = std::max(0, wz - step), wzU = std::min(WORLD_SIZE, wz + step);
            float nn[3] = {
                g_terrain[wz][wxL] - g_terrain[wz][wxR],
                2.0f * step,
                g_terrain[wzD][wx] - g_terrain[wzU][wx]
            };
            normalize3(nn);

            // Height-based biome color
            float cr, cg, cb;
            if (h < WATER_LEVEL)      { cr=0.25f; cg=0.35f; cb=0.45f; }
            else if (h < 3.0f)        { cr=0.30f; cg=0.50f; cb=0.25f; }
            else if (h < 5.5f)        { cr=0.35f; cg=0.45f; cb=0.20f; }
            else                      { cr=0.50f; cg=0.45f; cb=0.40f; }

            int vi = iz * cols + ix;
            m.verts[vi] = {(float)wx, h, (float)wz, nn[0], nn[1], nn[2], cr, cg, cb};
        }
    }

    // Edge stitching: interpolate boundary vertices to match coarser neighbors
    auto nbrStep = [&](int ntx, int ntz) -> int {
        if (ntx < 0 || ntx >= GRID_TILES || ntz < 0 || ntz >= GRID_TILES) return step;
        return lodStepForDist(std::max(std::abs(ntx - g_activeTX), std::abs(ntz - g_activeTZ)));
    };
    int ns = nbrStep(tx, tz-1), ss = nbrStep(tx, tz+1);
    int ws = nbrStep(tx-1, tz), es = nbrStep(tx+1, tz);
    if (ns > step) stitchTileEdge(m.verts, 0, 1, cols, ns/step);
    if (ss > step) stitchTileEdge(m.verts, (rows-1)*cols, 1, cols, ss/step);
    if (ws > step) stitchTileEdge(m.verts, 0, cols, rows, ws/step);
    if (es > step) stitchTileEdge(m.verts, cols-1, cols, rows, es/step);

    m.indices.resize((rows - 1) * (cols - 1) * 6);
    int idx = 0;
    for (int iz = 0; iz < rows - 1; iz++) {
        for (int ix = 0; ix < cols - 1; ix++) {
            unsigned short v00 = (unsigned short)(iz * cols + ix);
            unsigned short v10 = v00 + 1;
            unsigned short v01 = (unsigned short)((iz + 1) * cols + ix);
            unsigned short v11 = v01 + 1;
            m.indices[idx++] = v00; m.indices[idx++] = v10; m.indices[idx++] = v01;
            m.indices[idx++] = v10; m.indices[idx++] = v11; m.indices[idx++] = v01;
        }
    }
    if(g_hasShaders){
        if(!m.vbo)gl_GenBuffers(1,&m.vbo);
        if(!m.ibo)gl_GenBuffers(1,&m.ibo);
        gl_BindBuffer(GL_ARRAY_BUFFER,m.vbo);
        gl_BufferData(GL_ARRAY_BUFFER,(GLsizeiptr)(m.verts.size()*sizeof(TerrainVertex)),m.verts.data(),GL_STATIC_DRAW);
        gl_BindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.ibo);
        gl_BufferData(GL_ELEMENT_ARRAY_BUFFER,(GLsizeiptr)(m.indices.size()*sizeof(unsigned short)),m.indices.data(),GL_STATIC_DRAW);
        gl_BindBuffer(GL_ARRAY_BUFFER,0);gl_BindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
    }
    m.step = step;
    m.built = true;
}

static void buildAllTileMeshes() {
    for (int tz = 0; tz < GRID_TILES; tz++)
        for (int tx = 0; tx < GRID_TILES; tx++) {
            int dist = std::max(std::abs(tx - g_activeTX), std::abs(tz - g_activeTZ));
            buildTileMesh(tx, tz, lodStepForDist(dist));
        }
}

// ══════════════════════════════════════════════════════════════════════
// SHADER PIPELINE (Phase 3: GLSL Terrain + Water)
// ══════════════════════════════════════════════════════════════════════

static GLuint g_terrainProg=0, g_waterProg=0;
static GLint g_tLocLight,g_tLocAmb,g_tLocFogD,g_tLocFogC;
static GLint g_wLocLight,g_wLocAmb,g_wLocFogD,g_wLocFogC,g_wLocTime;

static GLuint compileShader(GLenum type,const char*src){
    GLuint s=gl_CreateShader(type);
    gl_ShaderSource(s,1,&src,nullptr);gl_CompileShader(s);
    GLint ok;gl_GetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[512];gl_GetShaderInfoLog(s,512,nullptr,log);fprintf(stderr,"Shader: %s\n",log);}
    return s;
}
static GLuint buildProgram(const char*vs,const char*fs){
    GLuint v=compileShader(GL_VERTEX_SHADER,vs),f=compileShader(GL_FRAGMENT_SHADER,fs);
    GLuint p=gl_CreateProgram();
    gl_AttachShader(p,v);gl_AttachShader(p,f);gl_LinkProgram(p);
    GLint ok;gl_GetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){char log[512];gl_GetProgramInfoLog(p,512,nullptr,log);fprintf(stderr,"Link: %s\n",log);}
    gl_DeleteShader(v);gl_DeleteShader(f);return p;
}

static const char* TERRAIN_VS=R"(
#version 120
varying vec3 v_norm,v_wpos;varying float v_fog;
void main(){
    gl_Position=gl_ModelViewProjectionMatrix*gl_Vertex;
    v_norm=normalize(gl_NormalMatrix*gl_Normal);
    v_wpos=gl_Vertex.xyz;
    v_fog=length((gl_ModelViewMatrix*gl_Vertex).xyz);
})";

static const char* TERRAIN_FS=R"(
#version 120
uniform vec3 u_light;uniform float u_amb,u_fogD;uniform vec3 u_fogC;
varying vec3 v_norm,v_wpos;varying float v_fog;
void main(){
    float h=v_wpos.y;
    vec3 cW=vec3(.25,.35,.45),cL=vec3(.30,.50,.25),cM=vec3(.35,.45,.20),cH=vec3(.50,.45,.40);
    vec3 b=mix(cW,cL,smoothstep(.5,2.,h));b=mix(b,cM,smoothstep(2.,4.,h));b=mix(b,cH,smoothstep(4.,6.5,h));
    vec3 n=normalize(v_norm);float d=max(dot(n,u_light),0.);
    vec3 c=b*(u_amb+d*(1.-u_amb*.3));
    float f=clamp(exp(-u_fogD*v_fog),0.,1.);
    gl_FragColor=vec4(mix(u_fogC,c,f),1.);
})";

static const char* WATER_VS=R"(
#version 120
uniform float u_time;varying vec3 v_wpos;varying float v_fog;
void main(){
    vec3 p=gl_Vertex.xyz;
    p.y+=sin(p.x*.5+u_time*2.)*.12+sin(p.z*.7+u_time*1.5)*.09+sin((p.x+p.z)*.3+u_time*2.5)*.06;
    gl_Position=gl_ModelViewProjectionMatrix*vec4(p,1.);
    v_wpos=p;v_fog=length((gl_ModelViewMatrix*vec4(p,1.)).xyz);
})";

static const char* WATER_FS=R"(
#version 120
uniform vec3 u_light;uniform float u_amb,u_fogD,u_time;uniform vec3 u_fogC;
varying vec3 v_wpos;varying float v_fog;
void main(){
    float dx=cos(v_wpos.x*.5+u_time*2.)*.06+cos((v_wpos.x+v_wpos.z)*.3+u_time*2.5)*.03;
    float dz=cos(v_wpos.z*.7+u_time*1.5)*.063;
    vec3 n=normalize(vec3(-dx,1.,-dz));
    float d=max(dot(n,u_light),0.);float sp=pow(max(dot(reflect(-u_light,n),vec3(0,1,0)),0.),48.);
    vec3 c=vec3(.15,.35,.55)*(u_amb+d*.5)+vec3(1.)*sp*.5;
    float f=clamp(exp(-u_fogD*v_fog),0.,1.);
    gl_FragColor=vec4(mix(u_fogC,c,f),.45);
})";

static void initShaders(){
    if(!g_hasShaders)return;
    g_terrainProg=buildProgram(TERRAIN_VS,TERRAIN_FS);
    g_tLocLight=gl_GetUniformLocation(g_terrainProg,"u_light");
    g_tLocAmb=gl_GetUniformLocation(g_terrainProg,"u_amb");
    g_tLocFogD=gl_GetUniformLocation(g_terrainProg,"u_fogD");
    g_tLocFogC=gl_GetUniformLocation(g_terrainProg,"u_fogC");
    g_waterProg=buildProgram(WATER_VS,WATER_FS);
    g_wLocLight=gl_GetUniformLocation(g_waterProg,"u_light");
    g_wLocAmb=gl_GetUniformLocation(g_waterProg,"u_amb");
    g_wLocFogD=gl_GetUniformLocation(g_waterProg,"u_fogD");
    g_wLocFogC=gl_GetUniformLocation(g_waterProg,"u_fogC");
    g_wLocTime=gl_GetUniformLocation(g_waterProg,"u_time");
}

// ══════════════════════════════════════════════════════════════════════
// PER-TILE PLANT GENERATION
// ══════════════════════════════════════════════════════════════════════

static void generateTilePlants(int tx,int tz){
    g_plants.clear();
    float x0=(float)(tx*TILE_SIZE), z0=(float)(tz*TILE_SIZE);
    std::mt19937 tileRng(tx*1000+tz);
    std::uniform_real_distribution<float> tr(0.f,1.f);
    for(int i=0;i<MAX_PLANTS;i++){
        Plant p;
        p.x=x0+tr(tileRng)*(TILE_SIZE-2)+1;
        p.z=z0+tr(tileRng)*(TILE_SIZE-2)+1;
        p.y=getTerrainHeight(p.x,p.z);
        float r=tr(tileRng);
        if(r<0.6f){p.type=PlantType::GRASS;p.maxNutrition=10.f;}
        else if(r<0.9f){p.type=PlantType::BUSH;p.maxNutrition=25.f;}
        else{p.type=PlantType::TREE;p.maxNutrition=0.f;}
        p.growth=0.5f+tr(tileRng)*0.5f;
        p.nutrition=p.maxNutrition*p.growth;
        p.eaten=false;
        g_plants.push_back(p);
    }
}

// ══════════════════════════════════════════════════════════════════════
// PER-TILE CREATURE SPAWNING
// ══════════════════════════════════════════════════════════════════════

static void spawnCreature(CreatureType type,float x,float z){
    if((int)g_creatures.size()>=MAX_CREATURES)return;
    const auto& s=CREATURE_STATS[(int)type];
    Creature c{};
    c.type=type; c.x=x; c.z=z;
    c.y=getTerrainHeight(x,z)+(s.canFly?10.f:0.f);
    c.vx=c.vy=c.vz=0; c.rotation=g_rand(g_rng)*2*PI;
    c.energy=s.maxEnergy*0.8f; c.health=100.f; c.age=0;
    c.state=AIState::IDLE; c.targetId=-1; c.stateTimer=0;
    c.selected=false; c.lodLevel=0; c.walkCycle=0; c.wingAngle=0;
    if(s.isWaterBound && getTerrainHeight(x,z)>=WATER_LEVEL) return;
    g_creatures.push_back(c);
}

static void spawnTileCreatures(int tx,int tz){
    g_creatures.clear();
    float avgH=getTileAvgHeight(tx,tz);
    float x0=(float)(tx*TILE_SIZE), z0=(float)(tz*TILE_SIZE);
    auto sp=[&](CreatureType t,int n){
        for(int i=0;i<n;i++) spawnCreature(t,x0+g_rand(g_rng)*(TILE_SIZE-2)+1,z0+g_rand(g_rng)*(TILE_SIZE-2)+1);
    };
    if(avgH<WATER_LEVEL){
        sp(CreatureType::FROG,8);sp(CreatureType::FISH,5);sp(CreatureType::RABBIT,4);
        sp(CreatureType::DEER,3);sp(CreatureType::BUTTERFLY,3);sp(CreatureType::SNAKE,1);
    } else if(avgH<3.0f){
        sp(CreatureType::RABBIT,8);sp(CreatureType::DEER,4);sp(CreatureType::FOX,2);
        sp(CreatureType::BOAR,2);sp(CreatureType::FROG,3);sp(CreatureType::BUTTERFLY,3);sp(CreatureType::SNAKE,1);
    } else if(avgH<5.5f){
        sp(CreatureType::RABBIT,6);sp(CreatureType::DEER,3);sp(CreatureType::WOLF,2);
        sp(CreatureType::FOX,2);sp(CreatureType::OWL,1);sp(CreatureType::BOAR,2);sp(CreatureType::SNAKE,1);
    } else {
        sp(CreatureType::RABBIT,3);sp(CreatureType::WOLF,2);sp(CreatureType::EAGLE,1);sp(CreatureType::BEAR,1);
    }
}

// ══════════════════════════════════════════════════════════════════════
// TILE MANAGEMENT
// ══════════════════════════════════════════════════════════════════════

static void activateTile(int newTX,int newTZ){
    if(g_activeTX>=0 && g_activeTZ>=0){
        auto& c=g_tileCache[g_activeTZ][g_activeTX];
        c.creatures=g_creatures; c.plants=g_plants; c.valid=true;
    }
    g_activeTX=newTX; g_activeTZ=newTZ;
    auto& c=g_tileCache[newTZ][newTX];
    if(c.valid){g_creatures=c.creatures;g_plants=c.plants;}
    else{generateTilePlants(newTX,newTZ);spawnTileCreatures(newTX,newTZ);}
    g_selectedCreature=-1;
    for(int z=0;z<GRID_TILES;z++)for(int x=0;x<GRID_TILES;x++)g_tileMeshes[z][x].built=false;
    buildAllTileMeshes();
}

static void resetWorld(){
    generateTerrain();
    for(int z=0;z<GRID_TILES;z++)for(int x=0;x<GRID_TILES;x++)g_tileCache[z][x].valid=false;
    g_activeTX=g_activeTZ=-1;
    int startTX=4,startTZ=4;
    activateTile(startTX,startTZ);
    buildAllTileMeshes();
    g_camX=startTX*TILE_SIZE+TILE_SIZE/2.f;
    g_camZ=startTZ*TILE_SIZE+TILE_SIZE/2.f;
    g_camY=20.f;
}
// ══════════════════════════════════════════════════════════════════════
// CREATURE AI — FIND FUNCTIONS (tile-local)
// ══════════════════════════════════════════════════════════════════════

static int findNearestPrey(const Creature& c){
    const auto& s=CREATURE_STATS[(int)c.type];
    float best=s.sightRange; int id=-1;
    for(size_t i=0;i<g_creatures.size();i++){
        const auto& o=g_creatures[i];
        if(!o.isAlive()||&o==&c||!canHunt(c.type,o.type))continue;
        float d=distance2D(c.x,c.z,o.x,o.z);
        if(d<best){best=d;id=(int)i;}
    }
    return id;
}

static int findNearestPredator(const Creature& c){
    const auto& s=CREATURE_STATS[(int)c.type];
    float best=s.sightRange; int id=-1;
    for(size_t i=0;i<g_creatures.size();i++){
        const auto& o=g_creatures[i];
        if(!o.isAlive()||&o==&c||!canHunt(o.type,c.type))continue;
        float d=distance2D(c.x,c.z,o.x,o.z);
        if(d<best){best=d;id=(int)i;}
    }
    return id;
}

static int findNearestPlant(const Creature& c){
    const auto& s=CREATURE_STATS[(int)c.type];
    if(s.isPredator && !s.isOmnivore) return -1;
    float best=s.sightRange; int id=-1;
    for(size_t i=0;i<g_plants.size();i++){
        const auto& p=g_plants[i];
        if(p.nutrition<=0||p.type==PlantType::TREE)continue;
        float d=distance2D(c.x,c.z,p.x,p.z);
        if(d<best){best=d;id=(int)i;}
    }
    return id;
}

// ══════════════════════════════════════════════════════════════════════
// CREATURE AI — UPDATE
// ══════════════════════════════════════════════════════════════════════

static void updateCreatureAI(Creature& c,float dt){
    if(!c.isAlive())return;
    const auto& stats=CREATURE_STATS[(int)c.type];

    // Nocturnal: rest during day
    if(stats.isNocturnal && g_time.isDaytime()){
        c.vx=c.vz=0; c.state=AIState::RESTING; c.stateTimer=1.f; return;
    }
    // Passive: just wander, never hunt/flee
    if(stats.isPassive){
        c.stateTimer-=dt; c.energy-=stats.hungerRate*dt; c.age+=dt;
        if(c.energy<=0||c.age>300.f){c.health=0;c.state=AIState::DEAD;return;}
        if(c.stateTimer<=0){c.rotation+=(g_rand(g_rng)-0.5f)*PI;c.stateTimer=2.f+g_rand(g_rng)*3.f;}
        float sp=stats.maxSpeed*0.3f;
        c.vx=-sinf(c.rotation)*sp; c.vz=-cosf(c.rotation)*sp;
        c.wingAngle=sinf(c.age*12.f)*0.6f;
        return;
    }

    c.stateTimer-=dt; c.energy-=stats.hungerRate*dt; c.age+=dt;
    if(c.energy<=0||c.age>300.f){c.health=0;c.state=AIState::DEAD;return;}

    // Ambush: freeze in IDLE/WANDERING, only strike when prey enters attack range
    bool ambushFreeze=stats.isAmbush && (c.state==AIState::IDLE||c.state==AIState::WANDERING);

    switch(c.state){
    case AIState::IDLE:{
        if(ambushFreeze){c.vx=c.vz=0;}
        if(c.stateTimer<=0){
            if(c.energy<stats.maxEnergy*0.5f){
                if(stats.isPredator||stats.isAmbush){
                    int prey=findNearestPrey(c);
                    if(prey>=0){
                        float d=distance2D(c.x,c.z,g_creatures[prey].x,g_creatures[prey].z);
                        if(!stats.isAmbush || d<stats.attackRange*2.f){
                            c.targetId=prey;c.state=AIState::HUNTING;
                        }
                    } else if(stats.isOmnivore){
                        int pl=findNearestPlant(c);
                        if(pl>=0){c.targetId=pl;c.state=AIState::HUNTING;}
                        else{c.state=AIState::WANDERING;c.stateTimer=2.f+g_rand(g_rng)*3.f;}
                    } else {c.state=AIState::WANDERING;c.stateTimer=2.f+g_rand(g_rng)*3.f;}
                } else {
                    int pl=findNearestPlant(c);
                    if(pl>=0){c.targetId=pl;c.state=AIState::HUNTING;}
                    else{c.state=AIState::WANDERING;c.stateTimer=2.f+g_rand(g_rng)*3.f;}
                }
            } else if(c.energy>stats.reproductionEnergy && g_rand(g_rng)<0.01f){
                float ox=c.x+(g_rand(g_rng)-0.5f)*5.f,oz=c.z+(g_rand(g_rng)-0.5f)*5.f;
                spawnCreature(c.type,ox,oz);c.energy-=stats.reproductionEnergy*0.5f;
            } else {c.state=AIState::WANDERING;c.stateTimer=2.f+g_rand(g_rng)*5.f;}
        }
        // Defensive herbivore: charge instead of flee
        if(!stats.isPredator){
            int pred=findNearestPredator(c);
            if(pred>=0){
                float d=distance2D(c.x,c.z,g_creatures[pred].x,g_creatures[pred].z);
                if(stats.isDefensive && d<stats.sightRange*0.4f){
                    c.targetId=pred;c.state=AIState::HUNTING;
                } else {c.targetId=pred;c.state=AIState::FLEEING;}
            }
        }
        break;
    }
    case AIState::WANDERING:{
        if(ambushFreeze){c.vx=c.vz=0;}
        else{
            if(c.stateTimer<=0){c.rotation+=(g_rand(g_rng)-0.5f)*PI;c.stateTimer=1.f+g_rand(g_rng)*2.f;}
            float sp=stats.maxSpeed*0.3f;c.vx=-sinf(c.rotation)*sp;c.vz=-cosf(c.rotation)*sp;
        }
        if(!stats.isPredator && !stats.isAmbush){
            int pred=findNearestPredator(c);
            if(pred>=0 && distance2D(c.x,c.z,g_creatures[pred].x,g_creatures[pred].z)<stats.sightRange*0.7f){
                if(stats.isDefensive){c.targetId=pred;c.state=AIState::HUNTING;}
                else{c.targetId=pred;c.state=AIState::FLEEING;}
            }
        }
        if(c.stateTimer<=0 && g_rand(g_rng)<0.3f){c.state=AIState::IDLE;c.stateTimer=1.f;}
        break;
    }
    case AIState::FLEEING:{
        if(c.targetId<0||c.targetId>=(int)g_creatures.size()){c.state=AIState::IDLE;c.stateTimer=1.f;break;}
        const auto& pred=g_creatures[c.targetId];
        float d=distance2D(c.x,c.z,pred.x,pred.z);
        if(d>stats.sightRange||!pred.isAlive()){c.state=AIState::IDLE;c.stateTimer=2.f;}
        else{float a=atan2f(c.x-pred.x,c.z-pred.z);c.rotation=a;float sp=stats.maxSpeed;c.vx=-sinf(a)*sp;c.vz=-cosf(a)*sp;}
        break;
    }
    case AIState::HUNTING:{
        if(stats.isPredator||stats.isAmbush||stats.isDefensive){
            if(c.targetId<0||c.targetId>=(int)g_creatures.size()){c.state=AIState::IDLE;c.stateTimer=1.f;break;}
            auto& prey=g_creatures[c.targetId];
            if(!prey.isAlive()){c.state=AIState::IDLE;c.stateTimer=1.f;break;}
            float d=distance2D(c.x,c.z,prey.x,prey.z);
            if(d<stats.attackRange){
                prey.health-=stats.attackDamage*dt*10;
                if(prey.health<=0){prey.state=AIState::DEAD;c.energy=std::min(stats.maxEnergy,c.energy+50.f);c.state=AIState::EATING;c.stateTimer=3.f;}
            } else {
                float a=atan2f(prey.x-c.x,prey.z-c.z);c.rotation=a;c.vx=-sinf(a)*stats.maxSpeed;c.vz=-cosf(a)*stats.maxSpeed;
            }
        } else {
            if(c.targetId<0||c.targetId>=(int)g_plants.size()){c.state=AIState::IDLE;c.stateTimer=1.f;break;}
            auto& pl=g_plants[c.targetId];
            float d=distance2D(c.x,c.z,pl.x,pl.z);
            if(d<1.f){
                float e=std::min(pl.nutrition,5.f*dt);pl.nutrition-=e;c.energy=std::min(stats.maxEnergy,c.energy+e*2.f);
                if(pl.nutrition<=0){pl.eaten=true;c.state=AIState::IDLE;c.stateTimer=1.f;}
            } else {float a=atan2f(pl.x-c.x,pl.z-c.z);c.rotation=a;c.vx=-sinf(a)*stats.maxSpeed*0.5f;c.vz=-cosf(a)*stats.maxSpeed*0.5f;}
        }
        break;
    }
    case AIState::EATING: case AIState::RESTING:
        c.vx=c.vz=0; if(c.stateTimer<=0){c.state=AIState::IDLE;c.stateTimer=1.f;} break;
    case AIState::DEAD: c.vx=c.vy=c.vz=0; break;
    }
}
// ══════════════════════════════════════════════════════════════════════
// PHYSICS + SOFT TETHER
// ══════════════════════════════════════════════════════════════════════

static void updateCreaturePhysics(Creature& c,float dt){
    if(!c.isAlive())return;
    const auto& stats=CREATURE_STATS[(int)c.type];
    c.x+=c.vx*dt; c.z+=c.vz*dt;

    // Soft tether to tile edges
    float edgeX0=(float)(g_activeTX*TILE_SIZE), edgeX1=edgeX0+TILE_SIZE;
    float edgeZ0=(float)(g_activeTZ*TILE_SIZE), edgeZ1=edgeZ0+TILE_SIZE;
    float str=12.f;
    if(c.x<edgeX0+TETHER_MARGIN){float t=1.f-(c.x-edgeX0)/TETHER_MARGIN;c.vx+=t*str;}
    if(c.x>edgeX1-TETHER_MARGIN){float t=1.f-(edgeX1-c.x)/TETHER_MARGIN;c.vx-=t*str;}
    if(c.z<edgeZ0+TETHER_MARGIN){float t=1.f-(c.z-edgeZ0)/TETHER_MARGIN;c.vz+=t*str;}
    if(c.z>edgeZ1-TETHER_MARGIN){float t=1.f-(edgeZ1-c.z)/TETHER_MARGIN;c.vz-=t*str;}
    c.x=std::max(edgeX0+0.5f,std::min(edgeX1-0.5f,c.x));
    c.z=std::max(edgeZ0+0.5f,std::min(edgeZ1-0.5f,c.z));

    // Water-bound creatures
    if(stats.isWaterBound){
        float gh=getTerrainHeight(c.x,c.z);
        if(gh>=WATER_LEVEL){c.vx*=-1.f;c.vz*=-1.f;c.x+=c.vx*dt*2;c.z+=c.vz*dt*2;}
        c.y=std::min(WATER_LEVEL-0.1f,gh+0.1f);
    } else if(stats.canFly){
        float groundY=getTerrainHeight(c.x,c.z);
        float targetY=groundY+8.f+sinf(c.age*2.f)*2.f;
        c.y+=(targetY-c.y)*dt*2.f;
        c.wingAngle=sinf(c.age*8.f)*0.5f;
    } else {
        c.y=getTerrainHeight(c.x,c.z);
    }
    float sp=sqrtf(c.vx*c.vx+c.vz*c.vz);
    c.walkCycle+=sp*dt*2.f;
}

// ══════════════════════════════════════════════════════════════════════
// WEATHER & TIME UPDATE
// ══════════════════════════════════════════════════════════════════════

static void updateWeather(float dt){
    float tc=0,tr=0,tw=0;
    switch(g_weather.current){
    case Weather::CLEAR:  tc=0.1f;tr=0;tw=0.1f;break;
    case Weather::CLOUDY: tc=0.6f;tr=0;tw=0.2f;break;
    case Weather::RAIN:   tc=0.8f;tr=0.6f;tw=0.3f;break;
    case Weather::STORM:  tc=1.f;tr=1.f;tw=0.8f;break;
    }
    g_weather.cloudCover+=(tc-g_weather.cloudCover)*dt*0.5f;
    g_weather.rainIntensity+=(tr-g_weather.rainIntensity)*dt*0.5f;
    g_weather.windStrength+=(tw-g_weather.windStrength)*dt*0.5f;
    g_weather.windAngle+=dt*0.1f;
}

static void updateTime(float dt){
    if(g_time.paused)return;
    if(g_dayNightCycle){
        g_time.dayTime+=(dt*g_time.timeScale)/g_time.dayLength;
        if(g_time.dayTime>=1.f){g_time.dayTime-=1.f;g_time.day++;}
    }
    float sa=g_time.getSunAngle();
    g_lightDir[0]=cosf(sa)*0.5f;g_lightDir[1]=sinf(sa);g_lightDir[2]=0.3f;
    normalize3(g_lightDir);
    float dl=std::max(0.f,sinf(sa))*0.7f+0.3f;
    g_ambientLight=dl*(1.f-g_weather.cloudCover*0.5f);
    g_weather.lightLevel=g_ambientLight;
}

// ══════════════════════════════════════════════════════════════════════
// PLANT UPDATE (with butterfly pollination boost)
// ══════════════════════════════════════════════════════════════════════

static void updatePlants(float dt){
    for(auto& p:g_plants){
        float boost=1.f;
        for(const auto& c:g_creatures)
            if(c.isAlive()&&c.type==CreatureType::BUTTERFLY&&distance2D(c.x,c.z,p.x,p.z)<5.f){boost=1.5f;break;}
        if(p.eaten){
            p.growth+=dt*0.01f*g_weather.lightLevel*boost;
            if(p.growth>=0.3f){p.eaten=false;p.nutrition=p.maxNutrition*p.growth;}
        } else {
            if(p.growth<1.f){p.growth+=dt*0.005f*g_weather.lightLevel*boost;p.growth=std::min(1.f,p.growth);}
            if(p.nutrition<p.maxNutrition){p.nutrition+=dt*0.1f;p.nutrition=std::min(p.maxNutrition,p.nutrition);}
        }
    }
}
// ══════════════════════════════════════════════════════════════════════
// RENDERING HELPERS
// ══════════════════════════════════════════════════════════════════════

static void perspectiveGL(double fov,double asp,double zn,double zf){
    double f=1.0/tan(fov*PI/360.0);double m[16]={};
    m[0]=f/asp;m[5]=f;m[10]=(zf+zn)/(zn-zf);m[11]=-1;m[14]=2*zf*zn/(zn-zf);
    glMultMatrixd(m);
}
static void lookAtGL(float ex,float ey,float ez,float cx,float cy,float cz){
    float fx=cx-ex,fy=cy-ey,fz=cz-ez;
    float fl=sqrtf(fx*fx+fy*fy+fz*fz);fx/=fl;fy/=fl;fz/=fl;
    float sx=-fz,sy=0.f,sz=fx;
    float sl=sqrtf(sx*sx+sy*sy+sz*sz);if(sl>0){sx/=sl;sy/=sl;sz/=sl;}
    float ux=sy*fz-sz*fy,uy=sz*fx-sx*fz,uz=sx*fy-sy*fx;
    float m[16]={sx,ux,-fx,0,sy,uy,-fy,0,sz,uz,-fz,0,0,0,0,1};
    glMultMatrixf(m);glTranslatef(-ex,-ey,-ez);
}
static void glColorLit(float r,float g,float b,const float*n){
    float i=g_ambientLight+std::max(0.f,dot3(n,g_lightDir))*(1.f-g_ambientLight);
    glColor3f(r*i,g*i,b*i);
}

// ══════════════════════════════════════════════════════════════════════
// SHAPE HELPERS (Phase 2: Creature Mesh Upgrade)
// ══════════════════════════════════════════════════════════════════════

static void drawSphere(float cx,float cy,float cz,float rx,float ry,float rz,int sl,int st){
    for(int i=0;i<st;i++){
        float p0=PI*(-0.5f+(float)i/st),p1=PI*(-0.5f+(float)(i+1)/st);
        float cp0=cosf(p0),sp0=sinf(p0),cp1=cosf(p1),sp1=sinf(p1);
        glBegin(GL_TRIANGLE_STRIP);
        for(int j=0;j<=sl;j++){
            float th=2*PI*j/sl,ct=cosf(th),sn=sinf(th);
            glNormal3f(cp1*ct,sp1,cp1*sn);glVertex3f(cx+cp1*ct*rx,cy+sp1*ry,cz+cp1*sn*rz);
            glNormal3f(cp0*ct,sp0,cp0*sn);glVertex3f(cx+cp0*ct*rx,cy+sp0*ry,cz+cp0*sn*rz);
        }
        glEnd();
    }
}

static void drawCylinder(float cx,float cy,float cz,float r,float h,int segs){
    glBegin(GL_TRIANGLE_STRIP);
    for(int i=0;i<=segs;i++){
        float a=2*PI*i/segs,ca=cosf(a),sa=sinf(a);
        glNormal3f(ca,0,sa);
        glVertex3f(cx+ca*r,cy,cz+sa*r);
        glVertex3f(cx+ca*r,cy+h,cz+sa*r);
    }
    glEnd();
}

static void renderContactShadow(const Creature& c,float size){
    if(!c.isAlive())return;
    float gy=getTerrainHeight(c.x,c.z)+0.02f;
    float dist=c.y-gy;
    float alpha=std::max(0.f,0.35f-dist*0.015f);
    if(alpha<0.01f)return;
    // Sun-directional offset (Phase E)
    float shOffX=-g_lightDir[0]*dist*0.3f;
    float shOffZ=-g_lightDir[2]*dist*0.3f;
    float stretch=1.f+dist*0.08f;
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(0,0,0,alpha);
    glVertex3f(c.x+shOffX,gy,c.z+shOffZ);
    glColor4f(0,0,0,0.f);
    for(int i=0;i<=12;i++){float a=i/12.f*2*PI;glVertex3f(c.x+shOffX+cosf(a)*size*stretch,gy,c.z+shOffZ+sinf(a)*size*0.6f*stretch);}
    glEnd();
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

// ══════════════════════════════════════════════════════════════════════
// TERRAIN RENDERING (Phase 3: GLSL + VBO with fixed-function fallback)
// ══════════════════════════════════════════════════════════════════════

static void renderTerrain(){
    float fogD=0.003f+g_weather.rainIntensity*0.008f;
    float fogR,fogG,fogB; getSkyColors(fogR,fogG,fogB);
    bool useShader=g_hasShaders&&g_terrainProg;

    if(useShader){
        gl_UseProgram(g_terrainProg);
        gl_Uniform3f(g_tLocLight,g_lightDir[0],g_lightDir[1],g_lightDir[2]);
        gl_Uniform1f(g_tLocAmb,g_ambientLight);
        gl_Uniform1f(g_tLocFogD,fogD);
        gl_Uniform3f(g_tLocFogC,fogR,fogG,fogB);
    } else {
        glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);
        glEnable(GL_COLOR_MATERIAL);glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
        float lp[4]={g_lightDir[0],g_lightDir[1],g_lightDir[2],0.f};glLightfv(GL_LIGHT0,GL_POSITION,lp);
        float am[4]={g_ambientLight,g_ambientLight,g_ambientLight,1.f};glLightfv(GL_LIGHT0,GL_AMBIENT,am);
        float df[4]={1.f-g_ambientLight*.3f,1.f-g_ambientLight*.3f,1.f-g_ambientLight*.3f,1.f};glLightfv(GL_LIGHT0,GL_DIFFUSE,df);
    }

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    for(int tz=0;tz<GRID_TILES;tz++)
        for(int tx=0;tx<GRID_TILES;tx++){
            const auto& m=g_tileMeshes[tz][tx];
            if(!m.built||m.verts.empty())continue;
            if(useShader&&m.vbo){
                gl_BindBuffer(GL_ARRAY_BUFFER,m.vbo);
                gl_BindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.ibo);
                glVertexPointer(3,GL_FLOAT,sizeof(TerrainVertex),(void*)0);
                glNormalPointer(GL_FLOAT,sizeof(TerrainVertex),(void*)(3*sizeof(float)));
                glColorPointer(3,GL_FLOAT,sizeof(TerrainVertex),(void*)(6*sizeof(float)));
                glDrawElements(GL_TRIANGLES,(GLsizei)m.indices.size(),GL_UNSIGNED_SHORT,(void*)0);
            } else {
                const float*base=&m.verts[0].px;
                glVertexPointer(3,GL_FLOAT,sizeof(TerrainVertex),base);
                glNormalPointer(GL_FLOAT,sizeof(TerrainVertex),base+3);
                glColorPointer(3,GL_FLOAT,sizeof(TerrainVertex),base+6);
                glDrawElements(GL_TRIANGLES,(GLsizei)m.indices.size(),GL_UNSIGNED_SHORT,m.indices.data());
            }
        }

    if(useShader){gl_BindBuffer(GL_ARRAY_BUFFER,0);gl_BindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);}
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    if(useShader)gl_UseProgram(0);
    else{glDisable(GL_COLOR_MATERIAL);glDisable(GL_LIGHTING);}
}

// ══════════════════════════════════════════════════════════════════════
// WATER + TILE GRID LINES
// ══════════════════════════════════════════════════════════════════════

static void renderWater(){
    float fogD=0.003f+g_weather.rainIntensity*0.008f;
    float fogR,fogG,fogB; getSkyColors(fogR,fogG,fogB);
    bool useShader=g_hasShaders&&g_waterProg;

    if(useShader){
        gl_UseProgram(g_waterProg);
        gl_Uniform3f(g_wLocLight,g_lightDir[0],g_lightDir[1],g_lightDir[2]);
        gl_Uniform1f(g_wLocAmb,g_ambientLight);
        gl_Uniform1f(g_wLocFogD,fogD);
        gl_Uniform3f(g_wLocFogC,fogR,fogG,fogB);
        gl_Uniform1f(g_wLocTime,g_wallTime);
    } else {
        glColor4f(0.2f,0.4f,0.6f,0.3f);
    }

    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    for(int tz=0;tz<GRID_TILES;tz++)for(int tx=0;tx<GRID_TILES;tx++){
        int dist=std::max(std::abs(tx-g_activeTX),std::abs(tz-g_activeTZ));
        if(dist>2)continue;
        float x0=(float)(tx*TILE_SIZE),z0=(float)(tz*TILE_SIZE);
        if(useShader){
            int sub=8;float s=(float)TILE_SIZE/sub;
            glBegin(GL_TRIANGLES);
            for(int iz=0;iz<sub;iz++)for(int ix=0;ix<sub;ix++){
                float x=x0+ix*s,z=z0+iz*s,xn=x+s,zn=z+s;
                glVertex3f(x,WATER_LEVEL,z);glVertex3f(xn,WATER_LEVEL,z);glVertex3f(x,WATER_LEVEL,zn);
                glVertex3f(xn,WATER_LEVEL,z);glVertex3f(xn,WATER_LEVEL,zn);glVertex3f(x,WATER_LEVEL,zn);
            }
            glEnd();
        } else {
            glBegin(GL_QUADS);
            glVertex3f(x0,WATER_LEVEL,z0);glVertex3f(x0+TILE_SIZE,WATER_LEVEL,z0);
            glVertex3f(x0+TILE_SIZE,WATER_LEVEL,z0+TILE_SIZE);glVertex3f(x0,WATER_LEVEL,z0+TILE_SIZE);
            glEnd();
        }
    }
    glDisable(GL_BLEND);
    if(useShader)gl_UseProgram(0);
}

static void renderTileGrid(){
    glColor3f(0.2f,0.5f,0.2f);
    glBegin(GL_LINES);
    for(int i=0;i<=GRID_TILES;i++){
        float p=(float)(i*TILE_SIZE);
        glVertex3f(p,0.2f,0);glVertex3f(p,0.2f,(float)WORLD_SIZE);
        glVertex3f(0,0.2f,p);glVertex3f((float)WORLD_SIZE,0.2f,p);
    }
    glEnd();
    // Highlight active tile
    float ax=(float)(g_activeTX*TILE_SIZE),az=(float)(g_activeTZ*TILE_SIZE);
    glColor3f(1.f,1.f,0.3f);
    glBegin(GL_LINE_LOOP);
    glVertex3f(ax,0.3f,az);glVertex3f(ax+TILE_SIZE,0.3f,az);
    glVertex3f(ax+TILE_SIZE,0.3f,az+TILE_SIZE);glVertex3f(ax,0.3f,az+TILE_SIZE);
    glEnd();
}
// ══════════════════════════════════════════════════════════════════════
// PLANT RENDERING (active + adjacent tiles from cache)
// ══════════════════════════════════════════════════════════════════════

static void renderPlantList(const std::vector<Plant>& plants,bool half){
    int idx=0;
    for(const auto& p:plants){
        if(half && (idx++&1)) continue;
        if(p.eaten && p.growth<0.2f) continue;
        float s=p.growth; float n[3]={0,1,0};
        glPushMatrix(); glTranslatef(p.x,p.y,p.z);
        switch(p.type){
        case PlantType::GRASS:
            glColorLit(0.2f,0.6f*s,0.15f,n);
            glBegin(GL_TRIANGLES);glVertex3f(0,0,0);glVertex3f(-0.1f,0.3f*s,0);glVertex3f(0.1f,0.35f*s,0.05f);glEnd();
            break;
        case PlantType::BUSH:
            glColorLit(0.15f,0.5f*s,0.1f,n);
            glBegin(GL_TRIANGLE_FAN);glVertex3f(0,0.4f*s,0);
            for(int i=0;i<=8;i++){float a=i/8.f*2*PI;glVertex3f(cosf(a)*0.3f*s,0,sinf(a)*0.3f*s);}
            glEnd();break;
        case PlantType::TREE:{
            // Wind sway (Phase E)
            float treePhase=p.x*0.7f+p.z*0.5f;
            float wStr=g_weather.windStrength*0.15f;
            float swX=sinf(g_wallTime*1.8f+treePhase)*wStr*cosf(g_weather.windAngle);
            float swZ=sinf(g_wallTime*1.8f+treePhase)*wStr*sinf(g_weather.windAngle);
            float swX2=swX+sinf(g_wallTime*3.1f+treePhase*1.3f)*wStr*0.25f;
            float swZ2=swZ+cosf(g_wallTime*2.7f+treePhase*0.9f)*wStr*0.25f;
            // Directional ground shadow (Phase E)
            float shOff=s*2.5f;
            float shCX=-g_lightDir[0]*shOff,shCZ=-g_lightDir[2]*shOff;
            glDisable(GL_LIGHTING);
            glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
            glBegin(GL_TRIANGLE_FAN);
            glColor4f(0,0.05f,0,0.22f*g_ambientLight);
            glVertex3f(shCX+swX*0.5f,0.02f,shCZ+swZ*0.5f);
            glColor4f(0,0.05f,0,0.f);
            for(int i=0;i<=8;i++){float a=i/8.f*2*PI;glVertex3f(shCX+swX*0.5f+cosf(a)*1.4f*s,0.02f,shCZ+swZ*0.5f+sinf(a)*1.0f*s);}
            glEnd();
            glDisable(GL_BLEND);
            glEnable(GL_LIGHTING);
            // Trunk (top sways)
            glColor3f(0.35f*g_ambientLight,0.25f*g_ambientLight,0.15f*g_ambientLight);
            glBegin(GL_QUAD_STRIP);
            for(int i=0;i<=6;i++){float a=i/6.f*2*PI;glVertex3f(cosf(a)*0.15f,0,sinf(a)*0.15f);glVertex3f(swX*0.3f+cosf(a)*0.1f,2.f*s,swZ*0.3f+sinf(a)*0.1f);}
            glEnd();
            // Canopy (sways with wind)
            glColorLit(0.1f,0.45f,0.08f,n);
            glBegin(GL_TRIANGLE_FAN);glVertex3f(swX2,4.f*s,swZ2);
            for(int i=0;i<=8;i++){float a=i/8.f*2*PI;
                float r=1.2f*s*(0.85f+0.15f*sinf(a*3.f+g_wallTime*0.5f));
                glVertex3f(swX+cosf(a)*r,1.5f*s,swZ+sinf(a)*r);}
            glEnd();break;}
        }
        glPopMatrix();
    }
}

static void renderPlants(){
    renderPlantList(g_plants,false);
    // Adjacent tiles: render cached plants at half density
    for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++){
        if(dx==0&&dz==0)continue;
        int tx=g_activeTX+dx,tz=g_activeTZ+dz;
        if(tx<0||tx>=GRID_TILES||tz<0||tz>=GRID_TILES)continue;
        const auto& cache=g_tileCache[tz][tx];
        if(cache.valid) renderPlantList(cache.plants,true);
    }
}
// ══════════════════════════════════════════════════════════════════════
// CREATURE RENDERING (Phase 2: Sphere Bodies, Cylinder Legs, Shadows)
// ══════════════════════════════════════════════════════════════════════

static void renderCreature(const Creature& c){
    const auto& stats=CREATURE_STATS[(int)c.type];
    glPushMatrix();
    glTranslatef(c.x,c.y,c.z);
    glRotatef(-c.rotation*180.f/PI,0,1,0);
    glScalef(stats.size,stats.size,stats.size);
    float al=c.isAlive()?1.f:0.5f;
    float r=stats.color[0],g=stats.color[1],b=stats.color[2];
    if(c.state==AIState::HUNTING&&(stats.isPredator||stats.isDefensive))r=std::min(1.f,r+0.3f);
    if(c.selected){
        glDisable(GL_LIGHTING);glColor3f(1,1,0);glBegin(GL_LINE_LOOP);
        for(int i=0;i<16;i++){float a=i/16.f*2*PI;glVertex3f(cosf(a)*1.5f,0.1f,sinf(a)*1.5f);}
        glEnd();glEnable(GL_LIGHTING);
    }
    glColor3f(r*al,g*al,b*al);

    switch(c.type){
    case CreatureType::RABBIT:{
        float bob=sinf(c.walkCycle)*0.05f;
        drawSphere(0,0.22f+bob,0, 0.25f,0.18f,0.35f, 8,5);
        drawSphere(0,0.38f+bob,0.22f, 0.11f,0.10f,0.10f, 6,4);
        glNormal3f(0,0.7f,0.7f);glBegin(GL_TRIANGLES);
        glVertex3f(-0.08f,0.35f,0.2f);glVertex3f(-0.04f,0.65f,0.15f);glVertex3f(0.01f,0.35f,0.18f);
        glVertex3f(0.08f,0.35f,0.2f);glVertex3f(0.04f,0.65f,0.15f);glVertex3f(-0.01f,0.35f,0.18f);
        glEnd();break;}
    case CreatureType::DEER:{
        float bob=sinf(c.walkCycle)*0.08f;
        drawSphere(0,0.75f+bob,0, 0.22f,0.22f,0.5f, 8,5);
        drawSphere(0,0.92f+bob,0.52f, 0.10f,0.12f,0.12f, 6,4);
        drawCylinder(-0.14f,0,-0.3f, 0.035f,0.72f+bob, 5);
        drawCylinder(0.14f,0,-0.3f, 0.035f,0.72f+bob, 5);
        drawCylinder(-0.14f,0,0.25f, 0.035f,0.72f+bob, 5);
        drawCylinder(0.14f,0,0.25f, 0.035f,0.72f+bob, 5);
        glColor3f(r*al*0.7f,g*al*0.5f,b*al*0.3f);
        glNormal3f(0,1,0);glBegin(GL_LINES);
        glVertex3f(-0.05f,1.02f+bob,0.52f);glVertex3f(-0.18f,1.35f+bob,0.45f);
        glVertex3f(-0.18f,1.35f+bob,0.45f);glVertex3f(-0.28f,1.4f+bob,0.48f);
        glVertex3f(0.05f,1.02f+bob,0.52f);glVertex3f(0.18f,1.35f+bob,0.45f);
        glVertex3f(0.18f,1.35f+bob,0.45f);glVertex3f(0.28f,1.4f+bob,0.48f);
        glEnd();glColor3f(r*al,g*al,b*al);break;}
    case CreatureType::WOLF:{
        float bob=sinf(c.walkCycle)*0.07f;
        drawSphere(0,0.48f+bob,0, 0.22f,0.18f,0.55f, 8,5);
        drawSphere(0,0.55f+bob,0.45f, 0.12f,0.10f,0.14f, 6,4);
        glNormal3f(0,0.3f,1);glBegin(GL_TRIANGLES);
        glVertex3f(0,0.52f+bob,0.55f);glVertex3f(-0.06f,0.48f+bob,0.8f);glVertex3f(0.06f,0.48f+bob,0.8f);
        glEnd();
        drawCylinder(-0.13f,0,-0.35f, 0.03f,0.45f+bob, 5);
        drawCylinder(0.13f,0,-0.35f, 0.03f,0.45f+bob, 5);
        drawCylinder(-0.13f,0,0.18f, 0.03f,0.45f+bob, 5);
        drawCylinder(0.13f,0,0.18f, 0.03f,0.45f+bob, 5);break;}
    case CreatureType::EAGLE:{
        float w=c.wingAngle;
        drawSphere(0,0,0, 0.15f,0.10f,0.3f, 6,4);
        drawSphere(0,0.06f,0.28f, 0.08f,0.08f,0.08f, 6,3);
        glNormal3f(0,1,0);glBegin(GL_TRIANGLES);
        glVertex3f(-0.12f,0,0);glVertex3f(-1.f,sinf(w)*0.3f,-0.1f);glVertex3f(-0.8f,sinf(w)*0.3f,0.2f);
        glVertex3f(-0.12f,0,0);glVertex3f(-0.6f,sinf(w)*0.25f,0.15f);glVertex3f(-1.f,sinf(w)*0.3f,-0.1f);
        glVertex3f(0.12f,0,0);glVertex3f(1.f,sinf(w)*0.3f,-0.1f);glVertex3f(0.8f,sinf(w)*0.3f,0.2f);
        glVertex3f(0.12f,0,0);glVertex3f(0.6f,sinf(w)*0.25f,0.15f);glVertex3f(1.f,sinf(w)*0.3f,-0.1f);
        glEnd();break;}
    case CreatureType::FOX:{
        float bob=sinf(c.walkCycle)*0.05f;
        drawSphere(0,0.32f+bob,0, 0.18f,0.14f,0.42f, 8,5);
        drawSphere(0,0.40f+bob,0.38f, 0.09f,0.08f,0.10f, 6,4);
        glNormal3f(0,0.3f,1);glBegin(GL_TRIANGLES);
        glVertex3f(0,0.38f+bob,0.45f);glVertex3f(-0.04f,0.35f+bob,0.7f);glVertex3f(0.04f,0.35f+bob,0.7f);
        glEnd();
        drawCylinder(-0.10f,0,-0.28f, 0.025f,0.30f+bob, 5);
        drawCylinder(0.10f,0,-0.28f, 0.025f,0.30f+bob, 5);
        drawCylinder(-0.10f,0,0.12f, 0.025f,0.30f+bob, 5);
        drawCylinder(0.10f,0,0.12f, 0.025f,0.30f+bob, 5);
        float tr=r*0.9f*al,tg=g*0.7f*al,tb=b*0.6f*al;glColor3f(tr,tg,tb);
        glNormal3f(0,0.5f,-1);glBegin(GL_TRIANGLES);
        glVertex3f(0,0.35f+bob,-0.42f);glVertex3f(-0.08f,0.4f+bob,-0.85f);glVertex3f(0.08f,0.38f+bob,-0.8f);
        glEnd();glColor3f(r*al,g*al,b*al);break;}
    case CreatureType::BEAR:{
        float bob=sinf(c.walkCycle)*0.04f;
        drawSphere(0,0.65f+bob,0, 0.35f,0.30f,0.55f, 8,5);
        drawSphere(0,0.85f+bob,0.48f, 0.18f,0.16f,0.16f, 6,4);
        drawCylinder(-0.22f,0,-0.35f, 0.06f,0.62f+bob, 5);
        drawCylinder(0.22f,0,-0.35f, 0.06f,0.62f+bob, 5);
        drawCylinder(-0.22f,0,0.2f, 0.06f,0.62f+bob, 5);
        drawCylinder(0.22f,0,0.2f, 0.06f,0.62f+bob, 5);
        drawSphere(-0.12f,0.92f+bob,0.5f, 0.06f,0.06f,0.04f, 4,3);
        drawSphere(0.12f,0.92f+bob,0.5f, 0.06f,0.06f,0.04f, 4,3);break;}
    case CreatureType::BOAR:{
        float bob=sinf(c.walkCycle)*0.05f;
        drawSphere(0,0.32f+bob,0, 0.30f,0.22f,0.45f, 8,5);
        drawSphere(0,0.38f+bob,0.42f, 0.12f,0.10f,0.14f, 6,4);
        drawCylinder(-0.16f,0,-0.28f, 0.035f,0.30f+bob, 5);
        drawCylinder(0.16f,0,-0.28f, 0.035f,0.30f+bob, 5);
        drawCylinder(-0.16f,0,0.18f, 0.035f,0.30f+bob, 5);
        drawCylinder(0.16f,0,0.18f, 0.035f,0.30f+bob, 5);
        glNormal3f(0,0.5f,1);glBegin(GL_LINES);
        glVertex3f(-0.06f,0.35f+bob,0.52f);glVertex3f(-0.1f,0.28f+bob,0.65f);
        glVertex3f(0.06f,0.35f+bob,0.52f);glVertex3f(0.1f,0.28f+bob,0.65f);
        glEnd();break;}
    case CreatureType::FROG:{
        float hop=std::abs(sinf(c.walkCycle*2))*0.15f;
        drawSphere(0,0.1f+hop,0, 0.2f,0.12f,0.15f, 6,4);
        drawSphere(0,0.18f+hop,0.12f, 0.08f,0.06f,0.06f, 6,3);
        drawSphere(-0.06f,0.22f+hop,0.14f, 0.03f,0.04f,0.03f, 4,3);
        drawSphere(0.06f,0.22f+hop,0.14f, 0.03f,0.04f,0.03f, 4,3);
        glNormal3f(0,1,0);glBegin(GL_LINES);
        glVertex3f(-0.1f,0.05f+hop,-0.1f);glVertex3f(-0.2f,0,-0.25f);
        glVertex3f(0.1f,0.05f+hop,-0.1f);glVertex3f(0.2f,0,-0.25f);
        glVertex3f(-0.06f,0.05f+hop,0.08f);glVertex3f(-0.12f,0,0.12f);
        glVertex3f(0.06f,0.05f+hop,0.08f);glVertex3f(0.12f,0,0.12f);
        glEnd();break;}
    case CreatureType::OWL:{
        float w=c.wingAngle;
        drawSphere(0,0.2f,0, 0.18f,0.18f,0.18f, 6,4);
        drawSphere(0,0.45f,0.05f, 0.15f,0.14f,0.13f, 6,4);
        drawSphere(-0.06f,0.52f,0.12f, 0.04f,0.05f,0.04f, 4,3);
        drawSphere(0.06f,0.52f,0.12f, 0.04f,0.05f,0.04f, 4,3);
        glNormal3f(0,1,0);glBegin(GL_TRIANGLES);
        glVertex3f(-0.15f,0.28f,0);glVertex3f(-0.6f,0.18f+sinf(w)*0.2f,-0.1f);glVertex3f(-0.4f,0.08f+sinf(w)*0.15f,0.15f);
        glVertex3f(0.15f,0.28f,0);glVertex3f(0.6f,0.18f+sinf(w)*0.2f,-0.1f);glVertex3f(0.4f,0.08f+sinf(w)*0.15f,0.15f);
        glEnd();break;}
    case CreatureType::SNAKE:{
        for(int i=0;i<12;i++){
            float t=i/12.f,t1=(i+1)/12.f;
            float sz=0.06f*(1.f-t*0.5f);
            float x0=sinf(t*PI*2+c.walkCycle)*0.15f, z0=t*1.f-0.5f;
            float x1=sinf(t1*PI*2+c.walkCycle)*0.15f,z1=t1*1.f-0.5f;
            drawSphere((x0+x1)*0.5f,0.04f,(z0+z1)*0.5f, sz,sz*0.8f,sz*1.2f, 4,3);
        }
        drawSphere(sinf(c.walkCycle)*0.15f,0.06f,0.5f, 0.06f,0.05f,0.07f, 5,3);break;}
    case CreatureType::BUTTERFLY:{
        float w=sinf(c.wingAngle)*0.4f;
        float br=0.5f+sinf(c.age*1.5f)*0.3f,bg=0.3f+sinf(c.age*2.1f+1.f)*0.3f,bb=0.5f+cosf(c.age*1.8f)*0.3f;
        glColor3f(br*al,bg*al,bb*al);
        drawSphere(0,0,0, 0.04f,0.03f,0.08f, 4,3);
        glNormal3f(0,1,0);glBegin(GL_TRIANGLES);
        glVertex3f(0,0,0);glVertex3f(-0.3f,w,-0.05f);glVertex3f(-0.15f,w*0.7f,0.1f);
        glVertex3f(0,0,0);glVertex3f(-0.15f,w*0.7f,0.1f);glVertex3f(-0.1f,w*0.4f,-0.06f);
        glVertex3f(0,0,0);glVertex3f(0.3f,w,-0.05f);glVertex3f(0.15f,w*0.7f,0.1f);
        glVertex3f(0,0,0);glVertex3f(0.15f,w*0.7f,0.1f);glVertex3f(0.1f,w*0.4f,-0.06f);
        glEnd();break;}
    case CreatureType::FISH:{
        drawSphere(0,0,0, 0.1f,0.08f,0.3f, 6,4);
        glNormal3f(0,1,0);glBegin(GL_TRIANGLES);
        glVertex3f(0,0,-0.3f);glVertex3f(-0.1f,0.08f,-0.5f);glVertex3f(0.1f,-0.08f,-0.5f);
        glVertex3f(0,0.06f,0);glVertex3f(0,0.18f,-0.1f);glVertex3f(0,0.06f,-0.15f);
        glEnd();break;}
    default:break;
    }
    glPopMatrix();
}

static void renderCreatures(){
    glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
    float lightPos[4]={g_lightDir[0],g_lightDir[1],g_lightDir[2],0.f};
    glLightfv(GL_LIGHT0,GL_POSITION,lightPos);
    float amb[4]={g_ambientLight,g_ambientLight,g_ambientLight,1.f};
    float dif[4]={1.f-g_ambientLight*0.3f,1.f-g_ambientLight*0.3f,1.f-g_ambientLight*0.3f,1.f};
    glLightfv(GL_LIGHT0,GL_AMBIENT,amb);
    glLightfv(GL_LIGHT0,GL_DIFFUSE,dif);
    for(const auto& c:g_creatures){
        renderContactShadow(c,CREATURE_STATS[(int)c.type].size*1.2f);
        renderCreature(c);
    }
    glDisable(GL_COLOR_MATERIAL);glDisable(GL_LIGHTING);
}
// ══════════════════════════════════════════════════════════════════════
// SKY & WEATHER RENDERING (Phase D: Dynamic Sky & Sun)
// ══════════════════════════════════════════════════════════════════════

// Compute sky horizon color for fog/clear matching
static void getSkyColors(float& fogR, float& fogG, float& fogB) {
    float sa = g_time.getSunAngle();
    float sunH = sinf(sa);  // -1..1, >0 = daytime
    float cd = 1.f - g_weather.cloudCover * 0.3f;

    // Twilight factor: strong near horizon (sunH ~ 0)
    float twilight = std::max(0.f, 1.f - fabsf(sunH) * 5.f);
    twilight *= twilight;

    // Base horizon color: blueish day, dark night
    float dayF = std::max(0.f, sunH);
    fogR = (0.45f + dayF * 0.25f + twilight * 0.35f) * cd;
    fogG = (0.50f + dayF * 0.25f + twilight * 0.12f) * cd;
    fogB = (0.62f + dayF * 0.25f - twilight * 0.10f) * cd;

    // Night darkening
    float nightF = std::max(0.f, -sunH * 0.6f);
    fogR = std::max(0.02f, fogR - nightF * 0.3f);
    fogG = std::max(0.02f, fogG - nightF * 0.35f);
    fogB = std::max(0.04f, fogB - nightF * 0.25f);
}

static void renderSky(){
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    float sa = g_time.getSunAngle();
    float sunH = sinf(sa);       // sun altitude: -1..1
    float dayF = std::max(0.f, sunH);
    float cd = 1.f - g_weather.cloudCover * 0.3f;

    // Twilight factor (dawn/dusk glow)
    float twilight = std::max(0.f, 1.f - fabsf(sunH) * 5.f);
    twilight *= twilight;

    // ── Multi-band sky gradient (4 horizontal bands) ──
    // Zenith (top), upper, lower, horizon (bottom)
    float zR = (0.05f + dayF * 0.15f) * cd;
    float zG = (0.08f + dayF * 0.25f) * cd;
    float zB = (0.20f + dayF * 0.55f) * cd;

    float uR = (0.10f + dayF * 0.25f + twilight * 0.10f) * cd;
    float uG = (0.12f + dayF * 0.35f + twilight * 0.05f) * cd;
    float uB = (0.28f + dayF * 0.50f - twilight * 0.05f) * cd;

    float lR = (0.30f + dayF * 0.30f + twilight * 0.30f) * cd;
    float lG = (0.32f + dayF * 0.30f + twilight * 0.15f) * cd;
    float lB = (0.42f + dayF * 0.35f - twilight * 0.08f) * cd;

    float hR = (0.45f + dayF * 0.25f + twilight * 0.40f) * cd;
    float hG = (0.48f + dayF * 0.25f + twilight * 0.15f) * cd;
    float hB = (0.55f + dayF * 0.30f - twilight * 0.15f) * cd;

    // Night: darken all bands
    float nightDim = std::max(0.f, -sunH * 0.5f);
    zR=std::max(0.01f,zR-nightDim*0.2f); zG=std::max(0.01f,zG-nightDim*0.25f); zB=std::max(0.03f,zB-nightDim*0.15f);
    uR=std::max(0.01f,uR-nightDim*0.2f); uG=std::max(0.01f,uG-nightDim*0.25f); uB=std::max(0.03f,uB-nightDim*0.15f);
    lR=std::max(0.02f,lR-nightDim*0.25f);lG=std::max(0.02f,lG-nightDim*0.3f); lB=std::max(0.04f,lB-nightDim*0.2f);
    hR=std::max(0.02f,hR-nightDim*0.3f); hG=std::max(0.02f,hG-nightDim*0.35f);hB=std::max(0.04f,hB-nightDim*0.25f);

    // Draw 3 gradient strips: horizon→lower→upper→zenith
    glBegin(GL_QUADS);
    // Bottom strip (horizon → lower)
    glColor3f(hR,hG,hB); glVertex2f(-1,-1); glVertex2f(1,-1);
    glColor3f(lR,lG,lB); glVertex2f(1,-0.2f); glVertex2f(-1,-0.2f);
    // Middle strip (lower → upper)
    glColor3f(lR,lG,lB); glVertex2f(-1,-0.2f); glVertex2f(1,-0.2f);
    glColor3f(uR,uG,uB); glVertex2f(1,0.4f); glVertex2f(-1,0.4f);
    // Top strip (upper → zenith)
    glColor3f(uR,uG,uB); glVertex2f(-1,0.4f); glVertex2f(1,0.4f);
    glColor3f(zR,zG,zB); glVertex2f(1,1); glVertex2f(-1,1);
    glEnd();

    // ── Stars (visible at night, fade during twilight) ──
    float nightAlpha = std::max(0.f, std::min(1.f, -sunH * 3.f));
    nightAlpha *= (1.f - g_weather.cloudCover * 0.8f);
    if(nightAlpha > 0.02f){
        glBegin(GL_POINTS);
        for(int i=0;i<80;i++){
            float sx = hash2D((float)i * 7.3f, 1.1f);
            float sy = hash2D(2.2f, (float)i * 5.7f) * 0.5f + 0.2f;
            float bright = 0.5f + 0.5f * hash2D((float)i * 3.1f, (float)i * 1.9f);
            float twinkle = 0.7f + 0.3f * sinf(g_wallTime * (1.5f + (float)i * 0.3f) + (float)i);
            float a = nightAlpha * bright * twinkle;
            glColor4f(0.9f + bright * 0.1f, 0.9f + bright * 0.08f, 1.f, a);
            glVertex2f(sx, sy);
        }
        glEnd();
    }

    // ── Sun disc + corona glow ──
    float ssx = cosf(sa) * 0.6f, ssy = sinf(sa) * 0.7f;
    if(ssy > -0.3f){
        // Golden hour tint: warm at low angles
        float lowAngle = std::max(0.f, 1.f - sunH * 4.f);
        float sr = std::min(1.f, 1.0f + lowAngle * 0.0f);
        float sg = std::min(1.f, 0.95f - lowAngle * 0.25f);
        float sb = std::min(1.f, 0.80f - lowAngle * 0.45f);
        float sunAlpha = std::min(1.f, (ssy + 0.3f) * 2.5f);

        // Outer corona (large soft glow)
        float coronaR = 0.12f;
        glBegin(GL_TRIANGLE_FAN);
        glColor4f(sr, sg, sb, 0.25f * sunAlpha * cd);
        glVertex2f(ssx, ssy);
        glColor4f(sr, sg * 0.7f, sb * 0.3f, 0.0f);
        for(int i=0;i<=20;i++){float a=i/20.f*2*PI;glVertex2f(ssx+cosf(a)*coronaR,ssy+sinf(a)*coronaR);}
        glEnd();

        // Inner corona
        float innerR = 0.07f;
        glBegin(GL_TRIANGLE_FAN);
        glColor4f(sr, sg, sb, 0.45f * sunAlpha * cd);
        glVertex2f(ssx, ssy);
        glColor4f(sr, sg * 0.8f, sb * 0.5f, 0.0f);
        for(int i=0;i<=16;i++){float a=i/16.f*2*PI;glVertex2f(ssx+cosf(a)*innerR,ssy+sinf(a)*innerR);}
        glEnd();

        // Sun disc (solid core)
        glBegin(GL_TRIANGLE_FAN);
        glColor4f(sr, sg, sb, sunAlpha);
        glVertex2f(ssx, ssy);
        for(int i=0;i<=16;i++){float a=i/16.f*2*PI;glVertex2f(ssx+cosf(a)*0.04f,ssy+sinf(a)*0.04f);}
        glEnd();
    }

    // ── Moon (opposite side, with craters) ──
    if(nightAlpha > 0.05f){
        float mx = -cosf(sa) * 0.5f, my = -sinf(sa) * 0.6f + 0.15f;
        if(my > -0.2f){
            float moonA = std::min(1.f, nightAlpha * 1.5f) * cd;
            // Moon glow
            glBegin(GL_TRIANGLE_FAN);
            glColor4f(0.7f,0.75f,0.85f,0.12f*moonA);
            glVertex2f(mx,my);
            glColor4f(0.5f,0.55f,0.65f,0.0f);
            for(int i=0;i<=16;i++){float a=i/16.f*2*PI;glVertex2f(mx+cosf(a)*0.08f,my+sinf(a)*0.08f);}
            glEnd();
            // Moon disc
            glBegin(GL_TRIANGLE_FAN);
            glColor4f(0.85f,0.87f,0.92f,moonA);
            glVertex2f(mx,my);
            for(int i=0;i<=16;i++){float a=i/16.f*2*PI;glVertex2f(mx+cosf(a)*0.04f,my+sinf(a)*0.04f);}
            glEnd();
            // Craters (small dark spots)
            float craters[][3]={{-0.008f,0.01f,0.008f},{0.012f,-0.005f,0.006f},{-0.002f,-0.012f,0.005f}};
            for(auto& cr:craters){
                glBegin(GL_TRIANGLE_FAN);
                glColor4f(0.65f,0.67f,0.72f,moonA*0.6f);
                float cx=mx+cr[0],cy=my+cr[1],crr=cr[2];
                glVertex2f(cx,cy);
                for(int i=0;i<=8;i++){float a=i/8.f*2*PI;glVertex2f(cx+cosf(a)*crr,cy+sinf(a)*crr);}
                glEnd();
            }
        }
    }

    // ── Procedural cloud layer ──
    if(g_weather.cloudCover > 0.05f){
        float ca = g_weather.cloudCover * 0.45f;
        float drift = g_wallTime * 0.008f + g_weather.windStrength * g_wallTime * 0.01f;
        for(int i=0;i<12;i++){
            float cx = hash2D((float)i * 13.7f, 3.1f) * 2.f - 1.f;
            float cy = 0.1f + hash2D(5.5f, (float)i * 9.3f) * 0.55f;
            float cw = 0.15f + hash2D((float)i * 2.1f, 7.7f) * 0.20f;
            float ch = 0.02f + hash2D((float)i * 4.4f, 1.3f) * 0.03f;

            // Drift and wrap
            cx = fmodf(cx + drift + 3.f, 2.f) - 1.f;

            // Cloud brightness: lit by sun, darker at night/storm
            float cloudBright = 0.6f + dayF * 0.35f - g_weather.rainIntensity * 0.3f;
            float stormDark = g_weather.rainIntensity * 0.25f;

            glBegin(GL_TRIANGLE_FAN);
            glColor4f(cloudBright - stormDark, cloudBright - stormDark * 0.5f, cloudBright, ca);
            glVertex2f(cx, cy);
            glColor4f(cloudBright - stormDark, cloudBright - stormDark * 0.5f, cloudBright, 0.f);
            for(int j=0;j<=12;j++){
                float a=j/12.f*2*PI;
                float rx = cw * (0.8f + 0.2f * sinf(a * 3.f + (float)i));
                float ry = ch * (0.7f + 0.3f * cosf(a * 2.f + (float)i * 0.5f));
                glVertex2f(cx + cosf(a) * rx, cy + sinf(a) * ry);
            }
            glEnd();
        }
    }

    glDisable(GL_BLEND);
    glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
}

static void renderRain(){
    if(g_weather.rainIntensity<0.1f)return;
    static float ro=0;ro+=0.5f;
    glColor4f(0.6f,0.7f,0.8f,g_weather.rainIntensity*0.5f);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_LINES);
    int nd=(int)(g_weather.rainIntensity*200);
    for(int i=0;i<nd;i++){
        float x=g_camX+(hash2D(i*0.1f,ro)-0.5f)*50.f,z=g_camZ+(hash2D(ro,i*0.1f)-0.5f)*50.f;
        float y=fmodf(hash2D((float)i,(float)i)*30.f+ro,30.f)+g_camY-15.f;
        glVertex3f(x,y,z);glVertex3f(x+g_weather.windStrength*cosf(g_weather.windAngle),y-1.f,z+g_weather.windStrength*sinf(g_weather.windAngle));
    }
    glEnd();glDisable(GL_BLEND);
}
// ══════════════════════════════════════════════════════════════════════
// HUD — TILE INFO + DRO + POPULATION (12 types)
// ══════════════════════════════════════════════════════════════════════

static void renderUI(int winW,int winH){
    if(!g_showHUD)return;
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();
    glOrtho(0,winW,0,winH,-1,1);
    glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
    glDisable(GL_DEPTH_TEST);glEnable(GL_BLEND);

    char buf[128]; float y;

    // Left panel background
    glColor4f(0,0,0,0.6f);glBegin(GL_QUADS);
    glVertex2f(10,(float)winH-10);glVertex2f(290,(float)winH-10);glVertex2f(290,(float)winH-310);glVertex2f(10,(float)winH-310);glEnd();

    y=(float)winH-25.f;
    glColor3f(1,1,1);
    gltext::draw("=== TILE ECOSYSTEM ===",15,y);y-=18;

    // Tile info
    float avgH=getTileAvgHeight(g_activeTX,g_activeTZ);
    snprintf(buf,sizeof(buf),"Tile [%d,%d] %s",g_activeTX,g_activeTZ,getBiomeName(avgH));
    glColor3f(0.7f,1.f,0.7f);gltext::draw(buf,15,y);y-=15;

    // DRO precision
    snprintf(buf,sizeof(buf),"DRO: Active B-64  %.0f FPS  %s",g_fps,g_hasShaders?"GLSL":"FFP");
    glColor3f(0.6f,0.8f,1.f);gltext::draw(buf,15,y);y-=18;

    // Time
    glColor3f(1,1,1);
    int hour=(int)(g_time.dayTime*24),minute=(int)((g_time.dayTime*24-hour)*60);
    snprintf(buf,sizeof(buf),"Day %d  %02d:%02d",g_time.day,hour,minute);
    gltext::draw(buf,15,y);y-=15;
    snprintf(buf,sizeof(buf),"Time: %.0fx %s",g_time.timeScale,g_time.paused?"(PAUSED)":"");
    gltext::draw(buf,15,y);y-=15;
    snprintf(buf,sizeof(buf),"Weather: %s",WEATHER_NAMES[(int)g_weather.current]);
    gltext::draw(buf,15,y);y-=18;

    // Controls
    glColor3f(0.7f,0.8f,1.f);gltext::draw("--- CONTROLS ---",15,y);y-=15;
    glColor3f(1,1,1);
    snprintf(buf,sizeof(buf),"Spawn: %s (1-0,-,=)",CREATURE_STATS[(int)g_selectedType].name);
    gltext::draw(buf,15,y);y-=15;
    gltext::draw("Space:Spawn T:Time G:Weather",15,y);y-=15;
    gltext::draw("P:Pause H:HUD F:Follow R:Reset",15,y);y-=15;
    gltext::draw("Tab:Next  N:Day/Night",15,y);y-=18;

    // DRO bars
    glColor3f(0.6f,0.8f,1.f);gltext::draw("--- DRO LOD ---",15,y);y-=15;
    const char* lodLabels[]={"Active B-64","Adj    B-64","Ring2  B-32","Far    B-16"};
    float lodColors[][3]={{0.2f,1.f,0.2f},{0.2f,.9f,0.2f},{0.8f,0.8f,0.2f},{0.8f,0.5f,0.2f}};
    float lodWidths[]={1.f,1.f,0.5f,0.25f};
    for(int i=0;i<4;i++){
        glColor3f(lodColors[i][0],lodColors[i][1],lodColors[i][2]);
        gltext::draw(lodLabels[i],15,y);
        // Bar
        glBegin(GL_QUADS);
        glVertex2f(140,y+2);glVertex2f(140+lodWidths[i]*130,y+2);
        glVertex2f(140+lodWidths[i]*130,y+10);glVertex2f(140,y+10);
        glEnd();
        y-=14;
    }

    // Right panel — Population
    glColor4f(0,0,0,0.6f);glBegin(GL_QUADS);
    glVertex2f((float)winW-170,(float)winH-10);glVertex2f((float)winW-10,(float)winH-10);
    glVertex2f((float)winW-10,(float)winH-220);glVertex2f((float)winW-170,(float)winH-220);glEnd();

    y=(float)winH-25.f;
    glColor3f(1,1,1);gltext::draw("POPULATION",(float)winW-160,y);y-=18;

    int counts[NUM_CREATURE_TYPES]={};
    for(const auto& c:g_creatures)if(c.isAlive())counts[(int)c.type]++;
    for(int i=0;i<NUM_CREATURE_TYPES;i++){
        const auto& s=CREATURE_STATS[i];
        glColor3f(s.color[0],s.color[1],s.color[2]);
        snprintf(buf,sizeof(buf),"%-10s %d",s.name,counts[i]);
        gltext::draw(buf,(float)winW-160,y);y-=14;
    }

    glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);
    glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);
}
// ══════════════════════════════════════════════════════════════════════
// INPUT (12 creature keys, weather on G)
// ══════════════════════════════════════════════════════════════════════

static void keyCallback(GLFWwindow* w,int key,int,int action,int){
    if(action==GLFW_PRESS){
        g_keys[key]=true;
        if(key==GLFW_KEY_ESCAPE)glfwSetWindowShouldClose(w,GLFW_TRUE);
        if(key==GLFW_KEY_R)resetWorld();
        if(key==GLFW_KEY_P)g_time.paused=!g_time.paused;
        if(key==GLFW_KEY_H)g_showHUD=!g_showHUD;
        if(key==GLFW_KEY_N)g_dayNightCycle=!g_dayNightCycle;
        if(key==GLFW_KEY_F)g_followMode=!g_followMode;

        if(key==GLFW_KEY_1)g_selectedType=CreatureType::RABBIT;
        if(key==GLFW_KEY_2)g_selectedType=CreatureType::DEER;
        if(key==GLFW_KEY_3)g_selectedType=CreatureType::WOLF;
        if(key==GLFW_KEY_4)g_selectedType=CreatureType::EAGLE;
        if(key==GLFW_KEY_5)g_selectedType=CreatureType::FOX;
        if(key==GLFW_KEY_6)g_selectedType=CreatureType::BEAR;
        if(key==GLFW_KEY_7)g_selectedType=CreatureType::BOAR;
        if(key==GLFW_KEY_8)g_selectedType=CreatureType::FROG;
        if(key==GLFW_KEY_9)g_selectedType=CreatureType::OWL;
        if(key==GLFW_KEY_0)g_selectedType=CreatureType::SNAKE;
        if(key==GLFW_KEY_MINUS)g_selectedType=CreatureType::BUTTERFLY;
        if(key==GLFW_KEY_EQUAL)g_selectedType=CreatureType::FISH;

        if(key==GLFW_KEY_SPACE){
            float sx=g_camX-sinf(g_camYaw)*10.f,sz=g_camZ-cosf(g_camYaw)*10.f;
            spawnCreature(g_selectedType,sx,sz);
        }
        if(key==GLFW_KEY_T){
            if(g_time.timeScale<2.f)g_time.timeScale=5.f;
            else if(g_time.timeScale<10.f)g_time.timeScale=20.f;
            else g_time.timeScale=1.f;
        }
        if(key==GLFW_KEY_G)g_weather.current=(Weather)(((int)g_weather.current+1)%4);
        if(key==GLFW_KEY_TAB){
            if(g_selectedCreature>=0&&g_selectedCreature<(int)g_creatures.size())g_creatures[g_selectedCreature].selected=false;
            g_selectedCreature++;
            if(g_selectedCreature>=(int)g_creatures.size())g_selectedCreature=0;
            if(!g_creatures.empty())g_creatures[g_selectedCreature].selected=true;
        }
    } else if(action==GLFW_RELEASE){g_keys[key]=false;}
}

static void mouseButtonCallback(GLFWwindow* w,int button,int action,int){
    if(button==GLFW_MOUSE_BUTTON_RIGHT){
        g_mouseCaptured=(action==GLFW_PRESS);
        glfwSetInputMode(w,GLFW_CURSOR,g_mouseCaptured?GLFW_CURSOR_DISABLED:GLFW_CURSOR_NORMAL);
    }
}
static void cursorCallback(GLFWwindow*,double x,double y){
    static double lx=0,ly=0;
    if(g_mouseCaptured){g_camYaw-=(float)(x-lx)*0.003f;g_camPitch-=(float)(y-ly)*0.003f;g_camPitch=std::max(-1.5f,std::min(1.5f,g_camPitch));}
    lx=x;ly=y;
}
static void scrollCallback(GLFWwindow*,double,double yoff){
    g_camSpeed=std::max(5.f,std::min(80.f,g_camSpeed+(float)yoff*3.f));
}

// ══════════════════════════════════════════════════════════════════════
// UPDATE (tile-aware)
// ══════════════════════════════════════════════════════════════════════

static void update(float dt){
    float fx=-sinf(g_camYaw)*cosf(g_camPitch),fy=sinf(g_camPitch),fz=-cosf(g_camYaw)*cosf(g_camPitch);
    float rx=cosf(g_camYaw),rz=-sinf(g_camYaw);
    float sp=g_camSpeed*dt;
    if(g_keys[GLFW_KEY_W]){g_camX+=fx*sp;g_camY+=fy*sp;g_camZ+=fz*sp;}
    if(g_keys[GLFW_KEY_S]){g_camX-=fx*sp;g_camY-=fy*sp;g_camZ-=fz*sp;}
    if(g_keys[GLFW_KEY_A]){g_camX-=rx*sp;g_camZ-=rz*sp;}
    if(g_keys[GLFW_KEY_D]){g_camX+=rx*sp;g_camZ+=rz*sp;}
    if(g_keys[GLFW_KEY_LEFT_SHIFT])g_camY-=sp;

    if(g_followMode&&g_selectedCreature>=0&&g_selectedCreature<(int)g_creatures.size()){
        const auto& c=g_creatures[g_selectedCreature];
        g_camX=c.x+sinf(g_camYaw)*10.f;g_camZ=c.z+cosf(g_camYaw)*10.f;g_camY=c.y+5.f;
    }

    g_camX=std::max(1.f,std::min((float)WORLD_SIZE-1,g_camX));
    g_camZ=std::max(1.f,std::min((float)WORLD_SIZE-1,g_camZ));
    g_camY=std::max(2.f,g_camY);

    // Tile transition
    int ntx=std::max(0,std::min(GRID_TILES-1,(int)(g_camX/(float)TILE_SIZE)));
    int ntz=std::max(0,std::min(GRID_TILES-1,(int)(g_camZ/(float)TILE_SIZE)));
    if(ntx!=g_activeTX||ntz!=g_activeTZ)activateTile(ntx,ntz);

    updateTime(dt);updateWeather(dt);

    float simDt=dt*g_time.timeScale;
    if(!g_time.paused){
        updatePlants(simDt);
        for(auto& c:g_creatures){updateCreatureAI(c,simDt);updateCreaturePhysics(c,simDt);}
        g_creatures.erase(std::remove_if(g_creatures.begin(),g_creatures.end(),
            [](const Creature& c){return c.state==AIState::DEAD&&c.age>10.f;}),g_creatures.end());
    }
}
// ══════════════════════════════════════════════════════════════════════
// MAIN
// ══════════════════════════════════════════════════════════════════════

int main(){
    if(!glfwInit())return 1;
    glfwWindowHint(GLFW_SAMPLES,4);
    GLFWwindow* window=glfwCreateWindow(1280,720,"WLD-012: Tile Ecosystem (12 Creatures, DRO LOD)",nullptr,nullptr);
    if(!window){glfwTerminate();return 1;}

    glfwMakeContextCurrent(window);glfwSwapInterval(1);
    glfwSetKeyCallback(window,keyCallback);
    glfwSetMouseButtonCallback(window,mouseButtonCallback);
    glfwSetCursorPosCallback(window,cursorCallback);
    glfwSetScrollCallback(window,scrollCallback);

    loadGLExtensions();
    g_dro.initializeVRProfile();
    resetWorld();
    initShaders();

    glEnable(GL_DEPTH_TEST);glEnable(0x809D);

    double lastTime=glfwGetTime();
    double fpsAccum=0; int fpsFrames=0;
    while(!glfwWindowShouldClose(window)){
        double now=glfwGetTime();
        float dt=std::min(0.1f,(float)(now-lastTime));lastTime=now;
        g_wallTime=(float)now;
        fpsAccum+=dt; fpsFrames++;
        if(fpsAccum>=0.5){g_fps=(float)fpsFrames/fpsAccum;fpsFrames=0;fpsAccum=0;}

        glfwPollEvents();update(dt);

        int w,h;glfwGetFramebufferSize(window,&w,&h);
        glViewport(0,0,w,h);

        float sky=g_ambientLight;
        float fogR,fogG,fogB; getSkyColors(fogR,fogG,fogB);
        glClearColor(fogR,fogG,fogB,1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);glLoadIdentity();
        perspectiveGL(60.0,(double)w/h,0.5,800.0);
        glMatrixMode(GL_MODELVIEW);glLoadIdentity();

        float lx=g_camX-sinf(g_camYaw)*cosf(g_camPitch);
        float ly=g_camY+sinf(g_camPitch);
        float lz=g_camZ-cosf(g_camYaw)*cosf(g_camPitch);
        lookAtGL(g_camX,g_camY,g_camZ,lx,ly,lz);

        glEnable(GL_FOG);glFogi(GL_FOG_MODE,GL_EXP);
        float fc[4]={fogR,fogG,fogB,1};
        glFogfv(GL_FOG_COLOR,fc);
        glFogf(GL_FOG_DENSITY,0.003f+g_weather.rainIntensity*0.008f);

        renderSky();
        renderTerrain();
        renderWater();
        renderTileGrid();
        renderPlants();
        renderCreatures();
        renderRain();
        renderUI(w,h);

        glfwSwapBuffers(window);
    }
    glfwDestroyWindow(window);glfwTerminate();return 0;
}