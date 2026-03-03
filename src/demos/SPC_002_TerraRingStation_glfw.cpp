/**
 * SPC-002: Terra Ring Station — Rotating Space Station Simulator
 * Demonstrates artificial gravity via centripetal acceleration, Coriolis
 * effects, and precision comparison across DRO numerical bases.
 *
 * Ported from the HTML/Canvas demo to C++ / GLFW / legacy GL.
 *
 * Features:
 *   - External view of a rotating multi-ring station
 *   - Habitat view with artificial gravity, ball drops, person walk/jump
 *   - Coriolis deflection visualisation
 *   - Precision comparison bar chart across Base-16..64
 *   - Ring cross-section & gravity gradient HUD
 *   - Adjustable rotation speed, ring radius, time scale
 *
 * Controls:
 *   1 / 2 / 3      Select deck (Inner / Mid / Outer)
 *   Q / W           Decrease / Increase rotation speed
 *   A / S           Decrease / Increase ring radius
 *   Z / X           Decrease / Increase time scale
 *   D               Drop ball
 *   T               Throw ball
 *   J               Jump test
 *   K               Walk test
 *   E               Emergency stop
 *   R               Reset
 *   4-9             Select precision base (16/24/32/48/56/64)
 *   ESC             Quit
 */

#include <GLFW/glfw3.h>
#include <BaseX/DRO.h>
#include <BaseX/GLText.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <numeric>
#include <chrono>

static constexpr float PI  = 3.14159265f;
static constexpr float TAU = 6.28318530f;

// ══════════════════════════════════════════════════════════════════════
// COLOURS (from the HTML CSS variables, mapped to 0-1 floats)
// ══════════════════════════════════════════════════════════════════════

static constexpr float COL_BG[]        = {0.008f, 0.008f, 0.016f};
static constexpr float COL_PANEL[]     = {0.020f, 0.031f, 0.059f};
static constexpr float COL_RING_OUTER[]= {0.267f, 0.867f, 0.400f};  // #44dd66
static constexpr float COL_RING_MID[]  = {0.267f, 0.667f, 0.533f};  // #44aa88
static constexpr float COL_RING_INNER[]= {0.267f, 0.533f, 0.667f};  // #4488aa
static constexpr float COL_EARTH[]     = {0.267f, 0.533f, 0.867f};  // #4488dd
static constexpr float COL_GRAVITY[]   = {0.000f, 0.867f, 0.533f};  // #00dd88
static constexpr float COL_WARN[]      = {0.867f, 0.667f, 0.000f};  // #ddaa00
static constexpr float COL_DANGER[]    = {0.867f, 0.200f, 0.267f};  // #dd3344
static constexpr float COL_CYAN[]      = {0.000f, 0.800f, 0.867f};  // #00ccdd
static constexpr float COL_TEXT[]      = {0.816f, 0.847f, 0.910f};  // #d0d8e8
static constexpr float COL_DIM[]       = {0.333f, 0.400f, 0.533f};  // #556688
static constexpr float COL_BALL[]      = {1.000f, 0.400f, 0.267f};  // #ff6644
static constexpr float COL_HUB[]       = {0.267f, 0.333f, 0.400f};  // #445566
static constexpr float COL_BLDG[]      = {0.133f, 0.200f, 0.267f};  // #223344
static constexpr float COL_BODY[]      = {0.267f, 0.467f, 0.667f};  // #4477aa
static constexpr float COL_LEGS[]      = {0.200f, 0.333f, 0.467f};  // #335577
static constexpr float COL_SKIN[]      = {0.867f, 0.800f, 0.733f};  // #ddccbb

// ══════════════════════════════════════════════════════════════════════
// DECK DEFINITIONS
// ══════════════════════════════════════════════════════════════════════

struct DeckInfo {
    const char* name;
    float radiusFraction;   // fraction of full ring radius
    float colR, colG, colB;
};

static const DeckInfo DECKS[] = {
    {"Inner Ring", 0.33f, COL_RING_INNER[0], COL_RING_INNER[1], COL_RING_INNER[2]},
    {"Mid Ring",   0.66f, COL_RING_MID[0],   COL_RING_MID[1],   COL_RING_MID[2]},
    {"Outer Ring", 1.00f, COL_RING_OUTER[0],  COL_RING_OUTER[1],  COL_RING_OUTER[2]},
};
static constexpr int NUM_DECKS = 3;

// ══════════════════════════════════════════════════════════════════════
// BASE PRECISION MAP
// ══════════════════════════════════════════════════════════════════════

struct BasePrecision {
    int base;
    float errorFactor;
};

static const BasePrecision BASE_PREC[] = {
    {16, 0.02f},
    {24, 0.005f},
    {32, 0.001f},
    {48, 0.0001f},
    {56, 0.00005f},
    {64, 0.00001f},
};
static constexpr int NUM_BASES = 6;

static float basePrecisionLookup(int base) {
    for (int i = 0; i < NUM_BASES; ++i)
        if (BASE_PREC[i].base == base) return BASE_PREC[i].errorFactor;
    return 0.001f;
}

// ══════════════════════════════════════════════════════════════════════
// PHYSICS
// ══════════════════════════════════════════════════════════════════════

static std::mt19937 g_rng{42};

static float randFloat() {
    std::uniform_real_distribution<float> d(0.0f, 1.0f);
    return d(g_rng);
}

// a = omega^2 * r; g = a / 9.81
static float calcGravity(float radius, float rpm, float precision) {
    float omega = TAU * rpm / 60.0f;
    float accel = omega * omega * radius;
    float g = accel / 9.81f;
    float error = (randFloat() - 0.5f) * precision * 2.0f;
    return std::max(0.0f, g * (1.0f + error));
}

