/**
 * SBX-001: World Prompt Engine — Natural Language World Generation
 *
 * C++ / GLFW / OpenGL implementation of the HTML SBX-001 demo.
 *
 * Features:
 *   - Natural-language prompt → procedural world generation pipeline
 *   - NLP keyword extraction (terrain, biome, structure, weather)
 *   - 64×64 isometric heightmap with multi-octave noise
 *   - Biome painting, structure placement, weather particles
 *   - Real-time DRO precision shifting across generation phases
 *   - 12 preset world prompts
 *   - Clickable tag inventory for terrain, biome, structures, weather
 *
 * Controls:
 *   1-9,0,-,= : Load preset prompt
 *   Enter     : Generate world from current prompt
 *   Backspace : Delete last character in prompt
 *   R         : Reset / clear world
 *   ESC       : Quit
 *   Mouse     : Click tags to add/remove keywords
 *   Type      : Edit prompt text
 */

#include <GLFW/glfw3.h>
#ifdef _WIN32
#include <GL/glext.h>
#endif
#include <BaseX/DRO.h>
#include <BaseX/GLText.h>
#include <BaseX/WorldPromptEngine.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
#include <random>

static constexpr float PI = 3.14159265f;
static constexpr int   WIN_W = 1280;
static constexpr int   WIN_H = 800;
static constexpr int   GS = BaseX::WPE_GRID_SIZE;

// ══════════════════════════════════════════════════════════════════════
// GLOBAL STATE
// ══════════════════════════════════════════════════════════════════════

static BaseX::WorldPromptEngine g_engine;
static std::string g_promptText;
static float g_time = 0.0f;
static int g_totalTiles = 0;
static int g_totalStructs = 0;

// ── Tag inventory UI state ──
static int g_structTabIdx = 0;  // 0=All, 1..N = subcategory

struct TagHitBox { float x0, y0, x1, y1; std::string keyword; };
static std::vector<TagHitBox> g_tagHits;

struct StructSubcat { const char* label; std::vector<std::string> keywords; };
static std::vector<StructSubcat> g_structTabs;

