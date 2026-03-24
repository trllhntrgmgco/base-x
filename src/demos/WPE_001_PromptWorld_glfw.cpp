/**
 * WPE-001: Prompt World Explorer
 *
 * Combines WorldPromptEngine (SBX-001) with Ecosystem-style 3D rendering
 * (WLD-012) to create a prompt-driven walkable world with full atmosphere.
 *
 * Three states:
 *   PROMPT   — Full inventory tag picker, text input, preset buttons,
 *              weather/time/speed defaults, generation button.
 *   GENERATE — Progress bars per phase (heightmap→biome→structures→weather).
 *   EXPLORE  — 3D world with free camera, dynamic sky/sun/moon/stars/clouds,
 *              fog, rain/snow/mist/aurora particles, trees, structures.
 *
 * Controls (Prompt Screen):
 *   Click tags to toggle them into/out of the prompt.
 *   Type to edit prompt.  Backspace/Delete to erase.
 *   1-8: Load preset.    Enter: Generate world.
 *   Up/Down: Change start hour.  +/-: Change day speed.
 *
 * Controls (Explore):
 *   WASD: Move camera          Mouse+RMB: Look around
 *   Scroll: Adjust speed       Shift: Descend
 *   T: Cycle time speed        G: Cycle weather override
 *   N: Toggle day/night cycle  H: Toggle HUD
 *   P: Pause time              ESC: Back to prompt screen
 */

#include <GLFW/glfw3.h>
#include <BaseX/DRO.h>
#include <BaseX/GLText.h>
#include <BaseX/WorldPromptEngine.h>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <vector>
#include <random>
#include <algorithm>
#include <string>

static constexpr float PI = 3.14159265f;

// ══════════════════════════════════════════════════════════════════════
// APP STATE MACHINE
// ══════════════════════════════════════════════════════════════════════

enum class AppState { PROMPT, GENERATE, EXPLORE };
static AppState g_appState = AppState::PROMPT;

// ══════════════════════════════════════════════════════════════════════
// WORLD PROMPT ENGINE
// ══════════════════════════════════════════════════════════════════════

static BaseX::WorldPromptEngine g_engine;
static BaseX::DROManager g_dro;
static std::string g_promptText;

// ══════════════════════════════════════════════════════════════════════
// PROMPT SCREEN DEFAULTS
// ══════════════════════════════════════════════════════════════════════

static int   g_startHour   = 10;     // 0-23
static float g_daySpeed    = 1.0f;   // multiplier
static int   g_weatherIdx  = 0;      // index into weather keywords for default
static int   g_presetIdx   = -1;

// ══════════════════════════════════════════════════════════════════════
// WORLD GRID — 3D MESH FROM HEIGHTMAP
// ══════════════════════════════════════════════════════════════════════

static constexpr int GS = BaseX::WPE_GRID_SIZE;        // 64
static constexpr float CELL_SIZE  = 4.0f;              // world units per cell
static constexpr float WORLD_SPAN = GS * CELL_SIZE;    // 256
static constexpr float HEIGHT_SCALE = 40.0f;           // max height in world units
static constexpr float WATER_LEVEL = 0.18f * HEIGHT_SCALE;

static std::vector<float> g_heights;   // GS*GS world-space heights
static std::vector<float> g_colors;    // GS*GS*3 RGB

// Trees placed from biome analysis
struct TreeInst { float x,y,z, size, phase; float cr,cg,cb; };
static std::vector<TreeInst> g_trees;

// ══════════════════════════════════════════════════════════════════════
// TIME & WEATHER (Ecosystem-style day/night)
// ══════════════════════════════════════════════════════════════════════

struct TimeState {
    float dayTime = 0.25f;   // 0-1 (0.25 = 6am)
    float dayLength = 120.f; // seconds per full day
    float timeScale = 1.0f;
    int day = 1;
    bool paused = false;
    bool isDaytime() const { return dayTime > 0.2f && dayTime < 0.8f; }
    float getSunAngle() const { return (dayTime - 0.25f) * 2 * PI; }
};

enum class WeatherMode { CLEAR, CLOUDY, RAIN, STORM, SNOW, MIST };
static const char* WEATHER_MODE_NAMES[] = {"Clear","Cloudy","Rain","Storm","Snow","Mist"};
static constexpr int NUM_WEATHER_MODES = 6;

struct WeatherSim {
    WeatherMode mode = WeatherMode::CLEAR;
    float cloudCover = 0.0f, rainIntensity = 0.0f, snowIntensity = 0.0f;
    float mistDensity = 0.0f, windStrength = 0.0f, windAngle = 0.0f;
    float lightLevel = 1.0f;
};

static TimeState g_time;
static WeatherSim g_weatherSim;
static bool g_dayNightCycle = true;

static float g_lightDir[3] = {0.5f, 0.8f, 0.3f};
static float g_ambientLight = 0.3f;

// ══════════════════════════════════════════════════════════════════════
// CAMERA
// ══════════════════════════════════════════════════════════════════════

static float g_camX, g_camY, g_camZ;
static float g_camYaw = 0.f, g_camPitch = -0.4f, g_camSpeed = 25.f;
static bool g_mouseCaptured = false;
static double g_lastMX = 0, g_lastMY = 0;

// ══════════════════════════════════════════════════════════════════════
// MISC GLOBALS
// ══════════════════════════════════════════════════════════════════════

static float g_wallTime = 0.f;
static float g_fps = 0.f;
static bool g_showHUD = true;
static bool g_keys[512] = {};
static std::mt19937 g_rng(42);
static std::uniform_real_distribution<float> g_rand01(0.f, 1.f);

// ── Prompt Screen UI state ──
static int   g_structTabIdx   = 0;     // 0=All, 1..N = subcategory tab
static float g_tagScrollY     = 0.f;   // vertical scroll offset for tag area

// Clickable tag hit-boxes stored during render
struct TagHitBox { float x0, y0, x1, y1; std::string keyword; };
static std::vector<TagHitBox> g_tagHits;

// Structure subcategory definitions for the tabbed UI
struct StructSubcategory {
    const char* label;
    std::vector<std::string> keywords;
};
static std::vector<StructSubcategory> g_structTabs;

static void buildStructureTabs() {
    // Build categorised sublists from the flat keyword list.
    // Each keyword appears in exactly one tab; order matches the engine list.
    struct Cat { const char* label; std::vector<const char*> prefixes; };
    Cat cats[] = {
        {"Landmark", {"spire","fortress","cathedral","castle","tower","pyramid",
            "temple","palace","citadel","lighthouse","colosseum","amphitheater",
            "keep","pagoda","ziggurat","minaret","aqueduct","monastery","arena",
            "mansion","villa","warehouse","granary","barn","stable","bell_tower"}},
        {"Buildings", {"hut","tavern","cabin","chapel","forge","marketplace",
            "windmill","cottage","farmhouse","shack","shed","workshop","smithy",
            "market_stall","tent","lean_to","outhouse","guardhouse","barracks",
            "inn","bakery","apothecary"}},
        {"Walls/Paths", {"bridge","dock","gate","wall","arch","archway","torii",
            "pier","jetty","wharf","boathouse","platform","stairway","rampart",
            "gatehouse","drawbridge","colonnade","terrace","fence","palisade",
            "parapet","battlement","portcullis"}},
        {"Domed/Low", {"altar","shrine","dolmen","barrow","dome","cupola",
            "well","fountain","bird_bath","sundial","gazebo","pergola"}},
        {"Monuments", {"throne","statue","obelisk","monolith","pillar","column",
            "runestone","ward_stone","memorial","cenotaph","cross","gravestone",
            "headstone","sarcophagus","mausoleum","tomb"}},
        {"Fantasy", {"portal","crystal_orb","enchanted_well","fairy_ring",
            "summoning_circle","rift","crystal_spire","soul_lantern"}},
        {"Props", {"torch","lantern","brazier","campfire","bonfire","lamp_post",
            "banner","flag","signpost","cart","wagon","barrel","crate","chest",
            "anchor","cannon","catapult","ballista"}},
        {"Ruins", {"ruins","rubble","crumbled_wall","broken_pillar","ruin_arch",
            "collapsed_tower","sunken_ship","shipwreck"}},
        {"Trees", {"tree","pine","spruce","cedar","fir","redwood","cypress",
            "oak","willow","birch","maple","palm","acacia","baobab","mangrove",
            "elm","banyan","ash_tree","magnolia"}},
        {"Vegetation", {"shrub","bush","hedge","fern","grass_tuft","flower",
            "vine","cattail","lily_pad","moss_patch","root","mushroom_cluster",
            "flower_bed","garden","topiary"}},
        {"Geological", {"rock","boulder","stump","log","outcrop","cliff_face",
            "cave_mouth","stalagmite","stalactite","crystal_cluster","geode",
            "hot_spring","geyser","lava_pool","volcanic_vent","coral_formation",
            "sea_stack","rock_arch","tidepool"}},
    };
    g_structTabs.clear();
    // Tab 0 = "All" (full list from engine)
    for (auto& c : cats) {
        StructSubcategory sc;
        sc.label = c.label;
        for (auto* k : c.prefixes) sc.keywords.push_back(k);
        g_structTabs.push_back(std::move(sc));
    }
}