static float calcGravityExact(float radius, float rpm) {
    float omega = TAU * rpm / 60.0f;
    return omega * omega * radius / 9.81f;
}

// Coriolis acceleration = 2 * omega * v
static float calcCoriolis(float velocity, float rpm) {
    float omega = TAU * rpm / 60.0f;
    return 2.0f * omega * velocity;
}

// ══════════════════════════════════════════════════════════════════════
// SIMULATION STATE
// ══════════════════════════════════════════════════════════════════════

struct TrailPt { float x, y; };

struct Ball {
    float x, y, vx, vy, radius;
    std::vector<TrailPt> trail;
};

struct Person {
    float x, y, vx, vy;
    float width, height;
    bool onGround;
};

static struct {
    int   deck;       // 0..2
    int   baseHigh;   // 16,24,32,48,56,64
    float rotSpeed;   // RPM
    float ringRadius; // metres
    int   timeScale;
} g_settings = {2, 48, 1.5f, 450.0f, 5};

static float  g_stationAngle = 0;
static float  g_time          = 0;
static std::vector<Ball> g_balls;
static Person g_person;

// Viewport dimensions (computed each frame)
static int g_winW = 1280, g_winH = 720;

// Sub-viewport rectangles (pixels, origin bottom-left for GL)
struct Rect { int x, y, w, h; };
static Rect g_vpStation;
static Rect g_vpHabitat;
static Rect g_vpDetail;

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

static void drawCircle(float cx, float cy, float r, int segs = 40) {
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= segs; ++i) {
        float a = TAU * i / segs;
        glVertex2f(cx + cosf(a) * r, cy + sinf(a) * r);
    }
    glEnd();
}

static void drawCircleOutline(float cx, float cy, float r, int segs = 60) {
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segs; ++i) {
        float a = TAU * i / segs;
        glVertex2f(cx + cosf(a) * r, cy + sinf(a) * r);
    }
    glEnd();
}

static void drawLine(float x1, float y1, float x2, float y2) {
    glBegin(GL_LINES);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
}

static void drawDashedCircle(float cx, float cy, float r, int dashes = 30) {
    for (int i = 0; i < dashes; ++i) {
        float a0 = TAU * i / dashes;
        float a1 = TAU * (i + 0.5f) / dashes;
        glBegin(GL_LINES);
        glVertex2f(cx + cosf(a0) * r, cy + sinf(a0) * r);
        glVertex2f(cx + cosf(a1) * r, cy + sinf(a1) * r);
        glEnd();
    }
}

