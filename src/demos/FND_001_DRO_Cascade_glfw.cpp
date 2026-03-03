/**
 * FND-001: DRO Precision Cascade — Base-X Engine
 * Converted from html_demos/FND-001_DRO_Precision_Cascade.html
 *
 * Hundreds of physics objects with DRO-assigned precision levels.
 * Objects near the mouse cursor get Base-64, far ones drop to Base-4.
 * Color = precision:  RED=64 YELLOW=48 GREEN=32 CYAN=24 BLUE=16 PURPLE=8 GRAY=4
 * Opacity = precision (dim = low, bright = high)
 *
 * Controls:  Mouse moves focus  |  SPACE spawn 50 more  |  C clear  |  ESC quit
 */

#include <GLFW/glfw3.h>
#include <BaseX/DRO.h>
#include <BaseX/GLText.h>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <algorithm>

static constexpr float PI = 3.14159265f;

// ── Object types ───────────────────────────────────────────────────
enum ObjType { PLAYER, ENEMY, PROJECTILE, DEBRIS, PARTICLE, PROP };

static const float TYPE_COLORS[][3] = {
    {0.0f,1.0f,0.5f}, // PLAYER green
    {1.0f,0.3f,0.3f}, // ENEMY red
    {1.0f,0.8f,0.2f}, // PROJECTILE yellow
    {0.6f,0.6f,0.6f}, // DEBRIS gray
    {0.4f,0.7f,1.0f}, // PARTICLE blue
    {0.8f,0.5f,0.2f}, // PROP brown
};

// ── Base colors ────────────────────────────────────────────────────
static void baseColor(int base, float out[3]) {
    switch (base) {
        case 64: out[0]=1;    out[1]=.15f; out[2]=.15f; break; // red
        case 48: out[0]=1;    out[1]=.8f;  out[2]=.15f; break; // yellow
        case 32: out[0]=.15f; out[1]=1;    out[2]=.3f;  break; // green
        case 24: out[0]=0;    out[1]=.85f; out[2]=.9f;  break; // cyan
        case 16: out[0]=.2f;  out[1]=.4f;  out[2]=1;    break; // blue
        case 8:  out[0]=.6f;  out[1]=.2f;  out[2]=.9f;  break; // purple
        default: out[0]=.45f; out[1]=.45f; out[2]=.45f; break; // gray
    }
}

static int quantizeBase(int b) {
    if (b >= 56) return 64;
    if (b >= 40) return 48;
    if (b >= 28) return 32;
    if (b >= 20) return 24;
    if (b >= 12) return 16;
    if (b >= 6)  return 8;
    return 4;
}

// ── Physics object ─────────────────────────────────────────────────
struct Obj {
    float x, y, vx, vy, radius;
    ObjType type;
    int base;
};

// ── Globals ────────────────────────────────────────────────────────
static std::vector<Obj> g_objs;
static float g_focusX = 400, g_focusY = 300;
static int   g_winW = 900, g_winH = 700;
static bool  g_showBases = true;
static BaseX::DROManager g_dro;
static int g_baseCounts[8] = {}; // 4,8,16,24,32,48,64, total
static int g_fps = 0;
static int g_scenario = 0;
static const char* SCENARIOS[] = {"Distance","Importance","Velocity","Interaction"};

static void spawn(int n) {
    for (int i = 0; i < n; i++) {
        Obj o{};
        o.x  = (float)(rand() % g_winW);
        o.y  = (float)(rand() % g_winH);
        o.vx = (rand() % 400 - 200) / 100.0f;
        o.vy = (rand() % 400 - 200) / 100.0f;
        o.radius = 3.0f + (rand() % 120) / 10.0f;
        int r = rand() % 100;
        if      (r < 2)  o.type = PLAYER;
        else if (r < 12) o.type = ENEMY;
        else if (r < 27) o.type = PROJECTILE;
        else if (r < 57) o.type = DEBRIS;
        else if (r < 90) o.type = PARTICLE;
        else              o.type = PROP;
        o.base = 32;
        g_objs.push_back(o);
    }
}

static int droBase(float dist, ObjType type) {
    // Distance-based
    int b;
    if      (dist <  50) b = 64;
    else if (dist < 100) b = 48;
    else if (dist < 200) b = 32;
    else if (dist < 350) b = 24;
    else if (dist < 500) b = 16;
    else if (dist < 700) b = 8;
    else                 b = 4;

    // Importance boost
    if (type == PLAYER)     b = std::min(64, b + 16);
    else if (type == ENEMY) b = std::min(64, b + 8);

    return quantizeBase(b);
}

