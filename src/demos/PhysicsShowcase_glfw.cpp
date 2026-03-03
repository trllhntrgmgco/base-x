/**
 * PHY-001: Base-X Physics Precision Showcase (GLFW + Bullet3)
 *
 * Color-coded falling cubes whose precision changes with distance:
 *   RED    Base-64  (0-0.5 m)   YELLOW Base-32  (0.5-2 m)
 *   GREEN  Base-16  (2-10 m)    BLUE   Base-8   (10-100 m)
 *   GRAY   Base-4   (100 m+)
 *
 * Controls:  SPACE spawn  |  R reset  |  ESC quit
 */

#include <GLFW/glfw3.h>
#include <BaseX/DRO.h>

#ifdef BASEX_HAVE_BULLET3
#include <BaseX/BulletIntegration.h>
#endif

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <vector>

// ── helpers ────────────────────────────────────────────────────────
static void perspectiveGL(double fovY, double aspect, double zNear, double zFar) {
    double f = 1.0 / tan(fovY * 3.14159265358979323846 / 360.0);
    double m[16] = {};
    m[0]  = f / aspect;
    m[5]  = f;
    m[10] = (zFar + zNear) / (zNear - zFar);
    m[11] = -1.0;
    m[14] = (2.0 * zFar * zNear) / (zNear - zFar);
    glMultMatrixd(m);
}

static void lookAtGL(float ex, float ey, float ez,
                     float cx, float cy, float cz,
                     float ux, float uy, float uz) {
    float fx = cx - ex, fy = cy - ey, fz = cz - ez;
    float fl = sqrtf(fx*fx + fy*fy + fz*fz);
    fx /= fl; fy /= fl; fz /= fl;
    float sx = fy*uz - fz*uy, sy = fz*ux - fx*uz, sz = fx*uy - fy*ux;
    float sl = sqrtf(sx*sx + sy*sy + sz*sz);
    sx /= sl; sy /= sl; sz /= sl;
    float uux = sy*fz - sz*fy, uuy = sz*fx - sx*fz, uuz = sx*fy - sy*fx;
    float m[16] = {
        sx, uux, -fx, 0,
        sy, uuy, -fy, 0,
        sz, uuz, -fz, 0,
        0,  0,    0,  1
    };
    glMultMatrixf(m);
    glTranslatef(-ex, -ey, -ez);
}

// ── data ───────────────────────────────────────────────────────────
struct Cube {
    float pos[3], vel[3], color[3];
    float size;
    int   base;
    bool  alive;
#ifdef BASEX_HAVE_BULLET3
    void* body = nullptr;
#endif
};

static std::vector<Cube>   g_cubes;
static BaseX::DROManager   g_dro;
static float g_camAngle = 0;
static float g_time      = 0;

#ifdef BASEX_HAVE_BULLET3
static BaseX::PhysicsWorld g_world;
static bool                g_bullet = false;
#endif

static void getBaseColor(int base, float out[3]) {
    switch (base) {
        case 64: out[0]=1; out[1]=.2f; out[2]=.2f; break;
        case 32: out[0]=1; out[1]=1;   out[2]=.2f; break;
        case 16: out[0]=.2f; out[1]=1; out[2]=.2f; break;
        case 8:  out[0]=.2f; out[1]=.5f;out[2]=1;  break;
        default: out[0]=.5f; out[1]=.5f;out[2]=.5f;break;
    }
}

static void spawn(float x, float y, float z) {
    Cube c{};
    c.pos[0] = x; c.pos[1] = y; c.pos[2] = z;
    c.vel[0] = (rand()%100-50)/50.0f;
    c.vel[1] = 0;
    c.vel[2] = (rand()%100-50)/50.0f;
    c.size   = 0.5f;
    c.base   = 16;
    getBaseColor(16, c.color);
    c.alive  = true;

#ifdef BASEX_HAVE_BULLET3
    if (g_bullet) {
        float box[3] = {c.size, c.size, c.size};
        BaseX::RigidBodyParams p;
        p.position[0]=x; p.position[1]=y; p.position[2]=z;
        p.mass=1; p.restitution=0.6f; p.friction=0.4f;
        c.body = g_world.createRigidBody(BaseX::CollisionShapeType::Box, box, p);
    }
#endif
    g_cubes.push_back(c);
}