// Set orthographic projection matching a sub-viewport in pixel coords
// (0,0) at top-left, (w,h) at bottom-right — matching the Canvas convention.
static void setOrthoTopLeft(const Rect& vp) {
    glViewport(vp.x, vp.y, vp.w, vp.h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, vp.w, vp.h, 0, -1, 1);  // top-left origin
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

// ══════════════════════════════════════════════════════════════════════
// SPAWN HELPERS
// ══════════════════════════════════════════════════════════════════════

static void spawnBall(float vx = 0, float vy = 0) {
    Ball b;
    b.x = g_vpHabitat.w / 2.0f + (randFloat() - 0.5f) * 100.0f;
    b.y = 100.0f;
    b.vx = vx;
    b.vy = vy;
    b.radius = 10.0f;
    g_balls.push_back(b);
}

static void spawnPerson() {
    g_person.x = g_vpHabitat.w / 2.0f;
    g_person.y = (float)g_vpHabitat.h - 150.0f;
    g_person.vx = 0;
    g_person.vy = 0;
    g_person.onGround = true;
    g_person.width = 20.0f;
    g_person.height = 50.0f;
}

// ══════════════════════════════════════════════════════════════════════
// UPDATE
// ══════════════════════════════════════════════════════════════════════

static void update(float /*rawDt*/) {
    float dt = 0.016f * g_settings.timeScale;
    g_time += dt;

    float rpm = g_settings.rotSpeed;
    g_stationAngle += (rpm / 60.0f) * 360.0f * dt;

    const DeckInfo& deck = DECKS[g_settings.deck];
    float deckRadius = deck.radiusFraction * g_settings.ringRadius;
    float gravity = calcGravity(deckRadius, rpm, basePrecisionLookup(g_settings.baseHigh));
    float gravityAccel = gravity * 9.81f;
    float omega = TAU * rpm / 60.0f;

    float groundY = (float)g_vpHabitat.h - 100.0f;

    // Balls
    for (auto& b : g_balls) {
        b.trail.push_back({b.x, b.y});
        if (b.trail.size() > 50) b.trail.erase(b.trail.begin());

        b.vy += gravityAccel * dt * 10.0f;
        float coriolisX = 2.0f * omega * b.vy * dt * 50.0f;
        b.vx += coriolisX;

        b.x += b.vx * dt * 60.0f;
        b.y += b.vy * dt * 60.0f;

        if (b.y + b.radius > groundY) {
            b.y = groundY - b.radius;
            b.vy *= -0.6f;
            b.vx *= 0.9f;
        }
        if (b.x < b.radius || b.x > g_vpHabitat.w - b.radius) {
            b.vx *= -0.8f;
            b.x = std::max(b.radius, std::min((float)g_vpHabitat.w - b.radius, b.x));
        }
    }

    // Person
    if (!g_person.onGround) {
        g_person.vy += gravityAccel * dt * 10.0f;
        float coriolisX = 2.0f * omega * g_person.vy * dt * 50.0f;
        g_person.vx += coriolisX;
    }
    g_person.x += g_person.vx * dt * 60.0f;
    g_person.y += g_person.vy * dt * 60.0f;

    if (g_person.y + g_person.height > groundY) {
        g_person.y = groundY - g_person.height;
        g_person.vy = 0;
        g_person.vx *= 0.8f;
        g_person.onGround = true;
    }

    // Remove balls that fell off-screen
    g_balls.erase(std::remove_if(g_balls.begin(), g_balls.end(),
        [](const Ball& b){ return b.y > 2000.0f; }), g_balls.end());
}

// ══════════════════════════════════════════════════════════════════════
// RENDER — External Station View (top-left quadrant)
// ══════════════════════════════════════════════════════════════════════

static void renderStation() {
    setOrthoTopLeft(g_vpStation);

    float w = (float)g_vpStation.w;
    float h = (float)g_vpStation.h;
    float cx = w * 0.5f;
    float cy = h * 0.5f;

    // Background
    setColor(COL_BG[0], COL_BG[1], COL_BG[2]);
    drawRect(0, 0, w, h);

    // Stars
    g_rng.seed(12345);
    setColor(1, 1, 1);
    for (int i = 0; i < 80; ++i) {
        float sx = randFloat() * w;
        float sy = randFloat() * h;
        float sz = 0.5f + fmodf(i * 1.7f, 2.0f);
        drawCircle(sx, sy, sz, 6);
    }
    g_rng.seed((unsigned)std::chrono::steady_clock::now().time_since_epoch().count());

    // Earth
    setColor(COL_EARTH[0], COL_EARTH[1], COL_EARTH[2]);
    drawCircle(w - 100, h - 80, 60, 40);

    // Station — rotate around centre
    glPushMatrix();
    glTranslatef(cx, cy, 0);
    glRotatef(g_stationAngle, 0, 0, 1);

    float scale = std::min(w, h) * 0.35f / g_settings.ringRadius;

    // Outer ring
    setColor(COL_RING_OUTER[0], COL_RING_OUTER[1], COL_RING_OUTER[2]);
    glLineWidth(6);
    drawCircleOutline(0, 0, g_settings.ringRadius * scale, 80);

    // Mid ring
    setColor(COL_RING_MID[0], COL_RING_MID[1], COL_RING_MID[2]);
    glLineWidth(4);
    drawCircleOutline(0, 0, g_settings.ringRadius * 0.66f * scale, 60);

    // Inner ring
    setColor(COL_RING_INNER[0], COL_RING_INNER[1], COL_RING_INNER[2]);
    glLineWidth(3);
    drawCircleOutline(0, 0, g_settings.ringRadius * 0.33f * scale, 50);

    // Spokes
    setColor(COL_DIM[0], COL_DIM[1], COL_DIM[2]);
    glLineWidth(2);
    for (int i = 0; i < 6; ++i) {
        float a = i * PI / 3.0f;
        drawLine(0, 0,
                 cosf(a) * g_settings.ringRadius * scale,
                 sinf(a) * g_settings.ringRadius * scale);
    }

    // Hub
    setColor(COL_HUB[0], COL_HUB[1], COL_HUB[2]);
    drawCircle(0, 0, 15, 20);

    // Selected-deck highlight
    const DeckInfo& dk = DECKS[g_settings.deck];
    if (dk.radiusFraction > 0) {
        setColor(1, 1, 1, 0.8f);
        glLineWidth(1);
        drawDashedCircle(0, 0, dk.radiusFraction * g_settings.ringRadius * scale, 40);
    }

    glPopMatrix();

    // Labels
    setColor(COL_CYAN[0], COL_CYAN[1], COL_CYAN[2]);
    char buf[64];
    snprintf(buf, sizeof(buf), "Rotation: %.1f RPM", g_settings.rotSpeed);
    gltext::draw(buf, 10, 20, 1.5f);
    snprintf(buf, sizeof(buf), "Angle: %.0f deg", fmodf(g_stationAngle, 360.0f));
    gltext::draw(buf, 10, 38, 1.5f);

    // Viewport label
    setColor(0, 0, 0, 0.85f);
    float lw = 220, lh = 18;
    drawRect(cx - lw / 2, 6, lw, lh);
    setColor(COL_CYAN[0], COL_CYAN[1], COL_CYAN[2]);
    gltext::draw("EXTERNAL VIEW - Station Rotation", cx - 105, 18, 1.2f);
}

// ══════════════════════════════════════════════════════════════════════
// RENDER — Habitat View (top-right quadrant)
// ══════════════════════════════════════════════════════════════════════

static void renderHabitat() {
    setOrthoTopLeft(g_vpHabitat);

    float w = (float)g_vpHabitat.w;
    float h = (float)g_vpHabitat.h;
    float groundY = h - 100.0f;

    // Sky gradient (approximate with vertical strips)
    for (int i = 0; i < 20; ++i) {
        float t = (float)i / 20.0f;
        float r = 0.10f + t * 0.05f;
        float g = 0.125f + t * 0.06f;
        float b = 0.19f + t * 0.07f;
        setColor(r, g, b);
        float stripH = groundY / 20.0f;
        drawRect(0, i * stripH, w, stripH + 1);
    }

    // Ground
    setColor(0.08f, 0.12f, 0.08f);
    drawRect(0, groundY, w, h - groundY);

    // Curved horizon line
    setColor(0.4f, 0.6f, 0.8f, 0.3f);
    glLineWidth(2);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= 40; ++i) {
        float t = (float)i / 40.0f;
        float x = t * w;
        float y = groundY - 20.0f * sinf(PI * t);
        glVertex2f(x, y);
    }
    glEnd();

    // Habitat curve lines (opposite side of ring visible above)
    setColor(COL_DIM[0], COL_DIM[1], COL_DIM[2], 0.15f);
    glLineWidth(1);
    for (int i = 0; i < 5; ++i) {
        float yy = 30.0f + i * 25.0f;
        glBegin(GL_LINE_STRIP);
        for (int j = 0; j <= 40; ++j) {
            float t = (float)j / 40.0f;
            float x = t * w;
            float curveY = yy + 15.0f * sinf(PI * t);
            glVertex2f(x, curveY);
        }
        glEnd();
    }

    // Buildings
    setColor(COL_BLDG[0], COL_BLDG[1], COL_BLDG[2]);
    for (int i = 0; i < 10; ++i) {
        float x = i * 100.0f + 30.0f;
        float bh = 30.0f + (float)((i * 17) % 50);
        drawRect(x, groundY - bh, 40, bh);
    }

    // Balls
    for (auto& b : g_balls) {
        // Trail
        if (b.trail.size() > 1) {
            setColor(COL_BALL[0], COL_BALL[1], COL_BALL[2], 0.25f);
            glLineWidth(2);
            glBegin(GL_LINE_STRIP);
            for (auto& p : b.trail) glVertex2f(p.x, p.y);
            glEnd();
        }
        // Body
        setColor(COL_BALL[0], COL_BALL[1], COL_BALL[2]);
        drawCircle(b.x, b.y, b.radius, 16);
        // Highlight
        setColor(1, 1, 1, 0.25f);
        drawCircle(b.x - 3, b.y - 3, b.radius * 0.4f, 8);
    }

    // Person
    // Head
    setColor(COL_SKIN[0], COL_SKIN[1], COL_SKIN[2]);
    drawCircle(g_person.x, g_person.y, 10, 16);
    // Body
    setColor(COL_BODY[0], COL_BODY[1], COL_BODY[2]);
    drawRect(g_person.x - 10, g_person.y + 10, 20, 30);
    // Legs
    setColor(COL_LEGS[0], COL_LEGS[1], COL_LEGS[2]);
    drawRect(g_person.x - 8, g_person.y + 40, 6, 20);
    drawRect(g_person.x + 2, g_person.y + 40, 6, 20);

    // Gravity vector arrow on person
    const DeckInfo& dk = DECKS[g_settings.deck];
    float grav = calcGravityExact(dk.radiusFraction * g_settings.ringRadius, g_settings.rotSpeed);
    float arrowLen = grav * 40.0f;

    setColor(COL_GRAVITY[0], COL_GRAVITY[1], COL_GRAVITY[2]);
    glLineWidth(3);
    drawLine(g_person.x, g_person.y + 30, g_person.x, g_person.y + 30 + arrowLen);
    // Arrowhead
    glBegin(GL_TRIANGLES);
    glVertex2f(g_person.x,     g_person.y + 30 + arrowLen + 10);
    glVertex2f(g_person.x - 5, g_person.y + 30 + arrowLen);
    glVertex2f(g_person.x + 5, g_person.y + 30 + arrowLen);
    glEnd();

    // Gravity HUD (bottom-left)
    setColor(0, 0, 0, 0.8f);
    drawRect(10, h - 75, 120, 60);
    setColor(COL_CYAN[0], COL_CYAN[1], COL_CYAN[2], 0.5f);
    glLineWidth(1);
    glBegin(GL_LINE_LOOP);
    glVertex2f(10, h - 75); glVertex2f(130, h - 75);
    glVertex2f(130, h - 15); glVertex2f(10, h - 15);
    glEnd();

    float dispG = calcGravityExact(dk.radiusFraction * g_settings.ringRadius, g_settings.rotSpeed);
    char buf[32];
    if (dispG >= 0.9f && dispG <= 1.1f) setColor(COL_GRAVITY[0], COL_GRAVITY[1], COL_GRAVITY[2]);
    else if (dispG >= 0.5f) setColor(COL_WARN[0], COL_WARN[1], COL_WARN[2]);
    else setColor(COL_DANGER[0], COL_DANGER[1], COL_DANGER[2]);
    snprintf(buf, sizeof(buf), "%.2f G", dispG);
    gltext::draw(buf, 30, h - 45, 2.5f);
    setColor(COL_DIM[0], COL_DIM[1], COL_DIM[2]);
    gltext::draw("Local Gravity", 30, h - 25, 1.2f);

    // Coriolis mini indicator (top-right)
    setColor(0, 0, 0, 0.8f);
    drawRect(w - 110, 40, 100, 100);
    float omega = TAU * g_settings.rotSpeed / 60.0f;
    float deflection = std::min(30.0f, omega * 50.0f);

    float ccx = w - 60, ccy = 90;
    // straight reference
    setColor(COL_DIM[0], COL_DIM[1], COL_DIM[2], 0.4f);
    glLineWidth(1);
    drawLine(ccx, 135, ccx, 50);
    // curved path
    setColor(COL_BALL[0], COL_BALL[1], COL_BALL[2]);
    glLineWidth(2);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= 20; ++i) {
        float t = (float)i / 20.0f;
        float px = ccx + deflection * sinf(PI * t);
        float py = 135.0f - t * 85.0f;
        glVertex2f(px, py);
    }
    glEnd();
    // Arrowhead
    {
        float tipX = ccx + deflection * sinf(PI * 0.0f); // top of path
        // actually the tip is at t=1
        float endX = ccx + deflection * sinf(PI);
        float endY = 50.0f;
        glBegin(GL_TRIANGLES);
        glVertex2f(endX, endY - 5);
        glVertex2f(endX - 4, endY + 5);
        glVertex2f(endX + 4, endY + 5);
        glEnd();
    }
    setColor(COL_DIM[0], COL_DIM[1], COL_DIM[2]);
    gltext::draw("Coriolis", w - 95, 52, 1.0f);

    // Viewport label
    setColor(0, 0, 0, 0.85f);
    float lw2 = 250, lh2 = 18;
    drawRect(w / 2 - lw2 / 2, 6, lw2, lh2);
    setColor(COL_GRAVITY[0], COL_GRAVITY[1], COL_GRAVITY[2]);
    gltext::draw("HABITAT VIEW - Artificial Gravity", w / 2 - 115, 18, 1.2f);
}