static void buildStructTabs() {
    struct C { const char* l; std::vector<const char*> kw; };
    C cats[] = {
        {"Landmark", {"spire","fortress","cathedral","castle","tower","pyramid",
            "temple","palace","citadel","lighthouse","colosseum","amphitheater",
            "keep","pagoda","ziggurat","minaret","aqueduct","monastery","arena",
            "mansion","villa","warehouse","granary","barn","stable","bell_tower"}},
        {"Buildings", {"hut","tavern","cabin","chapel","forge","marketplace",
            "windmill","cottage","farmhouse","shack","shed","workshop","smithy",
            "market_stall","tent","lean_to","outhouse","guardhouse","barracks",
            "inn","bakery","apothecary"}},
        {"Walls", {"bridge","dock","gate","wall","arch","archway","torii",
            "pier","jetty","wharf","boathouse","platform","stairway","rampart",
            "gatehouse","drawbridge","colonnade","terrace","fence","palisade",
            "parapet","battlement","portcullis"}},
        {"Low/Dome", {"altar","shrine","dolmen","barrow","dome","cupola",
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
    for (auto& c : cats) {
        StructSubcat sc; sc.label = c.l;
        for (auto* k : c.kw) sc.keywords.push_back(k);
        g_structTabs.push_back(std::move(sc));
    }
}

// Weather particles
struct Particle {
    float x, y, vx, vy, life, size;
    float r, g, b, a;
};
static std::vector<Particle> g_particles;
static std::mt19937 g_rng(12345);

// ══════════════════════════════════════════════════════════════════════
// GL HELPERS
// ══════════════════════════════════════════════════════════════════════

static void setColor(float r, float g, float b, float a = 1.0f) {
    glColor4f(r, g, b, a);
}

static void drawRect(float x, float y, float w, float h) {
    glBegin(GL_QUADS);
    glVertex2f(x,     y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x,     y + h);
    glEnd();
}

static void drawGradientRect(float x, float y, float w, float h,
                              float tr, float tg, float tb,
                              float br, float bg, float bb) {
    glBegin(GL_QUADS);
    glColor4f(tr, tg, tb, 1.0f);
    glVertex2f(x, y + h);
    glVertex2f(x + w, y + h);
    glColor4f(br, bg, bb, 1.0f);
    glVertex2f(x + w, y);
    glVertex2f(x, y);
    glEnd();
}

static void drawDiamond(float cx, float cy, float hw, float hh) {
    glBegin(GL_QUADS);
    glVertex2f(cx,      cy - hh);
    glVertex2f(cx + hw, cy);
    glVertex2f(cx,      cy + hh);
    glVertex2f(cx - hw, cy);
    glEnd();
}

static void drawTriangle(float x0, float y0, float x1, float y1, float x2, float y2) {
    glBegin(GL_TRIANGLES);
    glVertex2f(x0, y0);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
}

// ══════════════════════════════════════════════════════════════════════
// HUD RENDERING
// ══════════════════════════════════════════════════════════════════════

static constexpr float SIDEBAR_W = 270.0f;
static constexpr float HEADER_H  = 44.0f;
static constexpr float FOOTER_H  = 24.0f;

static void drawHeader(int winW) {
    // Background
    setColor(0.07f, 0.09f, 0.04f, 0.95f);
    drawRect(0, (float)WIN_H - HEADER_H, (float)winW, HEADER_H);
    // Bottom border
    setColor(0.20f, 0.80f, 0.33f, 0.3f);
    drawRect(0, (float)WIN_H - HEADER_H, (float)winW, 1);

    // Badge
    setColor(0.20f, 0.80f, 0.33f, 1.0f);
    drawRect(12, (float)WIN_H - HEADER_H + 14, 48, 16);
    setColor(0.0f, 0.0f, 0.0f, 1.0f);
    gltext::draw("BASE-X", 14, (float)WIN_H - HEADER_H + 17, 1.2f);

    // Title
    setColor(0.88f, 0.93f, 0.85f, 1.0f);
    gltext::draw("SBX-001: WORLD PROMPT ENGINE", 68, (float)WIN_H - HEADER_H + 18, 1.5f);

    // Subtitle
    setColor(0.40f, 0.53f, 0.40f, 0.7f);
    gltext::draw("Sandbox RPG VR - NLWG", 68, (float)WIN_H - HEADER_H + 6, 1.0f);

    // Right stats
    auto dro = g_engine.currentDRO();
    char buf[64];
    snprintf(buf, sizeof(buf), "DRO: B-%d", (int)dro.nearPlayer);
    setColor(0.83f, 0.63f, 0.09f, 1.0f);
    gltext::drawRight(buf, (float)winW - 14, (float)WIN_H - HEADER_H + 18, 1.3f);

    snprintf(buf, sizeof(buf), "Gen: %d", g_engine.genCount());
    setColor(0.40f, 0.53f, 0.40f, 0.7f);
    gltext::drawRight(buf, (float)winW - 14, (float)WIN_H - HEADER_H + 6, 1.0f);
}

static void drawFooter(int winW) {
    setColor(0.04f, 0.06f, 0.03f, 0.95f);
    drawRect(0, 0, (float)winW, FOOTER_H);
    setColor(0.20f, 0.80f, 0.33f, 0.15f);
    drawRect(0, FOOTER_H, (float)winW, 1);

    setColor(0.40f, 0.53f, 0.40f, 0.5f);
    gltext::draw("(c) 2026 Keenan Wallace Dunham - Troll Hunter Gaming LLC - Patent Pending",
                 14, 8, 0.9f);
    gltext::drawRight("Base-X Engine v0.9 - DRO: Dynamic Radix Optimization - B-4 to B-64",
                      (float)winW - 14, 8, 0.9f);
}

static void drawProgressBar(float x, float y, float w, float h, float pct,
                             float r, float g, float b) {
    // Background
    setColor(1.0f, 1.0f, 1.0f, 0.06f);
    drawRect(x, y, w, h);
    // Fill
    float fillW = w * std::clamp(pct / 100.0f, 0.0f, 1.0f);
    if (fillW > 0) {
        setColor(r, g, b, 0.8f);
        drawRect(x, y, fillW, h);
    }
}

static void drawDROBar(float x, float y, float w, float h,
                        BaseX::RadixBase radix) {
    int val = static_cast<int>(radix);
    float pct = val / 64.0f;

    // Background
    setColor(1.0f, 1.0f, 1.0f, 0.06f);
    drawRect(x, y, w, h);

    // Color by level
    float r, g, b;
    switch (val) {
        case 64: r=0.0f; g=0.87f; b=0.40f; break;
        case 32: r=0.20f;g=0.53f; b=0.80f; break;
        case 16: r=0.47f;g=0.53f; b=0.47f; break;
        case  8: r=0.47f;g=0.53f; b=0.47f; break;
        default: r=0.83f;g=0.63f; b=0.09f; break; // 48, 24, 4
    }
    float fillW = w * pct;
    setColor(r, g, b, 0.8f);
    drawRect(x, y, fillW, h);
}

static void drawSidebar(int winW, int winH) {
    float sx = winW - SIDEBAR_W;
    float sy = FOOTER_H;
    float sh = winH - HEADER_H - FOOTER_H;

    // Background
    setColor(0.05f, 0.07f, 0.04f, 0.95f);
    drawRect(sx, sy, SIDEBAR_W, sh);
    // Left border
    setColor(0.20f, 0.80f, 0.33f, 0.15f);
    drawRect(sx, sy, 1, sh);

    float cx = sx + 10;
    float cy = winH - HEADER_H - 16;
    float pw = SIDEBAR_W - 20;

    // ── Prompt section ──
    setColor(0.20f, 0.80f, 0.33f, 0.9f);
    gltext::draw("WORLD PROMPT", cx, cy, 1.0f);
    cy -= 4;
    setColor(0.20f, 0.80f, 0.33f, 0.2f);
    drawRect(cx, cy, pw, 1);
    cy -= 8;

    // Prompt text area background
    setColor(0.20f, 0.80f, 0.33f, 0.04f);
    drawRect(cx, cy - 44, pw, 50);
    setColor(0.20f, 0.80f, 0.33f, 0.15f);
    // Border
    glBegin(GL_LINE_LOOP);
    glVertex2f(cx, cy - 44);
    glVertex2f(cx + pw, cy - 44);
    glVertex2f(cx + pw, cy + 6);
    glVertex2f(cx, cy + 6);
    glEnd();

    // Prompt text (wrapped)
    if (g_promptText.empty()) {
        setColor(0.40f, 0.53f, 0.40f, 0.3f);
        gltext::draw("Type or click tags (1-9,0,-,=)", cx + 4, cy - 4, 0.9f);
    } else {
        setColor(0.88f, 0.91f, 0.85f, 0.9f);
        // Simple line wrapping at ~38 chars
        int charsPerLine = 38;
        float lineY = cy - 4;
        for (size_t i = 0; i < g_promptText.size(); i += charsPerLine) {
            std::string line = g_promptText.substr(i, charsPerLine);
            gltext::draw(line.c_str(), cx + 4, lineY, 0.9f);
            lineY -= 10;
        }
    }

    // Blinking cursor
    if (fmod(g_time, 1.0f) < 0.5f) {
        setColor(0.20f, 0.80f, 0.33f, 0.8f);
        int cursorLine = (int)g_promptText.size() / 38;
        int cursorCol  = (int)g_promptText.size() % 38;
        float curX = cx + 4 + cursorCol * 6 * 0.9f;
        float curY = cy - 4 - cursorLine * 10;
        drawRect(curX, curY - 1, 1, 9);
    }

    cy -= 56;

    // Buttons hint
    bool isGenerating = g_engine.phase() != BaseX::GenPhase::IDLE &&
                        g_engine.phase() != BaseX::GenPhase::COMPLETE;
    if (isGenerating) {
        setColor(0.83f, 0.63f, 0.09f, 0.8f);
        gltext::draw("[GENERATING...]", cx, cy, 1.1f);
    } else {
        setColor(0.20f, 0.80f, 0.33f, 0.9f);
        gltext::draw("[ENTER] Generate  [R] Clear", cx, cy, 0.9f);
    }
    cy -= 18;

    // ── Presets section ──
    setColor(0.20f, 0.80f, 0.33f, 0.9f);
    gltext::draw("PRESETS", cx, cy, 1.0f);
    cy -= 4;
    setColor(0.20f, 0.80f, 0.33f, 0.2f);
    drawRect(cx, cy, pw, 1);
    cy -= 4;

    static const char* presetNames[] = {
        "1: Volcanic Island", "2: Frozen Peaks",
        "3: Enchanted Forest","4: Desert Ruins",
        "5: Floating Isles",  "6: Necropolis",
        "7: Steampunk City",  "8: Crystal Caverns",
        "9: Bamboo Jungle",   "0: Crimson Badlands",
        "-: Tundra Archipel.", "=: Savanna Caldera"
    };
    for (int i = 0; i < 12; i++) {
        float bx = cx + (i % 2) * (pw / 2 + 2);
        float by = cy - (i / 2) * 14;
        setColor(1.0f, 1.0f, 1.0f, 0.04f);
        drawRect(bx, by - 2, pw / 2 - 4, 12);
        setColor(0.40f, 0.53f, 0.40f, 0.7f);
        gltext::draw(presetNames[i], bx + 3, by, 0.75f);
    }
    cy -= 6 * 14 + 8;

    // ── NLP Parse Result ──
    setColor(0.20f, 0.80f, 0.33f, 0.9f);
    gltext::draw("NLP PARSE RESULT", cx, cy, 1.0f);
    cy -= 4;
    setColor(0.20f, 0.80f, 0.33f, 0.2f);
    drawRect(cx, cy, pw, 1);
    cy -= 6;

    auto& parsed = g_engine.parsed();

    auto drawTagRow = [&](const char* label, const std::vector<std::string>& tags,
                          float tr, float tg, float tb) {
        setColor(0.40f, 0.53f, 0.40f, 0.5f);
        gltext::draw(label, cx, cy, 0.8f);
        cy -= 12;
        float tagX = cx;
        for (auto& t : tags) {
            float tw = gltext::textWidth(t.c_str(), 0.75f) + 8;
            if (tagX + tw > sx + SIDEBAR_W - 10) { tagX = cx; cy -= 11; }
            setColor(tr, tg, tb, 0.08f);
            drawRect(tagX, cy - 1, tw, 10);
            setColor(tr, tg, tb, 0.15f);
            glBegin(GL_LINE_LOOP);
            glVertex2f(tagX, cy - 1);
            glVertex2f(tagX + tw, cy - 1);
            glVertex2f(tagX + tw, cy + 9);
            glVertex2f(tagX, cy + 9);
            glEnd();
            setColor(tr, tg, tb, 0.9f);
            gltext::draw(t.c_str(), tagX + 4, cy + 1, 0.75f);
            tagX += tw + 3;
        }
        cy -= 14;
    };

    drawTagRow("Terrain",    parsed.terrain,   0.20f, 0.80f, 0.33f);
    drawTagRow("Biome",      parsed.biome,     0.93f, 0.53f, 0.20f);
    drawTagRow("Structures", parsed.structure,  0.00f, 0.80f, 0.93f);
    drawTagRow("Weather",    parsed.weather,    0.67f, 0.67f, 0.87f);

    // ── Generation Progress ──
    setColor(0.20f, 0.80f, 0.33f, 0.9f);
    gltext::draw("GENERATION PROGRESS", cx, cy, 1.0f);
    cy -= 4;
    setColor(0.20f, 0.80f, 0.33f, 0.2f);
    drawRect(cx, cy, pw, 1);
    cy -= 6;

    auto phase = g_engine.phase();
    float pct  = g_engine.phasePct();

    static const char* phaseNames[] = {"Heightmap","Biome Paint","Structures","Weather/Light"};
    for (int i = 0; i < 4; i++) {
        setColor(0.40f, 0.53f, 0.40f, 0.5f);
        gltext::draw(phaseNames[i], cx, cy, 0.8f);

        float barPct = 0.0f;
        BaseX::GenPhase gp = static_cast<BaseX::GenPhase>(i + 1);
        if (phase > gp)        barPct = 100.0f;
        else if (phase == gp)  barPct = pct;

        char pctStr[8];
        if (barPct > 0.0f) snprintf(pctStr, sizeof(pctStr), "%d%%", (int)barPct);
        else               snprintf(pctStr, sizeof(pctStr), "---");
        setColor(0.88f, 0.91f, 0.85f, 0.7f);
        gltext::drawRight(pctStr, cx + pw, cy, 0.8f);

        cy -= 10;
        drawProgressBar(cx, cy, pw, 5, barPct, 0.20f, 0.80f, 0.33f);
        cy -= 10;
    }

    // ── World Stats ──
    setColor(0.20f, 0.80f, 0.33f, 0.9f);
    gltext::draw("WORLD STATS", cx, cy, 1.0f);
    cy -= 4;
    setColor(0.20f, 0.80f, 0.33f, 0.2f);
    drawRect(cx, cy, pw, 1);
    cy -= 6;

    auto& stats = g_engine.stats();
    char buf[64];

    auto drawStatBox = [&](float bx, float by, const char* val, const char* label) {
        float bw = pw / 2 - 3;
        setColor(0.20f, 0.80f, 0.33f, 0.04f);
        drawRect(bx, by, bw, 28);
        setColor(0.20f, 0.80f, 0.33f, 0.08f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(bx, by); glVertex2f(bx+bw, by);
        glVertex2f(bx+bw, by+28); glVertex2f(bx, by+28);
        glEnd();
        setColor(0.83f, 0.63f, 0.09f, 1.0f);
        gltext::drawCentered(val, bx + bw/2, by + 16, 1.3f);
        setColor(0.40f, 0.53f, 0.40f, 0.5f);
        gltext::drawCentered(label, bx + bw/2, by + 4, 0.7f);
    };

    if (phase == BaseX::GenPhase::COMPLETE) {
        snprintf(buf, sizeof(buf), "%d", stats.tileCount);
        drawStatBox(cx, cy - 30, buf, "TERRAIN TILES");
        snprintf(buf, sizeof(buf), "%d", stats.structCount);
        drawStatBox(cx + pw/2 + 3, cy - 30, buf, "STRUCTURES");
        snprintf(buf, sizeof(buf), "%d", stats.biomeCount);
        drawStatBox(cx, cy - 62, buf, "BIOMES");
        snprintf(buf, sizeof(buf), "%.0fm", stats.maxElevation);
        drawStatBox(cx + pw/2 + 3, cy - 62, buf, "MAX ELEV");
    } else {
        drawStatBox(cx, cy - 30, "---", "TERRAIN TILES");
        drawStatBox(cx + pw/2 + 3, cy - 30, "---", "STRUCTURES");
        drawStatBox(cx, cy - 62, "---", "BIOMES");
        drawStatBox(cx + pw/2 + 3, cy - 62, "---", "MAX ELEV");
    }
    cy -= 70;

    // ── Session totals ──
    setColor(0.20f, 0.80f, 0.33f, 0.9f);
    gltext::draw("SESSION", cx, cy, 1.0f);
    cy -= 4;
    setColor(0.20f, 0.80f, 0.33f, 0.2f);
    drawRect(cx, cy, pw, 1);
    cy -= 6;

    float sw3 = pw / 3 - 2;
    auto drawSmallStat = [&](float bx, float by, const char* val, const char* label) {
        setColor(0.20f, 0.80f, 0.33f, 0.04f);
        drawRect(bx, by, sw3, 28);
        setColor(0.83f, 0.63f, 0.09f, 1.0f);
        gltext::drawCentered(val, bx + sw3/2, by + 16, 1.2f);
        setColor(0.40f, 0.53f, 0.40f, 0.5f);
        gltext::drawCentered(label, bx + sw3/2, by + 4, 0.65f);
    };

    snprintf(buf, sizeof(buf), "%d", g_engine.genCount());
    drawSmallStat(cx, cy - 30, buf, "GENERATED");
    snprintf(buf, sizeof(buf), "%d", g_totalTiles);
    drawSmallStat(cx + sw3 + 3, cy - 30, buf, "TOT TILES");
    snprintf(buf, sizeof(buf), "%d", g_totalStructs);
    drawSmallStat(cx + 2*(sw3 + 3), cy - 30, buf, "STRUCTS");
    cy -= 38;

    // ── DRO Precision ──
    setColor(0.20f, 0.80f, 0.33f, 0.9f);
    gltext::draw("DRO PRECISION", cx, cy, 1.0f);
    cy -= 4;
    setColor(0.20f, 0.80f, 0.33f, 0.2f);
    drawRect(cx, cy, pw, 1);
    cy -= 6;

    auto dro = g_engine.currentDRO();
    BaseX::RadixBase droValues[] = {
        dro.nearPlayer, dro.genFrontier, dro.heightmap,
        dro.biomePaint, dro.structures,  dro.atmosphere
    };
    static const char* droLabels[] = {
        "Near Player", "Gen Frontier", "Heightmap",
        "Biome Paint", "Structures",   "Atmosphere"
    };
    for (int i = 0; i < 6; i++) {
        setColor(0.40f, 0.53f, 0.40f, 0.5f);
        gltext::draw(droLabels[i], cx, cy, 0.75f);
        snprintf(buf, sizeof(buf), "B-%d", (int)droValues[i]);
        setColor(0.88f, 0.91f, 0.85f, 0.7f);
        gltext::drawRight(buf, cx + pw, cy, 0.75f);
        cy -= 9;
        drawDROBar(cx + 62, cy + 2, pw - 90, 5, droValues[i]);
        cy -= 8;
    }
}

// ══════════════════════════════════════════════════════════════════════
// WORLD RENDERING
// ══════════════════════════════════════════════════════════════════════

static void drawSky(int canvasW, int canvasH) {
    float tr, tg, tb, br, bg, bb;
    BaseX::WeatherSystem::getSkyColors(g_engine.weather(), tr, tg, tb, br, bg, bb);
    drawGradientRect(0, FOOTER_H, (float)canvasW, (float)canvasH, tr, tg, tb, br, bg, bb);
}

static void drawIdleScreen(float canvasW, float canvasH) {
    float midX = canvasW * 0.5f;

    // ── Tag Inventory Panel ──
    if (g_structTabs.empty()) buildStructTabs();
    g_tagHits.clear();

    const auto& parser = g_engine.parser();
    std::string lower = g_promptText;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c){ return (char)std::tolower(c); });

    // Helper: draw a row of clickable tags that wrap. Returns new y.
    auto drawClickTags = [&](const char* label, const std::vector<std::string>& tags,
                             float x, float y, float maxW,
                             float tr, float tg, float tb) -> float {
        setColor(tr*0.5f, tg*0.5f, tb*0.5f, 0.7f);
        gltext::draw(label, x, y, 1.0f);
        y -= 13;
        float px = x;
        for (const auto& tag : tags) {
            float tw = gltext::textWidth(tag.c_str(), 0.8f) + 8;
            if (px + tw > x + maxW) { px = x; y -= 12; }
            bool active = lower.find(tag) != std::string::npos;
            float bx0 = px, by0 = y - 1, bx1 = px + tw, by1 = y + 9;
            if (active) setColor(tr*0.3f, tg*0.3f, tb*0.3f, 0.8f);
            else        setColor(0.08f, 0.10f, 0.07f, 0.6f);
            drawRect(bx0, by0, tw, 10);
            if (active) setColor(tr, tg, tb, 0.9f);
            else        setColor(0.25f, 0.30f, 0.25f, 0.5f);
            glBegin(GL_LINE_LOOP);
            glVertex2f(bx0,by0);glVertex2f(bx1,by0);glVertex2f(bx1,by1);glVertex2f(bx0,by1);
            glEnd();
            if (active) setColor(1, 1, 1, 1);
            else        setColor(0.55f, 0.60f, 0.55f, 0.8f);
            gltext::draw(tag.c_str(), px + 4, y, 0.8f);
            g_tagHits.push_back({bx0, by0, bx1, by1, tag});
            px += tw + 3;
        }
        return y - 16;
    };

    float x = 14.f, y = FOOTER_H + canvasH - 12.f;
    float maxW = canvasW - 28.f;

    // Title
    setColor(0.20f, 0.80f, 0.33f, 0.6f);
    gltext::drawCentered("CLICK TAGS TO BUILD YOUR PROMPT", midX, y, 1.2f);
    y -= 18;

    // Terrain tags
    std::vector<std::string> terrainTags;
    for (auto& kv : parser.terrainDict()) terrainTags.push_back(kv.first);
    std::sort(terrainTags.begin(), terrainTags.end());
    y = drawClickTags("TERRAIN", terrainTags, x, y, maxW, 0.20f, 0.80f, 0.33f);

    // Biome tags
    std::vector<std::string> biomeTags;
    for (auto& kv : parser.biomeDict()) biomeTags.push_back(kv.first);
    std::sort(biomeTags.begin(), biomeTags.end());
    y = drawClickTags("BIOME", biomeTags, x, y, maxW, 0.30f, 0.60f, 1.00f);

    // Structure subcategory tabs
    setColor(0.45f, 0.35f, 0.15f, 0.7f);
    gltext::draw("STRUCTURES", x, y, 1.0f);
    y -= 13;
    float tabX = x;
    // "All" tab
    {
        bool sel = (g_structTabIdx == 0);
        float tw = gltext::textWidth("All", 0.8f) + 8;
        if (sel) setColor(0.30f, 0.25f, 0.08f, 0.9f);
        else     setColor(0.08f, 0.08f, 0.07f, 0.6f);
        drawRect(tabX, y - 1, tw, 10);
        if (sel) setColor(1.f, 0.9f, 0.4f, 1);
        else     setColor(0.45f, 0.45f, 0.40f, 0.8f);
        gltext::draw("All", tabX + 4, y, 0.8f);
        g_tagHits.push_back({tabX, y-1, tabX+tw, y+9, "__tab_0"});
        tabX += tw + 3;
    }
    for (int ti = 0; ti < (int)g_structTabs.size(); ti++) {
        const char* lbl = g_structTabs[ti].label;
        float tw = gltext::textWidth(lbl, 0.8f) + 8;
        if (tabX + tw > x + maxW) { tabX = x; y -= 12; }
        bool sel = (g_structTabIdx == ti + 1);
        if (sel) setColor(0.30f, 0.25f, 0.08f, 0.9f);
        else     setColor(0.08f, 0.08f, 0.07f, 0.6f);
        drawRect(tabX, y - 1, tw, 10);
        if (sel) setColor(1.f, 0.9f, 0.4f, 1);
        else     setColor(0.45f, 0.45f, 0.40f, 0.8f);
        gltext::draw(lbl, tabX + 4, y, 0.8f);
        char tid[16]; snprintf(tid, sizeof(tid), "__tab_%d", ti + 1);
        g_tagHits.push_back({tabX, y-1, tabX+tw, y+9, tid});
        tabX += tw + 3;
    }
    y -= 14;

    // Structure tags for selected tab
    if (g_structTabIdx == 0) {
        y = drawClickTags("", parser.structureKeywords(), x, y, maxW, 0.90f, 0.70f, 0.30f);
    } else {
        int idx = g_structTabIdx - 1;
        if (idx >= 0 && idx < (int)g_structTabs.size())
            y = drawClickTags("", g_structTabs[idx].keywords, x, y, maxW, 0.90f, 0.70f, 0.30f);
    }

    // Weather tags
    y = drawClickTags("WEATHER", parser.weatherKeywords(), x, y, maxW, 0.80f, 0.40f, 0.80f);

    // Bottom hint
    y -= 4;
    setColor(0.20f, 0.80f, 0.33f, 0.25f);
    gltext::drawCentered("Click tags above  |  Type in sidebar  |  1-9,0,-,= for presets  |  ENTER to generate",
                         midX, y, 0.9f);
}

static void drawIsometricWorld(float canvasW, float canvasH) {
    auto& hmap = g_engine.heightmap();
    auto& bmap = g_engine.biomeMap();
    auto& structs = g_engine.structures();
    auto phase = g_engine.phase();
    float pct  = g_engine.phasePct();

    if (hmap.empty()) return;

    float tileW = canvasW / (GS + 10);
    float tileH = tileW * 0.5f;
    float zoom  = std::min(canvasW, canvasH) / (GS * 8.0f);
    float ox    = canvasW * 0.5f;
    float oy    = FOOTER_H + canvasH * 0.85f; // bottom-up: high oy = high in window

    // Render tiles back to front
    for (int y = 0; y < GS; y++) {
        for (int x = 0; x < GS; x++) {
            float h = hmap[y * GS + x];

            // Progressive reveal during heightmap gen
            float genProgress = (phase == BaseX::GenPhase::HEIGHTMAP)
                ? (pct / 100.0f * GS) : (float)GS;
            if (phase == BaseX::GenPhase::HEIGHTMAP && (x + y) > genProgress * 2)
                continue;

            bool isWater = (h < 0.12f);

            // Smooth water wave displacement
            float waveOffset = 0.0f;
            if (isWater) {
                // Layered sine waves for smooth, natural water motion
                float wx = (float)x / GS;
                float wy = (float)y / GS;
                float wave1 = std::sin(wx * 12.0f + g_time * 0.8f) * 0.004f;
                float wave2 = std::sin(wy * 10.0f + g_time * 0.6f + 1.5f) * 0.003f;
                float wave3 = std::sin((wx + wy) * 8.0f + g_time * 1.1f) * 0.002f;
                waveOffset = wave1 + wave2 + wave3;
            }

            float renderH = isWater ? (h + waveOffset) : h;

            // Isometric projection
            float ix = (x - y) * tileW * 0.5f * zoom + ox;
            float iy = (x + y) * tileH * 0.5f * zoom * -1.0f + oy + renderH * 80.0f * zoom;

            // Biome colour
            float cr = 0.27f, cg = 0.53f, cb = 0.20f;
            if (!bmap.empty()) {
                cr = bmap[y * GS + x].r;
                cg = bmap[y * GS + x].g;
                cb = bmap[y * GS + x].b;
            }

            // Water: animated specular highlight and smooth colour shift
            if (isWater) {
                float wx = (float)x / GS;
                float wy = (float)y / GS;
                // Gentle colour ripple
                float shimmer = std::sin(wx * 15.0f + wy * 10.0f + g_time * 1.2f) * 0.5f + 0.5f;
                float specular = shimmer * shimmer * shimmer;  // sharpen for caustic-like highlights
                cr += specular * 0.08f;
                cg += specular * 0.10f;
                cb += specular * 0.15f;
                // Depth darkening for deeper water
                float depth = 1.0f - (h / 0.12f);
                cr *= (1.0f - depth * 0.25f);
                cg *= (1.0f - depth * 0.15f);
                cb *= (1.0f - depth * 0.05f);
            }

            float brightness = 0.6f + renderH * 0.5f;
            float hw = tileW * 0.5f * zoom;
            float hh = tileH * 0.5f * zoom;

            // Top face
            float alpha = isWater ? 0.85f : 1.0f;
            setColor(cr * brightness, cg * brightness, cb * brightness, alpha);
            drawDiamond(ix, iy, hw, hh);

            // Side faces for height
            if (h > 0.1f) {
                float sideH = h * 40.0f * zoom;

                // Right side
                setColor(cr * brightness * 0.5f, cg * brightness * 0.5f,
                         cb * brightness * 0.5f, 1.0f);
                glBegin(GL_QUADS);
                glVertex2f(ix, iy + hh);
                glVertex2f(ix + hw, iy);
                glVertex2f(ix + hw, iy - sideH);
                glVertex2f(ix, iy + hh - sideH);
                glEnd();

                // Left side
                setColor(cr * brightness * 0.35f, cg * brightness * 0.35f,
                         cb * brightness * 0.35f, 1.0f);
                glBegin(GL_QUADS);
                glVertex2f(ix, iy + hh);
                glVertex2f(ix - hw, iy);
                glVertex2f(ix - hw, iy - sideH);
                glVertex2f(ix, iy + hh - sideH);
                glEnd();
            }
        }
    }

    // Structures
    for (auto& s : structs) {
        float ix = (s.gx - s.gy) * tileW * 0.5f * zoom + ox;
        float iy = (s.gx + s.gy) * tileH * 0.5f * zoom * -1.0f + oy + s.height * 80.0f * zoom;
        float sz = s.size * zoom;

        // Building body
        setColor(s.r, s.g, s.b, 0.8f);
        drawRect(ix - sz/2, iy, sz, sz * 1.5f);

        // Roof
        setColor(1.0f, 1.0f, 1.0f, 0.15f);
        drawTriangle(ix - sz * 0.6f, iy + sz * 1.5f,
                     ix,             iy + sz * 2.2f,
                     ix + sz * 0.6f, iy + sz * 1.5f);

        // Label
        setColor(1.0f, 1.0f, 1.0f, 0.5f);
        float labelScale = std::max(0.5f, 0.6f * zoom);
        gltext::drawCentered(s.type.c_str(), ix, iy + sz * 2.4f, labelScale);
    }
}

static void spawnWeatherParticles(float canvasW, float canvasH) {
    auto& ws = g_engine.weather();
    std::uniform_real_distribution<float> distX(0.0f, canvasW);
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    if (ws.hasRain || ws.hasStorm) {
        for (int i = 0; i < 3; i++) {
            Particle p;
            p.x = distX(g_rng);
            p.y = FOOTER_H + canvasH + 5;
            p.vx = -60.0f;
            p.vy = -(480.0f + dist01(g_rng) * 240.0f);
            p.life = 1.5f; p.size = 1.0f;
            p.r = 0.59f; p.g = 0.71f; p.b = 0.86f; p.a = 0.3f;
            g_particles.push_back(p);
        }
    }
    if (ws.hasSnow) {
        for (int i = 0; i < 2; i++) {
            Particle p;
            p.x = distX(g_rng);
            p.y = FOOTER_H + canvasH + 5;
            p.vx = (dist01(g_rng) - 0.5f) * 120.0f;
            p.vy = -(60.0f + dist01(g_rng) * 120.0f);
            p.life = 3.0f; p.size = 2.0f;
            p.r = 0.86f; p.g = 0.90f; p.b = 1.0f; p.a = 0.4f;
            g_particles.push_back(p);
        }
    }
    if (ws.hasAsh && dist01(g_rng) < 0.3f) {
        Particle p;
        p.x = canvasW * 0.5f + (dist01(g_rng) - 0.5f) * 100.0f;
        p.y = FOOTER_H + canvasH * 0.2f;
        p.vx = (dist01(g_rng) - 0.5f) * 120.0f;
        p.vy = 60.0f + dist01(g_rng) * 60.0f;
        p.life = 2.0f; p.size = 1.5f;
        p.r = 0.59f; p.g = 0.55f; p.b = 0.51f; p.a = 0.3f;
        g_particles.push_back(p);
    }
}

static void drawWeatherEffects(float canvasW, float canvasH, float dt) {
    auto& ws = g_engine.weather();

    // Mist / fog
    if (ws.hasMist) {
        setColor(0.71f, 0.75f, 0.78f, 0.04f);
        for (int i = 0; i < 5; i++) {
            float mx = std::sin(g_time * 0.2f + i * 2.0f) * canvasW * 0.3f + canvasW * 0.5f;
            float my = FOOTER_H + canvasH * 0.4f + std::sin(g_time * 0.3f + i) * 30.0f;
            float mr = 80.0f + std::sin(g_time + i) * 20.0f;
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(mx, my);
            for (int j = 0; j <= 24; j++) {
                float a = j * (2.0f * PI / 24.0f);
                glVertex2f(mx + std::cos(a) * mr, my + std::sin(a) * mr);
            }
            glEnd();
        }
    }

    // Aurora
    if (ws.hasAurora) {
        for (int i = 0; i < 5; i++) {
            float ax = canvasW * 0.2f + i * canvasW * 0.15f + std::sin(g_time + i) * 30.0f;
            float hue = 0.33f + i * 0.08f + std::sin(g_time * 0.5f) * 0.05f;
            // HSL → approximate RGB
            float ar = std::clamp(std::sin(hue * 6.28f) * 0.3f + 0.3f, 0.0f, 1.0f);
            float ag = std::clamp(std::sin((hue + 0.33f) * 6.28f) * 0.3f + 0.5f, 0.0f, 1.0f);
            float ab = std::clamp(std::sin((hue + 0.66f) * 6.28f) * 0.3f + 0.3f, 0.0f, 1.0f);
            setColor(ar, ag, ab, 0.08f);
            float lineW = 20.0f + std::sin(g_time + i) * 10.0f;
            drawRect(ax - lineW / 2, FOOTER_H + canvasH * 0.6f, lineW, canvasH * 0.35f);
        }
    }

    // Particles
    for (auto& p : g_particles) {
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.life -= dt;
        float alpha = std::min(1.0f, p.life) * p.a;
        setColor(p.r, p.g, p.b, alpha);
        drawRect(p.x - p.size/2, p.y - p.size/2, p.size, p.size);
    }
    g_particles.erase(
        std::remove_if(g_particles.begin(), g_particles.end(),
                       [](const Particle& p){ return p.life <= 0.0f || p.y < 0.0f; }),
        g_particles.end());

    // Generation frontier glow
    auto phase = g_engine.phase();
    if (phase >= BaseX::GenPhase::HEIGHTMAP && phase <= BaseX::GenPhase::WEATHER) {
        float frontY = FOOTER_H + canvasH * (1.0f - g_engine.phasePct() / 100.0f);
        setColor(0.20f, 0.80f, 0.33f, 0.3f);
        glEnable(GL_LINE_STIPPLE);
        glLineStipple(1, 0xAAAA);
        glBegin(GL_LINES);
        glVertex2f(0, frontY);
        glVertex2f(canvasW, frontY);
        glEnd();
        glDisable(GL_LINE_STIPPLE);

        // Glow band
        setColor(0.20f, 0.80f, 0.33f, 0.05f);
        drawRect(0, frontY - 20, canvasW, 40);
    }
}

// ══════════════════════════════════════════════════════════════════════
// INPUT HANDLING
// ══════════════════════════════════════════════════════════════════════

static void charCallback(GLFWwindow* /*window*/, unsigned int codepoint) {
    if (codepoint >= 32 && codepoint < 127 && g_promptText.size() < 200) {
        g_promptText += (char)codepoint;
    }
}

static void mouseButtonCallback(GLFWwindow* w, int button, int action, int) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;
    bool isGenerating = g_engine.phase() != BaseX::GenPhase::IDLE &&
                        g_engine.phase() != BaseX::GenPhase::COMPLETE;
    if (isGenerating) return;

    double mx, my; glfwGetCursorPos(w, &mx, &my);
    int ww, wh; glfwGetFramebufferSize(w, &ww, &wh);
    float cx = (float)mx, cy = (float)(wh - my);

    for (const auto& hit : g_tagHits) {
        if (cx >= hit.x0 && cx <= hit.x1 && cy >= hit.y0 && cy <= hit.y1) {
            // Tab switching
            if (hit.keyword.rfind("__tab_", 0) == 0) {
                g_structTabIdx = std::atoi(hit.keyword.c_str() + 6);
                break;
            }
            // Normal keyword: toggle in/out of prompt
            std::string low = g_promptText;
            std::transform(low.begin(), low.end(), low.begin(),
                [](unsigned char c){ return (char)std::tolower(c); });
            size_t pos = low.find(hit.keyword);
            if (pos != std::string::npos) {
                size_t end = pos + hit.keyword.size();
                if (end < g_promptText.size() && g_promptText[end] == ' ') end++;
                else if (pos > 0 && g_promptText[pos-1] == ' ') pos--;
                g_promptText.erase(pos, end - pos);
            } else {
                if (!g_promptText.empty() && g_promptText.back() != ' ')
                    g_promptText += ' ';
                g_promptText += hit.keyword;
            }
            break;
        }
    }
}

static void keyCallback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    bool isGenerating = g_engine.phase() != BaseX::GenPhase::IDLE &&
                        g_engine.phase() != BaseX::GenPhase::COMPLETE;

    if (key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        return;
    }

    if (key == GLFW_KEY_BACKSPACE && !g_promptText.empty()) {
        g_promptText.pop_back();
        return;
    }

    // Preset keys 1-9, 0, -, =
    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9 && !isGenerating) {
        g_promptText = BaseX::WorldPromptEngine::preset(key - GLFW_KEY_1);
        return;
    }
    if (key == GLFW_KEY_0 && !isGenerating) {
        g_promptText = BaseX::WorldPromptEngine::preset(9);
        return;
    }
    if (key == GLFW_KEY_MINUS && !isGenerating) {
        g_promptText = BaseX::WorldPromptEngine::preset(10);
        return;
    }
    if (key == GLFW_KEY_EQUAL && !isGenerating) {
        g_promptText = BaseX::WorldPromptEngine::preset(11);
        return;
    }

    // Enter → generate
    if (key == GLFW_KEY_ENTER && !isGenerating && !g_promptText.empty()) {
        g_engine.beginGeneration(g_promptText);
        return;
    }

    // R → reset
    if (key == GLFW_KEY_R && !isGenerating) {
        g_engine.reset();
        g_promptText.clear();
        g_particles.clear();
        return;
    }
}

