/**
 * SRG-001: Scalpel Incision — Base-X Engine
 * Converted from html_demos/SRG-001_Scalpel_Incision.html
 *
 * Cross-section view of tissue layers. Mouse controls scalpel depth.
 * DRO precision determines how accurately you can control depth.
 * Base-64 = sub-mm control.  Base-16 = dangerous overshoot.
 *
 * Tissue layers (mm):
 *   Epidermis 0-0.1  |  Dermis 0.1-2  |  Subcutaneous 2-8
 *   Fascia 8-10  |  Muscle 10-25  |  VESSEL 11.5-12.5 (DANGER)
 *
 * Controls:
 *   Hold LEFT MOUSE   cut (depth follows cursor Y)
 *   Q/E               lower/raise precision base
 *   1-4               procedure preset
 *   R                 reset incision
 *   ESC               quit
 */

#include <GLFW/glfw3.h>
#include <BaseX/DRO.h>
#include <BaseX/GLText.h>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <random>

static constexpr float PI = 3.14159265f;

// ── tissue layers ──────────────────────────────────────────────────
struct Layer { const char*name; float start,end; float col[3]; bool danger; };
static const Layer LAYERS[] = {
    {"Epidermis",    0,    0.1f, {0.93f,0.87f,0.80f}, false},
    {"Dermis",       0.1f, 2.0f, {0.87f,0.67f,0.60f}, false},
    {"Subcutaneous", 2.0f, 8.0f, {1.00f,0.87f,0.53f}, false},
    {"Fascia",       8.0f,10.0f, {0.67f,0.73f,0.80f}, false},
    {"Muscle",      10.0f,25.0f, {0.80f,0.40f,0.40f}, false},
    {"VESSEL",      11.5f,12.5f, {0.27f,0.40f,0.67f}, true},
};
static constexpr int LAYER_COUNT = 6;

// ── procedures ─────────────────────────────────────────────────────
struct Procedure { const char*name; float target,maxD,tolerance; };
static const Procedure PROCS[] = {
    {"Skin Incision",    3.0f, 4.0f, 0.5f},
    {"Fascial Release",  8.0f,10.0f, 0.3f},
    {"Muscle Dissection",15.0f,20.0f,0.5f},
    {"Vessel Exposure",  11.0f,11.5f,0.1f},
};

// ── incision point ─────────────────────────────────────────────────
struct IncPt { float x, depth; };

// ── globals ────────────────────────────────────────────────────────
static BaseX::DROManager g_dro;
static std::mt19937 g_rng(42);
static std::uniform_real_distribution<float> g_r01(0,1);

static int   g_base = 64;
static int   g_proc = 0;
static float g_depth = 0;
static bool  g_cutting = false;
static float g_cursorY = 0;  // normalized 0=top 1=bottom
static float g_cursorX = 0.5f;
static std::vector<IncPt> g_incHi, g_incLo;  // high-base / low-base tracks
static int   g_complications = 0;
static float g_time = 0;
static int   g_winW = 1100, g_winH = 720;
static int   g_fps = 0;
static float g_depthHi = 0, g_depthLo = 0;
static bool  g_vesselHitHi = false, g_vesselHitLo = false;

static float precError(int base) {
    switch(base){
        case 64: return 0.001f;
        case 48: return 0.005f;
        case 32: return 0.02f;
        case 24: return 0.05f;
        case 16: return 0.10f;
        case 8:  return 0.25f;
        default: return 0.50f;
    }
}

static const Layer* layerAt(float d) {
    for (int i = 0; i < LAYER_COUNT; i++)
        if (d >= LAYERS[i].start && d < LAYERS[i].end) return &LAYERS[i];
    return &LAYERS[4]; // muscle
}

static void resetIncision() {
    g_incHi.clear(); g_incLo.clear();
    g_depthHi = g_depthLo = 0;
    g_complications = 0;
    g_vesselHitHi = g_vesselHitLo = false;
}

