/**
 * SPC-001: Solar System Navigator — Base-X DRO Demo
 * Converted from html_demos/SPC-001_Solar_System_Navigator.html
 *
 * Navigate the solar system. Zoom determines DRO precision:
 *   Base-64  surface detail (μm)     Base-32  planetary scale (mm)
 *   Base-16  inner system (km)       Base-8   outer system (AU)
 *
 * Controls:
 *   1-8        Jump to planet         Scroll   Zoom in/out
 *   Left-drag  Pan camera             R        Reset to Sun
 *   ESC        Quit
 */

#include <GLFW/glfw3.h>
#include <BaseX/DRO.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>

static constexpr double AU   = 149597870.7;   // km
static constexpr double PI   = 3.14159265358979323846;
static constexpr float  PI_F = 3.14159265f;

// ── Planet data ────────────────────────────────────────────────────
struct Body {
    const char* name;
    double distance;     // km from sun
    double radius;       // km
    float  color[3];
    float  glowColor[3];
    bool   rings;
};

static const Body BODIES[] = {
    {"Sun",     0,           695700,  {1.0f,0.85f,0.2f}, {1.0f,0.6f,0.0f}, false},
    {"Mercury", 0.39*AU,     2439.7,  {0.7f,0.7f,0.65f}, {0.5f,0.5f,0.45f},false},
    {"Venus",   0.72*AU,     6051.8,  {0.9f,0.75f,0.4f}, {0.8f,0.65f,0.3f},false},
    {"Earth",   1.00*AU,     6371,    {0.27f,0.53f,1.0f}, {0.13f,0.4f,0.87f},false},
    {"Mars",    1.52*AU,     3389.5,  {0.87f,0.4f,0.27f}, {0.8f,0.33f,0.2f},false},
    {"Jupiter", 5.20*AU,     69911,   {0.87f,0.67f,0.53f},{0.8f,0.6f,0.47f},false},
    {"Saturn",  9.50*AU,     58232,   {0.87f,0.8f,0.53f}, {0.8f,0.73f,0.47f},true},
    {"Neptune", 30.0*AU,     24622,   {0.27f,0.4f,0.87f}, {0.2f,0.33f,0.8f},false},
};
static constexpr int BODY_COUNT = 8;

// ── Camera ─────────────────────────────────────────────────────────
static struct {
    double x = 0, y = 0;
    double zoom       = 1e7;    // km per pixel — start seeing full system
    double targetZoom  = 1e7;
    double minZoom     = 1e-9;
    double maxZoom     = 1e+2;   // wide enough to see full solar system
} cam;

static int   g_selected = 0;
static bool  g_dragging = false;
static double g_dragX, g_dragY;
static float g_time = 0;
static int   g_winW = 1024, g_winH = 768;
static BaseX::DROManager g_dro;

// ── Navigation ─────────────────────────────────────────────────────
static void goToBody(int idx) {
    g_selected = idx;
    cam.x = BODIES[idx].distance;
    cam.y = 0;
    if (idx == 0) {
        // Sun → show full solar system (Neptune orbit fits on screen)
        cam.targetZoom = BODIES[BODY_COUNT-1].distance * 1.2 / (g_winW / 2.0);
    } else {
        // Planet → zoom to fit it with some padding
        cam.targetZoom = BODIES[idx].radius * 5.0 / (g_winW / 2.0);
    }
}

// ── Helpers for OpenGL without GLU ─────────────────────────────────
static void ortho2D(int w, int h) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float m[16] = {};
    m[0]  =  2.0f / w;
    m[5]  =  2.0f / h;
    m[10] = -1.0f;
    m[12] = -1.0f;
    m[13] = -1.0f;
    m[15] =  1.0f;
    glLoadMatrixf(m);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void drawCircle(float cx, float cy, float r, int segs, bool fill) {
    if (fill) glBegin(GL_TRIANGLE_FAN); else glBegin(GL_LINE_LOOP);
    if (fill) glVertex2f(cx, cy);
    for (int i = 0; i <= segs; i++) {
        float a = 2.0f * PI_F * i / segs;
        glVertex2f(cx + cosf(a)*r, cy + sinf(a)*r);
    }
    glEnd();
}

static void drawEllipse(float cx, float cy, float rx, float ry, int segs) {
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segs; i++) {
        float a = 2.0f * PI_F * i / segs;
        glVertex2f(cx + cosf(a)*rx, cy + sinf(a)*ry);
    }
    glEnd();
}