// ══════════════════════════════════════════════════════════════════════
// RENDER — Precision Comparison (bottom half)
// ══════════════════════════════════════════════════════════════════════

static void renderDetail() {
    setOrthoTopLeft(g_vpDetail);

    float w = (float)g_vpDetail.w;
    float h = (float)g_vpDetail.h;

    setColor(0.04f, 0.04f, 0.063f);
    drawRect(0, 0, w, h);

    const DeckInfo& dk = DECKS[g_settings.deck];
    float deckRadius = dk.radiusFraction * g_settings.ringRadius;
    float trueG = calcGravityExact(deckRadius, g_settings.rotSpeed);

    int bases[] = {16, 24, 32, 48, 64};
    int numB = 5;
    float barW = (w - 100.0f) / numB;

    float centerY = h / 2.0f;
    float scale = (h - 60.0f) / 0.2f;

    for (int i = 0; i < numB; ++i) {
        int base = bases[i];
        float prec = basePrecisionLookup(base);
        float x = 50.0f + i * barW + barW / 2.0f;

        // Compute sample variance
        float mn = 1e9f, mx = -1e9f;
        for (int j = 0; j < 20; ++j) {
            float s = calcGravity(deckRadius, g_settings.rotSpeed, prec);
            mn = std::min(mn, s);
            mx = std::max(mx, s);
        }

        float minY = centerY - (mn - trueG) * scale;
        float maxY = centerY - (mx - trueG) * scale;
        float top = std::min(minY, maxY);
        float bot = std::max(minY, maxY);
        float barH = (bot - top) + 2.0f;

        // Error bar
        if (base >= 48)      setColor(COL_GRAVITY[0], COL_GRAVITY[1], COL_GRAVITY[2], 0.25f);
        else if (base >= 32) setColor(COL_WARN[0], COL_WARN[1], COL_WARN[2], 0.25f);
        else                 setColor(COL_DANGER[0], COL_DANGER[1], COL_DANGER[2], 0.25f);
        drawRect(x - 15, top, 30, barH);

        // True value line
        setColor(COL_GRAVITY[0], COL_GRAVITY[1], COL_GRAVITY[2]);
        glLineWidth(2);
        drawLine(x - 20, centerY, x + 20, centerY);

        // Base label
        char buf[32];
        if (base == g_settings.baseHigh) setColor(COL_CYAN[0], COL_CYAN[1], COL_CYAN[2]);
        else setColor(0.53f, 0.53f, 0.53f);
        snprintf(buf, sizeof(buf), "B-%d", base);
        gltext::draw(buf, x - 12, h - 10, 1.2f);

        // Error %
        float errPct = (trueG > 0.001f) ? ((mx - mn) / trueG * 100.0f) : 0.0f;
        if (base >= 48)      setColor(COL_GRAVITY[0], COL_GRAVITY[1], COL_GRAVITY[2]);
        else if (base >= 32) setColor(COL_WARN[0], COL_WARN[1], COL_WARN[2]);
        else                 setColor(COL_DANGER[0], COL_DANGER[1], COL_DANGER[2]);
        snprintf(buf, sizeof(buf), "+/-%.2f%%", errPct);
        gltext::draw(buf, x - 20, 25, 1.1f);
    }

    // Title
    setColor(COL_DIM[0], COL_DIM[1], COL_DIM[2]);
    char tbuf[64];
    snprintf(tbuf, sizeof(tbuf), "True: %.3fG", trueG);
    gltext::draw(tbuf, 10, centerY + 4, 1.1f);

    // Viewport label
    setColor(0, 0, 0, 0.85f);
    float lw3 = 320, lh3 = 18;
    drawRect(w / 2 - lw3 / 2, 6, lw3, lh3);
    setColor(COL_WARN[0], COL_WARN[1], COL_WARN[2]);
    gltext::draw("PRECISION COMPARISON - Gravity Calculation Error", w / 2 - 155, 18, 1.2f);
}

