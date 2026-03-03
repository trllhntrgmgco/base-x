/**
 * NEC-002: Melee Combat Precision — Base-X Engine
 * Converted from html_demos/NEC-002_Melee_Combat_Precision.html
 *
 * 3D combat dummy with hit zones. Weapon swings with DRO precision.
 * Higher base = tighter collision = clean hits.
 * Lower base = quantization error = ghost hits / misses.
 *
 * Controls:
 *   SPACE   swing weapon       1-4  select weapon
 *   Q/E     raise/lower base   W/S  move target fwd/back
 *   Mouse   orbit camera       Scroll zoom
 *   R       reset stats        ESC  quit
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

static constexpr float PI = 3.14159265f;

// ── helpers ────────────────────────────────────────────────────────
static void perspGL(double fov,double a,double n,double f){
    double t=1/tan(fov*PI/360); double m[16]={};
    m[0]=t/a;m[5]=t;m[10]=(f+n)/(n-f);m[11]=-1;m[14]=2*f*n/(n-f);
    glMultMatrixd(m);
}
static void lookGL(float ex,float ey,float ez,float tx,float ty,float tz){
    float fx=tx-ex,fy=ty-ey,fz=tz-ez;
    float fl=sqrtf(fx*fx+fy*fy+fz*fz); fx/=fl;fy/=fl;fz/=fl;
    float sx=-fz, sy=0, sz=fx;
    float sl=sqrtf(sx*sx+sz*sz); if(sl>0){sx/=sl;sz/=sl;}
    float ux=sy*fz-sz*fy, uy=sz*fx-sx*fz, uz=sx*fy-sy*fx;
    float m[16]={sx,ux,-fx,0, sy,uy,-fy,0, sz,uz,-fz,0, 0,0,0,1};
    glMultMatrixf(m); glTranslatef(-ex,-ey,-ez);
}

// ── weapons ────────────────────────────────────────────────────────
struct Weapon { const char*name; float damage,speed,range,width; };
static const Weapon WEAPONS[] = {
    {"Longsword", 25, 1.0f, 2.5f, 0.08f},
    {"Battle Axe",40, 0.6f, 2.0f, 0.25f},
    {"Dagger",    15, 1.8f, 1.2f, 0.05f},
    {"War Mace",  30, 0.8f, 1.8f, 0.30f},
};

// ── skeleton hit zones ─────────────────────────────────────────────
struct Bone { const char*name; float x,y,z,radius,mult; float col[3]; };
static const Bone SKELETON[] = {
    {"Head",     0, 2.8f, 0, 0.18f, 3.0f, {1,.2f,.2f}},
    {"Neck",     0, 2.5f, 0, 0.08f, 2.5f, {1,.4f,.27f}},
    {"Torso",    0, 1.8f, 0, 0.30f, 1.0f, {1,.53f,.33f}},
    {"Pelvis",   0, 1.2f, 0, 0.22f, 0.9f, {1,.6f,.4f}},
    {"L.Arm",   -.45f,2.1f,0,0.12f, 0.7f, {1,.67f,.4f}},
    {"R.Arm",    .45f,2.1f,0,0.12f, 0.7f, {1,.67f,.4f}},
    {"L.Hand",  -.6f,1.6f,0,0.08f, 0.5f, {1,.73f,.47f}},
    {"R.Hand",   .6f,1.6f,0,0.08f, 0.5f, {1,.73f,.47f}},
    {"L.Leg",   -.18f,0.6f,0,0.14f,0.8f, {1,.8f,.47f}},
    {"R.Leg",    .18f,0.6f,0,0.14f,0.8f, {1,.8f,.47f}},
    {"L.Foot",  -.2f,0.1f,0,0.10f, 0.4f, {1,.87f,.53f}},
    {"R.Foot",   .2f,0.1f,0,0.10f, 0.4f, {1,.87f,.53f}},
};
static constexpr int BONE_COUNT = 12;

// ── hit flash ──────────────────────────────────────────────────────
struct HitFlash { int bone; float timer; float damage; bool ghost; };

// ── globals ────────────────────────────────────────────────────────
static BaseX::DROManager g_dro;
static std::mt19937 g_rng(42);
static std::uniform_real_distribution<float> g_r01(0,1);

static int   g_weapon = 0;
static int   g_base = 48;
static float g_targetZ = 0;
static float g_swingAngle = -999;   // <-PI/2 = not swinging
static float g_swingSpeed = 1;
static bool  g_swinging = false;

static int   g_swings=0, g_hits=0, g_misses=0, g_ghosts=0, g_crits=0;
static float g_totalDmg = 0;
static std::vector<HitFlash> g_flashes;
static bool  g_boneHit[BONE_COUNT] = {};

static float g_camDist=6, g_camYaw=30, g_camPitch=20;
static bool  g_mouseDown=false;
static double g_mx,g_my;
static int   g_winW=1024, g_winH=720;
static int   g_fps=0;
static float g_time=0;

// ── precision error for a given base ───────────────────────────────
static float precisionError(int base) {
    // Higher base = less error. Base-64 ~ 0.001m, Base-4 ~ 0.5m
    switch(base) {
        case 64: return 0.001f;
        case 48: return 0.005f;
        case 32: return 0.02f;
        case 24: return 0.05f;
        case 16: return 0.1f;
        case 8:  return 0.25f;
        default: return 0.5f;
    }
}

// ── swing logic ────────────────────────────────────────────────────
static void startSwing() {
    g_swinging = true;
    g_swingAngle = -PI/2;
    g_swings++;
    memset(g_boneHit, 0, sizeof(g_boneHit));
}

static void updateSwing(float dt) {
    if (!g_swinging) return;

    float speed = WEAPONS[g_weapon].speed * 4.0f;
    g_swingAngle += speed * dt;

    if (g_swingAngle > PI/2) {
        g_swinging = false;
        // Check if any hits happened
        bool anyHit = false;
        for (int i = 0; i < BONE_COUNT; i++) if (g_boneHit[i]) anyHit = true;
        if (!anyHit) g_misses++;
        return;
    }

    // Weapon tip position (swing arc from left to right)
    const Weapon& w = WEAPONS[g_weapon];
    float tipX = cosf(g_swingAngle) * w.range;
    float tipY = 1.8f + sinf(g_swingAngle) * 0.5f; // slight vertical arc
    float tipZ = g_targetZ;

    // Add precision error
    float err = precisionError(g_base);
    float errX = (g_r01(g_rng)-0.5f) * err * 2;
    float errY = (g_r01(g_rng)-0.5f) * err * 2;
    float errZ = (g_r01(g_rng)-0.5f) * err * 2;

    float checkX = tipX + errX;
    float checkY = tipY + errY;
    float checkZ = tipZ + errZ;

    // Quantize to precision grid
    float qX = roundf(checkX / err) * err;
    float qY = roundf(checkY / err) * err;
    float qZ = roundf(checkZ / err) * err;

    // Check each bone
    for (int i = 0; i < BONE_COUNT; i++) {
        if (g_boneHit[i]) continue;
        const Bone& b = SKELETON[i];

        float bx = b.x, by = b.y, bz = g_targetZ + b.z;
        float qbx = roundf(bx/err)*err;
        float qby = roundf(by/err)*err;
        float qbz = roundf(bz/err)*err;

        float dx=qX-qbx, dy=qY-qby, dz=qZ-qbz;
        float dist = sqrtf(dx*dx+dy*dy+dz*dz);
        float hitR = b.radius + w.width;

        if (dist < hitR) {
            g_boneHit[i] = true;
            float dmg = w.damage * b.mult;

            // Check if it's a "ghost hit" (precision error caused false positive)
            float realDx = tipX-b.x, realDy = tipY-b.y, realDz = tipZ-(g_targetZ+b.z);
            float realDist = sqrtf(realDx*realDx+realDy*realDy+realDz*realDz);

            bool ghost = (realDist > hitR * 1.3f);
            if (ghost) { g_ghosts++; dmg *= 0.3f; }
            else { g_hits++; if (b.mult >= 2.5f) g_crits++; }

            g_totalDmg += dmg;
            g_flashes.push_back({i, 1.0f, dmg, ghost});
        }
    }
}

// ── 3D drawing ─────────────────────────────────────────────────────
static void drawSphere(float cx,float cy,float cz,float r,int slices,int stacks){
    for(int i=0;i<stacks;i++){
        float t0=PI*i/stacks, t1=PI*(i+1)/stacks;
        glBegin(GL_QUAD_STRIP);
        for(int j=0;j<=slices;j++){
            float p=2*PI*j/slices;
            glVertex3f(cx+r*sinf(t0)*cosf(p), cy+r*cosf(t0), cz+r*sinf(t0)*sinf(p));
            glVertex3f(cx+r*sinf(t1)*cosf(p), cy+r*cosf(t1), cz+r*sinf(t1)*sinf(p));
        }
        glEnd();
    }
}

static void drawCube3D(float x,float y,float z,float s){
    glPushMatrix(); glTranslatef(x,y,z);
    float v[][3]={{-s,-s,s},{s,-s,s},{s,s,s},{-s,s,s},{-s,-s,-s},{-s,s,-s},{s,s,-s},{s,-s,-s}};
    int f[][4]={{0,1,2,3},{7,6,5,4},{3,2,6,5},{4,7,1,0},{4,0,3,5},{1,7,6,2}};
    glBegin(GL_QUADS); for(auto&q:f)for(int i=0;i<4;i++)glVertex3fv(v[q[i]]); glEnd();
    glPopMatrix();
}

static void drawWeaponArc() {
    if (!g_swinging) return;
    const Weapon& w = WEAPONS[g_weapon];

    // Draw swing trail
    glLineWidth(3);
    glBegin(GL_LINE_STRIP);
    for (float a = -PI/2; a <= g_swingAngle; a += 0.05f) {
        float t = (a + PI/2) / PI;
        glColor4f(1, 0.8f, 0.2f, t*0.8f);
        float tx = cosf(a)*w.range;
        float ty = 1.8f + sinf(a)*0.5f;
        glVertex3f(tx, ty, g_targetZ);
    }
    glEnd();

    // Weapon tip
    float tx = cosf(g_swingAngle)*w.range;
    float ty = 1.8f + sinf(g_swingAngle)*0.5f;
    glColor4f(1, 0.9f, 0.3f, 1);
    drawCube3D(tx, ty, g_targetZ, w.width);
}

static void drawTarget() {
    // Draw skeleton bones as spheres
    for (int i = 0; i < BONE_COUNT; i++) {
        const Bone& b = SKELETON[i];
        float flashMul = 1.0f;
        bool isGhost = false;

        // Check flash
        for (auto& f : g_flashes) {
            if (f.bone == i && f.timer > 0) {
                flashMul = 2.0f;
                isGhost = f.ghost;
            }
        }

        if (isGhost) {
            glColor4f(0.5f, 0.5f, 1.0f, 0.5f); // blue = ghost
        } else if (flashMul > 1) {
            glColor4f(1, 1, 1, 0.9f); // white = hit
        } else {
            glColor4f(b.col[0], b.col[1], b.col[2], 0.7f);
        }

        drawSphere(b.x, b.y, g_targetZ + b.z, b.radius, 12, 8);

        // Wireframe
        glColor4f(b.col[0]*0.6f, b.col[1]*0.6f, b.col[2]*0.6f, 0.5f);
        drawSphere(b.x, b.y, g_targetZ + b.z, b.radius*1.01f, 8, 6);
    }

    // Connecting lines (skeleton)
    glColor4f(0.6f, 0.5f, 0.4f, 0.4f);
    glLineWidth(2);
    glBegin(GL_LINES);
    int bones[][2]={{0,1},{1,2},{2,3},{2,4},{2,5},{4,6},{5,7},{3,8},{3,9},{8,10},{9,11}};
    for (auto& c : bones) {
        glVertex3f(SKELETON[c[0]].x, SKELETON[c[0]].y, g_targetZ);
        glVertex3f(SKELETON[c[1]].x, SKELETON[c[1]].y, g_targetZ);
    }
    glEnd();
}

static void drawGround() {
    glColor4f(0.12f,0.14f,0.1f,1);
    glBegin(GL_QUADS);
    glVertex3f(-20,0,-20);glVertex3f(20,0,-20);glVertex3f(20,0,20);glVertex3f(-20,0,20);
    glEnd();
    glColor4f(0.18f,0.2f,0.15f,0.5f);
    glBegin(GL_LINES);
    for(int i=-4;i<=4;i++){glVertex3f(i*5.f,0.01f,-20);glVertex3f(i*5.f,0.01f,20);
        glVertex3f(-20,0.01f,i*5.f);glVertex3f(20,0.01f,i*5.f);}
    glEnd();
}

static void baseColor(int base, float c[3]) {
    switch(base){
        case 64: c[0]=1;c[1]=.15f;c[2]=.15f; break;
        case 48: c[0]=1;c[1]=.8f;c[2]=.15f; break;
        case 32: c[0]=.15f;c[1]=1;c[2]=.3f; break;
        case 24: c[0]=0;c[1]=.85f;c[2]=.9f; break;
        case 16: c[0]=.2f;c[1]=.4f;c[2]=1; break;
        case 8:  c[0]=.6f;c[1]=.2f;c[2]=.9f; break;
        default: c[0]=.45f;c[1]=.45f;c[2]=.45f; break;
    }
}

static void render() {
    glViewport(0,0,g_winW,g_winH);
    glClearColor(0.06f,0.06f,0.1f,1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    // 3D
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    perspGL(60,(double)g_winW/g_winH,0.1,200);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    float cx=cosf(g_camYaw*PI/180)*cosf(g_camPitch*PI/180)*g_camDist;
    float cy=sinf(g_camPitch*PI/180)*g_camDist + 1.5f;
    float cz=sinf(g_camYaw*PI/180)*cosf(g_camPitch*PI/180)*g_camDist;
    lookGL(cx,cy,cz, 0,1.5f,g_targetZ);

    drawGround();
    drawTarget();
    drawWeaponArc();

    // Precision error visualization: ring around target
    float err = precisionError(g_base);
    float bc[3]; baseColor(g_base, bc);
    glColor4f(bc[0],bc[1],bc[2],0.2f);
    glBegin(GL_LINE_LOOP);
    for(int i=0;i<48;i++){
        float a=2*PI*i/48;
        glVertex3f(cosf(a)*err*20, 1.8f+sinf(a)*err*20, g_targetZ);
    }
    glEnd();

    // ── 2D HUD ────────────────────────────────────────────────────
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    float m[16]={}; m[0]=2.f/g_winW;m[5]=2.f/g_winH;m[10]=-1;m[12]=-1;m[13]=-1;m[15]=1;
    glLoadMatrixf(m);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();

    // Panel
    glColor4f(0.02f,0.03f,0.06f,0.85f);
    glBegin(GL_QUADS);
    glVertex2f(0,0);glVertex2f(185,0);glVertex2f(185,(float)g_winH);glVertex2f(0,(float)g_winH);
    glEnd();
    glColor4f(0,0.87f,0.93f,0.3f);
    glBegin(GL_LINES);glVertex2f(185,0);glVertex2f(185,(float)g_winH);glEnd();

    float py = g_winH - 18.0f;
    char buf[128];

    glColor4f(0,0.87f,0.93f,1);
    gltext::draw("MELEE COMBAT", 8, py, 1.2f); py-=18;

    // Weapon
    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("WEAPON:", 8, py, 0.8f);
    glColor4f(1,0.85f,0.2f,1);
    gltext::draw(WEAPONS[g_weapon].name, 65, py, 0.8f);
    py -= 16;

    // Base
    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("PRECISION:", 8, py, 0.8f);
    baseColor(g_base, bc);
    glColor4f(bc[0],bc[1],bc[2],1);
    snprintf(buf,128,"BASE-%d", g_base);
    gltext::draw(buf, 90, py, 0.8f);
    py -= 14;

    // Error
    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("ERROR:", 8, py, 0.7f);
    glColor4f(.8f,.8f,.9f,1);
    float e = precisionError(g_base);
    if (e >= 0.01f) snprintf(buf,128,"%.0f mm", e*1000);
    else snprintf(buf,128,"%.1f mm", e*1000);
    gltext::draw(buf, 55, py, 0.7f);
    py -= 20;

    // Divider
    glColor4f(0,0.87f,0.93f,0.2f);
    glBegin(GL_LINES);glVertex2f(8,py);glVertex2f(175,py);glEnd();
    py -= 6;

    // Stats
    glColor4f(0,0.87f,0.93f,0.8f);
    gltext::draw("COMBAT STATS", 8, py, 0.8f); py-=16;

    auto stat = [&](const char* label, int val, float r, float g, float b) {
        glColor4f(.5f,.5f,.6f,1);
        gltext::draw(label, 8, py, 0.7f);
        glColor4f(r,g,b,1);
        snprintf(buf,128,"%d",val);
        gltext::draw(buf, 95, py, 0.7f);
        py -= 13;
    };

    stat("SWINGS:", g_swings, .8f,.8f,.9f);
    stat("CLEAN HITS:", g_hits, .2f,1,.3f);
    stat("GHOST HITS:", g_ghosts, .5f,.5f,1);
    stat("MISSES:", g_misses, 1,.3f,.3f);
    stat("CRITICALS:", g_crits, 1,.85f,.2f);
    py -= 4;

    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("TOTAL DMG:", 8, py, 0.7f);
    glColor4f(1,.6f,.2f,1);
    snprintf(buf,128,"%.0f", g_totalDmg);
    gltext::draw(buf, 95, py, 0.7f);
    py -= 4;

    // Accuracy
    if (g_swings > 0) {
        py -= 16;
        glColor4f(0,0.87f,0.93f,0.2f);
        glBegin(GL_LINES);glVertex2f(8,py+10);glVertex2f(175,py+10);glEnd();
        py -= 4;
        float accuracy = g_hits * 100.0f / std::max(1, g_swings);
        glColor4f(.5f,.5f,.6f,1);
        gltext::draw("ACCURACY:", 8, py, 0.8f);
        glColor4f(accuracy>70?.2f:1, accuracy>70?1:.3f, .2f, 1);
        snprintf(buf,128,"%.0f%%", accuracy);
        gltext::draw(buf, 85, py, 0.8f);
        py -= 14;

        // Accuracy bar
        glColor4f(.2f,.2f,.25f,1);
        glBegin(GL_QUADS);
        glVertex2f(8,py);glVertex2f(175,py);glVertex2f(175,py+8);glVertex2f(8,py+8);
        glEnd();
        float aw = accuracy/100.0f * 167;
        glColor4f(accuracy>70?.2f:1, accuracy>70?1:.3f, .2f, 0.8f);
        glBegin(GL_QUADS);
        glVertex2f(8,py);glVertex2f(8+aw,py);glVertex2f(8+aw,py+8);glVertex2f(8,py+8);
        glEnd();
    }

    // Controls
    glColor4f(.4f,.4f,.5f,1);
    gltext::draw("SPACE SWING", 8, 64, 0.6f);
    gltext::draw("1-4 WEAPON", 8, 54, 0.6f);
    gltext::draw("Q/E BASE", 8, 44, 0.6f);
    gltext::draw("W/S TARGET", 8, 34, 0.6f);
    gltext::draw("R RESET", 8, 24, 0.6f);

    // FPS
    glColor4f(.4f,.4f,.5f,1);
    snprintf(buf,128,"FPS %d", g_fps);
    gltext::draw(buf, 8, 8, 0.6f);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

// ── Callbacks ──────────────────────────────────────────────────────
static void keyCB(GLFWwindow*w,int key,int,int act,int){
    if(act==GLFW_PRESS){
        if(key==GLFW_KEY_ESCAPE)glfwSetWindowShouldClose(w,1);
        if(key==GLFW_KEY_SPACE&&!g_swinging) startSwing();
        if(key>=GLFW_KEY_1&&key<=GLFW_KEY_4) g_weapon=key-GLFW_KEY_1;
        if(key==GLFW_KEY_R){g_swings=g_hits=g_misses=g_ghosts=g_crits=0;g_totalDmg=0;g_flashes.clear();}
    }
    if(act==GLFW_PRESS||act==GLFW_REPEAT){
        int bases[]={4,8,16,24,32,48,64};
        if(key==GLFW_KEY_Q){
            for(int i=6;i>=0;i--) if(bases[i]<g_base){g_base=bases[i];break;}
        }
        if(key==GLFW_KEY_E){
            for(int i=0;i<7;i++) if(bases[i]>g_base){g_base=bases[i];break;}
        }
        if(key==GLFW_KEY_W) g_targetZ-=0.3f;
        if(key==GLFW_KEY_S) g_targetZ+=0.3f;
    }
}
static void scrollCB(GLFWwindow*,double,double y){
    g_camDist*=y>0?0.9f:1.1f; g_camDist=std::clamp(g_camDist,2.f,20.f);
}
static void mbtnCB(GLFWwindow*w,int b,int a,int){
    if(b==GLFW_MOUSE_BUTTON_LEFT){g_mouseDown=(a==GLFW_PRESS);glfwGetCursorPos(w,&g_mx,&g_my);}
}
static void cursorCB(GLFWwindow*,double x,double y){
    if(g_mouseDown){
        g_camYaw+=(float)(x-g_mx)*0.3f;
        g_camPitch+=(float)(g_my-y)*0.3f;
        g_camPitch=std::clamp(g_camPitch,-10.f,80.f);
        g_mx=x;g_my=y;
    }
}

int main(){
    if(!glfwInit())return 1;
    GLFWwindow*win=glfwCreateWindow(1024,720,"NEC-002 Melee Combat Precision | Base-X Engine",0,0);
    if(!win){glfwTerminate();return 1;}
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    glfwSetKeyCallback(win,keyCB);
    glfwSetScrollCallback(win,scrollCB);
    glfwSetMouseButtonCallback(win,mbtnCB);
    glfwSetCursorPosCallback(win,cursorCB);
    glEnable(GL_DEPTH_TEST);
    g_dro.initializeVRProfile();

    printf("NEC-002 Melee Combat Precision\n");
    printf("SPACE swing | 1-4 weapon | Q/E base | W/S target dist | R reset\n");

    double prev=glfwGetTime();
    int fc=0; double ft=prev;
    while(!glfwWindowShouldClose(win)){
        glfwGetFramebufferSize(win,&g_winW,&g_winH);
        double now=glfwGetTime();
        float dt=std::min((float)(now-prev),0.05f);
        prev=now;
        g_time+=dt;
        fc++;
        if(now-ft>0.5){g_fps=(int)(fc/(now-ft));fc=0;ft=now;}

        updateSwing(dt);

        // Update flashes
        for(auto it=g_flashes.begin();it!=g_flashes.end();){
            it->timer-=dt*2; if(it->timer<=0)it=g_flashes.erase(it);else++it;
        }

        render();
        glfwSwapBuffers(win);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}