// ══════════════════════════════════════════════════════════════════════
// MAIN
// ══════════════════════════════════════════════════════════════════════

int main() {
    std::printf("SBX-001: World Prompt Engine — Base-X Engine Demo\n");
    std::printf("Controls: 1-9,0,-,= presets | Click tags | Enter = generate | R = reset | ESC = quit\n\n");

    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialise GLFW\n");
        return 1;
    }

    GLFWwindow* window = glfwCreateWindow(WIN_W, WIN_H,
        "BASE-X ENGINE | SBX-001: World Prompt Engine", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync
    glfwSetCharCallback(window, charCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);

    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;
        g_time += dt;

        // Advance generation
        bool wasGenerating = g_engine.phase() != BaseX::GenPhase::IDLE &&
                             g_engine.phase() != BaseX::GenPhase::COMPLETE;
        g_engine.tick(dt);
        bool isComplete = g_engine.phase() == BaseX::GenPhase::COMPLETE;

        // Update session totals when generation just completed
        if (wasGenerating && isComplete) {
            g_totalTiles  += g_engine.stats().tileCount;
            g_totalStructs += g_engine.stats().structCount;
        }

        // Get window size
        int winW, winH;
        glfwGetFramebufferSize(window, &winW, &winH);
        float canvasW = winW - SIDEBAR_W;
        float canvasH = winH - HEADER_H - FOOTER_H;

        // Setup 2D projection (origin at bottom-left)
        glViewport(0, 0, winW, winH);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, winW, 0, winH, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glClearColor(0.03f, 0.04f, 0.02f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Clip canvas to left side
        glEnable(GL_SCISSOR_TEST);
        glScissor(0, (int)FOOTER_H, (int)canvasW, (int)canvasH);

        // Draw world
        drawSky((int)canvasW, (int)canvasH);

        bool showInventory = g_engine.phase() == BaseX::GenPhase::IDLE ||
                             g_engine.phase() == BaseX::GenPhase::COMPLETE;

        if (g_engine.heightmap().empty() && g_engine.phase() == BaseX::GenPhase::IDLE) {
            drawIdleScreen(canvasW, canvasH);
        } else {
            drawIsometricWorld(canvasW, canvasH);
            spawnWeatherParticles(canvasW, canvasH);
            drawWeatherEffects(canvasW, canvasH, dt);
            // Show tag inventory overlay after generation too
            if (showInventory && !g_engine.heightmap().empty()) {
                drawIdleScreen(canvasW, canvasH);
            }
        }

        if (!showInventory) g_tagHits.clear();

        glDisable(GL_SCISSOR_TEST);

        // HUD
        drawHeader(winW);
        drawFooter(winW);
        drawSidebar(winW, winH);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    std::printf("Session: %d worlds generated, %d total tiles, %d structures\n",
                g_engine.genCount(), g_totalTiles, g_totalStructs);
    return 0;
}