// ══════════════════════════════════════════════════════════════════════
// RENDER — Right-side HUD panel (station stats, gradient, controls)
// ══════════════════════════════════════════════════════════════════════

static void renderHUD() {
    // Draw the HUD in a strip on the right side of the window
    int panelW = 230;
    Rect vp;
    vp.x = g_winW - panelW;
    vp.y = 0;
    vp.w = panelW;
    vp.h = g_winH;
    setOrthoTopLeft(vp);

    float w = (float)vp.w;
    float h = (float)vp.h;

    // Panel background
    setColor(COL_PANEL[0], COL_PANEL[1], COL_PANEL[2], 0.95f);
    drawRect(0, 0, w, h);

    float y = 15;
    auto sectionTitle = [&](const char* title) {
        setColor(COL_CYAN[0], COL_CYAN[1], COL_CYAN[2]);
        gltext::draw(title, 10, y, 1.2f);
        y += 5;
        setColor(COL_CYAN[0], COL_CYAN[1], COL_CYAN[2], 0.3f);
        drawLine(10, y, w - 10, y);
        y += 10;
    };

    char buf[80];

    // ── Deck Selector ──
    sectionTitle("DECK SELECTOR");
    for (int i = 0; i < NUM_DECKS; ++i) {
        bool sel = (i == g_settings.deck);
        if (sel) {
            setColor(DECKS[i].colR, DECKS[i].colG, DECKS[i].colB, 0.2f);
            drawRect(5, y - 2, w - 10, 22);
        }
        // Colour dot
        setColor(DECKS[i].colR, DECKS[i].colG, DECKS[i].colB);
        drawCircle(18, y + 8, 5, 10);

        // Name
        setColor(COL_TEXT[0], COL_TEXT[1], COL_TEXT[2]);
        gltext::draw(DECKS[i].name, 30, y + 12, 1.2f);

        // Gravity label
        float dg = calcGravityExact(DECKS[i].radiusFraction * g_settings.ringRadius, g_settings.rotSpeed);
        snprintf(buf, sizeof(buf), "%.2fG", dg);
        setColor(COL_DIM[0], COL_DIM[1], COL_DIM[2]);
        gltext::draw(buf, 140, y + 12, 1.1f);

        // Key hint
        snprintf(buf, sizeof(buf), "[%d]", i + 1);
        setColor(COL_CYAN[0], COL_CYAN[1], COL_CYAN[2], 0.5f);
        gltext::draw(buf, w - 30, y + 12, 1.0f);

        y += 24;
    }
    y += 6;

    // ── Precision Base ──
    sectionTitle("PRECISION BASE");
    float bx = 10;
    for (int i = 0; i < NUM_BASES; ++i) {
        bool sel = (BASE_PREC[i].base == g_settings.baseHigh);
        if (sel) {
            setColor(COL_CYAN[0], COL_CYAN[1], COL_CYAN[2], 0.25f);
            drawRect(bx - 2, y - 2, 32, 18);
        }
        setColor(sel ? COL_CYAN[0] : COL_DIM[0],
                 sel ? COL_CYAN[1] : COL_DIM[1],
                 sel ? COL_CYAN[2] : COL_DIM[2]);
        snprintf(buf, sizeof(buf), "B%d", BASE_PREC[i].base);
        gltext::draw(buf, bx + 2, y + 10, 1.1f);
        bx += 36;
        if (i == 2) { bx = 10; y += 20; }
    }
    y += 24;

    // ── Station Parameters ──
    sectionTitle("STATION PARAMETERS");

    auto paramRow = [&](const char* label, const char* valStr, const char* keys) {
        setColor(COL_DIM[0], COL_DIM[1], COL_DIM[2]);
        gltext::draw(label, 10, y + 10, 1.1f);
        setColor(COL_CYAN[0], COL_CYAN[1], COL_CYAN[2]);
        gltext::draw(valStr, 110, y + 10, 1.2f);
        setColor(COL_CYAN[0], COL_CYAN[1], COL_CYAN[2], 0.4f);
        gltext::draw(keys, w - 40, y + 10, 0.9f);
        y += 18;
    };

    snprintf(buf, sizeof(buf), "%.1f RPM", g_settings.rotSpeed);
    paramRow("Rot Speed", buf, "[Q/W]");
    snprintf(buf, sizeof(buf), "%.0f m", g_settings.ringRadius);
    paramRow("Radius", buf, "[A/S]");
    snprintf(buf, sizeof(buf), "%dx", g_settings.timeScale);
    paramRow("Time Scale", buf, "[Z/X]");
    y += 6;

    // ── Station Stats ──
    sectionTitle("STATION STATS");

    auto statRow = [&](const char* key, const char* val) {
        setColor(COL_DIM[0], COL_DIM[1], COL_DIM[2]);
        gltext::draw(key, 10, y + 10, 1.1f);
        setColor(COL_CYAN[0], COL_CYAN[1], COL_CYAN[2]);
        gltext::draw(val, 120, y + 10, 1.1f);
        setColor(COL_DIM[0], COL_DIM[1], COL_DIM[2], 0.1f);
        drawLine(10, y + 14, w - 10, y + 14);
        y += 18;
    };

    float circumf = TAU * g_settings.ringRadius;
    float rimVel  = circumf * g_settings.rotSpeed / 60.0f;
    float dayLen  = (g_settings.rotSpeed > 0.001f) ? (60.0f / g_settings.rotSpeed) : 999.0f;

    snprintf(buf, sizeof(buf), "%.0f m", circumf);
    statRow("Circumference", buf);
    snprintf(buf, sizeof(buf), "%.1f m/s", rimVel);
    statRow("Rim Velocity", buf);
    snprintf(buf, sizeof(buf), "%.0f sec", dayLen);
    statRow("Day Length", buf);
    statRow("Population", "12,500");
    y += 6;

    // ── Gravity Gradient ──
    sectionTitle("GRAVITY GRADIENT");

    auto gradRow = [&](const char* label, float frac) {
        float gg = calcGravityExact(frac * g_settings.ringRadius, g_settings.rotSpeed);
        snprintf(buf, sizeof(buf), "%.2f G", gg);
        setColor(COL_DIM[0], COL_DIM[1], COL_DIM[2]);
        gltext::draw(label, 10, y + 10, 1.1f);
        if (gg >= 0.9f && gg <= 1.1f)  setColor(COL_GRAVITY[0], COL_GRAVITY[1], COL_GRAVITY[2]);
        else if (gg >= 0.5f)           setColor(COL_WARN[0], COL_WARN[1], COL_WARN[2]);
        else                           setColor(COL_DANGER[0], COL_DANGER[1], COL_DANGER[2]);
        gltext::draw(buf, 120, y + 10, 1.1f);
        y += 18;
    };

    gradRow("Hub (0m)", 0.0f);
    gradRow("Inner", 0.33f);
    gradRow("Mid",   0.66f);
    gradRow("Outer", 1.00f);
    y += 6;

    // ── Coriolis ──
    sectionTitle("CORIOLIS @ 1 m/s");
    float cor = calcCoriolis(1.0f, g_settings.rotSpeed);
    snprintf(buf, sizeof(buf), "%.4f m/s2", cor);
    setColor(COL_BALL[0], COL_BALL[1], COL_BALL[2]);
    gltext::draw(buf, 10, y + 10, 1.3f);
    y += 22;

    // ── Actions ──
    sectionTitle("ACTIONS");

    auto actionRow = [&](const char* label, const char* key) {
        setColor(COL_DIM[0], COL_DIM[1], COL_DIM[2]);
        gltext::draw(key, 10, y + 10, 1.0f);
        setColor(COL_TEXT[0], COL_TEXT[1], COL_TEXT[2]);
        gltext::draw(label, 40, y + 10, 1.1f);
        y += 16;
    };

    actionRow("Drop Ball",      "[D]");
    actionRow("Throw Ball",     "[T]");
    actionRow("Jump Test",      "[J]");
    actionRow("Walk Test",      "[K]");
    actionRow("Emergency Stop", "[E]");
    actionRow("Reset",          "[R]");
    y += 6;

    // ── Ring Cross-Section ──
    if (y + 200 < h) {
        sectionTitle("RING CROSS-SECTION");
        float rcx = w / 2.0f;
        float rcy = y + 80;
        float maxR = 70.0f;

        // Outer
        setColor(COL_RING_OUTER[0], COL_RING_OUTER[1], COL_RING_OUTER[2]);
        glLineWidth(6);
        drawCircleOutline(rcx, rcy, maxR, 50);
        // Mid
        setColor(COL_RING_MID[0], COL_RING_MID[1], COL_RING_MID[2]);
        glLineWidth(4);
        drawCircleOutline(rcx, rcy, maxR * 0.66f, 40);
        // Inner
        setColor(COL_RING_INNER[0], COL_RING_INNER[1], COL_RING_INNER[2]);
        glLineWidth(3);
        drawCircleOutline(rcx, rcy, maxR * 0.33f, 30);
        // Hub
        setColor(COL_HUB[0], COL_HUB[1], COL_HUB[2]);
        drawCircle(rcx, rcy, 8, 12);

        // Gravity arrows (outward)
        setColor(COL_GRAVITY[0], COL_GRAVITY[1], COL_GRAVITY[2], 0.5f);
        glLineWidth(2);
        for (int i = 0; i < 8; ++i) {
            float a = TAU * i / 8.0f;
            float x1 = rcx + cosf(a) * (maxR - 12);
            float y1 = rcy + sinf(a) * (maxR - 12);
            float x2 = rcx + cosf(a) * (maxR + 8);
            float y2 = rcy + sinf(a) * (maxR + 8);
            drawLine(x1, y1, x2, y2);
        }

        // Selected deck
        const DeckInfo& dk2 = DECKS[g_settings.deck];
        if (dk2.radiusFraction > 0) {
            setColor(1, 1, 1, 0.6f);
            glLineWidth(1);
            drawDashedCircle(rcx, rcy, maxR * dk2.radiusFraction, 24);
        }
    }

    // ── Physics Notes ──
    // (at the very bottom of the panel if there's room)
    float noteY = h - 80;
    if (noteY > y + 200) {
        setColor(COL_CYAN[0], COL_CYAN[1], COL_CYAN[2]);
        gltext::draw("PHYSICS NOTES", 10, noteY, 1.2f);
        noteY += 14;
        setColor(COL_DIM[0], COL_DIM[1], COL_DIM[2]);
        gltext::draw("a = w^2 * r = (2pi*f)^2 * r", 10, noteY, 1.0f); noteY += 12;
        gltext::draw("For 1G at 450m:", 10, noteY, 1.0f); noteY += 12;
        gltext::draw("w = sqrt(g/r) = 0.148 rad/s", 10, noteY, 1.0f); noteY += 12;
        gltext::draw("= 1.41 RPM", 10, noteY, 1.0f); noteY += 16;
        gltext::draw("Coriolis: a = 2w x v", 10, noteY, 1.0f);
    }
}

