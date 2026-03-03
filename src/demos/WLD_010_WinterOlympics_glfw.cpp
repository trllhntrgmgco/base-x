/**
 * WLD-010: Grok Milano Holodeck — Quantum Alpine Gold
 * Winter Olympics VR Demo: Procedural Alps, quantum downhill/slalom,
 * spin-gravity bobsled, ice hockey. Grok-narrated coaching.
 *
 * Controls:
 *   WASD: ski left/right, speed up/down
 *   SPACE: quantum branch (show superposition paths)
 *   Mouse: look around
 *   1-3: switch events (downhill, slalom, bobsled)
 *   R: reset
 *   ESC: quit
 */

#include <GLFW/glfw3.h>
#include <BaseX/DRO.h>
#include <BaseX/GLText.h>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>

static constexpr float PI = 3.14159265f;

// Terrain
static std::vector<std::vector<float>> g_terrain; // Height map
static const int TERRAIN_SIZE = 256;
static const float TERRAIN_SCALE = 2.0f;

// Player
static float g_px = 0, g_py = 50, g_pz = 0;
static float g_vx = 0, g_vy = 0, g_vz = 10; // Speed down slope
static float g_yaw = 0, g_pitch = 0;
static float g_camDist = 5, g_camYaw = 0, g_camPitch = -20;
static int g_event = 0; // 0=downhill, 1=slalom, 2=bobsled
static bool g_quantumMode = false;
static std::vector<std::pair<float,float>> g_quantumPaths[3]; // 3 branches

// Gates for slalom
static std::vector<std::pair<float,float>> g_gates;

// Mock Grok narration
static std::string g_grokMsg = "Welcome to Milano! Quantum paths ready.";

static std::mt19937 g_rng(42);
static std::uniform_real_distribution<float> g_r01(0,1);

static int g_winW=1280, g_winH=720;
static int g_fps=0;
static float g_time=0;
static bool g_keys[512] = {};
static bool g_mouseDown = false;
static double g_mx, g_my;

static void generateTerrain() {
    g_terrain.resize(TERRAIN_SIZE, std::vector<float>(TERRAIN_SIZE));
    for(int x=0; x<TERRAIN_SIZE; x++){
        for(int z=0; z<TERRAIN_SIZE; z++){
            float nx = (float)x / TERRAIN_SIZE - 0.5f;
            float nz = (float)z / TERRAIN_SIZE - 0.5f;
            float dist = sqrtf(nx*nx + nz*nz);
            float height = 20 * (1 - dist*2) + 5 * sinf(nx*10) * cosf(nz*10);
            g_terrain[x][z] = std::max(0.0f, height);
        }
    }
}

static void generateGates() {
    g_gates.clear();
    for(int i=0; i<20; i++){
        float z = i * 10.0f;
        float x = (g_r01(g_rng)-0.5f) * 20;
        g_gates.push_back({x, z});
    }
}

static float getTerrainHeight(float x, float z) {
    int ix = (int)((x / TERRAIN_SCALE + TERRAIN_SIZE/2));
    int iz = (int)((z / TERRAIN_SCALE + TERRAIN_SIZE/2));
    if(ix>=0 && ix<TERRAIN_SIZE && iz>=0 && iz<TERRAIN_SIZE){
        return g_terrain[ix][iz];
    }
    return 0;
}

static void updatePlayer(float dt) {
    // Mouse look
    if(g_mouseDown){
        g_camYaw += (float)(g_mx - g_winW/2) * 0.002f;
        g_camPitch += (float)(g_my - g_winH/2) * 0.002f;
        g_camPitch = std::clamp(g_camPitch, -80.0f, 80.0f);
        glfwSetCursorPos(glfwGetCurrentContext(), g_winW/2, g_winH/2);
    }

    // Skiing physics
    float turn = 0;
    if(g_keys[GLFW_KEY_A]) turn -= 1;
    if(g_keys[GLFW_KEY_D]) turn += 1;
    g_yaw += turn * dt * 2;

    // Speed control
    if(g_keys[GLFW_KEY_W]) g_vz += 5*dt;
    if(g_keys[GLFW_KEY_S]) g_vz -= 5*dt;
    g_vz = std::clamp(g_vz, 5.0f, 50.0f);

    // Gravity and slope
    float slopeX = (getTerrainHeight(g_px+1, g_pz) - getTerrainHeight(g_px-1, g_pz)) / 2;
    float slopeZ = (getTerrainHeight(g_px, g_pz+1) - getTerrainHeight(g_px, g_pz-1)) / 2;
    g_vx += slopeX * dt * 10;
    g_vz += slopeZ * dt * 10 + 2*dt; // Downhill bias

    // Friction
    g_vx *= 0.98f;
    g_vz *= 0.98f;

    // Move
    g_px += g_vx * dt;
    g_pz += g_vz * dt;
    g_py = getTerrainHeight(g_px, g_pz) + 2; // Above snow

    // Wrap around
    if(g_pz > TERRAIN_SIZE*TERRAIN_SCALE/2) g_pz = -TERRAIN_SIZE*TERRAIN_SCALE/2;
}