// ══════════════════════════════════════════════════════════════════════
// UTILITY
// ══════════════════════════════════════════════════════════════════════

static void normalize3(float* v) {
    float len = sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    if (len > 0.0001f) { v[0]/=len; v[1]/=len; v[2]/=len; }
}
static float dot3(const float*a,const float*b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}

static float hash2D(float x, float z) {
    int ix=(int)floorf(x)*1619, iz=(int)floorf(z)*31337, n=ix+iz;
    n=(n<<13)^n;
    return 1.0f-((n*(n*n*15731+789221)+1376312589)&0x7fffffff)/1073741824.0f;
}

static float getWorldHeight(float wx, float wz) {
    float gx = wx / CELL_SIZE, gz = wz / CELL_SIZE;
    int ix = (int)gx, iz = (int)gz;
    if (ix < 0 || ix >= GS-1 || iz < 0 || iz >= GS-1) return 0;
    float fx = gx - ix, fz = gz - iz;
    float h00 = g_heights[iz*GS+ix], h10 = g_heights[iz*GS+ix+1];
    float h01 = g_heights[(iz+1)*GS+ix], h11 = g_heights[(iz+1)*GS+ix+1];
    return h00*(1-fx)*(1-fz) + h10*fx*(1-fz) + h01*(1-fx)*fz + h11*fx*fz;
}

// ══════════════════════════════════════════════════════════════════════
// BUILD 3D WORLD FROM ENGINE OUTPUT
// ══════════════════════════════════════════════════════════════════════

static void buildWorldMesh() {
    const auto& hmap = g_engine.heightmap();
    const auto& bmap = g_engine.biomeMap();
    if (hmap.empty()) return;

    g_heights.resize(GS * GS);
    g_colors.resize(GS * GS * 3);
    for (int i = 0; i < GS * GS; i++) {
        g_heights[i] = hmap[i] * HEIGHT_SCALE;
        g_colors[i*3+0] = bmap[i].r;
        g_colors[i*3+1] = bmap[i].g;
        g_colors[i*3+2] = bmap[i].b;
    }

    // Place trees where terrain is moderate height and greenish biome
    g_trees.clear();
    std::mt19937 treeRng(12345);
    std::uniform_real_distribution<float> tr(0.f, 1.f);
    for (int z = 1; z < GS-1; z++) {
        for (int x = 1; x < GS-1; x++) {
            float h = hmap[z*GS+x];
            float gr = bmap[z*GS+x].g;
            if (h > 0.2f && h < 0.7f && gr > 0.25f && tr(treeRng) < 0.12f) {
                TreeInst t;
                t.x = x * CELL_SIZE + tr(treeRng) * CELL_SIZE;
                t.z = z * CELL_SIZE + tr(treeRng) * CELL_SIZE;
                t.y = getWorldHeight(t.x, t.z);
                t.size = 0.6f + tr(treeRng) * 0.8f;
                t.phase = tr(treeRng) * 2 * PI;
                t.cr = bmap[z*GS+x].r * 0.6f;
                t.cg = bmap[z*GS+x].g * 1.2f;
                t.cb = bmap[z*GS+x].b * 0.5f;
                g_trees.push_back(t);
            }
        }
    }

    // Camera start: centre of world, above terrain
    g_camX = WORLD_SPAN / 2.f;
    g_camZ = WORLD_SPAN / 2.f;
    g_camY = getWorldHeight(g_camX, g_camZ) + 12.f;
}

// ══════════════════════════════════════════════════════════════════════
// SKY COLOR (Ecosystem Phase D style)
// ══════════════════════════════════════════════════════════════════════

static void getSkyColors(float& fogR, float& fogG, float& fogB) {
    float sa = g_time.getSunAngle();
    float sunH = sinf(sa);
    float cd = 1.f - g_weatherSim.cloudCover * 0.3f;
    float twilight = std::max(0.f, 1.f - fabsf(sunH) * 5.f);
    twilight *= twilight;
    float dayF = std::max(0.f, sunH);
    fogR = (0.45f + dayF * 0.25f + twilight * 0.35f) * cd;
    fogG = (0.50f + dayF * 0.25f + twilight * 0.12f) * cd;
    fogB = (0.62f + dayF * 0.25f - twilight * 0.10f) * cd;
    float nightF = std::max(0.f, -sunH * 0.6f);
    fogR = std::max(0.02f, fogR - nightF * 0.3f);
    fogG = std::max(0.02f, fogG - nightF * 0.35f);
    fogB = std::max(0.04f, fogB - nightF * 0.25f);
}

// ══════════════════════════════════════════════════════════════════════
// TIME & WEATHER UPDATE
// ══════════════════════════════════════════════════════════════════════

static void updateTime(float dt) {
    if (g_time.paused) return;
    if (g_dayNightCycle) {
        g_time.dayTime += (dt * g_time.timeScale) / g_time.dayLength;
        if (g_time.dayTime >= 1.f) { g_time.dayTime -= 1.f; g_time.day++; }
    }
    float sa = g_time.getSunAngle();
    g_lightDir[0] = cosf(sa) * 0.5f; g_lightDir[1] = sinf(sa); g_lightDir[2] = 0.3f;
    normalize3(g_lightDir);
    float dl = std::max(0.f, sinf(sa)) * 0.7f + 0.3f;
    g_ambientLight = dl * (1.f - g_weatherSim.cloudCover * 0.5f);
    g_weatherSim.lightLevel = g_ambientLight;
}

static void updateWeather(float dt) {
    float tc=0,tr=0,sn=0,ms=0,tw=0;
    switch (g_weatherSim.mode) {
        case WeatherMode::CLEAR:  tc=0.1f;tw=0.1f;break;
        case WeatherMode::CLOUDY: tc=0.6f;tw=0.2f;break;
        case WeatherMode::RAIN:   tc=0.8f;tr=0.6f;tw=0.3f;break;
        case WeatherMode::STORM:  tc=1.f;tr=1.f;tw=0.8f;break;
        case WeatherMode::SNOW:   tc=0.7f;sn=0.8f;tw=0.15f;break;
        case WeatherMode::MIST:   tc=0.4f;ms=0.7f;tw=0.05f;break;
    }
    g_weatherSim.cloudCover    += (tc - g_weatherSim.cloudCover)    * dt * 0.5f;
    g_weatherSim.rainIntensity += (tr - g_weatherSim.rainIntensity) * dt * 0.5f;
    g_weatherSim.snowIntensity += (sn - g_weatherSim.snowIntensity) * dt * 0.5f;
    g_weatherSim.mistDensity   += (ms - g_weatherSim.mistDensity)   * dt * 0.5f;
    g_weatherSim.windStrength  += (tw - g_weatherSim.windStrength)  * dt * 0.5f;
    g_weatherSim.windAngle     += dt * 0.1f;
}

// ══════════════════════════════════════════════════════════════════════
// APPLY ENGINE WEATHER → SIM WEATHER
// ══════════════════════════════════════════════════════════════════════

static void applyEngineWeather() {
    const auto& ws = g_engine.weather();
    if (ws.hasStorm)       g_weatherSim.mode = WeatherMode::STORM;
    else if (ws.hasRain)   g_weatherSim.mode = WeatherMode::RAIN;
    else if (ws.hasSnow)   g_weatherSim.mode = WeatherMode::SNOW;
    else if (ws.hasMist)   g_weatherSim.mode = WeatherMode::MIST;
    else                   g_weatherSim.mode = WeatherMode::CLEAR;
}