// ══════════════════════════════════════════════════════════════════════
// RENDER — Header bar (top)
// ══════════════════════════════════════════════════════════════════════

static void renderHeader() {
    int headerH = 40;
    int panelW = 230;
    Rect vp;
    vp.x = 0;
    vp.y = g_winH - headerH;  // GL origin is bottom-left
    vp.w = g_winW - panelW;
    vp.h = headerH;
    setOrthoTopLeft(vp);

    float w = (float)vp.w;
    float h = (float)vp.h;

    setColor(COL_PANEL[0], COL_PANEL[1], COL_PANEL[2], 0.9f);
    drawRect(0, 0, w, h);
    setColor(COL_CYAN[0], COL_CYAN[1], COL_CYAN[2], 0.2f);
    drawLine(0, h - 1, w, h - 1);

    // Logo
    setColor(COL_CYAN[0], COL_CYAN[1], COL_CYAN[2], 0.8f);
    drawRect(10, 8, 50, 20);
    setColor(1, 1, 1);
    gltext::draw("BX-X", 14, 22, 1.2f);

    setColor(COL_TEXT[0], COL_TEXT[1], COL_TEXT[2]);
    gltext::draw("SPC-002: TERRA RING STATION", 70, 24, 1.5f);

    // Header stats
    const DeckInfo& dk = DECKS[g_settings.deck];
    float grav = calcGravityExact(dk.radiusFraction * g_settings.ringRadius, g_settings.rotSpeed);
    char buf[64];

    float sx = w - 350;
    auto headerStat = [&](const char* label, const char* val, const float* col) {
        setColor(col[0], col[1], col[2]);
        gltext::draw(val, sx, 18, 1.8f);
        setColor(COL_DIM[0], COL_DIM[1], COL_DIM[2]);
        gltext::draw(label, sx, 30, 0.9f);
        sx += 90;
    };

    snprintf(buf, sizeof(buf), "%.2f", grav);
    const float* gravCol = (grav >= 0.9f && grav <= 1.1f) ? COL_GRAVITY
                         : (grav >= 0.5f) ? COL_WARN : COL_DANGER;
    headerStat("GRAVITY", buf, gravCol);

    snprintf(buf, sizeof(buf), "%.1f", g_settings.rotSpeed);
    headerStat("RPM", buf, COL_CYAN);

    snprintf(buf, sizeof(buf), "%.0f", g_settings.ringRadius);
    headerStat("RADIUS", buf, COL_CYAN);

    float cor = calcCoriolis(1.0f, g_settings.rotSpeed);
    snprintf(buf, sizeof(buf), "%.3f", cor);
    headerStat("CORIOLIS", buf, COL_BALL);
}

