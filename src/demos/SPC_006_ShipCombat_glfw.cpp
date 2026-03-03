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

static constexpr float PI = 3.14159265f;
static constexpr float TAU = 6.28318530f;

// Colors from HTML
static constexpr float COL_VOID[] = {0.016f, 0.008f, 0.024f};
static constexpr float COL_PANEL[] = {0.039f, 0.024f, 0.063f};
static constexpr float COL_RED[] = {1.000f, 0.200f, 0.267f};
static constexpr float COL_RED_DIM[] = {0.533f, 0.133f, 0.200f};
static constexpr float COL_CYAN[] = {0.000f, 0.706f, 1.000f};
static constexpr float COL_GREEN[] = {0.000f, 1.000f, 0.533f};
static constexpr float COL_ORANGE[] = {1.000f, 0.584f, 0.000f};
static constexpr float COL_YELLOW[] = {1.000f, 0.816f, 0.000f};
static constexpr float COL_WHITE[] = {0.816f, 0.831f, 0.910f};
static constexpr float COL_DIM[] = {0.290f, 0.314f, 0.376f};
static constexpr float COL_SHIELD_BLUE[] = {0.267f, 0.533f, 1.000f};
static constexpr float COL_HULL_ORANGE[] = {1.000f, 0.400f, 0.133f};
static constexpr float COL_LASER_RED[] = {1.000f, 0.133f, 0.267f};
static constexpr float COL_TORP_CYAN[] = {0.000f, 0.933f, 1.000f};
static constexpr float COL_RAIL_YELLOW[] = {1.000f, 0.933f, 0.000f};

// Simulation state
struct Ship {
    float x, y, vx, vy, angle;
    float shield, hull, reactor;
    float throttle;
    int weapon; // 0: laser, 1: torpedo, 2: railgun
    float weaponCharge;
    int torpedoes;
    std::vector<float> damage; // fwd, aft, port, star, bridge, engine
};

struct Enemy {
    float x, y, vx, vy, angle;
    float shield, hull;
    std::string name, type;
    bool destroyed;
};

struct Projectile {
    float x, y, vx, vy;
    int type; // 0: laser, 1: torpedo, 2: railgun
    float damage;
};

static Ship player;
static std::vector<Enemy> enemies;
static std::vector<Projectile> projectiles;
static int selectedTarget = -1;
static int droBase = 32;
static float droPrecision = 1.0f;
static int droSwitches = 0;
static float droAvg = 32.0f;

// Viewport dimensions
static int g_winW = 1280, g_winH = 720;
static int panelW = 230, rightPanelW = 270;

// GL helpers
static void setColor(float r, float g, float b, float a = 1.0f) {
    glColor4f(r, g, b, a);
}

static void drawRect(float x, float y, float w, float h) {
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
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

static void drawLine(float x1, float y1, float x2, float y2) {
    glBegin(GL_LINES);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
}

static void setOrtho(float x, float y, float w, float h) {
    glViewport(x, y, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

// Initialize simulation
static void initSimulation() {
    player = {400, 300, 0, 0, 0, 100, 100, 100, 0, 0, 100, 12, {100,100,100,100,100,100}};
    enemies = {
        {600, 200, 0, 0, 0, 100, 100, "RAIDER-01", "Corvette", false},
        {500, 400, 0, 0, 0, 100, 100, "RAIDER-02", "Corvette", false},
        {300, 150, 0, 0, 0, 100, 100, "VIPER-01", "Fighter", false},
        {700, 450, 0, 0, 0, 100, 100, "VIPER-02", "Fighter", false},
        {800, 250, 0, 0, 0, 100, 100, "DREADNOUGHT", "Battleship", false}
    };
}

// Update simulation
static void update(float dt) {
    // Player movement
    float speed = player.throttle * 50 * dt;
    player.vx += cosf(player.angle) * speed;
    player.vy += sinf(player.angle) * speed;
    player.x += player.vx * dt;
    player.y += player.vy * dt;

    // Damp velocity
    player.vx *= 0.98f;
    player.vy *= 0.98f;

    // Update projectiles
    for (auto& p : projectiles) {
        p.x += p.vx * dt;
        p.y += p.vy * dt;
    }

    // Remove off-screen projectiles
    projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(),
        [](const Projectile& p) { return p.x < -100 || p.x > g_winW + 100 || p.y < -100 || p.y > g_winH + 100; }), projectiles.end());

    // Simple AI for enemies
    for (auto& e : enemies) {
        if (!e.destroyed) {
            float dx = player.x - e.x;
            float dy = player.y - e.y;
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist > 0) {
                e.angle = atan2f(dy, dx);
                e.vx += cosf(e.angle) * 20 * dt;
                e.vy += sinf(e.angle) * 20 * dt;
            }
            e.vx *= 0.95f;
            e.vy *= 0.95f;
            e.x += e.vx * dt;
            e.y += e.vy * dt;
        }
    }
}