// ── update ─────────────────────────────────────────────────────────
static void update(float dt) {
    g_time += dt;
    if (!g_cutting) return;

    float targetDepth = g_cursorY * 25.0f; // 0-25mm range
    float pressure = 0.5f;
    float rate = 0.05f * pressure;

    // High-precision side
    {
        float errHi = precError(g_base);
        float noise = (g_r01(g_rng)-0.5f) * errHi * 2;
        float newD = roundf((g_depthHi + (targetDepth - g_depthHi)*rate + noise) / errHi) * errHi;
        g_depthHi = std::max(0.0f, newD);
        g_incHi.push_back({g_cursorX, g_depthHi});
        if (g_incHi.size() > 2000) g_incHi.erase(g_incHi.begin());

        const Layer* l = layerAt(g_depthHi);
        if (l->danger && !g_vesselHitHi) { g_vesselHitHi = true; }
    }

    // Low-precision side (always Base-16)
    {
        float errLo = precError(16);
        float noise = (g_r01(g_rng)-0.5f) * errLo * 2;
        float newD = roundf((g_depthLo + (targetDepth - g_depthLo)*rate + noise) / errLo) * errLo;
        g_depthLo = std::max(0.0f, newD);
        g_incLo.push_back({g_cursorX, g_depthLo});
        if (g_incLo.size() > 2000) g_incLo.erase(g_incLo.begin());

        const Layer* l = layerAt(g_depthLo);
        if (l->danger && !g_vesselHitLo) { g_vesselHitLo = true; g_complications++; }
    }
}

// ── draw helpers ───────────────────────────────────────────────────
static void drawRect(float x,float y,float w,float h) {
    glBegin(GL_QUADS);
    glVertex2f(x,y);glVertex2f(x+w,y);glVertex2f(x+w,y+h);glVertex2f(x,y+h);
    glEnd();
}

static float depthToY(float depth, float top, float height) {
    return top + height - (depth / 25.0f) * height;
}