// ══════════════════════════════════════════════════════════════════════
// INPUT
// ══════════════════════════════════════════════════════════════════════

static void keyCallback(GLFWwindow* win, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;

    switch (key) {
    case GLFW_KEY_ESCAPE:
        glfwSetWindowShouldClose(win, GLFW_TRUE);
        break;

    // Deck selection
    case GLFW_KEY_1: g_settings.deck = 0; spawnPerson(); break;
    case GLFW_KEY_2: g_settings.deck = 1; spawnPerson(); break;
    case GLFW_KEY_3: g_settings.deck = 2; spawnPerson(); break;

    // Precision base selection  (keys 4-9 → bases 16,24,32,48,56,64)
    case GLFW_KEY_4: g_settings.baseHigh = 16; break;
    case GLFW_KEY_5: g_settings.baseHigh = 24; break;
    case GLFW_KEY_6: g_settings.baseHigh = 32; break;
    case GLFW_KEY_7: g_settings.baseHigh = 48; break;
    case GLFW_KEY_8: g_settings.baseHigh = 56; break;
    case GLFW_KEY_9: g_settings.baseHigh = 64; break;

    // Rotation speed
    case GLFW_KEY_Q:
        g_settings.rotSpeed = std::max(0.0f, g_settings.rotSpeed - 0.1f);
        break;
    case GLFW_KEY_W:
        g_settings.rotSpeed = std::min(3.0f, g_settings.rotSpeed + 0.1f);
        break;

    // Ring radius
    case GLFW_KEY_A:
        g_settings.ringRadius = std::max(100.0f, g_settings.ringRadius - 50.0f);
        break;
    case GLFW_KEY_S:
        g_settings.ringRadius = std::min(1000.0f, g_settings.ringRadius + 50.0f);
        break;

    // Time scale
    case GLFW_KEY_Z:
        g_settings.timeScale = std::max(1, g_settings.timeScale - 1);
        break;
    case GLFW_KEY_X:
        g_settings.timeScale = std::min(20, g_settings.timeScale + 1);
        break;

    // Actions
    case GLFW_KEY_D: spawnBall(0, 0); break;
    case GLFW_KEY_T: spawnBall((randFloat() - 0.5f) * 5.0f, -3.0f); break;
    case GLFW_KEY_J:
        if (g_person.onGround) {
            g_person.vy = -8.0f;
            g_person.onGround = false;
        }
        break;
    case GLFW_KEY_K:
        g_person.vx = 3.0f;
        break;
    case GLFW_KEY_E:  // Emergency stop
        g_settings.rotSpeed = 0;
        break;
    case GLFW_KEY_R:  // Reset
        g_balls.clear();
        g_settings.rotSpeed = 1.5f;
        spawnPerson();
        break;
    }
}