// Render canvas
static void renderCanvas() {
    setOrtho(panelW, 0, g_winW - panelW - rightPanelW, g_winH);

    // Background
    setColor(COL_VOID[0], COL_VOID[1], COL_VOID[2]);
    drawRect(0, 0, g_winW - panelW - rightPanelW, g_winH);

    // Stars
    static std::mt19937 rng(42);
    setColor(1, 1, 1, 0.5f);
    for (int i = 0; i < 100; ++i) {
        float x = (rng() % 10000) / 10000.0f * (g_winW - panelW - rightPanelW);
        float y = (rng() % 10000) / 10000.0f * g_winH;
        drawCircle(x, y, 1, 4);
    }

    // Player ship
    setColor(COL_CYAN[0], COL_CYAN[1], COL_CYAN[2]);
    drawCircle(player.x, player.y, 15, 20);

    // Enemies
    for (const auto& e : enemies) {
        if (!e.destroyed) {
            setColor(COL_RED[0], COL_RED[1], COL_RED[2]);
            drawCircle(e.x, e.y, 10, 16);
        }
    }

    // Projectiles
    for (const auto& p : projectiles) {
        if (p.type == 0) setColor(COL_LASER_RED[0], COL_LASER_RED[1], COL_LASER_RED[2]);
        else if (p.type == 1) setColor(COL_TORP_CYAN[0], COL_TORP_CYAN[1], COL_TORP_CYAN[2]);
        else setColor(COL_RAIL_YELLOW[0], COL_RAIL_YELLOW[1], COL_RAIL_YELLOW[2]);
        drawCircle(p.x, p.y, 3, 8);
    }

    // Crosshair
    setColor(COL_RED[0], COL_RED[1], COL_RED[2], 0.5f);
    drawLine(g_winW/2 - 30, g_winH/2, g_winW/2 + 30, g_winH/2);
    drawLine(g_winW/2, g_winH/2 - 30, g_winW/2, g_winH/2 + 30);
}

// Render left panel
static void renderLeftPanel() {
    setOrtho(0, 0, panelW, g_winH);

    setColor(COL_PANEL[0], COL_PANEL[1], COL_PANEL[2], 0.95f);
    drawRect(0, 0, panelW, g_winH);

    // Title
    setColor(COL_RED_DIM[0], COL_RED_DIM[1], COL_RED_DIM[2]);
    gltext::draw("SHIP STATUS", 10, 20, 1.2f);

    // Stats
    float y = 40;
    auto drawStat = [&](const char* label, const char* val, const float* col) {
        setColor(COL_DIM[0], COL_DIM[1], COL_DIM[2]);
        gltext::draw(label, 10, y, 1.0f);
        setColor(col[0], col[1], col[2]);
        gltext::draw(val, 150, y, 1.0f);
        y += 15;
    };

    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f%%", player.shield);
    drawStat("Shield Strength", buf, COL_SHIELD_BLUE);
    snprintf(buf, sizeof(buf), "%.0f%%", player.hull);
    drawStat("Hull Integrity", buf, COL_GREEN);
    snprintf(buf, sizeof(buf), "%.0f%%", player.reactor);
    drawStat("Reactor Power", buf, COL_CYAN);

    // Weapons
    y += 20;
    setColor(COL_RED_DIM[0], COL_RED_DIM[1], COL_RED_DIM[2]);
    gltext::draw("WEAPONS", 10, y, 1.2f);
    y += 20;

    const char* wpnNames[] = {"LASER", "TORPEDO", "RAILGUN"};
    const float* wpnCols[] = {COL_LASER_RED, COL_TORP_CYAN, COL_RAIL_YELLOW};
    for (int i = 0; i < 3; ++i) {
        if (i == player.weapon) setColor(COL_RED[0], COL_RED[1], COL_RED[2]);
        else setColor(COL_DIM[0], COL_DIM[1], COL_DIM[2]);
        gltext::draw(wpnNames[i], 10 + i * 70, y, 0.9f);
    }
    y += 20;

    drawStat("Weapon", wpnNames[player.weapon], wpnCols[player.weapon]);
    snprintf(buf, sizeof(buf), "%.0f%%", player.weaponCharge);
    drawStat("Charge", buf, COL_YELLOW);
    drawStat("Torpedoes", std::to_string(player.torpedoes).c_str(), COL_WHITE);
}