// ══════════════════════════════════════════════════════════════════════
// GL RENDERING HELPERS
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

// ══════════════════════════════════════════════════════════════════════
// SKY RENDERING (Multi-band gradient, sun, moon, stars, clouds)
// ══════════════════════════════════════════════════════════════════════

static void renderSky() {
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    float sa = g_time.getSunAngle();
    float sunH = sinf(sa), dayF = std::max(0.f, sunH);
    float cd = 1.f - g_weatherSim.cloudCover * 0.3f;
    float twilight = std::max(0.f, 1.f - fabsf(sunH) * 5.f); twilight *= twilight;

    // 4-band gradient
    float zR=(0.05f+dayF*0.15f)*cd, zG=(0.08f+dayF*0.25f)*cd, zB=(0.20f+dayF*0.55f)*cd;
    float uR=(0.10f+dayF*0.25f+twilight*0.10f)*cd, uG=(0.12f+dayF*0.35f+twilight*0.05f)*cd, uB=(0.28f+dayF*0.50f-twilight*0.05f)*cd;
    float lR=(0.30f+dayF*0.30f+twilight*0.30f)*cd, lG=(0.32f+dayF*0.30f+twilight*0.15f)*cd, lB=(0.42f+dayF*0.35f-twilight*0.08f)*cd;
    float hR=(0.45f+dayF*0.25f+twilight*0.40f)*cd, hG=(0.48f+dayF*0.25f+twilight*0.15f)*cd, hB=(0.55f+dayF*0.30f-twilight*0.15f)*cd;

    float nd = std::max(0.f, -sunH * 0.5f);
    zR=std::max(0.01f,zR-nd*0.2f);zG=std::max(0.01f,zG-nd*0.25f);zB=std::max(0.03f,zB-nd*0.15f);
    uR=std::max(0.01f,uR-nd*0.2f);uG=std::max(0.01f,uG-nd*0.25f);uB=std::max(0.03f,uB-nd*0.15f);
    lR=std::max(0.02f,lR-nd*0.25f);lG=std::max(0.02f,lG-nd*0.3f);lB=std::max(0.04f,lB-nd*0.2f);
    hR=std::max(0.02f,hR-nd*0.3f);hG=std::max(0.02f,hG-nd*0.35f);hB=std::max(0.04f,hB-nd*0.25f);

    glBegin(GL_QUADS);
    glColor3f(hR,hG,hB);glVertex2f(-1,-1);glVertex2f(1,-1);
    glColor3f(lR,lG,lB);glVertex2f(1,-0.2f);glVertex2f(-1,-0.2f);
    glColor3f(lR,lG,lB);glVertex2f(-1,-0.2f);glVertex2f(1,-0.2f);
    glColor3f(uR,uG,uB);glVertex2f(1,0.4f);glVertex2f(-1,0.4f);
    glColor3f(uR,uG,uB);glVertex2f(-1,0.4f);glVertex2f(1,0.4f);
    glColor3f(zR,zG,zB);glVertex2f(1,1);glVertex2f(-1,1);
    glEnd();

    // Stars
    float nightAlpha = std::max(0.f, std::min(1.f, -sunH * 3.f)) * (1.f - g_weatherSim.cloudCover * 0.8f);
    if (nightAlpha > 0.02f) {
        glBegin(GL_POINTS);
        for (int i = 0; i < 80; i++) {
            float sx = hash2D((float)i*7.3f,1.1f), sy = hash2D(2.2f,(float)i*5.7f)*0.5f+0.2f;
            float bright = 0.5f+0.5f*hash2D((float)i*3.1f,(float)i*1.9f);
            float twinkle = 0.7f+0.3f*sinf(g_wallTime*(1.5f+(float)i*0.3f)+(float)i);
            glColor4f(0.9f+bright*0.1f,0.9f+bright*0.08f,1.f,nightAlpha*bright*twinkle);
            glVertex2f(sx,sy);
        }
        glEnd();
    }

    // Sun
    float ssx=cosf(sa)*0.6f, ssy=sinf(sa)*0.7f;
    if (ssy > -0.3f) {
        float lowAngle=std::max(0.f,1.f-sunH*4.f);
        float sr=std::min(1.f,1.0f),sg=std::min(1.f,0.95f-lowAngle*0.25f),sb=std::min(1.f,0.80f-lowAngle*0.45f);
        float sunAlpha=std::min(1.f,(ssy+0.3f)*2.5f);
        // Corona
        glBegin(GL_TRIANGLE_FAN);
        glColor4f(sr,sg,sb,0.25f*sunAlpha*cd);glVertex2f(ssx,ssy);
        glColor4f(sr,sg*0.7f,sb*0.3f,0.f);
        for(int i=0;i<=20;i++){float a=i/20.f*2*PI;glVertex2f(ssx+cosf(a)*0.12f,ssy+sinf(a)*0.12f);}
        glEnd();
        // Disc
        glBegin(GL_TRIANGLE_FAN);
        glColor4f(sr,sg,sb,sunAlpha);glVertex2f(ssx,ssy);
        for(int i=0;i<=16;i++){float a=i/16.f*2*PI;glVertex2f(ssx+cosf(a)*0.04f,ssy+sinf(a)*0.04f);}
        glEnd();
    }

    // Moon
    if (nightAlpha > 0.05f) {
        float mx=-cosf(sa)*0.5f,my=-sinf(sa)*0.6f+0.15f;
        if (my > -0.2f) {
            float moonA=std::min(1.f,nightAlpha*1.5f)*cd;
            glBegin(GL_TRIANGLE_FAN);
            glColor4f(0.85f,0.87f,0.92f,moonA);glVertex2f(mx,my);
            for(int i=0;i<=16;i++){float a=i/16.f*2*PI;glVertex2f(mx+cosf(a)*0.04f,my+sinf(a)*0.04f);}
            glEnd();
        }
    }

    // Clouds
    if (g_weatherSim.cloudCover > 0.05f) {
        float ca = g_weatherSim.cloudCover * 0.45f;
        float drift = g_wallTime*0.008f + g_weatherSim.windStrength*g_wallTime*0.01f;
        for (int i = 0; i < 12; i++) {
            float cx = fmodf(hash2D((float)i*13.7f,3.1f)*2.f-1.f+drift+3.f,2.f)-1.f;
            float cy = 0.1f+hash2D(5.5f,(float)i*9.3f)*0.55f;
            float cw = 0.15f+hash2D((float)i*2.1f,7.7f)*0.20f;
            float ch = 0.02f+hash2D((float)i*4.4f,1.3f)*0.03f;
            float cloudBright = 0.6f+dayF*0.35f-g_weatherSim.rainIntensity*0.3f;
            float sd = g_weatherSim.rainIntensity*0.25f;
            glBegin(GL_TRIANGLE_FAN);
            glColor4f(cloudBright-sd,cloudBright-sd*0.5f,cloudBright,ca);glVertex2f(cx,cy);
            glColor4f(cloudBright-sd,cloudBright-sd*0.5f,cloudBright,0.f);
            for(int j=0;j<=12;j++){float a=j/12.f*2*PI;
                glVertex2f(cx+cosf(a)*cw*(0.8f+0.2f*sinf(a*3.f+(float)i)),
                           cy+sinf(a)*ch*(0.7f+0.3f*cosf(a*2.f+(float)i*0.5f)));}
            glEnd();
        }
    }

    glDisable(GL_BLEND);
    glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
}

// ══════════════════════════════════════════════════════════════════════
// TERRAIN RENDERING (vertex arrays)
// ══════════════════════════════════════════════════════════════════════