// ── render one tissue cross-section panel ──────────────────────────
static void renderPanel(float px, float py, float pw, float ph,
                        const std::vector<IncPt>& inc, float curDepth,
                        int base, const char* label, bool vesselHit) {
    // Clip
    glEnable(GL_SCISSOR_TEST);
    glScissor((int)px, (int)py, (int)pw, (int)ph);

    // Background
    glColor4f(0.05f,0.06f,0.08f,1);
    drawRect(px, py, pw, ph);

    float tissueTop = py + 30;
    float tissueH = ph - 60;

    // Draw layers
    for (int i = 0; i < LAYER_COUNT; i++) {
        float y0 = depthToY(LAYERS[i].end, tissueTop, tissueH);
        float y1 = depthToY(LAYERS[i].start, tissueTop, tissueH);
        glColor4f(LAYERS[i].col[0], LAYERS[i].col[1], LAYERS[i].col[2],
                  LAYERS[i].danger ? 0.6f : 0.85f);
        drawRect(px, y0, pw, y1-y0);

        // Layer boundary
        glColor4f(0,0,0,0.3f);
        glBegin(GL_LINES);
        glVertex2f(px, y0); glVertex2f(px+pw, y0);
        glEnd();

        // Danger zone
        if (LAYERS[i].danger) {
            glColor4f(1,0,0, 0.15f);
            drawRect(px, y0, pw, y1-y0);
            glColor4f(1,0,0,0.5f);
            glLineWidth(2);
            glBegin(GL_LINES);
            // Dashed horizontal lines
            for (float dx = px; dx < px+pw; dx += 10) {
                glVertex2f(dx, y0); glVertex2f(dx+5, y0);
                glVertex2f(dx, y1); glVertex2f(dx+5, y1);
            }
            glEnd();
        }

        // Layer name
        if (y1-y0 > 12) {
            glColor4f(0,0,0,0.5f);
            gltext::draw(LAYERS[i].name, px+4, y0+4, 0.6f);
        }
    }

    // Precision grid
    float err = precError(base);
    float gridStep = std::max(3.0f, err * tissueH / 25.0f * 10);
    glColor4f(err<0.01f ? 0:1, err<0.01f ? 1:0.3f, err<0.01f ? 0.5f:0.3f, 0.08f);
    for (float gy = tissueTop; gy < tissueTop+tissueH; gy += gridStep) {
        glBegin(GL_LINES); glVertex2f(px, gy); glVertex2f(px+pw, gy); glEnd();
    }

    // Target depth line
    float targetY = depthToY(PROCS[g_proc].target, tissueTop, tissueH);
    glColor4f(0, 1, 0.5f, 0.5f);
    glLineWidth(2);
    glBegin(GL_LINES); glVertex2f(px, targetY); glVertex2f(px+pw, targetY); glEnd();
    glColor4f(0, 1, 0.5f, 0.8f);
    gltext::draw("TARGET", px+pw-55, targetY+2, 0.5f);

    // Max depth line
    float maxY = depthToY(PROCS[g_proc].maxD, tissueTop, tissueH);
    glColor4f(1, 0.3f, 0.2f, 0.5f);
    glBegin(GL_LINES); glVertex2f(px, maxY); glVertex2f(px+pw, maxY); glEnd();
    glColor4f(1, 0.3f, 0.2f, 0.8f);
    gltext::draw("MAX", px+pw-35, maxY+2, 0.5f);

    // Incision trail
    if (inc.size() > 1) {
        glColor4f(0.73f, 0.13f, 0.2f, 0.9f);
        glLineWidth(3);
        glBegin(GL_LINE_STRIP);
        for (auto& p : inc) {
            float ix = px + p.x * pw;
            float iy = depthToY(p.depth, tissueTop, tissueH);
            glVertex2f(ix, iy);
        }
        glEnd();
    }

    // Current depth indicator
    float cdY = depthToY(curDepth, tissueTop, tissueH);
    glColor4f(1, 1, 1, 0.8f);
    glLineWidth(1);
    glBegin(GL_LINES); glVertex2f(px, cdY); glVertex2f(px+pw, cdY); glEnd();

    // Scalpel cursor
    if (g_cutting) {
        float sx = px + g_cursorX * pw;
        glColor4f(0.8f, 0.9f, 1, 0.9f);
        glLineWidth(2);
        glBegin(GL_LINES);
        glVertex2f(sx, cdY-15); glVertex2f(sx, cdY+5);
        glVertex2f(sx-3, cdY-15); glVertex2f(sx, cdY-20);
        glVertex2f(sx+3, cdY-15); glVertex2f(sx, cdY-20);
        glEnd();
    }

    // Label
    float bc[3];
    switch(base){
        case 64: bc[0]=0;bc[1]=1;bc[2]=.5f; break;
        case 48: bc[0]=.5f;bc[1]=1;bc[2]=.3f; break;
        case 32: bc[0]=1;bc[1]=1;bc[2]=.2f; break;
        default: bc[0]=1;bc[1]=.4f;bc[2]=.2f; break;
    }
    glColor4f(bc[0],bc[1],bc[2],1);
    char buf[64];
    snprintf(buf,64,"%s  BASE-%d", label, base);
    gltext::draw(buf, px+4, py+ph-14, 0.9f);

    // Depth readout
    glColor4f(1,1,1,0.9f);
    snprintf(buf,64,"%.2f mm", curDepth);
    gltext::drawRight(buf, px+pw-4, py+ph-14, 0.9f);

    // Vessel hit warning
    if (vesselHit) {
        glColor4f(1, 0.1f, 0.1f, 0.6f + sinf(g_time*6)*0.3f);
        gltext::drawCentered("VESSEL BREACH", px+pw/2, py+ph/2, 1.5f);
    }

    glDisable(GL_SCISSOR_TEST);
}