static void resetSim() {
#ifdef BASEX_HAVE_BULLET3
    for (auto& c : g_cubes) if (c.body) g_world.removeRigidBody(c.body);
#endif
    g_cubes.clear();
    g_time = 0;
    // ring of cubes at varying heights
    for (int i = 0; i < 15; i++) {
        float a = i * 6.2831853f / 15.0f;
        float r = 3.0f + i * 2.0f;
        spawn(cosf(a)*r, 8.0f + i*1.5f, sinf(a)*r);
    }
}

static void update(float dt) {
    g_time += dt;
    g_camAngle += dt * 0.15f;

    float camX = cosf(g_camAngle) * 30.0f;
    float camZ = sinf(g_camAngle) * 30.0f;
    float camY = 15.0f;

#ifdef BASEX_HAVE_BULLET3
    if (g_bullet) g_world.stepSimulation(dt);
#endif

    for (auto& c : g_cubes) {
        if (!c.alive) continue;

#ifdef BASEX_HAVE_BULLET3
        if (g_bullet && c.body) {
            g_world.getBodyPosition(c.body, c.pos);
        } else
#endif
        {
            c.vel[1] -= 9.81f * dt;
            c.pos[0] += c.vel[0] * dt;
            c.pos[1] += c.vel[1] * dt;
            c.pos[2] += c.vel[2] * dt;
            if (c.pos[1] < c.size) {
                c.pos[1] = c.size;
                c.vel[1] = -c.vel[1] * 0.65f;
                c.vel[0] *= 0.92f;
                c.vel[2] *= 0.92f;
            }
        }

        float dx = c.pos[0]-camX, dy = c.pos[1]-camY, dz = c.pos[2]-camZ;
        float dist = sqrtf(dx*dx + dy*dy + dz*dz);
        int newBase = (int)g_dro.getOptimalRadix(dist);
        if (newBase != c.base) {
            c.base = newBase;
            getBaseColor(newBase, c.color);
#ifdef BASEX_HAVE_BULLET3
            if (g_bullet && c.body) g_world.updateBodyPrecision(c.body, dist);
#endif
        }
        if (c.pos[1] < -20) c.alive = false;
    }

    g_cubes.erase(std::remove_if(g_cubes.begin(), g_cubes.end(),
                  [](const Cube& c){ return !c.alive; }), g_cubes.end());
}

static void drawCube(float x, float y, float z, float s, const float col[3]) {
    glPushMatrix();
    glTranslatef(x, y, z);

    // Faces
    glColor3f(col[0], col[1], col[2]);
    glBegin(GL_QUADS);
    float v[][3]={{-s,-s,s},{s,-s,s},{s,s,s},{-s,s,s},
                  {-s,-s,-s},{-s,s,-s},{s,s,-s},{s,-s,-s}};
    int faces[][4]={{0,1,2,3},{7,6,5,4},{3,2,6,5},{4,7,1,0},{4,0,3,5},{1,7,6,2}};
    for (auto& f : faces)
        for (int i = 0; i < 4; i++) glVertex3fv(v[f[i]]);
    glEnd();

    // Wireframe edges
    glColor3f(col[0]*0.5f, col[1]*0.5f, col[2]*0.5f);
    glBegin(GL_LINES);
    int edges[][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},
                    {0,4},{1,7},{2,6},{3,5}};
    for (auto& e : edges) { glVertex3fv(v[e[0]]); glVertex3fv(v[e[1]]); }
    glEnd();

    glPopMatrix();
}

static void drawGround() {
    glColor3f(0.25f, 0.25f, 0.28f);
    glBegin(GL_QUADS);
    glVertex3f(-60,0,-60); glVertex3f(60,0,-60);
    glVertex3f(60,0,60);   glVertex3f(-60,0,60);
    glEnd();
    glColor3f(0.35f, 0.35f, 0.38f);
    glBegin(GL_LINES);
    for (int i = -6; i <= 6; i++) {
        glVertex3f(i*10.f,0.01f,-60); glVertex3f(i*10.f,0.01f,60);
        glVertex3f(-60,0.01f,i*10.f); glVertex3f(60,0.01f,i*10.f);
    }
    glEnd();
}