static void renderTerrain() {
    if (g_heights.empty()) return;

    glEnable(GL_LIGHTING); glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL); glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    float lp[4]={g_lightDir[0],g_lightDir[1],g_lightDir[2],0.f};
    glLightfv(GL_LIGHT0, GL_POSITION, lp);
    float am[4]={g_ambientLight,g_ambientLight,g_ambientLight,1.f};
    glLightfv(GL_LIGHT0, GL_AMBIENT, am);
    float df[4]={1.f-g_ambientLight*0.3f,1.f-g_ambientLight*0.3f,1.f-g_ambientLight*0.3f,1.f};
    glLightfv(GL_LIGHT0, GL_DIFFUSE, df);

    for (int z = 0; z < GS-1; z++) {
        glBegin(GL_TRIANGLE_STRIP);
        for (int x = 0; x <= GS-1; x++) {
            for (int dz = 1; dz >= 0; dz--) {
                int iz = z + dz, idx = iz*GS+x;
                float wx = x*CELL_SIZE, wz = iz*CELL_SIZE, wy = g_heights[idx];

                // Normal from neighbours
                float nn[3];
                int xl=std::max(0,x-1),xr=std::min(GS-1,x+1),zd=std::max(0,iz-1),zu=std::min(GS-1,iz+1);
                nn[0]=g_heights[iz*GS+xl]-g_heights[iz*GS+xr];
                nn[1]=2.f*CELL_SIZE;
                nn[2]=g_heights[zd*GS+x]-g_heights[zu*GS+x];
                normalize3(nn);

                glNormal3f(nn[0],nn[1],nn[2]);
                glColor3f(g_colors[idx*3],g_colors[idx*3+1],g_colors[idx*3+2]);
                glVertex3f(wx, wy, wz);
            }
        }
        glEnd();
    }

    glDisable(GL_COLOR_MATERIAL); glDisable(GL_LIGHTING);
}

// ══════════════════════════════════════════════════════════════════════
// WATER PLANE — tessellated with smooth animated waves
// ══════════════════════════════════════════════════════════════════════

static float waterWaveHeight(float wx, float wz, float t) {
    // Layer multiple sine waves at different frequencies and directions
    // for natural-looking smooth water motion
    float w = 0.0f;
    w += sinf(wx * 0.08f + t * 0.7f)                  * 0.35f;  // broad swell
    w += sinf(wz * 0.10f + t * 0.5f + 1.2f)           * 0.25f;  // cross swell
    w += sinf((wx + wz) * 0.15f + t * 1.1f)           * 0.15f;  // diagonal ripple
    w += sinf(wx * 0.25f - wz * 0.18f + t * 1.6f)     * 0.10f;  // fine detail
    w += sinf(wx * 0.40f + wz * 0.30f + t * 2.2f)     * 0.05f;  // micro ripple
    return w;
}

static void renderWater() {
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Tessellate the water plane into a grid for smooth wave animation
    constexpr int WATER_TESS = 48;  // subdivisions per axis
    float step = WORLD_SPAN / WATER_TESS;
    float t = g_wallTime;

    for (int gy = 0; gy < WATER_TESS; gy++) {
        glBegin(GL_TRIANGLE_STRIP);
        for (int gx = 0; gx <= WATER_TESS; gx++) {
            for (int dy = 0; dy <= 1; dy++) {
                float wx = gx * step;
                float wz = (gy + dy) * step;

                float wave = waterWaveHeight(wx, wz, t);
                float wy = WATER_LEVEL + wave;

                // Specular-like highlight from wave slope
                float slope = (waterWaveHeight(wx + 0.5f, wz, t) - wave) * 2.0f;
                float spec = std::max(0.0f, slope);
                spec = spec * spec * spec;  // sharpen highlight

                // Base water colour with depth variation and specular
                float depth = std::max(0.0f, 1.0f - wave * 0.3f);
                float cr = 0.10f * depth + spec * 0.25f;
                float cg = 0.25f * depth + spec * 0.30f;
                float cb = 0.45f * depth + spec * 0.35f;
                float ca = 0.50f - spec * 0.1f;  // slightly more transparent at highlights

                glColor4f(cr, cg, cb, ca);
                glVertex3f(wx, wy, wz);
            }
        }
        glEnd();
    }

    glDisable(GL_BLEND);
}

// ══════════════════════════════════════════════════════════════════════
// TREE RENDERING (trunk + canopy with wind sway)
// ══════════════════════════════════════════════════════════════════════

static void renderTrees() {
    glEnable(GL_LIGHTING); glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    float n[3] = {0,1,0};

    for (const auto& t : g_trees) {
        float wStr = g_weatherSim.windStrength * 0.15f;
        float swX = sinf(g_wallTime*1.8f+t.phase)*wStr*cosf(g_weatherSim.windAngle);
        float swZ = sinf(g_wallTime*1.8f+t.phase)*wStr*sinf(g_weatherSim.windAngle);
        float s = t.size;

        glPushMatrix();
        glTranslatef(t.x, t.y, t.z);

        // Shadow
        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        float shOff = s*2.5f;
        float shCX = -g_lightDir[0]*shOff, shCZ = -g_lightDir[2]*shOff;
        glBegin(GL_TRIANGLE_FAN);
        glColor4f(0,0.05f,0,0.22f*g_ambientLight); glVertex3f(shCX,0.02f,shCZ);
        glColor4f(0,0.05f,0,0.f);
        for(int i=0;i<=8;i++){float a=i/8.f*2*PI;glVertex3f(shCX+cosf(a)*1.4f*s,0.02f,shCZ+sinf(a)*1.0f*s);}
        glEnd();
        glDisable(GL_BLEND); glEnable(GL_LIGHTING);

        // Trunk
        float trunkLit = g_ambientLight;
        glColor3f(0.35f*trunkLit, 0.25f*trunkLit, 0.15f*trunkLit);
        glBegin(GL_QUAD_STRIP);
        for(int i=0;i<=6;i++){float a=i/6.f*2*PI;
            glVertex3f(cosf(a)*0.15f*s,0,sinf(a)*0.15f*s);
            glVertex3f(swX*0.3f+cosf(a)*0.1f*s,2.f*s,swZ*0.3f+sinf(a)*0.1f*s);}
        glEnd();

        // Canopy
        float i = g_ambientLight + std::max(0.f,dot3(n,g_lightDir))*(1.f-g_ambientLight);
        glColor3f(t.cr*i, t.cg*i, t.cb*i);
        glBegin(GL_TRIANGLE_FAN);
        glVertex3f(swX,4.f*s,swZ);
        for(int j=0;j<=8;j++){float a=j/8.f*2*PI;
            float r=1.2f*s*(0.85f+0.15f*sinf(a*3.f+g_wallTime*0.5f));
            glVertex3f(swX*0.7f+cosf(a)*r,1.5f*s,swZ*0.7f+sinf(a)*r);}
        glEnd();

        glPopMatrix();
    }
    glDisable(GL_COLOR_MATERIAL); glDisable(GL_LIGHTING);
}

// ══════════════════════════════════════════════════════════════════════
// STRUCTURE RENDERING (from engine placement)
// ══════════════════════════════════════════════════════════════════════

static void renderStructures() {
    const auto& structs = g_engine.structures();
    if (structs.empty()) return;

    glEnable(GL_LIGHTING); glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);

    for (const auto& st : structs) {
        float wx = st.gx * CELL_SIZE, wz = st.gy * CELL_SIZE;
        float wy = getWorldHeight(wx, wz);
        float sz = st.size * 0.15f;

        glPushMatrix();
        glTranslatef(wx, wy, wz);
        glColor3f(st.r * g_ambientLight, st.g * g_ambientLight, st.b * g_ambientLight);

        // Base block
        float hw = sz*0.5f, hh = sz;
        glBegin(GL_QUADS);
        // Front
        glNormal3f(0,0,1);
        glVertex3f(-hw,0,hw);glVertex3f(hw,0,hw);glVertex3f(hw,hh,hw);glVertex3f(-hw,hh,hw);
        // Back
        glNormal3f(0,0,-1);
        glVertex3f(hw,0,-hw);glVertex3f(-hw,0,-hw);glVertex3f(-hw,hh,-hw);glVertex3f(hw,hh,-hw);
        // Left
        glNormal3f(-1,0,0);
        glVertex3f(-hw,0,-hw);glVertex3f(-hw,0,hw);glVertex3f(-hw,hh,hw);glVertex3f(-hw,hh,-hw);
        // Right
        glNormal3f(1,0,0);
        glVertex3f(hw,0,hw);glVertex3f(hw,0,-hw);glVertex3f(hw,hh,-hw);glVertex3f(hw,hh,hw);
        // Top
        glNormal3f(0,1,0);
        glVertex3f(-hw,hh,-hw);glVertex3f(-hw,hh,hw);glVertex3f(hw,hh,hw);glVertex3f(hw,hh,-hw);
        glEnd();

        // Roof pyramid for towers/spires/castles
        if (st.type == "tower" || st.type == "spire" || st.type == "castle" ||
            st.type == "cathedral" || st.type == "fortress" || st.type == "lighthouse") {
            glColor3f(st.r*0.7f*g_ambientLight, st.g*0.6f*g_ambientLight, st.b*0.5f*g_ambientLight);
            glBegin(GL_TRIANGLES);
            glNormal3f(0,0.7f,0.7f);
            glVertex3f(-hw,hh,hw);glVertex3f(hw,hh,hw);glVertex3f(0,hh+sz*0.7f,0);
            glNormal3f(0,0.7f,-0.7f);
            glVertex3f(hw,hh,-hw);glVertex3f(-hw,hh,-hw);glVertex3f(0,hh+sz*0.7f,0);
            glNormal3f(-0.7f,0.7f,0);
            glVertex3f(-hw,hh,-hw);glVertex3f(-hw,hh,hw);glVertex3f(0,hh+sz*0.7f,0);
            glNormal3f(0.7f,0.7f,0);
            glVertex3f(hw,hh,hw);glVertex3f(hw,hh,-hw);glVertex3f(0,hh+sz*0.7f,0);
            glEnd();
        }

        glPopMatrix();
    }

    glDisable(GL_COLOR_MATERIAL); glDisable(GL_LIGHTING);
}

