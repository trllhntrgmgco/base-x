/**
 * WLD-007-3D: Tornado Demo — 3D Base-X Engine
 * 3D vortex with orbiting camera, Rankine physics, DRO precision levels.
 *
 * Controls:
 *   1-6      EF scale         W/S   rotation speed
 *   A/D      funnel width     SPACE toggle debris
 *   Mouse    orbit camera     Scroll zoom
 *   R        reset            ESC   quit
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
static void perspectiveGL(double fov, double asp, double zn, double zf) {
    double f = 1.0 / tan(fov * PI / 360.0);
    double m[16] = {};
    m[0]  = f / asp;  m[5]  = f;
    m[10] = (zf+zn)/(zn-zf); m[11] = -1;
    m[14] = 2*zf*zn/(zn-zf);
    glMultMatrixd(m);
}
static void lookAtGL(float ex,float ey,float ez,float cx,float cy,float cz) {
    float fx=cx-ex,fy=cy-ey,fz=cz-ez;
    float fl=sqrtf(fx*fx+fy*fy+fz*fz); fx/=fl;fy/=fl;fz/=fl;
    float sx=fy*0-fz*1, sy=fz*0-fx*0, sz=fx*1-fy*0;  // up=(0,1,0)
    // proper cross: s = f x up
    sx = fy*0.0f - fz*1.0f;  // fy*uz - fz*uy  (uz=0, uy=1)
    sy = fz*0.0f - fx*0.0f;
    sz = fx*1.0f - fy*0.0f;
    // Oops, let me redo:  up = (0,1,0)
    sx = fy*0 - fz*1; // wrong sign, let me just do it properly:
    // s = f x up = (fy*0 - fz*1, fz*0 - fx*0, fx*1 - fy*0) = (-fz, 0, fx)
    sx = -fz; sy = 0; sz = fx;
    float sl=sqrtf(sx*sx+sy*sy+sz*sz);
    if(sl>0){sx/=sl;sy/=sl;sz/=sl;}
    float ux=sy*fz-sz*fy, uy=sz*fx-sx*fz, uz=sx*fy-sy*fx;
    float m[16]={sx,ux,-fx,0, sy,uy,-fy,0, sz,uz,-fz,0, 0,0,0,1};
    glMultMatrixf(m);
    glTranslatef(-ex,-ey,-ez);
}

// ── EF scale ───────────────────────────────────────────────────────
struct EFDef { const char*n; int wMin,wMax,width; float c[3]; };
static const EFDef EF[]={
    {"EF0",65,85,50,{.27f,.67f,.27f}},
    {"EF1",86,110,100,{.53f,.8f,.27f}},
    {"EF2",111,135,200,{.8f,.8f,.27f}},
    {"EF3",136,165,300,{1,.67f,.27f}},
    {"EF4",166,200,400,{1,.4f,.27f}},
    {"EF5",201,250,500,{1,.27f,.27f}},
};

// ── 3D debris particle ────────────────────────────────────────────
struct Particle {
    float x, y, z;
    float vx, vy, vz;
    float size;
    float life;
    float rot;
    int   base;       // DRO base
    int   type;       // 0=dust 1=leaf 2=wood 3=brick 4=car
};

// ── Globals ────────────────────────────────────────────────────────
static std::vector<Particle> g_parts;
static std::mt19937 g_rng(42);
static std::uniform_real_distribution<float> g_r01(0,1);
static BaseX::DROManager g_dro;

static int   g_ef = 2;
static float g_rotSpeed = 150;
static float g_funnelW  = 200;
static float g_tornadoX = 0;
static float g_tornadoAngle = 0;
static float g_time = 0;
static bool  g_showDebris = true;

static float g_camDist = 40, g_camYaw = 0, g_camPitch = 25;
static bool  g_mouseDown = false;
static double g_mx, g_my;
static int   g_winW = 1280, g_winH = 720;
static int   g_fps = 0;
static int   g_baseCounts[5] = {}; // 64,32,16,8,4

// ── Vortex force (Rankine) ─────────────────────────────────────────
static void vortexForce(float px,float py,float pz, float cx,float cz,
                        float radius, float &fx,float &fy,float &fz) {
    float dx = px-cx, dz = pz-cz;
    float dist = sqrtf(dx*dx + dz*dz);
    if (dist < 0.1f) { fx=fy=fz=0; return; }
    float core = radius * 0.3f;
    float tang = dist < core
        ? (g_rotSpeed/100.f) * (dist/core)
        : (g_rotSpeed/100.f) * (core/dist);
    float angle = atan2f(dz, dx);
    fx = -sinf(angle) * tang;
    fz =  cosf(angle) * tang;
    // Inward pull
    float inward = dist < radius ? 0.5f*(1-dist/radius) : 0;
    fx -= (dx/dist)*inward;
    fz -= (dz/dist)*inward;
    // Updraft
    fy = dist < core ? 3.0f*(1-dist/core) : 0;
}

static void spawn(float dt) {
    if (!g_showDebris) return;
    float rate = g_ef * 15.0f * dt;
    float funnelR = g_funnelW * 0.01f;
    for (int i = 0; i < (int)rate && g_parts.size() < 3000; i++) {
        Particle p{};
        float a = g_r01(g_rng)*2*PI;
        float r = g_r01(g_rng)*funnelR*0.5f;
        p.x = g_tornadoX + cosf(a)*r;
        p.z = sinf(a)*r;
        p.y = 0;
        p.vx = p.vy = p.vz = 0;
        p.rot = g_r01(g_rng)*2*PI;
        p.life = 1;
        float tr = g_r01(g_rng);
        if (tr < 0.5f) { p.type=0; p.size=0.15f; }
        else if (tr<0.7f) { p.type=1; p.size=0.25f; }
        else if (tr<0.85f){ p.type=2; p.size=0.35f; }
        else if (tr<0.95f){ p.type=3; p.size=0.5f; }
        else              { p.type=4; p.size=0.7f; }
        p.base = 32;
        g_parts.push_back(p);
    }
}

static void update(float dt) {
    g_time += dt;
    g_tornadoAngle += g_rotSpeed * 0.01f * dt;
    g_tornadoX += 0.5f * dt;
    if (g_tornadoX > 30) g_tornadoX = -30;

    float funnelR = g_funnelW * 0.01f;
    float camX = cosf(g_camYaw*PI/180)*cosf(g_camPitch*PI/180)*g_camDist;
    float camY = sinf(g_camPitch*PI/180)*g_camDist;
    float camZ = sinf(g_camYaw*PI/180)*cosf(g_camPitch*PI/180)*g_camDist;

    memset(g_baseCounts, 0, sizeof(g_baseCounts));

    spawn(dt);

    for (auto it = g_parts.begin(); it != g_parts.end();) {
        auto& p = *it;
        float fx,fy,fz;
        vortexForce(p.x,p.y,p.z, g_tornadoX,0, funnelR, fx,fy,fz);
        p.vx += fx*dt*2; p.vy += fy*dt*2; p.vz += fz*dt*2;
        p.vy -= 0.8f*dt; // gravity
        p.vx *= 0.98f; p.vy *= 0.98f; p.vz *= 0.98f;
        p.x += p.vx*dt*5; p.y += p.vy*dt*5; p.z += p.vz*dt*5;
        p.rot += dt * 2;

        if (p.y < 0) { p.y = 0; p.vy *= -0.3f; p.life -= 0.05f; }

        // DRO: distance to camera
        float dx=p.x-camX, dy=p.y-camY, dz=p.z-camZ;
        float dist = sqrtf(dx*dx+dy*dy+dz*dz);
        p.base = (int)g_dro.getOptimalRadix(dist);

        switch(p.base) {
            case 64: g_baseCounts[0]++; break;
            case 32: g_baseCounts[1]++; break;
            case 16: g_baseCounts[2]++; break;
            case 8:  g_baseCounts[3]++; break;
            default: g_baseCounts[4]++; break;
        }

        float ddist = sqrtf((p.x-g_tornadoX)*(p.x-g_tornadoX)+p.z*p.z);
        if (ddist > funnelR*2) p.life -= 0.01f*dt;
        if (p.y > 25) p.life -= 0.02f*dt;

        if (p.life <= 0 || p.y > 30 || fabsf(p.x)>50 || fabsf(p.z)>50) {
            it = g_parts.erase(it);
        } else { ++it; }
    }
}

// ── 3D drawing ─────────────────────────────────────────────────────
static void drawCube3D(float x,float y,float z,float s, const float c[3], float alpha) {
    glPushMatrix();
    glTranslatef(x,y,z);
    glColor4f(c[0],c[1],c[2],alpha);
    float v[][3]={{-s,-s,s},{s,-s,s},{s,s,s},{-s,s,s},
                  {-s,-s,-s},{-s,s,-s},{s,s,-s},{s,-s,-s}};
    int f[][4]={{0,1,2,3},{7,6,5,4},{3,2,6,5},{4,7,1,0},{4,0,3,5},{1,7,6,2}};
    glBegin(GL_QUADS);
    for (auto& q:f) for(int i=0;i<4;i++) glVertex3fv(v[q[i]]);
    glEnd();
    glColor4f(c[0]*0.5f,c[1]*0.5f,c[2]*0.5f,alpha);
    int e[][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,7},{2,6},{3,5}};
    glBegin(GL_LINES);
    for(auto&l:e){glVertex3fv(v[l[0]]);glVertex3fv(v[l[1]]);}
    glEnd();
    glPopMatrix();
}

static void drawFunnel3D() {
    float funnelR = g_funnelW * 0.01f;
    int rings = 20, segs = 24;
    for (int r = 0; r < rings; r++) {
        float t0 = r / (float)rings;
        float t1 = (r+1) / (float)rings;
        float y0 = t0 * 20.0f;
        float y1 = t1 * 20.0f;
        float r0 = funnelR * (1.0f - t0*0.7f);
        float r1 = funnelR * (1.0f - t1*0.7f);
        float spiral0 = g_tornadoAngle + t0 * 8;
        float spiral1 = g_tornadoAngle + t1 * 8;
        float alpha = 0.12f + 0.08f * sinf(t0 * PI);

        glBegin(GL_QUAD_STRIP);
        for (int s = 0; s <= segs; s++) {
            float a = 2*PI*s/segs;
            float wobble0 = sinf(a*3+spiral0)*r0*0.15f;
            float wobble1 = sinf(a*3+spiral1)*r1*0.15f;
            glColor4f(0.5f,0.55f,0.6f, alpha);
            glVertex3f(g_tornadoX+cosf(a+spiral0)*(r0+wobble0), y0, sinf(a+spiral0)*(r0+wobble0));
            glVertex3f(g_tornadoX+cosf(a+spiral1)*(r1+wobble1), y1, sinf(a+spiral1)*(r1+wobble1));
        }
        glEnd();
    }
}

static void drawGround() {
    // Ground plane
    glColor4f(0.15f, 0.18f, 0.12f, 1);
    glBegin(GL_QUADS);
    glVertex3f(-60,0,-60); glVertex3f(60,0,-60);
    glVertex3f(60,0,60); glVertex3f(-60,0,60);
    glEnd();
    // Grid
    glColor4f(0.2f,0.22f,0.17f,0.5f);
    glBegin(GL_LINES);
    for(int i=-6;i<=6;i++){
        glVertex3f(i*10.f,0.01f,-60);glVertex3f(i*10.f,0.01f,60);
        glVertex3f(-60,0.01f,i*10.f);glVertex3f(60,0.01f,i*10.f);
    }
    glEnd();
}

static void baseColor(int base, float c[3]) {
    switch(base){
        case 64: c[0]=1;c[1]=.15f;c[2]=.15f; break;
        case 32: c[0]=1;c[1]=.8f;c[2]=.15f; break;
        case 16: c[0]=.15f;c[1]=1;c[2]=.3f; break;
        case 8:  c[0]=.2f;c[1]=.4f;c[2]=1; break;
        default: c[0]=.45f;c[1]=.45f;c[2]=.45f; break;
    }
}

static void render() {
    glViewport(0,0,g_winW,g_winH);
    glClearColor(0.15f,0.18f,0.22f,1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    // 3D scene
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    perspectiveGL(60,(double)g_winW/g_winH,0.1,500);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    float cx=cosf(g_camYaw*PI/180)*cosf(g_camPitch*PI/180)*g_camDist;
    float cy=sinf(g_camPitch*PI/180)*g_camDist;
    float cz=sinf(g_camYaw*PI/180)*cosf(g_camPitch*PI/180)*g_camDist;
    lookAtGL(cx,cy,cz, g_tornadoX,5,0);

    drawGround();
    drawFunnel3D();

    // Particles
    for (auto& p : g_parts) {
        float c[3]; baseColor(p.base, c);
        float alpha = 0.4f + (p.base/64.0f)*0.6f;
        drawCube3D(p.x, p.y, p.z, p.size, c, alpha * p.life);
    }

    // ── 2D HUD ──────────────────────────────────────────────────
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    float m[16]={}; m[0]=2.f/g_winW; m[5]=2.f/g_winH; m[10]=-1; m[12]=-1; m[13]=-1; m[15]=1;
    glLoadMatrixf(m);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();

    // Panel background
    glColor4f(0.02f,0.03f,0.06f,0.8f);
    glBegin(GL_QUADS);
    glVertex2f(0,0);glVertex2f(170,0);glVertex2f(170,(float)g_winH);glVertex2f(0,(float)g_winH);
    glEnd();
    glColor4f(0,0.87f,0.93f,0.3f);
    glBegin(GL_LINES);glVertex2f(170,0);glVertex2f(170,(float)g_winH);glEnd();

    float py = g_winH - 18.0f;
    char buf[64];
    glColor4f(0,0.87f,0.93f,1);
    gltext::draw("TORNADO 3D", 8, py, 1.3f); py-=20;

    // EF scale
    glColor4f(EF[g_ef].c[0],EF[g_ef].c[1],EF[g_ef].c[2],1);
    gltext::draw(EF[g_ef].n, 8, py, 1.5f);
    snprintf(buf,64,"%d MPH",(EF[g_ef].wMin+EF[g_ef].wMax)/2);
    glColor4f(.7f,.7f,.8f,1);
    gltext::draw(buf, 60, py, 1.0f);
    py -= 18;

    // Stats
    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("PARTICLES:", 8, py, 0.8f);
    glColor4f(0,0.87f,0.93f,1);
    snprintf(buf,64,"%d",(int)g_parts.size());
    gltext::draw(buf, 95, py, 0.8f);
    py -= 14;

    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("FPS:", 8, py, 0.8f);
    glColor4f(g_fps>55?.2f:1,g_fps>55?1:.3f,.2f,1);
    snprintf(buf,64,"%d",g_fps);
    gltext::draw(buf, 40, py, 0.8f);
    py -= 20;

    // DRO distribution
    glColor4f(0,0.87f,0.93f,0.8f);
    gltext::draw("DRO BASES", 8, py, 0.8f);
    py -= 4;
    int bases[] = {64,32,16,8,4};
    const char* bn[] = {"B-64","B-32","B-16","B-8","B-4"};
    int total = std::max(1, (int)g_parts.size());
    for (int i = 0; i < 5; i++) {
        py -= 16;
        float c[3]; baseColor(bases[i], c);
        glColor4f(c[0],c[1],c[2],0.9f);
        glBegin(GL_QUADS);
        glVertex2f(8,py);glVertex2f(18,py);glVertex2f(18,py+10);glVertex2f(8,py+10);
        glEnd();
        glColor4f(.7f,.7f,.8f,1);
        gltext::draw(bn[i], 22, py+1, 0.7f);
        float pct = g_baseCounts[i]*100.f/total;
        float bw = g_baseCounts[i]*80.f/total;
        glColor4f(c[0],c[1],c[2],0.5f);
        glBegin(GL_QUADS);
        glVertex2f(62,py);glVertex2f(62+bw,py);glVertex2f(62+bw,py+10);glVertex2f(62,py+10);
        glEnd();
        glColor4f(c[0],c[1],c[2],1);
        snprintf(buf,64,"%d%%",(int)pct);
        gltext::draw(buf, 62+bw+3, py+1, 0.6f);
    }

    // Controls
    glColor4f(.4f,.4f,.5f,1);
    gltext::draw("1-6 EF SCALE", 8, 52, 0.6f);
    gltext::draw("W/S SPEED", 8, 42, 0.6f);
    gltext::draw("A/D WIDTH", 8, 32, 0.6f);
    gltext::draw("SPACE DEBRIS", 8, 22, 0.6f);
    gltext::draw("R RESET", 8, 12, 0.6f);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

// ── Callbacks ──────────────────────────────────────────────────────
static void keyCallback(GLFWwindow*w,int key,int,int action,int) {
    if(action==GLFW_PRESS){
        if(key==GLFW_KEY_ESCAPE)glfwSetWindowShouldClose(w,1);
        if(key>=GLFW_KEY_1&&key<=GLFW_KEY_6){g_ef=key-GLFW_KEY_1;g_rotSpeed=(EF[g_ef].wMin+EF[g_ef].wMax)/2.f;g_funnelW=(float)EF[g_ef].width;}
        if(key==GLFW_KEY_SPACE)g_showDebris=!g_showDebris;
        if(key==GLFW_KEY_R){g_parts.clear();g_tornadoX=0;}
    }
    if(action==GLFW_PRESS||action==GLFW_REPEAT){
        if(key==GLFW_KEY_W)g_rotSpeed=std::min(250.f,g_rotSpeed+5);
        if(key==GLFW_KEY_S)g_rotSpeed=std::max(65.f,g_rotSpeed-5);
        if(key==GLFW_KEY_A)g_funnelW=std::max(50.f,g_funnelW-10);
        if(key==GLFW_KEY_D)g_funnelW=std::min(500.f,g_funnelW+10);
    }
}
static void scrollCB(GLFWwindow*,double,double y){
    g_camDist *= y>0?0.9f:1.1f;
    g_camDist=std::clamp(g_camDist,5.f,120.f);
}
static void mouseBtnCB(GLFWwindow*w,int btn,int act,int){
    if(btn==GLFW_MOUSE_BUTTON_LEFT){g_mouseDown=(act==GLFW_PRESS);glfwGetCursorPos(w,&g_mx,&g_my);}
}
static void cursorCB(GLFWwindow*,double x,double y){
    if(g_mouseDown){
        g_camYaw+=(float)(x-g_mx)*0.3f;
        g_camPitch+=(float)(g_my-y)*0.3f;
        g_camPitch=std::clamp(g_camPitch,5.f,85.f);
        g_mx=x;g_my=y;
    }
}

int main() {
    if(!glfwInit())return 1;
    GLFWwindow*win=glfwCreateWindow(1280,720,"WLD-007 3D Tornado | Base-X Engine",0,0);
    if(!win){glfwTerminate();return 1;}
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    glfwSetKeyCallback(win,keyCallback);
    glfwSetScrollCallback(win,scrollCB);
    glfwSetMouseButtonCallback(win,mouseBtnCB);
    glfwSetCursorPosCallback(win,cursorCB);
    glEnable(GL_DEPTH_TEST);
    g_dro.initializeVRProfile();

    printf("WLD-007 3D Tornado Demo\n");
    printf("1-6 EF scale | W/S speed | A/D width | Mouse orbit | Scroll zoom | R reset\n");

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
