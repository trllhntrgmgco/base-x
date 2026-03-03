/**
 * FND-003: LOD Physics Stress Test — Base-X Engine
 * Converted from html_demos/FND-003_LOD_Physics_Stress.html
 *
 * Split-screen: LEFT = LOD enabled, RIGHT = no LOD.
 * 1000+ objects bounce around.  LOD side skips physics updates for
 * distant objects and uses fewer draw segments → higher FPS.
 * Mouse Y controls focus height.  Title bar shows FPS comparison.
 *
 * Controls:  Mouse = focus  | SPACE +100 | - remove 100 | ESC quit
 */

#include <GLFW/glfw3.h>
#include <BaseX/DRO.h>
#include <BaseX/GLText.h>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <chrono>

static constexpr float PI = 3.14159265f;

// ── LOD config ─────────────────────────────────────────────────────
struct LODLevel {
    int updateFreq;     // run physics every N frames
    float complexity;   // 1.0 = full, 0.1 = minimal
    float distMax;
};

static const LODLevel LOD_CFG[] = {
    {1,  1.0f,  80},   // LOD 0 — every frame
    {1,  0.8f, 150},   // LOD 1
    {2,  0.6f, 250},   // LOD 2 — every 2nd frame
    {4,  0.4f, 400},   // LOD 3
    {8,  0.2f, 600},   // LOD 4
    {16, 0.1f, 1e9f},  // LOD 5 — rarely updated
};

// ── Physics object ─────────────────────────────────────────────────
struct Obj {
    float x, y, vx, vy, radius;
    int   lod;
    int   framesSince;
    float interpX, interpY, lastX, lastY;
    bool  skipped;
};

// ── Globals ────────────────────────────────────────────────────────
static std::vector<Obj> g_lod, g_nolod;
static float g_focusX = 0.5f, g_focusY = 0.5f;   // normalized
static int   g_winW = 1100, g_winH = 700;
static int   g_frame = 0;
static BaseX::DROManager g_dro;
static int   g_lodDist[7] = {};   // LOD 0-5 + skipped
static double g_fpsLod = 0, g_fpsNoLod = 0;

static Obj makeObj(float areaW, float areaH) {
    Obj o{};
    o.x = (float)(rand() % (int)areaW);
    o.y = (float)(rand() % (int)areaH);
    o.vx = (rand() % 300 - 150) / 100.0f;
    o.vy = (rand() % 300 - 150) / 100.0f;
    o.radius = 2.0f + (rand() % 60) / 10.0f;
    o.lod = 0;
    o.framesSince = 0;
    o.interpX = o.x; o.interpY = o.y;
    o.lastX = o.x;  o.lastY = o.y;
    return o;
}

static void spawn(int n) {
    float hw = g_winW / 2.0f - 10;
    float hh = (float)g_winH - 10;
    for (int i = 0; i < n; i++) {
        g_lod.push_back(makeObj(hw, hh));
        g_nolod.push_back(makeObj(hw, hh));
    }
}

static void removeSome(int n) {
    for (int i = 0; i < n && !g_lod.empty(); i++) { g_lod.pop_back(); g_nolod.pop_back(); }
}

// ── Update one side ────────────────────────────────────────────────
static int updateSide(std::vector<Obj>& objs, float w, float h,
                      float fx, float fy, bool useLod) {
    int ops = 0;
    for (auto& o : objs) {
        if (useLod) {
            float dx = o.x - fx, dy = o.y - fy;
            float dist = sqrtf(dx*dx + dy*dy);
            o.lod = 5;
            for (int l = 0; l <= 5; l++) {
                if (dist < LOD_CFG[l].distMax) { o.lod = l; break; }
            }
            int freq = LOD_CFG[o.lod].updateFreq;
            if (g_frame % freq != 0) {
                // Interpolate
                float t = (float)(g_frame % freq) / freq;
                o.interpX = o.lastX + (o.x - o.lastX) * (1 + t);
                o.interpY = o.lastY + (o.y - o.lastY) * (1 + t);
                o.skipped = true;
                continue;
            }
        }

        o.skipped = false;
        o.lastX = o.x; o.lastY = o.y;
        o.x += o.vx;
        o.y += o.vy;
        if (o.x < o.radius)      { o.x = o.radius;      o.vx = fabsf(o.vx); }
        if (o.x > w - o.radius)  { o.x = w - o.radius;  o.vx = -fabsf(o.vx); }
        if (o.y < o.radius)      { o.y = o.radius;      o.vy = fabsf(o.vy); }
        if (o.y > h - o.radius)  { o.y = h - o.radius;  o.vy = -fabsf(o.vy); }
        o.interpX = o.x; o.interpY = o.y;
        ops++;
    }
    return ops;
}