static void update(float dt) {
    memset(g_baseCounts, 0, sizeof(g_baseCounts));

    for (auto& o : g_objs) {
        // Physics
        o.x += o.vx * 60.0f * dt;
        o.y += o.vy * 60.0f * dt;
        if (o.x < o.radius)          { o.x = o.radius;          o.vx = fabsf(o.vx); }
        if (o.x > g_winW - o.radius) { o.x = g_winW - o.radius; o.vx = -fabsf(o.vx); }
        if (o.y < o.radius)          { o.y = o.radius;          o.vy = fabsf(o.vy); }
        if (o.y > g_winH - o.radius) { o.y = g_winH - o.radius; o.vy = -fabsf(o.vy); }

        // DRO
        float dx = o.x - g_focusX, dy = o.y - g_focusY;
        float dist = sqrtf(dx*dx + dy*dy);
        o.base = droBase(dist, o.type);

        // Counts
        switch (o.base) {
            case 4:  g_baseCounts[0]++; break;
            case 8:  g_baseCounts[1]++; break;
            case 16: g_baseCounts[2]++; break;
            case 24: g_baseCounts[3]++; break;
            case 32: g_baseCounts[4]++; break;
            case 48: g_baseCounts[5]++; break;
            case 64: g_baseCounts[6]++; break;
        }
    }
    g_baseCounts[7] = (int)g_objs.size();
}

// ── Drawing helpers ────────────────────────────────────────────────
static void drawCircle(float cx, float cy, float r, int segs, bool fill) {
    if (fill) glBegin(GL_TRIANGLE_FAN); else glBegin(GL_LINE_LOOP);
    if (fill) glVertex2f(cx, cy);
    for (int i = 0; i <= segs; i++) {
        float a = 2.0f * PI * i / segs;
        glVertex2f(cx + cosf(a)*r, cy + sinf(a)*r);
    }
    glEnd();
}

static void drawRect(float x, float y, float w, float h) {
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x+w, y);
    glVertex2f(x+w, y+h); glVertex2f(x, y+h);
    glEnd();
}

static void render() {
    glViewport(0, 0, g_winW, g_winH);
    glClearColor(0.03f, 0.03f, 0.06f, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float m[16] = {};
    m[0] = 2.0f/g_winW; m[5] = 2.0f/g_winH; m[10] = -1; m[12] = -1; m[13] = -1; m[15] = 1;
    glLoadMatrixf(m);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Focus ring
    glColor4f(0, 0.87f, 0.93f, 0.15f);
    drawCircle(g_focusX, g_focusY, 50, 48, true);
    glColor4f(0, 0.87f, 0.93f, 0.08f);
    drawCircle(g_focusX, g_focusY, 100, 48, true);
    drawCircle(g_focusX, g_focusY, 200, 64, true);
    glColor4f(0, 0.87f, 0.93f, 0.3f);
    glLineWidth(1);
    drawCircle(g_focusX, g_focusY, 50, 48, false);
    drawCircle(g_focusX, g_focusY, 100, 48, false);
    drawCircle(g_focusX, g_focusY, 200, 64, false);
    drawCircle(g_focusX, g_focusY, 350, 64, false);
    drawCircle(g_focusX, g_focusY, 500, 64, false);

    // Objects
    for (auto& o : g_objs) {
        float col[3];
        if (g_showBases) baseColor(o.base, col);
        else { col[0]=TYPE_COLORS[o.type][0]; col[1]=TYPE_COLORS[o.type][1]; col[2]=TYPE_COLORS[o.type][2]; }

        float alpha = 0.3f + (o.base / 64.0f) * 0.7f;

        // Glow
        glColor4f(col[0], col[1], col[2], alpha * 0.15f);
        drawCircle(o.x, o.y, o.radius * 1.8f, 16, true);

        // Body
        glColor4f(col[0], col[1], col[2], alpha);
        int segs = 8 + o.base / 4; // Higher base = smoother circle
        drawCircle(o.x, o.y, o.radius, segs, true);

        // Border
        glColor4f(col[0]*0.7f, col[1]*0.7f, col[2]*0.7f, alpha);
        drawCircle(o.x, o.y, o.radius, segs, false);
    }

    // ── HUD PANEL ──────────────────────────────────────────────────
    int total = g_baseCounts[7] > 0 ? g_baseCounts[7] : 1;
    int bases[] = {4, 8, 16, 24, 32, 48, 64};
    const char* baseNames[] = {"B-4","B-8","B-16","B-24","B-32","B-48","B-64"};

    // Left panel background
    glColor4f(0.02f, 0.03f, 0.06f, 0.85f);
    drawRect(0, 0, 180, (float)g_winH);
    glColor4f(0, 0.87f, 0.93f, 0.3f);
    glLineWidth(1);
    glBegin(GL_LINES); glVertex2f(180, 0); glVertex2f(180, (float)g_winH); glEnd();

    // Title
    glColor4f(0, 0.87f, 0.93f, 1);
    gltext::draw("DRO CASCADE", 10, g_winH - 20.0f, 1.5f);

    // Scenario
    glColor4f(0.6f, 0.6f, 0.7f, 1);
    gltext::draw("MODE:", 10, g_winH - 42.0f, 1.0f);
    glColor4f(1, 0.85f, 0.2f, 1);
    gltext::draw(SCENARIOS[g_scenario], 50, g_winH - 42.0f, 1.0f);

    // Stats
    float sy = g_winH - 65.0f;
    char buf[64];
    glColor4f(0.6f, 0.6f, 0.7f, 1);
    gltext::draw("OBJECTS:", 10, sy, 1.0f);
    glColor4f(0, 0.87f, 0.93f, 1);
    snprintf(buf, 64, "%d", total);
    gltext::draw(buf, 85, sy, 1.0f);

    glColor4f(0.6f, 0.6f, 0.7f, 1);
    gltext::draw("FPS:", 10, sy - 16, 1.0f);
    glColor4f(g_fps > 55 ? 0.2f : 1.0f, g_fps > 55 ? 1.0f : 0.3f, 0.2f, 1);
    snprintf(buf, 64, "%d", g_fps);
    gltext::draw(buf, 50, sy - 16, 1.0f);

    // Divider
    glColor4f(0, 0.87f, 0.93f, 0.2f);
    glBegin(GL_LINES); glVertex2f(10, sy-30); glVertex2f(170, sy-30); glEnd();

    // Base distribution - vertical bars with labels
    glColor4f(0, 0.87f, 0.93f, 0.8f);
    gltext::draw("BASE DISTRIBUTION", 10, sy - 45, 0.8f);

    float barTop = sy - 60;
    for (int i = 6; i >= 0; i--) {
        float by = barTop - (6-i) * 22.0f;
        float pct = g_baseCounts[i] * 100.0f / total;
        float barW = (g_baseCounts[i] / (float)total) * 100.0f;
        float c[3]; baseColor(bases[i], c);

        // Color square
        glColor4f(c[0], c[1], c[2], 0.9f);
        drawRect(10, by, 10, 10);

        // Label
        glColor4f(0.7f, 0.7f, 0.8f, 1);
        gltext::draw(baseNames[i], 24, by + 1, 0.8f);

        // Bar
        glColor4f(c[0], c[1], c[2], 0.6f);
        drawRect(65, by, barW, 10);

        // Percentage
        glColor4f(c[0], c[1], c[2], 1);
        snprintf(buf, 64, "%d%%", (int)pct);
        gltext::draw(buf, 65 + barW + 4, by + 1, 0.7f);
    }

    // Controls help at bottom
    glColor4f(0.4f, 0.4f, 0.5f, 1);
    gltext::draw("SPACE +50", 10, 40, 0.7f);
    gltext::draw("C CLEAR", 10, 28, 0.7f);
    gltext::draw("B COLOR", 10, 16, 0.7f);
    gltext::draw("TAB MODE", 10, 4, 0.7f);

    // Focus crosshair
    glColor4f(0, 0.87f, 0.93f, 0.6f);
    glLineWidth(1);
    glBegin(GL_LINES);
    glVertex2f(g_focusX - 12, g_focusY); glVertex2f(g_focusX + 12, g_focusY);
    glVertex2f(g_focusX, g_focusY - 12); glVertex2f(g_focusX, g_focusY + 12);
    glEnd();

    glDisable(GL_BLEND);
}

// ── Callbacks ──────────────────────────────────────────────────────
static void cursorCallback(GLFWwindow*, double x, double y) {
    g_focusX = (float)x;
    g_focusY = (float)(g_winH - y); // flip Y
}

static void keyCallback(GLFWwindow* w, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, 1);
    if (key == GLFW_KEY_SPACE)  spawn(50);
    if (key == GLFW_KEY_C)      g_objs.clear();
    if (key == GLFW_KEY_B)      g_showBases = !g_showBases;
    if (key == GLFW_KEY_TAB)    g_scenario = (g_scenario + 1) % 4;
}