static void render(int w, int h) {
    glViewport(0, 0, w, h);
    glClearColor(0.08f, 0.08f, 0.12f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 3-D scene
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    perspectiveGL(60, (double)w/h, 0.1, 500);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float cx = cosf(g_camAngle)*30, cz = sinf(g_camAngle)*30;
    lookAtGL(cx, 15, cz, 0, 2, 0, 0, 1, 0);

    glEnable(GL_DEPTH_TEST);
    drawGround();
    for (auto& c : g_cubes)
        if (c.alive) drawCube(c.pos[0], c.pos[1], c.pos[2], c.size, c.color);

    // 2-D legend (colored quads, no text API needed)
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, 0, h, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    struct { float r,g,b; const char* label; } legend[] = {
        {1,.2f,.2f,"Base-64"},{1,1,.2f,"Base-32"},{.2f,1,.2f,"Base-16"},
        {.2f,.5f,1,"Base-8"},{.5f,.5f,.5f,"Base-4"}
    };
    for (int i = 0; i < 5; i++) {
        float y = (float)(h - 20 - i*18);
        glColor3f(legend[i].r, legend[i].g, legend[i].b);
        glBegin(GL_QUADS);
        glVertex2f(10, y); glVertex2f(26, y);
        glVertex2f(26, y-12); glVertex2f(10, y-12);
        glEnd();
    }

    // Title bar
    glColor3f(0.15f, 0.15f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(0,(float)h); glVertex2f((float)w,(float)h);
    glVertex2f((float)w,(float)(h-4)); glVertex2f(0,(float)(h-4));
    glEnd();

    glEnable(GL_DEPTH_TEST);
}

static void keyCallback(GLFWwindow* win, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(win, 1);
    if (key == GLFW_KEY_SPACE) {
        float a = (rand()%360)*3.14159f/180.f;
        spawn(cosf(a)*(3+rand()%15), 10.f+rand()%12, sinf(a)*(3+rand()%15));
    }
    if (key == GLFW_KEY_R) resetSim();
}

int main() {
    if (!glfwInit()) return 1;
    GLFWwindow* win = glfwCreateWindow(1024, 768,
        "PHY-001  Base-X Physics Showcase", nullptr, nullptr);
    if (!win) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    glfwSetKeyCallback(win, keyCallback);
    glfwSwapInterval(1);

    g_dro.initializeVRProfile();

#ifdef BASEX_HAVE_BULLET3
    {
        BaseX::PhysicsConfig cfg;
        cfg.enableDRO = true;
        if (g_world.initialize(cfg)) {
            g_world.attachDROManager(&g_dro);
            g_bullet = true;
            // ground plane
            float ground[3] = {60,0.1f,60};
            BaseX::RigidBodyParams gp;
            gp.mass = 0; gp.friction = 0.8f; gp.restitution = 0.5f;
            g_world.createRigidBody(BaseX::CollisionShapeType::Box, ground, gp);
        }
    }
    printf("Physics: %s\n", g_bullet ? "Bullet3" : "fallback");
#else
    printf("Physics: fallback (Bullet3 not compiled)\n");
#endif

    resetSim();
    double prev = glfwGetTime();

    while (!glfwWindowShouldClose(win)) {
        double now = glfwGetTime();
        float dt = (float)(now - prev);
        prev = now;
        if (dt > 0.05f) dt = 0.05f;

        update(dt);

        int w, h;
        glfwGetFramebufferSize(win, &w, &h);
        render(w, h);

        glfwSwapBuffers(win);
        glfwPollEvents();

        // auto-spawn every ~3 seconds
        if (fmodf(g_time, 3.0f) < dt && g_cubes.size() < 40) {
            float a = g_time;
            spawn(cosf(a)*10, 15, sinf(a)*10);
        }
    }

#ifdef BASEX_HAVE_BULLET3
    for (auto& c : g_cubes) if (c.body) g_world.removeRigidBody(c.body);
    g_world.shutdown();
#endif
    glfwTerminate();
    return 0;
}