// Render right panel
static void renderRightPanel() {
    setOrtho(g_winW - rightPanelW, 0, rightPanelW, g_winH);

    setColor(COL_PANEL[0], COL_PANEL[1], COL_PANEL[2], 0.95f);
    drawRect(0, 0, rightPanelW, g_winH);

    // Title
    setColor(COL_RED_DIM[0], COL_RED_DIM[1], COL_RED_DIM[2]);
    gltext::draw("TACTICAL - CONTACTS", 10, 20, 1.2f);

    // Targets
    float y = 40;
    for (size_t i = 0; i < enemies.size(); ++i) {
        const auto& e = enemies[i];
        if (i == (size_t)selectedTarget) setColor(COL_RED[0], COL_RED[1], COL_RED[2], 0.2f);
        else setColor(COL_DIM[0], COL_DIM[1], COL_DIM[2], 0.1f);
        drawRect(5, y - 2, rightPanelW - 10, 25);

        setColor(COL_RED[0], COL_RED[1], COL_RED[2]);
        gltext::draw(e.name.c_str(), 10, y + 5, 1.0f);
        setColor(COL_DIM[0], COL_DIM[1], COL_DIM[2]);
        gltext::draw(e.type.c_str(), 10, y + 15, 0.8f);

        char dist[16];
        float dx = e.x - player.x, dy = e.y - player.y;
        snprintf(dist, sizeof(dist), "%.1fkm", sqrtf(dx*dx + dy*dy) / 100);
        gltext::draw(dist, 150, y + 10, 0.9f);

        y += 30;
    }

    // DRO
    y += 20;
    setColor(COL_RED_DIM[0], COL_RED_DIM[1], COL_RED_DIM[2]);
    gltext::draw("DRO ANALYSIS", 10, y, 1.2f);
    y += 20;

    auto drawStat = [&](const char* label, const char* val, const float* col) {
        setColor(COL_DIM[0], COL_DIM[1], COL_DIM[2]);
        gltext::draw(label, 10, y, 1.0f);
        setColor(col[0], col[1], col[2]);
        gltext::draw(val, 120, y, 1.0f);
        y += 15;
    };

    char buf[32];
    snprintf(buf, sizeof(buf), "%d", droBase);
    drawStat("Active Base", buf, COL_CYAN);
    snprintf(buf, sizeof(buf), "%.1f mm", droPrecision);
    drawStat("Precision", buf, COL_WHITE);
    snprintf(buf, sizeof(buf), "%d", droSwitches);
    drawStat("Base Switches", buf, COL_WHITE);
    snprintf(buf, sizeof(buf), "%.1f", droAvg);
    drawStat("Avg Base", buf, COL_WHITE);
}

// Helper for right panel stats
static void drawStat(const char* label, const char* val, const float* col) {
    // This is a local function, but in C++ we need to define it properly
    // For simplicity, inline it
}

// Input
static void keyCallback(GLFWwindow* win, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;

    switch (key) {
    case GLFW_KEY_ESCAPE:
        glfwSetWindowShouldClose(win, GLFW_TRUE);
        break;
    case GLFW_KEY_W:
        player.throttle = std::min(1.0f, player.throttle + 0.1f);
        break;
    case GLFW_KEY_S:
        player.throttle = std::max(0.0f, player.throttle - 0.1f);
        break;
    case GLFW_KEY_A:
        player.angle -= 0.1f;
        break;
    case GLFW_KEY_D:
        player.angle += 0.1f;
        break;
    case GLFW_KEY_1:
        player.weapon = 0;
        break;
    case GLFW_KEY_2:
        player.weapon = 1;
        break;
    case GLFW_KEY_3:
        player.weapon = 2;
        break;
    case GLFW_KEY_SPACE:
        // Fire weapon
        if (player.weaponCharge >= 20) {
            Projectile p;
            p.x = player.x;
            p.y = player.y;
            p.vx = cosf(player.angle) * 200;
            p.vy = sinf(player.angle) * 200;
            p.type = player.weapon;
            p.damage = 10;
            projectiles.push_back(p);
            player.weaponCharge -= 20;
            if (player.weapon == 1) player.torpedoes--;
        }
        break;
    case GLFW_KEY_TAB:
        selectedTarget = (selectedTarget + 1) % enemies.size();
        break;
    }
}

int main() {
    if (!glfwInit()) return 1;

    GLFWwindow* window = glfwCreateWindow(g_winW, g_winH, "BASE-X ENGINE | SPC-006: Ship Combat", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetKeyCallback(window, keyCallback);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    initSimulation();

    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = std::min(0.05f, (float)(now - lastTime));
        lastTime = now;

        glfwPollEvents();
        glfwGetFramebufferSize(window, &g_winW, &g_winH);

        update(dt);

        glViewport(0, 0, g_winW, g_winH);
        glClearColor(COL_VOID[0], COL_VOID[1], COL_VOID[2], 1);
        glClear(GL_COLOR_BUFFER_BIT);

        renderCanvas();
        renderLeftPanel();
        renderRightPanel();

        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}