// ── Draw helpers ───────────────────────────────────────────────────
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
    glVertex2f(x,y); glVertex2f(x+w,y); glVertex2f(x+w,y+h); glVertex2f(x,y+h);
    glEnd();
}

// ── Render one side ────────────────────────────────────────────────
static void renderSide(const std::vector<Obj>& objs, float ox, float oy,
                       float w, float h, bool useLod) {
    // Clip region via scissor
    glEnable(GL_SCISSOR_TEST);
    glScissor((int)ox, 0, (int)w, g_winH);

    // Background
    glColor4f(0.03f, 0.03f, 0.06f, 1);
    drawRect(ox, oy, w, h);

    // Focus rings
    float fx = ox + g_focusX * w;
    float fy = g_focusY * h;
    float radii[] = {80, 150, 250, 400};
    for (float r : radii) {
        glColor4f(0, 0.87f, 0.93f, 0.08f);
        drawCircle(fx, fy, r, 48, true);
        glColor4f(0, 0.87f, 0.93f, 0.2f);
        drawCircle(fx, fy, r, 48, false);
    }

    // Objects
    for (auto& o : objs) {
        float px = ox + o.interpX;
        float py = o.interpY;

        float col[3];
        if (useLod) {
            // Color by LOD level
            float t = o.lod / 5.0f;
            col[0] = 0.2f + t * 0.8f;  // red increases with LOD
            col[1] = 1.0f - t * 0.7f;  // green decreases
            col[2] = 0.3f;
        } else {
            col[0] = 0.4f; col[1] = 0.7f; col[2] = 1.0f; // uniform blue
        }

        float alpha = useLod ? (1.0f - o.lod * 0.12f) : 0.8f;
        int segs = useLod ? (16 - o.lod * 2) : 16;
        if (segs < 4) segs = 4;

        glColor4f(col[0], col[1], col[2], alpha);
        drawCircle(px, py, o.radius, segs, true);

        if (o.skipped && useLod) {
            // Dim border for skipped objects
            glColor4f(0.5f, 0.5f, 0.5f, 0.2f);
            drawCircle(px, py, o.radius, segs, false);
        }
    }

    // Label
    float lx = ox + 5, ly = h - 8;
    glColor4f(0, 0.87f, 0.93f, 0.5f);
    drawRect(lx, ly, 80, 6);

    glDisable(GL_SCISSOR_TEST);
}