// ══════════════════════════════════════════════════════════════════════
// WEATHER PARTICLES (rain, snow, mist)
// ══════════════════════════════════════════════════════════════════════

static void renderWeatherParticles() {
    // Rain
    if (g_weatherSim.rainIntensity > 0.1f) {
        static float ro = 0; ro += 0.5f;
        glColor4f(0.6f,0.7f,0.8f,g_weatherSim.rainIntensity*0.5f);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glBegin(GL_LINES);
        int nd = (int)(g_weatherSim.rainIntensity * 200);
        for (int i = 0; i < nd; i++) {
            float x=g_camX+(hash2D(i*0.1f,ro)-0.5f)*50.f, z=g_camZ+(hash2D(ro,i*0.1f)-0.5f)*50.f;
            float y=fmodf(hash2D((float)i,(float)i)*30.f+ro,30.f)+g_camY-15.f;
            glVertex3f(x,y,z);
            glVertex3f(x+g_weatherSim.windStrength*cosf(g_weatherSim.windAngle),y-1.f,
                       z+g_weatherSim.windStrength*sinf(g_weatherSim.windAngle));
        }
        glEnd(); glDisable(GL_BLEND);
    }

    // Snow
    if (g_weatherSim.snowIntensity > 0.1f) {
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glPointSize(2.f);
        glBegin(GL_POINTS);
        int ns = (int)(g_weatherSim.snowIntensity * 150);
        for (int i = 0; i < ns; i++) {
            float x=g_camX+(hash2D(i*0.3f,g_wallTime*0.1f)-0.5f)*40.f;
            float z=g_camZ+(hash2D(g_wallTime*0.1f,i*0.3f)-0.5f)*40.f;
            float y=fmodf(hash2D((float)i*1.7f,(float)i*0.9f)*25.f+g_wallTime*2.f,25.f)+g_camY-12.f;
            glColor4f(0.9f,0.95f,1.f,g_weatherSim.snowIntensity*0.7f);
            glVertex3f(x,y,z);
        }
        glEnd();
        glPointSize(1.f); glDisable(GL_BLEND);
    }
}

// ══════════════════════════════════════════════════════════════════════
// EXPLORE HUD
// ══════════════════════════════════════════════════════════════════════

static void renderExploreHUD(int winW, int winH) {
    if (!g_showHUD) return;
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();
    glOrtho(0,winW,0,winH,-1,1);
    glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
    glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND);

    char buf[256]; float y;

    // Left panel
    glColor4f(0,0,0,0.55f);
    glBegin(GL_QUADS);
    glVertex2f(8,(float)winH-8);glVertex2f(310,(float)winH-8);
    glVertex2f(310,(float)winH-200);glVertex2f(8,(float)winH-200);
    glEnd();

    y = (float)winH - 22.f;
    glColor3f(1,1,1); gltext::draw("=== PROMPT WORLD ===", 14, y); y -= 16;

    int hour=(int)(g_time.dayTime*24), minute=(int)((g_time.dayTime*24-hour)*60);
    snprintf(buf,sizeof(buf),"Day %d  %02d:%02d  %.0fx %s",g_time.day,hour,minute,g_time.timeScale,g_time.paused?"PAUSED":"");
    glColor3f(0.7f,1.f,0.7f); gltext::draw(buf,14,y); y -= 14;

    snprintf(buf,sizeof(buf),"Weather: %s",WEATHER_MODE_NAMES[(int)g_weatherSim.mode]);
    gltext::draw(buf,14,y); y -= 14;

    snprintf(buf,sizeof(buf),"FPS: %.0f  Trees: %d  Structs: %d",g_fps,(int)g_trees.size(),(int)g_engine.structures().size());
    glColor3f(0.6f,0.8f,1.f); gltext::draw(buf,14,y); y -= 16;

    // Parsed keywords
    const auto& parsed = g_engine.parsed();
    std::string kw;
    for (auto& t : parsed.terrain) { kw += t + " "; }
    for (auto& b : parsed.biome) { kw += b + " "; }
    snprintf(buf,sizeof(buf),"World: %s",kw.c_str());
    glColor3f(0.8f,0.8f,0.6f); gltext::draw(buf,14,y); y -= 16;

    glColor3f(0.6f,0.7f,0.9f); gltext::draw("WASD:Move RMB:Look T:Time G:Weather",14,y); y -= 13;
    gltext::draw("N:DayNight H:HUD P:Pause ESC:Menu",14,y);

    glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST);
    glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);
}

// ══════════════════════════════════════════════════════════════════════
// PROMPT SCREEN RENDERING
// ══════════════════════════════════════════════════════════════════════

static void drawTagRow(const char* label, const std::vector<std::string>& tags,
                       float x, float& y, float winW, float r, float g, float b,
                       const std::string& prompt, bool storeHits = true) {
    glColor3f(r*0.7f, g*0.7f, b*0.7f);
    gltext::draw(label, x, y, 1.8f);
    y -= 16;

    float px = x + 4;
    std::string lower = prompt;
    std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return (char)tolower(c);});

    for (const auto& tag : tags) {
        float tw = gltext::textWidth(tag.c_str(), 1.3f) + 12;
        if (px + tw > winW - 40) { px = x + 4; y -= 16; }

        bool active = lower.find(tag) != std::string::npos;

        float bx0 = px, by0 = y - 2, bx1 = px + tw, by1 = y + 12;

        // Tag background
        if (active) glColor4f(r*0.4f, g*0.4f, b*0.4f, 0.8f);
        else        glColor4f(0.12f, 0.12f, 0.15f, 0.7f);
        glBegin(GL_QUADS);
        glVertex2f(bx0, by0); glVertex2f(bx1, by0);
        glVertex2f(bx1, by1); glVertex2f(bx0, by1);
        glEnd();

        // Tag border
        if (active) glColor3f(r, g, b);
        else        glColor3f(0.3f, 0.3f, 0.35f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(bx0,by0);glVertex2f(bx1,by0);glVertex2f(bx1,by1);glVertex2f(bx0,by1);
        glEnd();

        // Tag text
        if (active) glColor3f(1, 1, 1);
        else        glColor3f(0.6f, 0.6f, 0.65f);
        gltext::draw(tag.c_str(), px + 6, y, 1.3f);

        if (storeHits)
            g_tagHits.push_back({bx0, by0, bx1, by1, tag});

        px += tw + 4;
    }
    y -= 20;
}