// ── Main ───────────────────────────────────────────────────────────
int main() {
    if (!glfwInit()) return 1;
    GLFWwindow* win = glfwCreateWindow(900, 700,
        "FND-001  DRO Precision Cascade  |  Base-X Engine", nullptr, nullptr);
    if (!win) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    glfwSetCursorPosCallback(win, cursorCallback);
    glfwSetKeyCallback(win, keyCallback);

    glEnable(GL_POINT_SMOOTH);
    g_dro.initializeVRProfile();
    spawn(200);

    printf("FND-001 DRO Precision Cascade\n");
    printf("Move mouse = focus point | SPACE = spawn 50 | B = toggle base/type colors | C = clear | ESC = quit\n");

    double prev = glfwGetTime();
    int fpsCounter = 0;
    double fpsTimer = glfwGetTime();
    while (!glfwWindowShouldClose(win)) {
        glfwGetFramebufferSize(win, &g_winW, &g_winH);
        double now = glfwGetTime();
        float dt = std::min((float)(now - prev), 0.05f);
        prev = now;

        fpsCounter++;
        if (now - fpsTimer > 0.5) {
            g_fps = (int)(fpsCounter / (now - fpsTimer));
            fpsCounter = 0;
            fpsTimer = now;
        }

        update(dt);

        char title[256];
        snprintf(title, sizeof(title),
            "FND-001  DRO Cascade  |  %d objects  |  64:%d 48:%d 32:%d 24:%d 16:%d 8:%d 4:%d",
            g_baseCounts[7], g_baseCounts[6], g_baseCounts[5], g_baseCounts[4],
            g_baseCounts[3], g_baseCounts[2], g_baseCounts[1], g_baseCounts[0]);
        glfwSetWindowTitle(win, title);

        render();
        glfwSwapBuffers(win);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}