// ── Determine active base from zoom level ──────────────────────────
static int getActiveBase() {
    if (cam.zoom < 1e-7) return 64;
    if (cam.zoom < 1e-5) return 48;
    if (cam.zoom < 1e-3) return 32;
    if (cam.zoom < 1e-1) return 24;
    return 16;
}

// ── Render ──────────────────────────────────────────────────────────
static void render() {
    glViewport(0, 0, g_winW, g_winH);
    glClearColor(0.008f, 0.008f, 0.024f, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    ortho2D(g_winW, g_winH);
    float cx = g_winW / 2.0f;
    float cy = g_winH / 2.0f;

    // ── Stars ──────────────────────────────────────────────────────
    glPointSize(1.5f);
    glBegin(GL_POINTS);
    for (int i = 0; i < 300; i++) {
        float sx = (float)((i * 7919) % g_winW);
        float sy = (float)((i * 7727) % g_winH);
        float a  = 0.3f + ((i * 17) % 70) / 100.0f;
        glColor4f(1, 1, 1, a);
        glVertex2f(sx, sy);
    }
    glEnd();

    // ── Orbital paths (when zoomed out) ────────────────────────────
    if (cam.zoom > 1e-4) {
        float sunSX = cx + (float)((0 - cam.x) / cam.zoom);
        float sunSY = cy + (float)((0 - cam.y) / cam.zoom);
        glColor4f(1, 1, 1, 0.08f);
        glLineWidth(1);
        for (int i = 1; i < BODY_COUNT; i++) {
            float orbitR = (float)(BODIES[i].distance / cam.zoom);
            if (orbitR > 5 && orbitR < g_winW * 2)
                drawCircle(sunSX, sunSY, orbitR, 120, false);
        }
    }

    // ── Bodies ──────────────────────────────────────────────────────
    for (int i = 0; i < BODY_COUNT; i++) {
        const Body& b = BODIES[i];
        float bx = cx + (float)((b.distance - cam.x) / cam.zoom);
        float by = cy + (float)((0          - cam.y) / cam.zoom);

        if (bx < -200 || bx > g_winW+200 || by < -200 || by > g_winH+200)
            continue;

        float vr = (float)(b.radius / cam.zoom);
        vr = std::clamp(vr, 3.0f, 500.0f);

        // Glow
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glColor4f(b.glowColor[0], b.glowColor[1], b.glowColor[2], 0.15f);
        drawCircle(bx, by, vr * 2.0f, 48, true);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Body
        glColor3fv(b.color);
        drawCircle(bx, by, vr, 48, true);

        // Rings (Saturn)
        if (b.rings && vr > 10) {
            glColor4f(b.color[0], b.color[1], b.color[2], 0.45f);
            glLineWidth(vr * 0.25f);
            drawEllipse(bx, by, vr * 2.0f, vr * 0.5f, 64);
        }

        // Selection ring
        if (i == g_selected) {
            glColor4f(0, 0.87f, 0.93f, 0.7f);
            glLineWidth(2);
            drawCircle(bx, by, vr + 8, 48, false);
        }
    }

    // ── HUD ────────────────────────────────────────────────────────
    int base = getActiveBase();

    // Legend quads
    struct { float r,g,b; int base; } legend[] = {
        {1,.2f,.2f, 64}, {1,1,.2f, 48}, {.2f,1,.2f, 32},
        {.2f,.5f,1, 24}, {.5f,.5f,.5f, 16}
    };
    for (int i = 0; i < 5; i++) {
        float y = g_winH - 20.0f - i * 20.0f;
        bool active = (base == legend[i].base);
        float alpha = active ? 1.0f : 0.3f;
        glColor4f(legend[i].r, legend[i].g, legend[i].b, alpha);
        glBegin(GL_QUADS);
        glVertex2f(10,y); glVertex2f(26,y); glVertex2f(26,y-14); glVertex2f(10,y-14);
        glEnd();
        // Active highlight bar
        if (active) {
            glColor4f(legend[i].r, legend[i].g, legend[i].b, 0.25f);
            glBegin(GL_QUADS);
            glVertex2f(8,y+2); glVertex2f(120,y+2); glVertex2f(120,y-16); glVertex2f(8,y-16);
            glEnd();
        }
    }

    // Title bar
    glColor4f(0, 0.87f, 0.93f, 0.15f);
    glBegin(GL_QUADS);
    glVertex2f(0,(float)g_winH); glVertex2f((float)g_winW,(float)g_winH);
    glVertex2f((float)g_winW,(float)(g_winH-4)); glVertex2f(0,(float)(g_winH-4));
    glEnd();

    // Selected body indicator
    glColor4f(0, 0.87f, 0.93f, 0.6f);
    glBegin(GL_QUADS);
    float ix = g_winW - 160.0f, iy = g_winH - 20.0f;
    glVertex2f(ix,iy); glVertex2f(ix+150,iy); glVertex2f(ix+150,iy-18); glVertex2f(ix,iy-18);
    glEnd();

    // Scale bar
    glColor4f(1, 1, 1, 0.3f);
    glLineWidth(2);
    glBegin(GL_LINES);
    glVertex2f(g_winW-130.0f, 30); glVertex2f(g_winW-30.0f, 30);
    glEnd();

    glDisable(GL_BLEND);
}

// ── Callbacks ──────────────────────────────────────────────────────
static void keyCallback(GLFWwindow* w, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, 1);
    if (key == GLFW_KEY_R) goToBody(0);
    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_8) goToBody(key - GLFW_KEY_1);
    if (key == GLFW_KEY_EQUAL) cam.targetZoom = std::max(cam.minZoom, cam.targetZoom * 0.3);
    if (key == GLFW_KEY_MINUS) cam.targetZoom = std::min(cam.maxZoom, cam.targetZoom * 3.0);
}