static void renderPromptScreen(int winW, int winH) {
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glOrtho(0, winW, 0, winH, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    g_tagHits.clear();  // rebuild every frame

    // Background gradient
    glBegin(GL_QUADS);
    glColor3f(0.03f,0.04f,0.06f); glVertex2f(0,0); glVertex2f((float)winW,0);
    glColor3f(0.06f,0.08f,0.14f); glVertex2f((float)winW,(float)winH); glVertex2f(0,(float)winH);
    glEnd();

    float y = (float)winH - 25.f;
    float x = 20.f;

    // Title
    glColor3f(0.4f, 0.7f, 1.f);
    gltext::draw("BASE-X ENGINE | PROMPT WORLD EXPLORER", x, y, 2.2f);
    y -= 20;

    glColor3f(0.5f, 0.5f, 0.55f);
    gltext::draw("Click tags to add/remove. Type prompt. ENTER to generate.", x, y, 1.3f);
    y -= 20;

    // ── Inventory Tags ──
    const auto& parser = g_engine.parser();

    // Terrain (compact)
    std::vector<std::string> terrainTags;
    for (auto& kv : parser.terrainDict()) terrainTags.push_back(kv.first);
    std::sort(terrainTags.begin(), terrainTags.end());
    drawTagRow("TERRAIN", terrainTags, x, y, (float)winW, 0.4f, 0.9f, 0.4f, g_promptText);

    // Biomes (compact)
    std::vector<std::string> biomeTags;
    for (auto& kv : parser.biomeDict()) biomeTags.push_back(kv.first);
    std::sort(biomeTags.begin(), biomeTags.end());
    drawTagRow("BIOME", biomeTags, x, y, (float)winW, 0.3f, 0.6f, 1.0f, g_promptText);

    // ── Structure subcategory tabs ──
    if (g_structTabs.empty()) buildStructureTabs();

    glColor3f(0.7f*0.9f, 0.7f*0.7f, 0.7f*0.3f);
    gltext::draw("STRUCTURES", x, y, 1.8f);
    y -= 16;

    // Draw tab bar
    float tabX = x + 4;
    // "All" tab
    {
        bool sel = (g_structTabIdx == 0);
        float tw = gltext::textWidth("All", 1.2f) + 10;
        if (sel) glColor4f(0.35f,0.28f,0.1f,0.9f);
        else     glColor4f(0.1f,0.1f,0.12f,0.7f);
        glBegin(GL_QUADS);
        glVertex2f(tabX,y-2);glVertex2f(tabX+tw,y-2);
        glVertex2f(tabX+tw,y+12);glVertex2f(tabX,y+12);
        glEnd();
        if (sel) glColor3f(1.f,0.9f,0.4f);
        else     glColor3f(0.5f,0.5f,0.5f);
        gltext::draw("All", tabX+5, y, 1.2f);
        g_tagHits.push_back({tabX, y-2, tabX+tw, y+12, "__tab_0"});
        tabX += tw + 3;
    }
    for (int ti = 0; ti < (int)g_structTabs.size(); ti++) {
        const char* lbl = g_structTabs[ti].label;
        float tw = gltext::textWidth(lbl, 1.2f) + 10;
        if (tabX + tw > (float)winW - 40) { tabX = x + 4; y -= 16; }
        bool sel = (g_structTabIdx == ti + 1);
        if (sel) glColor4f(0.35f,0.28f,0.1f,0.9f);
        else     glColor4f(0.1f,0.1f,0.12f,0.7f);
        glBegin(GL_QUADS);
        glVertex2f(tabX,y-2);glVertex2f(tabX+tw,y-2);
        glVertex2f(tabX+tw,y+12);glVertex2f(tabX,y+12);
        glEnd();
        if (sel) glColor3f(1.f,0.9f,0.4f);
        else     glColor3f(0.5f,0.5f,0.5f);
        gltext::draw(lbl, tabX+5, y, 1.2f);
        char tabId[32]; snprintf(tabId, sizeof(tabId), "__tab_%d", ti + 1);
        g_tagHits.push_back({tabX, y-2, tabX+tw, y+12, tabId});
        tabX += tw + 3;
    }
    y -= 18;

    // Draw structure tags for selected tab
    if (g_structTabIdx == 0) {
        drawTagRow("", parser.structureKeywords(), x, y, (float)winW, 0.9f, 0.7f, 0.3f, g_promptText);
    } else {
        int idx = g_structTabIdx - 1;
        if (idx >= 0 && idx < (int)g_structTabs.size())
            drawTagRow("", g_structTabs[idx].keywords, x, y, (float)winW, 0.9f, 0.7f, 0.3f, g_promptText);
    }

    // Weather
    drawTagRow("WEATHER", parser.weatherKeywords(), x, y, (float)winW, 0.8f, 0.4f, 0.8f, g_promptText);

    y -= 4;

    // ── Prompt Text Box ──
    glColor4f(0.08f, 0.08f, 0.12f, 0.9f);
    float boxY = y - 26;
    glBegin(GL_QUADS);
    glVertex2f(x, boxY); glVertex2f((float)winW - 20, boxY);
    glVertex2f((float)winW - 20, y); glVertex2f(x, y);
    glEnd();
    glColor3f(0.3f, 0.5f, 0.8f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x,boxY);glVertex2f((float)winW-20,boxY);
    glVertex2f((float)winW-20,y);glVertex2f(x,y);
    glEnd();

    glColor3f(0.3f,0.3f,0.4f);
    gltext::draw(">", x+4, boxY+6, 1.5f);

    std::string dispText = g_promptText;
    if (fmodf(g_wallTime, 1.0f) < 0.5f) dispText += "_";
    if (dispText.empty()) {
        glColor3f(0.35f,0.35f,0.4f);
        gltext::draw("Type your world description here...", x+18, boxY+6, 1.4f);
    } else {
        glColor3f(0.9f, 0.95f, 1.f);
        gltext::draw(dispText.c_str(), x+18, boxY+6, 1.4f);
    }
    y = boxY - 14;

    // ── Presets Row (all 12) ──
    glColor3f(0.5f,0.5f,0.6f);
    gltext::draw("PRESETS:", x, y, 1.4f);
    y -= 14;
    const char* presetLabels[] = {
        "1:Volcanic Island","2:Frozen Peaks","3:Enchanted Forest","4:Desert Ruins",
        "5:Floating Islands","6:Necropolis","7:Steampunk City","8:Crystal Caverns",
        "9:Bamboo Jungle","0:Crimson Badlands","-:Tundra Archipelago","=:Savanna Caldera"
    };
    float px = x;
    for (int i = 0; i < 12; i++) {
        float tw = gltext::textWidth(presetLabels[i], 1.1f) + 10;
        if (px + tw > (float)winW - 40) { px = x; y -= 14; }
        bool sel = (g_presetIdx == i);
        if (sel) glColor4f(0.15f,0.2f,0.35f,0.9f);
        else     glColor4f(0.08f,0.08f,0.1f,0.7f);
        glBegin(GL_QUADS);
        glVertex2f(px,y-2);glVertex2f(px+tw,y-2);glVertex2f(px+tw,y+12);glVertex2f(px,y+12);
        glEnd();
        if (sel) glColor3f(0.5f,0.7f,1.f);
        else     glColor3f(0.4f,0.4f,0.45f);
        gltext::draw(presetLabels[i], px+5, y, 1.1f);
        char pid[16]; snprintf(pid, sizeof(pid), "__preset_%d", i);
        g_tagHits.push_back({px, y-2, px+tw, y+12, pid});
        px += tw + 5;
    }
    y -= 20;

    // ── Defaults Panel ──
    glColor4f(0.06f,0.06f,0.1f,0.85f);
    float panelTop = y, panelBot = y - 58;
    glBegin(GL_QUADS);
    glVertex2f(x,panelBot);glVertex2f((float)winW-20,panelBot);
    glVertex2f((float)winW-20,panelTop);glVertex2f(x,panelTop);
    glEnd();
    glColor3f(0.25f,0.35f,0.55f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x,panelBot);glVertex2f((float)winW-20,panelBot);
    glVertex2f((float)winW-20,panelTop);glVertex2f(x,panelTop);
    glEnd();

    y -= 4;
    char buf[128];
    glColor3f(0.6f,0.8f,1.f);
    gltext::draw("WORLD DEFAULTS", x+8, y, 1.4f);
    y -= 14;

    snprintf(buf, sizeof(buf), "Start Time: %02d:00  (Up/Down)", g_startHour);
    glColor3f(0.8f,0.8f,0.6f);
    gltext::draw(buf, x+8, y, 1.2f);

    snprintf(buf, sizeof(buf), "Day Speed: %.0fx  ([/])", g_daySpeed);
    gltext::draw(buf, x+250, y, 1.2f);
    y -= 14;

    // Weather default
    const auto& wkw = parser.weatherKeywords();
    const char* wName = (g_weatherIdx >= 0 && g_weatherIdx < (int)wkw.size()) ? wkw[g_weatherIdx].c_str() : "clear";
    snprintf(buf, sizeof(buf), "Default Weather: %s  (G to cycle)", wName);
    gltext::draw(buf, x+8, y, 1.2f);
    y -= 18;

    // ── Generate Button ──
    float btnW = 260, btnH = 28;
    float btnX = ((float)winW - btnW) / 2, btnY = y - btnH;

    glColor4f(0.1f, 0.3f, 0.6f, 0.9f);
    glBegin(GL_QUADS);
    glVertex2f(btnX,btnY);glVertex2f(btnX+btnW,btnY);
    glVertex2f(btnX+btnW,btnY+btnH);glVertex2f(btnX,btnY+btnH);
    glEnd();
    glColor3f(0.3f,0.6f,1.f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(btnX,btnY);glVertex2f(btnX+btnW,btnY);
    glVertex2f(btnX+btnW,btnY+btnH);glVertex2f(btnX,btnY+btnH);
    glEnd();

    glColor3f(1,1,1);
    gltext::drawCentered("[ ENTER ] GENERATE WORLD", (float)winW/2, btnY+7, 1.7f);

    // Store generate button hit
    g_tagHits.push_back({btnX, btnY, btnX+btnW, btnY+btnH, "__generate"});

    glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST);
}

// ══════════════════════════════════════════════════════════════════════
// GENERATION PROGRESS SCREEN
// ══════════════════════════════════════════════════════════════════════

static void renderGenerateScreen(int winW, int winH) {
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glOrtho(0,winW,0,winH,-1,1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // BG
    glBegin(GL_QUADS);
    glColor3f(0.02f,0.03f,0.05f);glVertex2f(0,0);glVertex2f((float)winW,0);
    glColor3f(0.05f,0.07f,0.12f);glVertex2f((float)winW,(float)winH);glVertex2f(0,(float)winH);
    glEnd();

    float cx = (float)winW/2, cy = (float)winH/2 + 40;

    glColor3f(0.5f,0.7f,1.f);
    gltext::drawCentered("GENERATING WORLD...", cx, cy + 50, 2.2f);

    const char* phaseNames[] = {"Idle","Heightmap","Biomes","Structures","Weather","Complete"};
    int pi = (int)g_engine.phase();
    float pct = g_engine.phasePct();

    glColor3f(0.7f,0.7f,0.8f);
    char buf[128];
    snprintf(buf, sizeof(buf), "Phase %d/4: %s  (%.0f%%)", pi, phaseNames[pi], pct);
    gltext::drawCentered(buf, cx, cy + 10, 1.6f);

    // Progress bars per phase
    float barW = 400, barH = 14, barX = cx - barW/2;
    for (int p = 1; p <= 4; p++) {
        float by = cy - 20 - (p-1) * 22;
        float fill = 0.f;
        if (p < pi) fill = 1.f;
        else if (p == pi) fill = pct / 100.f;

        // BG
        glColor4f(0.1f,0.1f,0.15f,0.8f);
        glBegin(GL_QUADS);
        glVertex2f(barX,by);glVertex2f(barX+barW,by);
        glVertex2f(barX+barW,by+barH);glVertex2f(barX,by+barH);
        glEnd();

        // Fill
        float fr = (p==1)?0.3f:(p==2)?0.2f:(p==3)?0.7f:0.5f;
        float fg = (p==1)?0.6f:(p==2)?0.5f:(p==3)?0.5f:0.3f;
        float fb = (p==1)?0.2f:(p==2)?0.8f:(p==3)?0.2f:0.7f;
        glColor4f(fr,fg,fb,0.8f);
        glBegin(GL_QUADS);
        glVertex2f(barX,by);glVertex2f(barX+barW*fill,by);
        glVertex2f(barX+barW*fill,by+barH);glVertex2f(barX,by+barH);
        glEnd();

        // Label
        glColor3f(0.8f,0.8f,0.9f);
        gltext::draw(phaseNames[p], barX+4, by+2, 1.2f);
    }

    // Prompt text
    glColor3f(0.4f,0.4f,0.5f);
    gltext::drawCentered(g_promptText.c_str(), cx, cy - 120, 1.2f);

    glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST);
}

// ══════════════════════════════════════════════════════════════════════
// LAUNCH GENERATION
// ══════════════════════════════════════════════════════════════════════

static void launchGeneration() {
    // Inject weather default if not already in prompt
    const auto& wkw = g_engine.parser().weatherKeywords();
    if (g_weatherIdx >= 0 && g_weatherIdx < (int)wkw.size()) {
        std::string lower = g_promptText;
        std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return (char)tolower(c);});
        bool hasWeather = false;
        for (auto& w : wkw) if (lower.find(w) != std::string::npos) { hasWeather = true; break; }
        if (!hasWeather) {
            if (!g_promptText.empty()) g_promptText += " ";
            g_promptText += wkw[g_weatherIdx];
        }
    }

    if (g_promptText.empty()) g_promptText = "hill forest clear";

    g_engine.beginGeneration(g_promptText);
    g_time.dayTime = (float)g_startHour / 24.f;
    g_time.timeScale = g_daySpeed;
    g_time.day = 1;
    g_time.paused = false;
    g_appState = AppState::GENERATE;
}