static void quantumBranch() {
    g_quantumMode = true;
    for(int i=0; i<3; i++){
        g_quantumPaths[i].clear();
        float baseVx = g_vx + (g_r01(g_rng)-0.5f)*5;
        float baseVz = g_vz + (g_r01(g_rng)-0.5f)*5;
        for(int t=0; t<100; t++){
            float x = g_px + baseVx * t * 0.1f;
            float z = g_pz + baseVz * t * 0.1f;
            g_quantumPaths[i].push_back({x, z});
        }
    }
    g_grokMsg = "Quantum branches: Red for speed, Blue for control, Green for risk!";
}

static void drawTerrain() {
    glColor4f(0.9f, 0.95f, 1, 1);
    glBegin(GL_QUADS);
    for(int x=0; x<TERRAIN_SIZE-1; x++){
        for(int z=0; z<TERRAIN_SIZE-1; z++){
            float x1 = (x - TERRAIN_SIZE/2) * TERRAIN_SCALE;
            float z1 = (z - TERRAIN_SIZE/2) * TERRAIN_SCALE;
            float x2 = (x+1 - TERRAIN_SIZE/2) * TERRAIN_SCALE;
            float z2 = (z+1 - TERRAIN_SIZE/2) * TERRAIN_SCALE;
            glVertex3f(x1, g_terrain[x][z], z1);
            glVertex3f(x2, g_terrain[x+1][z], z1);
            glVertex3f(x2, g_terrain[x+1][z+1], z2);
            glVertex3f(x1, g_terrain[x][z+1], z2);
        }
    }
    glEnd();
}

static void drawGates() {
    if(g_event != 1) return; // Slalom only
    glColor4f(1,0,0,1);
    for(auto& g : g_gates){
        glPushMatrix();
        glTranslatef(g.first, getTerrainHeight(g.first, g.second) + 3, g.second);
        glBegin(GL_QUADS);
        glVertex3f(-0.5f, 0, 0); glVertex3f(0.5f, 0, 0);
        glVertex3f(0.5f, 6, 0); glVertex3f(-0.5f, 6, 0);
        glEnd();
        glPopMatrix();
    }
}

static void drawQuantumPaths() {
    if(!g_quantumMode) return;
    float colors[3][3] = {{1,0,0}, {0,0,1}, {0,1,0}};
    for(int i=0; i<3; i++){
        glColor4f(colors[i][0], colors[i][1], colors[i][2], 0.7f);
        glLineWidth(3);
        glBegin(GL_LINE_STRIP);
        for(auto& p : g_quantumPaths[i]){
            glVertex3f(p.first, getTerrainHeight(p.first, p.second) + 1, p.second);
        }
        glEnd();
    }
}

static void drawPlayer() {
    glColor4f(0,0.8f,1,1);
    glPushMatrix();
    glTranslatef(g_px, g_py, g_pz);
    glRotatef(g_yaw*180/PI, 0,1,0);
    // Skier
    glBegin(GL_QUADS);
    glVertex3f(-0.5f, 0, -1); glVertex3f(0.5f, 0, -1);
    glVertex3f(0.5f, 2, 0); glVertex3f(-0.5f, 2, 0);
    glEnd();
    // Skis
    glColor4f(0.2f,0.2f,0.2f,1);
    glBegin(GL_QUADS);
    glVertex3f(-0.1f, 0, -1); glVertex3f(0.1f, 0, -1);
    glVertex3f(0.1f, 0, 1); glVertex3f(-0.1f, 0, 1);
    glEnd();
    glPopMatrix();
}