static void render() {
    glViewport(0, 0, g_winW, g_winH);
    glClearColor(0.02f, 0.02f, 0.04f, 1);
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

    float hw = g_winW / 2.0f;

    // Left = LOD enabled
    renderSide(g_lod, 0, 0, hw - 2, (float)g_winH, true);

    // Right = no LOD
    renderSide(g_nolod, hw + 2, 0, hw - 2, (float)g_winH, false);

    // Divider
    glColor4f(0, 0.87f, 0.93f, 0.5f);
    drawRect(hw - 1, 0, 2, (float)g_winH);

    // LOD distribution bar (bottom-left)
    memset(g_lodDist, 0, sizeof(g_lodDist));
    int skipped = 0;
    for (auto& o : g_lod) {
        if (o.skipped) skipped++;
        else g_lodDist[o.lod]++;
    }
    g_lodDist[6] = skipped;
    float barX = 10, barY = 10, barW = hw - 20, barH = 12;
    int total = (int)g_lod.size();
    if (total < 1) total = 1;
    float bx = barX;
    float lodColors[][3] = {{.2f,1,.3f},{.5f,1,.2f},{.8f,.9f,.2f},
                            {1,.7f,.2f},{1,.4f,.2f},{1,.2f,.2f},{.3f,.3f,.3f}};
    for (int i = 0; i < 7; i++) {
        float w = (g_lodDist[i] / (float)total) * barW;
        glColor4f(lodColors[i][0], lodColors[i][1], lodColors[i][2], 0.85f);
        drawRect(bx, barY, w, barH);
        bx += w;
    }
    glColor4f(1, 1, 1, 0.3f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(barX,barY); glVertex2f(barX+barW,barY);
    glVertex2f(barX+barW,barY+barH); glVertex2f(barX,barY+barH);
    glEnd();

    glDisable(GL_BLEND);
}

// ── Callbacks ──────────────────────────────────────────────────────
static void cursorCallback(GLFWwindow*, double x, double y) {
    g_focusX = (float)(x / g_winW);  // 0-1 normalized
    g_focusY = 1.0f - (float)(y / g_winH);
}

static void keyCallback(GLFWwindow* w, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, 1);
    if (key == GLFW_KEY_SPACE)  spawn(100);
    if (key == GLFW_KEY_MINUS)  removeSome(100);
}

int main() {
    if (!glfwInit()) return 1;
    GLFWwindow* win = glfwCreateWindow(1100, 700,
        "FND-003  LOD Physics Stress  |  Base-X Engine", nullptr, nullptr);
    if (!win) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(0);  // uncapped to see real FPS difference
    glfwSetCursorPosCallback(win, cursorCallback);
    glfwSetKeyCallback(win, keyCallback);
    glEnable(GL_POINT_SMOOTH);

    g_dro.initializeVRProfile();
    spawn(500);

    printf("FND-003 LOD Physics Stress Test\n");
    printf("LEFT = LOD enabled | RIGHT = no LOD\n");
    printf("SPACE = +100 objects | MINUS = -100 | ESC = quit\n");

    double prev = glfwGetTime();
    int fpsCounter = 0;
    double fpsTimer = glfwGetTime();

    while (!glfwWindowShouldClose(win)) {
        glfwGetFramebufferSize(win, &g_winW, &g_winH);
        double now = glfwGetTime();
        float dt = std::min((float)(now - prev), 0.05f);
        prev = now;
        g_frame++;

        float hw = g_winW / 2.0f;
        float fx_lod = g_focusX * hw;
        float fy_lod = g_focusY * g_winH;

        // Time LOD side
        auto t0 = std::chrono::high_resolution_clock::now();
        int opsL = updateSide(g_lod, hw, (float)g_winH, fx_lod, fy_lod, true);
        auto t1 = std::chrono::high_resolution_clock::now();

        // Time no-LOD side
        int opsN = updateSide(g_nolod, hw, (float)g_winH, fx_lod, fy_lod, false);
        auto t2 = std::chrono::high_resolution_clock::now();

        double msL = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double msN = std::chrono::duration<double, std::milli>(t2 - t1).count();

        fpsCounter++;
        if (now - fpsTimer > 0.5) {
            double fps = fpsCounter / (now - fpsTimer);
            fpsCounter = 0;
            fpsTimer = now;

            char title[256];
            snprintf(title, sizeof(title),
                "FND-003  LOD Stress  |  %d objects  |  LOD: %d ops (%.1fms)  NoLOD: %d ops (%.1fms)  |  %.0f FPS",
                (int)g_lod.size(), opsL, msL, opsN, msN, fps);
            glfwSetWindowTitle(win, title);
        }

        render();
        glfwSwapBuffers(win);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}