// ══════════════════════════════════════════════════════════════════════
// INPUT CALLBACKS
// ══════════════════════════════════════════════════════════════════════

static void charCallback(GLFWwindow*, unsigned int codepoint) {
    if (g_appState != AppState::PROMPT) return;
    if (codepoint >= 32 && codepoint < 128)
        g_promptText += (char)codepoint;
}

static void keyCallback(GLFWwindow* w, int key, int, int action, int) {
    if (action == GLFW_PRESS) {
        g_keys[key] = true;

        if (g_appState == AppState::PROMPT) {
            if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, GLFW_TRUE);
            if (key == GLFW_KEY_ENTER)  launchGeneration();
            if (key == GLFW_KEY_BACKSPACE && !g_promptText.empty()) g_promptText.pop_back();
            if (key == GLFW_KEY_DELETE) g_promptText.clear();

            // Presets 1-9, 0, -, =
            for (int i = 0; i < 9; i++) {
                if (key == GLFW_KEY_1 + i) {
                    g_promptText = BaseX::WorldPromptEngine::preset(i);
                    g_presetIdx = i;
                }
            }
            if (key == GLFW_KEY_0) {
                g_promptText = BaseX::WorldPromptEngine::preset(9);
                g_presetIdx = 9;
            }
            if (key == GLFW_KEY_MINUS) {
                g_promptText = BaseX::WorldPromptEngine::preset(10);
                g_presetIdx = 10;
            }
            if (key == GLFW_KEY_EQUAL) {
                g_promptText = BaseX::WorldPromptEngine::preset(11);
                g_presetIdx = 11;
            }

            // Defaults
            if (key == GLFW_KEY_UP)   g_startHour = (g_startHour + 1) % 24;
            if (key == GLFW_KEY_DOWN) g_startHour = (g_startHour + 23) % 24;
            if (key == GLFW_KEY_KP_ADD || key == GLFW_KEY_RIGHT_BRACKET) {
                g_daySpeed *= 2.f; if (g_daySpeed > 64.f) g_daySpeed = 1.f;
            }
            if (key == GLFW_KEY_KP_SUBTRACT || key == GLFW_KEY_LEFT_BRACKET) {
                g_daySpeed /= 2.f; if (g_daySpeed < 0.25f) g_daySpeed = 64.f;
            }
            if (key == GLFW_KEY_G) {
                int sz = (int)g_engine.parser().weatherKeywords().size();
                g_weatherIdx = (g_weatherIdx + 1) % sz;
            }
        }
        else if (g_appState == AppState::EXPLORE) {
            if (key == GLFW_KEY_ESCAPE) {
                g_appState = AppState::PROMPT;
                g_mouseCaptured = false;
                glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                return;
            }
            if (key == GLFW_KEY_P) g_time.paused = !g_time.paused;
            if (key == GLFW_KEY_H) g_showHUD = !g_showHUD;
            if (key == GLFW_KEY_N) g_dayNightCycle = !g_dayNightCycle;
            if (key == GLFW_KEY_T) {
                if (g_time.timeScale < 2.f) g_time.timeScale = 5.f;
                else if (g_time.timeScale < 10.f) g_time.timeScale = 20.f;
                else g_time.timeScale = 1.f;
            }
            if (key == GLFW_KEY_G) {
                g_weatherSim.mode = (WeatherMode)(((int)g_weatherSim.mode + 1) % NUM_WEATHER_MODES);
            }
        }
    }
    else if (action == GLFW_RELEASE) {
        g_keys[key] = false;
    }
}