static void render() {
    glViewport(0,0,g_winW,g_winH);
    glClearColor(0.6f,0.8f,1,1); // Sky blue
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    // Camera
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    float asp=(float)g_winW/g_winH;
    float f=1.0f/tanf(PI/8);
    float zn=0.1f,zf=2000;
    float m[16]={};
    m[0]=f/asp;m[5]=f;m[10]=(zf+zn)/(zn-zf);m[11]=-1;m[14]=2*zf*zn/(zn-zf);
    glLoadMatrixf(m);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();

    float cx = g_px + cosf(g_camYaw*PI/180)*cosf(g_camPitch*PI/180)*g_camDist;
    float cy = g_py + sinf(g_camPitch*PI/180)*g_camDist + 2;
    float cz = g_pz + sinf(g_camYaw*PI/180)*cosf(g_camPitch*PI/180)*g_camDist;
    float ex = cx, ey = cy, ez = cz;
    float tx = g_px, ty = g_py, tz = g_pz;
    float ux = 0, uy = 1, uz = 0;
    float fx = tx - ex, fy = ty - ey, fz = tz - ez;
    float flen = sqrtf(fx*fx + fy*fy + fz*fz);
    fx /= flen; fy /= flen; fz /= flen;
    float rx = fy*uz - fz*uy, ry = fz*ux - fx*uz, rz = fx*uy - fy*ux;
    float rlen = sqrtf(rx*rx + ry*ry + rz*rz);
    rx /= rlen; ry /= rlen; rz /= rlen;
    float upx = ry*fz - rz*fy, upy = rz*fx - rx*fz, upz = rx*fy - ry*fx;
    float mat[16] = {rx, upx, -fx, 0, ry, upy, -fy, 0, rz, upz, -fz, 0, 0, 0, 0, 1};
    glMultMatrixf(mat);
    glTranslatef(-ex, -ey, -ez);

    drawTerrain();
    drawGates();
    drawQuantumPaths();
    drawPlayer();

    glDisable(GL_DEPTH_TEST);

    // HUD
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    float mm[16]={}; mm[0]=2.f/g_winW;mm[5]=2.f/g_winH;mm[10]=-1;mm[12]=-1;mm[13]=-1;mm[15]=1;
    glLoadMatrixf(mm);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();

    glColor4f(0,0.87f,0.93f,1);
    char buf[256];
    const char* events[] = {"DOWNHILL", "SLALOM", "BOBSLED"};
    float speed = sqrtf(g_vx*g_vx + g_vz*g_vz);
    snprintf(buf,256,"GROK MILANO HOLODECK  |  EVENT: %s  |  SPEED: %.1f  |  ALT: %.0f  |  FPS: %d",
             events[g_event], speed, g_py, g_fps);
    gltext::draw(buf, 10, g_winH-24, 0.9f);

    glColor4f(0.7f,0.7f,0.8f,1);
    gltext::draw(g_grokMsg.c_str(), 10, g_winH-44, 0.8f);
    gltext::draw("WASD: Ski | SPACE: Quantum Branch | Mouse: Look | 1-3: Events | R: Reset | ESC: Quit", 10, 10, 0.7f);
}

static void keyCB(GLFWwindow*w,int key,int,int act,int){
    if(key>=0&&key<512){
        if(act==GLFW_PRESS)g_keys[key]=true;
        if(act==GLFW_RELEASE)g_keys[key]=false;
    }
    if(act==GLFW_PRESS){
        if(key==GLFW_KEY_ESCAPE)glfwSetWindowShouldClose(w,1);
        if(key==GLFW_KEY_SPACE) quantumBranch();
        if(key>=GLFW_KEY_1&&key<=GLFW_KEY_3){g_event=key-GLFW_KEY_1; if(g_event==1) generateGates(); g_quantumMode=false;}
        if(key==GLFW_KEY_R){g_px=0;g_py=50;g_pz=0;g_vx=0;g_vy=0;g_vz=10;g_quantumMode=false;g_grokMsg="Reset! Choose your path.";}
    }
}

static void mbtnCB(GLFWwindow*,int b,int a,int){
    if(b==GLFW_MOUSE_BUTTON_LEFT) g_mouseDown=(a==GLFW_PRESS);
}

static void cursorCB(GLFWwindow*,double x,double y){
    g_mx = x; g_my = y;
}

int main(){
    if(!glfwInit())return 1;
    GLFWwindow*win=glfwCreateWindow(1280,720,"WLD-010 Grok Milano Holodeck | Base-X Engine",0,0);
    if(!win){glfwTerminate();return 1;}
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    glfwSetKeyCallback(win,keyCB);
    glfwSetMouseButtonCallback(win,mbtnCB);
    glfwSetCursorPosCallback(win,cursorCB);
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glEnable(GL_DEPTH_TEST);

    generateTerrain();
    generateGates();

    printf("WLD-010 Grok Milano Holodeck\n");
    printf("WASD: Ski | SPACE: Quantum Branch | Mouse: Look | 1-3: Events | R: Reset | ESC: Quit\n");

    double prev=glfwGetTime();
    int fc=0; double ft=prev;
    while(!glfwWindowShouldClose(win)){
        glfwGetFramebufferSize(win,&g_winW,&g_winH);
        double now=glfwGetTime();
        float dt=std::min((float)(now-prev),0.05f);
        prev=now;
        fc++;
        if(now-ft>0.5){g_fps=(int)(fc/(now-ft));fc=0;ft=now;}
        updatePlayer(dt);
        render();
        glfwSwapBuffers(win);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}