// ══════════════════════════════════════════════════════════════════════
// MAIN
// ══════════════════════════════════════════════════════════════════════

int main() {
    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_SAMPLES, 4);
    GLFWwindow* window = glfwCreateWindow(1280, 720,
        "BASE-X ENGINE | SPC-002: Terra Ring Station", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetKeyCallback(window, keyCallback);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(0x809D);  // GL_MULTISAMPLE
    glDisable(GL_DEPTH_TEST);

    // Seed RNG
    g_rng.seed((unsigned)std::chrono::steady_clock::now().time_since_epoch().count());

    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = std::min(0.05f, (float)(now - lastTime));
        lastTime = now;

        glfwPollEvents();

        glfwGetFramebufferSize(window, &g_winW, &g_winH);

        // Layout sub-viewports
        int headerH = 40;
        int panelW  = 230;
        int contentW = g_winW - panelW;
        int contentH = g_winH - headerH;

        int topH    = contentH * 2 / 3;
        int bottomH = contentH - topH;
        int halfW   = contentW / 2;

        // GL viewport origin is bottom-left
        g_vpStation  = {0,          bottomH,     halfW,    topH};
        g_vpHabitat  = {halfW,      bottomH,     contentW - halfW, topH};
        g_vpDetail   = {0,          0,           contentW, bottomH};

        // Compute person/ball spawns relative to habitat viewport
        // (only when viewport changes significantly)
        static int lastHabW = 0, lastHabH = 0;
        if (lastHabW == 0 || std::abs(g_vpHabitat.w - lastHabW) > 20 ||
            std::abs(g_vpHabitat.h - lastHabH) > 20) {
            lastHabW = g_vpHabitat.w;
            lastHabH = g_vpHabitat.h;
            spawnPerson();
        }

        update(dt);

        // Clear entire window
        glViewport(0, 0, g_winW, g_winH);
        glClearColor(COL_BG[0], COL_BG[1], COL_BG[2], 1);
        glClear(GL_COLOR_BUFFER_BIT);

        // Render sub-views
        renderStation();
        renderHabitat();
        renderDetail();
        renderHeader();
        renderHUD();

        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