static void render() {
    glViewport(0,0,g_winW,g_winH);
    glClearColor(0.04f,0.04f,0.07f,1);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    float m[16]={}; m[0]=2.f/g_winW;m[5]=2.f/g_winH;m[10]=-1;m[12]=-1;m[13]=-1;m[15]=1;
    glLoadMatrixf(m);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    // Left panel (high precision)
    float hw = (g_winW - 190) / 2.0f;
    renderPanel(185, 0, hw-2, (float)g_winH, g_incHi, g_depthHi, g_base, "HIGH", g_vesselHitHi);

    // Right panel (low precision = Base-16)
    renderPanel(185+hw+2, 0, hw-2, (float)g_winH, g_incLo, g_depthLo, 16, "LOW", g_vesselHitLo);

    // Divider
    glColor4f(0,0.87f,0.93f,0.4f);
    drawRect(185+hw-1, 0, 2, (float)g_winH);

    // ── Side panel ────────────────────────────────────────────────
    glColor4f(0.02f,0.03f,0.06f,0.9f);
    drawRect(0, 0, 183, (float)g_winH);
    glColor4f(0,0.87f,0.93f,0.3f);
    glBegin(GL_LINES);glVertex2f(183,0);glVertex2f(183,(float)g_winH);glEnd();

    float py = g_winH - 18.0f;
    char buf[128];

    glColor4f(0,0.87f,0.93f,1);
    gltext::draw("SCALPEL SIM", 8, py, 1.2f); py-=18;

    // Procedure
    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("PROCEDURE:", 8, py, 0.7f); py-=12;
    glColor4f(1,0.85f,0.2f,1);
    gltext::draw(PROCS[g_proc].name, 8, py, 0.8f); py-=16;

    // Target
    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("TARGET:", 8, py, 0.7f);
    glColor4f(0,1,0.5f,1);
    snprintf(buf,128,"%.1f mm", PROCS[g_proc].target);
    gltext::draw(buf, 65, py, 0.7f); py-=12;

    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("MAX:", 8, py, 0.7f);
    glColor4f(1,0.3f,0.2f,1);
    snprintf(buf,128,"%.1f mm", PROCS[g_proc].maxD);
    gltext::draw(buf, 40, py, 0.7f); py-=12;

    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("TOLERANCE:", 8, py, 0.7f);
    glColor4f(.8f,.8f,.9f,1);
    snprintf(buf,128,"%.1f mm", PROCS[g_proc].tolerance);
    gltext::draw(buf, 90, py, 0.7f); py-=18;

    // Divider
    glColor4f(0,0.87f,0.93f,0.2f);
    glBegin(GL_LINES);glVertex2f(8,py);glVertex2f(175,py);glEnd(); py-=8;

    // Precision
    glColor4f(0,0.87f,0.93f,0.8f);
    gltext::draw("PRECISION", 8, py, 0.8f); py-=14;

    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("HIGH SIDE:", 8, py, 0.7f);
    float bc[3];
    switch(g_base){
        case 64: bc[0]=0;bc[1]=1;bc[2]=.5f; break;
        case 48: bc[0]=.5f;bc[1]=1;bc[2]=.3f; break;
        case 32: bc[0]=1;bc[1]=1;bc[2]=.2f; break;
        default: bc[0]=1;bc[1]=.4f;bc[2]=.2f; break;
    }
    glColor4f(bc[0],bc[1],bc[2],1);
    snprintf(buf,128,"BASE-%d", g_base);
    gltext::draw(buf, 90, py, 0.7f); py-=12;

    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("ERROR:", 8, py, 0.7f);
    float e = precError(g_base);
    glColor4f(.8f,.8f,.9f,1);
    if (e>=0.01f) snprintf(buf,128,"%.0f mm", e*1000);
    else snprintf(buf,128,"%.1f um", e*1e6f);
    gltext::draw(buf, 55, py, 0.7f); py-=12;

    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("LOW SIDE:", 8, py, 0.7f);
    glColor4f(1,.4f,.2f,1);
    gltext::draw("BASE-16", 90, py, 0.7f); py-=18;

    // Divider
    glColor4f(0,0.87f,0.93f,0.2f);
    glBegin(GL_LINES);glVertex2f(8,py);glVertex2f(175,py);glEnd(); py-=8;

    // Results
    glColor4f(0,0.87f,0.93f,0.8f);
    gltext::draw("RESULTS", 8, py, 0.8f); py-=16;

    // Grade
    float errHi = fabsf(g_depthHi - PROCS[g_proc].target);
    const char* grade = "A+";
    if (errHi > PROCS[g_proc].tolerance*0.5f) grade = "A";
    if (errHi > PROCS[g_proc].tolerance) grade = "B";
    if (errHi > PROCS[g_proc].tolerance*2) grade = "C";
    if (errHi > PROCS[g_proc].tolerance*3) grade = "F";
    if (g_vesselHitHi) grade = "F";

    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("GRADE:", 8, py, 0.7f);
    bool good = (grade[0]=='A');
    glColor4f(good?0:1, good?1:0.3f, good?0.5f:0.2f, 1);
    gltext::draw(grade, 55, py, 1.0f); py-=14;

    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("DEPTH HI:", 8, py, 0.7f);
    glColor4f(.8f,.8f,.9f,1);
    snprintf(buf,128,"%.2f mm", g_depthHi);
    gltext::draw(buf, 80, py, 0.7f); py-=12;

    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("DEPTH LO:", 8, py, 0.7f);
    glColor4f(.8f,.8f,.9f,1);
    snprintf(buf,128,"%.2f mm", g_depthLo);
    gltext::draw(buf, 80, py, 0.7f); py-=14;

    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("ISSUES:", 8, py, 0.7f);
    glColor4f(g_complications==0?0:1, g_complications==0?1:0.3f, 0.2f, 1);
    snprintf(buf,128,"%d", g_complications);
    gltext::draw(buf, 65, py, 0.7f); py-=18;

    // Controls
    glColor4f(.4f,.4f,.5f,1);
    gltext::draw("HOLD MOUSE CUT", 8, 64, 0.6f);
    gltext::draw("1-4 PROCEDURE", 8, 54, 0.6f);
    gltext::draw("Q/E BASE", 8, 44, 0.6f);
    gltext::draw("R RESET", 8, 34, 0.6f);

    glColor4f(.4f,.4f,.5f,1);
    snprintf(buf,128,"FPS %d", g_fps);
    gltext::draw(buf, 8, 8, 0.6f);

    glDisable(GL_BLEND);
}