static void mouseButtonCallback(GLFWwindow* w, int button, int action, int) {
    if (g_appState == AppState::EXPLORE && button == GLFW_MOUSE_BUTTON_RIGHT) {
        g_mouseCaptured = (action == GLFW_PRESS);
        glfwSetInputMode(w, GLFW_CURSOR, g_mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }

    // Tag clicking on prompt screen
    if (g_appState == AppState::PROMPT && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double mx, my; glfwGetCursorPos(w, &mx, &my);
        int ww, wh; glfwGetFramebufferSize(w, &ww, &wh);
        // my is top-down, convert to bottom-up
        float cx = (float)mx, cy = (float)(wh - my);

        for (const auto& hit : g_tagHits) {
            if (cx >= hit.x0 && cx <= hit.x1 && cy >= hit.y0 && cy <= hit.y1) {
                // Tab switching
                if (hit.keyword.rfind("__tab_", 0) == 0) {
                    g_structTabIdx = std::atoi(hit.keyword.c_str() + 6);
                    break;
                }
                // Preset clicking
                if (hit.keyword.rfind("__preset_", 0) == 0) {
                    int pi = std::atoi(hit.keyword.c_str() + 9);
                    g_promptText = BaseX::WorldPromptEngine::preset(pi);
                    g_presetIdx = pi;
                    break;
                }
                // Generate button
                if (hit.keyword == "__generate") {
                    launchGeneration();
                    break;
                }
                // Normal keyword tag: toggle in/out of prompt
                std::string lower = g_promptText;
                std::transform(lower.begin(), lower.end(), lower.begin(),
                    [](unsigned char c){ return (char)std::tolower(c); });
                size_t pos = lower.find(hit.keyword);
                if (pos != std::string::npos) {
                    // Remove keyword (and trailing/leading space)
                    size_t end = pos + hit.keyword.size();
                    if (end < g_promptText.size() && g_promptText[end] == ' ') end++;
                    else if (pos > 0 && g_promptText[pos-1] == ' ') pos--;
                    g_promptText.erase(pos, end - pos);
                } else {
                    if (!g_promptText.empty() && g_promptText.back() != ' ')
                        g_promptText += ' ';
                    g_promptText += hit.keyword;
                }
                g_presetIdx = -1;
                break;
            }
        }
    }
}

static void cursorCallback(GLFWwindow*, double x, double y) {
    if (g_mouseCaptured && g_appState == AppState::EXPLORE) {
        g_camYaw -= (float)(x - g_lastMX) * 0.003f;
        g_camPitch -= (float)(y - g_lastMY) * 0.003f;
        g_camPitch = std::max(-1.5f, std::min(1.5f, g_camPitch));
    }
    g_lastMX = x; g_lastMY = y;
}

static void scrollCallback(GLFWwindow*, double, double yoff) {
    if (g_appState == AppState::EXPLORE)
        g_camSpeed = std::max(5.f, std::min(120.f, g_camSpeed + (float)yoff * 3.f));
    else if (g_appState == AppState::PROMPT)
        g_tagScrollY += (float)yoff * 20.f;
}

// ══════════════════════════════════════════════════════════════════════
// UPDATE
// ══════════════════════════════════════════════════════════════════════

static void update(float dt) {
    if (g_appState == AppState::GENERATE) {
        bool still = g_engine.tick(dt);
        if (!still && g_engine.phase() == BaseX::GenPhase::COMPLETE) {
            buildWorldMesh();
            applyEngineWeather();
            g_appState = AppState::EXPLORE;
        }
        return;
    }

    if (g_appState != AppState::EXPLORE) return;

    // Camera movement
    float fx = -sinf(g_camYaw)*cosf(g_camPitch), fy = sinf(g_camPitch), fz = -cosf(g_camYaw)*cosf(g_camPitch);
    float rx = cosf(g_camYaw), rz = -sinf(g_camYaw);
    float sp = g_camSpeed * dt;
    if (g_keys[GLFW_KEY_W]) { g_camX+=fx*sp; g_camY+=fy*sp; g_camZ+=fz*sp; }
    if (g_keys[GLFW_KEY_S]) { g_camX-=fx*sp; g_camY-=fy*sp; g_camZ-=fz*sp; }
    if (g_keys[GLFW_KEY_A]) { g_camX-=rx*sp; g_camZ-=rz*sp; }
    if (g_keys[GLFW_KEY_D]) { g_camX+=rx*sp; g_camZ+=rz*sp; }
    if (g_keys[GLFW_KEY_LEFT_SHIFT]) g_camY -= sp;

    g_camX = std::max(1.f, std::min(WORLD_SPAN-1, g_camX));
    g_camZ = std::max(1.f, std::min(WORLD_SPAN-1, g_camZ));
    g_camY = std::max(getWorldHeight(g_camX, g_camZ) + 1.5f, g_camY);

    updateTime(dt);
    updateWeather(dt);
}

// ══════════════════════════════════════════════════════════════════════
// MAIN
// ══════════════════════════════════════════════════════════════════════

int main() {
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_SAMPLES, 4);
    GLFWwindow* window = glfwCreateWindow(1280, 800,
        "BASE-X | WPE-001: Prompt World Explorer", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }

    glfwMakeContextCurrent(window); glfwSwapInterval(1);
    glfwSetCharCallback(window, charCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorCallback);
    glfwSetScrollCallback(window, scrollCallback);

    g_dro.initializeVRProfile();

    glEnable(GL_DEPTH_TEST);
    glEnable(0x809D); // GL_MULTISAMPLE

    double lastTime = glfwGetTime();
    double fpsAccum = 0; int fpsFrames = 0;

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = std::min(0.1f, (float)(now - lastTime));
        lastTime = now;
        g_wallTime = (float)now;
        fpsAccum += dt; fpsFrames++;
        if (fpsAccum >= 0.5) { g_fps = (float)fpsFrames / (float)fpsAccum; fpsFrames = 0; fpsAccum = 0; }

        glfwPollEvents();
        update(dt);

        int winW, winH;
        glfwGetFramebufferSize(window, &winW, &winH);
        glViewport(0, 0, winW, winH);

        if (g_appState == AppState::PROMPT) {
            glClearColor(0.03f, 0.04f, 0.06f, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderPromptScreen(winW, winH);
        }
        else if (g_appState == AppState::GENERATE) {
            glClearColor(0.02f, 0.03f, 0.05f, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderGenerateScreen(winW, winH);
        }
        else {
            // 3D Explore
            float fogR, fogG, fogB; getSkyColors(fogR, fogG, fogB);
            glClearColor(fogR, fogG, fogB, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glMatrixMode(GL_PROJECTION); glLoadIdentity();
            perspectiveGL(60.0, (double)winW / winH, 0.5, 600.0);
            glMatrixMode(GL_MODELVIEW); glLoadIdentity();

            float lx = g_camX - sinf(g_camYaw)*cosf(g_camPitch);
            float ly = g_camY + sinf(g_camPitch);
            float lz = g_camZ - cosf(g_camYaw)*cosf(g_camPitch);
            lookAtGL(g_camX, g_camY, g_camZ, lx, ly, lz);

            // Fog
            glEnable(GL_FOG); glFogi(GL_FOG_MODE, GL_EXP);
            float fogDensity = 0.004f + g_weatherSim.rainIntensity*0.008f + g_weatherSim.mistDensity*0.012f;
            float fc[4] = {fogR, fogG, fogB, 1};
            glFogfv(GL_FOG_COLOR, fc);
            glFogf(GL_FOG_DENSITY, fogDensity);

            renderSky();
            renderTerrain();
            renderWater();
            renderTrees();
            renderStructures();
            renderWeatherParticles();
            renderExploreHUD(winW, winH);
        }

        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