static void scrollCallback(GLFWwindow*, double, double yoff) {
    if (yoff > 0.01) {
        // Scroll up → zoom IN → smaller km/px
        cam.targetZoom = std::max(cam.minZoom, cam.targetZoom * 0.5);
    } else if (yoff < -0.01) {
        // Scroll down → zoom OUT → larger km/px
        cam.targetZoom = std::min(cam.maxZoom, cam.targetZoom * 2.0);
    }
    printf("scroll yoff=%.2f  targetZoom=%.2e\n", yoff, cam.targetZoom);
}

static void mouseButtonCallback(GLFWwindow* w, int button, int action, int) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        g_dragging = (action == GLFW_PRESS);
        glfwGetCursorPos(w, &g_dragX, &g_dragY);
    }
}

static void cursorPosCallback(GLFWwindow*, double xpos, double ypos) {
    if (g_dragging) {
        double dx = xpos - g_dragX;
        double dy = ypos - g_dragY;
        cam.x -= dx * cam.zoom;
        cam.y += dy * cam.zoom;  // flip Y
        g_dragX = xpos;
        g_dragY = ypos;
    }
}

// ── Main ───────────────────────────────────────────────────────────
int main() {
    if (!glfwInit()) return 1;

    GLFWwindow* win = glfwCreateWindow(1024, 768,
        "SPC-001  Solar System Navigator  |  Base-X Engine", nullptr, nullptr);
    if (!win) { glfwTerminate(); return 1; }

    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    glfwSetKeyCallback(win, keyCallback);
    glfwSetScrollCallback(win, scrollCallback);
    glfwSetMouseButtonCallback(win, mouseButtonCallback);
    glfwSetCursorPosCallback(win, cursorPosCallback);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_POINT_SMOOTH);

    g_dro.initializeVRProfile();
    goToBody(0); // start at Sun

    printf("SPC-001 Solar System Navigator\n");
    printf("Keys 1-8: jump to planet | Scroll: zoom | Drag: pan | ESC: quit\n");

    while (!glfwWindowShouldClose(win)) {
        glfwGetFramebufferSize(win, &g_winW, &g_winH);
        if (g_winW < 1) g_winW = 1;
        if (g_winH < 1) g_winH = 1;

        // Smooth zoom
        cam.zoom += (cam.targetZoom - cam.zoom) * 0.25;
        g_time += 0.016f;

        render();

        // Print current state to title bar
        int base = getActiveBase();
        char title[256];
        snprintf(title, sizeof(title),
            "SPC-001  %s  |  Base-%d  |  Zoom %.2e km/px  |  Base-X Engine",
            BODIES[g_selected].name, base, cam.zoom);
        glfwSetWindowTitle(win, title);

        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