// ── callbacks ──────────────────────────────────────────────────────
static void keyCB(GLFWwindow*w,int key,int,int act,int){
    if(act==GLFW_PRESS){
        if(key==GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w,1);
        if(key==GLFW_KEY_R) resetIncision();
        if(key>=GLFW_KEY_1&&key<=GLFW_KEY_4){g_proc=key-GLFW_KEY_1;resetIncision();}
    }
    if(act==GLFW_PRESS||act==GLFW_REPEAT){
        int bases[]={4,8,16,24,32,48,64};
        if(key==GLFW_KEY_Q){for(int i=6;i>=0;i--)if(bases[i]<g_base){g_base=bases[i];break;}}
        if(key==GLFW_KEY_E){for(int i=0;i<7;i++)if(bases[i]>g_base){g_base=bases[i];break;}}
    }
}
static void mbtnCB(GLFWwindow*,int b,int a,int){
    if(b==GLFW_MOUSE_BUTTON_LEFT) g_cutting=(a==GLFW_PRESS);
}
static void cursorCB(GLFWwindow*,double x,double y){
    g_cursorX = (float)(x - 185) / (g_winW - 190);
    g_cursorX = std::clamp(g_cursorX, 0.0f, 0.5f) * 2; // normalize to left panel
    g_cursorY = (float)y / g_winH;
    g_cursorY = std::clamp(g_cursorY, 0.0f, 1.0f);
}

int main(){
    if(!glfwInit()) return 1;
    GLFWwindow*win=glfwCreateWindow(1100,720,"SRG-001 Scalpel Incision | Base-X Engine",0,0);
    if(!win){glfwTerminate();return 1;}
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    glfwSetKeyCallback(win,keyCB);
    glfwSetMouseButtonCallback(win,mbtnCB);
    glfwSetCursorPosCallback(win,cursorCB);
    g_dro.initializeVRProfile();

    printf("SRG-001 Scalpel Incision\n");
    printf("Hold left mouse to cut | Q/E base | 1-4 procedure | R reset\n");
    printf("1=Skin  2=Fascia  3=Muscle  4=Vessel Exposure (hardest)\n");

    double prev=glfwGetTime();
    int fc=0; double ft=prev;
    while(!glfwWindowShouldClose(win)){
        glfwGetFramebufferSize(win,&g_winW,&g_winH);
        double now=glfwGetTime();
        float dt=std::min((float)(now-prev),0.05f);
        prev=now;
        fc++;
        if(now-ft>0.5){g_fps=(int)(fc/(now-ft));fc=0;ft=now;}
        update(dt);
        render();
        glfwSwapBuffers(win);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}
