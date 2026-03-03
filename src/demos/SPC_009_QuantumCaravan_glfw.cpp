/**
 * SPC-009: Quantum Caravan Siege — Base-X Engine
 * Phase 2: Docking + Threats + Quantum Paths
 *
 * Controls:
 *   WASD: thrust        Mouse: look/orbit
 *   SPACE: quantum fork R: reset
 *   D: dock (near station)  TAB: LOD mode
 *   ESC: quit
 */

#include <GLFW/glfw3.h>
#include <BaseX/DRO.h>
#include <BaseX/GLText.h>
#include <btBulletDynamicsCommon.h>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>

static constexpr float PI = 3.14159265f;
static constexpr int   ASTEROID_COUNT = 500000; // Reduced for Phase 2

struct Asteroid {
    float x, y, z, vx, vy, vz, r;
    int   lod;
};

struct Station {
    float x, y, z, rot, rotSpeed;
    btRigidBody* body;
};

struct Drone {
    float x, y, z, vx, vy, vz, hp;
    btRigidBody* body;
};

struct QuantumPath {
    std::vector<std::pair<float,float>> points;
    bool active;
};

static std::vector<Asteroid> g_asteroids;
static std::mt19937 g_rng(42);
static std::uniform_real_distribution<float> g_r01(0,1);

// Player
static float g_px=0, g_py=0, g_pz=0, g_yaw=0, g_pitch=0;
static float g_vx=0, g_vy=0, g_vz=0;
static float g_camDist=30, g_camYaw=0, g_camPitch=20;
static int   g_lodMode=0;
static int   g_winW=1280, g_winH=800;
static int   g_fps=0;
static float g_time=0;
static bool  g_keys[512] = {};
static bool  g_mouseDown = false;
static double g_mx, g_my;
static bool  g_docked = false;
static int   g_health = 100;

// Bullet physics
static btBroadphaseInterface* g_broadphase;
static btDefaultCollisionConfiguration* g_collisionConfig;
static btCollisionDispatcher* g_dispatcher;
static btSequentialImpulseConstraintSolver* g_solver;
static btDiscreteDynamicsWorld* g_world;
static btRigidBody* g_podBody;

// Stations & Drones
static std::vector<Station> g_stations;
static std::vector<Drone> g_drones;
static std::vector<QuantumPath> g_quantumPaths;

// Stars
static std::vector<std::pair<float,float>> g_stars;

static void initPhysics() {
    g_broadphase = new btDbvtBroadphase();
    g_collisionConfig = new btDefaultCollisionConfiguration();
    g_dispatcher = new btCollisionDispatcher(g_collisionConfig);
    g_solver = new btSequentialImpulseConstraintSolver();
    g_world = new btDiscreteDynamicsWorld(g_dispatcher, g_broadphase, g_solver, g_collisionConfig);
    g_world->setGravity(btVector3(0, 0, 0));

    // Player pod
    btCollisionShape* podShape = new btSphereShape(0.7f);
    btDefaultMotionState* podMotion = new btDefaultMotionState(btTransform(btQuaternion(0,0,0,1), btVector3(0,0,0)));
    btRigidBody::btRigidBodyConstructionInfo podCI(1, podMotion, podShape);
    g_podBody = new btRigidBody(podCI);
    g_world->addRigidBody(g_podBody);
}

static void spawnStations() {
    g_stations.clear();
    for(int i=0; i<5; i++){
        float x = (g_r01(g_rng)-0.5f)*200, z = (g_r01(g_rng)-0.5f)*200;
        btCollisionShape* shape = new btCylinderShape(btVector3(3,1,3));
        btDefaultMotionState* motion = new btDefaultMotionState(btTransform(btQuaternion(0,0,0,1), btVector3(x,0,z)));
        btRigidBody::btRigidBodyConstructionInfo ci(0, motion, shape);
        btRigidBody* body = new btRigidBody(ci);
        g_world->addRigidBody(body);
        g_stations.push_back({x,0,z, 0, 0.5f, body});
    }
}

static void spawnDrones() {
    g_drones.clear();
    for(int i=0; i<10; i++){
        float x = (g_r01(g_rng)-0.5f)*300, z = (g_r01(g_rng)-0.5f)*300;
        btCollisionShape* shape = new btSphereShape(0.5f);
        btDefaultMotionState* motion = new btDefaultMotionState(btTransform(btQuaternion(0,0,0,1), btVector3(x,0,z)));
        btRigidBody::btRigidBodyConstructionInfo ci(1, motion, shape);
        btRigidBody* body = new btRigidBody(ci);
        g_world->addRigidBody(body);
        g_drones.push_back({x,0,z,0,0,0,50, body});
    }
}

static void spawnAsteroids() {
    g_asteroids.clear();
    for (int i = 0; i < ASTEROID_COUNT; i++) {
        float theta = g_r01(g_rng)*2*PI;
        float phi = acosf(2*g_r01(g_rng)-1);
        float dist = 100 + g_r01(g_rng)*900;
        float x = cosf(theta)*sinf(phi)*dist;
        float y = sinf(theta)*sinf(phi)*dist;
        float z = cosf(phi)*dist;
        float r = 0.2f + g_r01(g_rng)*2.0f;
        float vx = -y*0.0001f, vy = x*0.0001f, vz = 0;
        g_asteroids.push_back({x,y,z,vx,vy,vz,r,0});
    }
    g_stars.clear();
    for(int i=0;i<1000;i++){
        float ra = g_r01(g_rng)*2*PI, dec = (g_r01(g_rng)-0.5f)*PI;
        g_stars.push_back({ra, dec});
    }
}

static void updatePhysics(float dt) {
    g_world->stepSimulation(dt, 10);

    // Sync player
    btTransform trans;
    g_podBody->getMotionState()->getWorldTransform(trans);
    btVector3 pos = trans.getOrigin();
    g_px = pos.x(); g_py = pos.y(); g_pz = pos.z();

    // Update stations
    for(auto& s : g_stations){
        s.rot += s.rotSpeed * dt;
        btTransform t(btQuaternion(0, s.rot, 0, 1), btVector3(s.x, s.y, s.z));
        s.body->getMotionState()->setWorldTransform(t);
    }

    // Update drones
    for(auto& d : g_drones){
        float dx = g_px - d.x, dy = g_py - d.y, dz = g_pz - d.z;
        float dist = sqrtf(dx*dx+dy*dy+dz*dz);
        if(dist > 1){
            d.vx += dx/dist * 20 * dt;
            d.vy += dy/dist * 20 * dt;
            d.vz += dz/dist * 20 * dt;
        }
        d.vx *= 0.95f; d.vy *= 0.95f; d.vz *= 0.95f;
        d.x += d.vx*dt; d.y += d.vy*dt; d.z += d.vz*dt;
        btTransform t(btQuaternion(0,0,0,1), btVector3(d.x, d.y, d.z));
        d.body->getMotionState()->setWorldTransform(t);
    }
}

static void updateAsteroids(float dt) {
    for (auto& a : g_asteroids) {
        a.x += a.vx*dt;
        a.y += a.vy*dt;
        a.z += a.vz*dt;
        float dx=a.x-g_px,dy=a.y-g_py,dz=a.z-g_pz;
        float d = sqrtf(dx*dx+dy*dy+dz*dz);
        if (g_lodMode==1) a.lod=0;
        else if (g_lodMode==2) a.lod=2;
        else if (d<10) a.lod=0;
        else if (d<50) a.lod=1;
        else a.lod=2;
    }
}

static void updatePlayer(float dt) {
    if (g_mouseDown) {
        g_camYaw += (float)(g_mx - g_winW/2) * 0.002f;
        g_camPitch += (float)(g_my - g_winH/2) * 0.002f;
        g_camPitch = std::clamp(g_camPitch, -80.0f, 80.0f);
        glfwSetCursorPos(glfwGetCurrentContext(), g_winW/2, g_winH/2);
    }

    if (!g_docked) {
        float thrust = 50*dt;
        btVector3 force(0,0,0);
        if (g_keys[GLFW_KEY_W]) force += btVector3(0,0,-thrust);
        if (g_keys[GLFW_KEY_S]) force += btVector3(0,0,thrust);
        if (g_keys[GLFW_KEY_A]) force += btVector3(-thrust,0,0);
        if (g_keys[GLFW_KEY_D]) force += btVector3(thrust,0,0);
        g_podBody->applyCentralForce(force);
    }

    // Check docking
    if (g_keys[GLFW_KEY_D]) {
        for(auto& s : g_stations){
            float dx=s.x-g_px, dy=s.y-g_py, dz=s.z-g_pz;
            if(sqrtf(dx*dx+dy*dy+dz*dz) < 5){
                g_docked = !g_docked;
                if(g_docked) g_podBody->setLinearVelocity(btVector3(0,0,0));
            }
        }
    }
}

static void quantumFork() {
    g_quantumPaths.clear();
    for(int i=0; i<3; i++){
        QuantumPath p;
        p.active = true;
        float baseVx = g_vx + (g_r01(g_rng)-0.5f)*10;
        float baseVy = g_vy + (g_r01(g_rng)-0.5f)*10;
        float baseVz = g_vz + (g_r01(g_rng)-0.5f)*10;
        for(int t=0; t<50; t++){
            float x = g_px + baseVx * t * 0.1f;
            float z = g_pz + baseVz * t * 0.1f;
            p.points.push_back({x,z});
        }
        g_quantumPaths.push_back(p);
    }
}

static void drawSphere(float cx,float cy,float cz,float r,int segs) {
    for(int i=0;i<segs;i++){
        float t0=PI*i/segs,t1=PI*(i+1)/segs;
        glBegin(GL_QUAD_STRIP);
        for(int j=0;j<=segs;j++){
            float p=2*PI*j/segs;
            glVertex3f(cx+r*sinf(t0)*cosf(p),cy+r*cosf(t0),cz+r*sinf(t0)*sinf(p));
            glVertex3f(cx+r*sinf(t1)*cosf(p),cy+r*cosf(t1),cz+r*sinf(t1)*sinf(p));
        }
        glEnd();
    }
}

static void drawTorus(float x, float y, float z, float r1, float r2, int segs) {
    for(int i=0;i<segs;i++){
        float a0 = 2*PI*i/segs, a1 = 2*PI*(i+1)/segs;
        glBegin(GL_QUAD_STRIP);
        for(int j=0;j<=segs;j++){
            float b = 2*PI*j/segs;
            float x1 = (r1 + r2*cosf(b))*cosf(a0);
            float y1 = r2*sinf(b);
            float z1 = (r1 + r2*cosf(b))*sinf(a0);
            float x2 = (r1 + r2*cosf(b))*cosf(a1);
            float y2 = r2*sinf(b);
            float z2 = (r1 + r2*cosf(b))*sinf(a1);
            glVertex3f(x+x1, y+y1, z+z1);
            glVertex3f(x+x2, y+y2, z+z2);
        }
        glEnd();
    }
}

static void drawAsteroid(float cx, float cy, float cz, float r, int detail) {
    // Rocky asteroid with craters
    glColor4f(0.4f, 0.35f, 0.3f, 1);
    drawSphere(cx, cy, cz, r, detail);
    // Add craters
    glColor4f(0.2f, 0.18f, 0.15f, 1);
    for(int i=0; i<5; i++){
        float a = i * 2*PI/5;
        float cr = r * 0.3f;
        drawSphere(cx + cosf(a)*r*0.7f, cy + sinf(a)*r*0.7f, cz, cr, detail/2);
    }
}

static void drawStation(float x, float y, float z, float rot) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(rot*180/PI, 0,1,0);
    
    // Main torus
    glColor4f(0.5f,0.8f,1,1);
    drawTorus(0,0,0, 2, 0.5f, 16);
    
    // Windows/lights
    glColor4f(1,1,0.8f,0.8f);
    for(int i=0; i<8; i++){
        float a = i * 2*PI/8;
        drawSphere(cosf(a)*2.5f, 0, sinf(a)*2.5f, 0.2f, 8);
    }
    
    // Solar panels
    glColor4f(0.3f,0.3f,0.5f,1);
    glBegin(GL_QUADS);
    glVertex3f(-3,0.1f,-1); glVertex3f(3,0.1f,-1); glVertex3f(3,0.1f,1); glVertex3f(-3,0.1f,1);
    glVertex3f(-3,-0.1f,-1); glVertex3f(3,-0.1f,-1); glVertex3f(3,-0.1f,1); glVertex3f(-3,-0.1f,1);
    glEnd();
    
    glPopMatrix();
}

static void drawDrone(float x, float y, float z) {
    // Main body
    glColor4f(1,0.3f,0.3f,1);
    drawSphere(x, y, z, 0.5f, 10);
    
    // Wings
    glColor4f(0.8f,0.2f,0.2f,1);
    glBegin(GL_QUADS);
    glVertex3f(x-1, y, z); glVertex3f(x-0.5f, y, z); glVertex3f(x-0.5f, y+0.2f, z); glVertex3f(x-1, y+0.2f, z);
    glVertex3f(x+0.5f, y, z); glVertex3f(x+1, y, z); glVertex3f(x+1, y+0.2f, z); glVertex3f(x+0.5f, y+0.2f, z);
    glEnd();
    
    // Engine glow
    glColor4f(1,0.5f,0,0.6f);
    drawSphere(x, y-0.3f, z, 0.2f, 6);
}

static void drawPod(float x, float y, float z) {
    // Main body
    glColor4f(0,1,0.5f,1);
    drawSphere(x, y, z, 0.7f, 12);
    
    // Cockpit
    glColor4f(0.8f,1,1,0.8f);
    drawSphere(x, y+0.3f, z, 0.3f, 8);
    
    // Thrusters
    if (g_keys[GLFW_KEY_W] || g_keys[GLFW_KEY_S] || g_keys[GLFW_KEY_A] || g_keys[GLFW_KEY_D]) {
        glColor4f(0.5f,0.8f,1,0.8f);
        drawSphere(x, y, z-1, 0.3f, 6);
        drawSphere(x-0.5f, y, z, 0.2f, 6);
        drawSphere(x+0.5f, y, z, 0.2f, 6);
    }
}

static void render() {
    glViewport(0,0,g_winW,g_winH);
    glClearColor(0.01f,0.01f,0.03f,1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    float lightPos[] = {1000,1000,1000,1};
    float lightAmb[] = {0.1f,0.1f,0.15f,1};
    float lightDiff[] = {0.8f,0.8f,0.9f,1};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiff);

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
    float cy = g_py + sinf(g_camPitch*PI/180)*g_camDist;
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

    // Stars
    glDisable(GL_LIGHTING);
    glColor4f(1,1,1,0.8f);
    glPointSize(1);
    glBegin(GL_POINTS);
    for(auto& s : g_stars){
        float x = cosf(s.second)*cosf(s.first)*1000;
        float y = sinf(s.second)*1000;
        float z = cosf(s.second)*sinf(s.first)*1000;
        glVertex3f(x,y,z);
    }
    glEnd();
    glEnable(GL_LIGHTING);

    // Asteroids
    glColor4f(0.6f,0.55f,0.5f,1);
    float matAmb[] = {0.4f,0.35f,0.3f,1};
    float matDiff[] = {0.8f,0.7f,0.6f,1};
    float matSpec[] = {0.1f,0.1f,0.1f,1};
    glMaterialfv(GL_FRONT, GL_AMBIENT, matAmb);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, matDiff);
    glMaterialfv(GL_FRONT, GL_SPECULAR, matSpec);
    glMaterialf(GL_FRONT, GL_SHININESS, 10);
    for(size_t i=0; i<g_asteroids.size(); i+=10){ // Draw every 10th instead of 100th
        auto& a = g_asteroids[i];
        if(a.lod < 2) drawAsteroid(a.x,a.y,a.z,a.r, a.lod==0?8:4);
    }

    // Stations
    glColor4f(0.5f,0.8f,1,1);
    float matAmbS[] = {0.3f,0.5f,0.6f,1};
    float matDiffS[] = {0.7f,0.9f,1,1};
    glMaterialfv(GL_FRONT, GL_AMBIENT, matAmbS);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, matDiffS);
    for(auto& s : g_stations){
        drawStation(s.x, s.y, s.z, s.rot);
    }

    // Drones
    glColor4f(1,0.3f,0.3f,1);
    float matAmbD[] = {0.6f,0.2f,0.2f,1};
    float matDiffD[] = {1,0.5f,0.5f,1};
    glMaterialfv(GL_FRONT, GL_AMBIENT, matAmbD);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, matDiffD);
    for(auto& d : g_drones){
        drawDrone(d.x, d.y, d.z);
    }

    // Player pod
    glColor4f(0,1,0.5f,1);
    float matAmbP[] = {0,0.6f,0.3f,1};
    float matDiffP[] = {0,1,0.7f,1};
    glMaterialfv(GL_FRONT, GL_AMBIENT, matAmbP);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, matDiffP);
    drawPod(g_px,g_py,g_pz);

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

    // HUD
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    float mm[16]={}; mm[0]=2.f/g_winW;mm[5]=2.f/g_winH;mm[10]=-1;mm[12]=-1;mm[13]=-1;mm[15]=1;
    glLoadMatrixf(mm);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();

    glColor4f(0,0.87f,0.93f,1);
    char buf[256];
    float speed = sqrtf(g_vx*g_vx + g_vy*g_vy + g_vz*g_vz);
    snprintf(buf,256,"QUANTUM CARAVAN  |  HEALTH: %d  |  DOCKED: %s  |  THREATS: %d  |  FPS: %d",
             g_health, g_docked?"YES":"NO", (int)g_drones.size(), g_fps);
    gltext::draw(buf, 10, g_winH-24, 0.9f);

    glColor4f(0.7f,0.7f,0.8f,1);
    gltext::draw("WASD: Thrust | Mouse: Look | SPACE: Quantum Fork | D: Dock | R: Reset | ESC: Quit", 10, 10, 0.7f);
}

static void keyCB(GLFWwindow*w,int key,int,int act,int){
    if(key>=0&&key<512){
        if(act==GLFW_PRESS)g_keys[key]=true;
        if(act==GLFW_RELEASE)g_keys[key]=false;
    }
    if(act==GLFW_PRESS){
        if(key==GLFW_KEY_ESCAPE)glfwSetWindowShouldClose(w,1);
        if(key==GLFW_KEY_TAB) g_lodMode=(g_lodMode+1)%3;
        if(key==GLFW_KEY_SPACE) quantumFork();
        if(key==GLFW_KEY_R){g_health=100;g_docked=false;g_quantumPaths.clear();spawnDrones();spawnStations();
                            btTransform t(btQuaternion(0,0,0,1), btVector3(0,0,0));
                            g_podBody->setWorldTransform(t); g_podBody->setLinearVelocity(btVector3(0,0,0));}
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
    GLFWwindow*win=glfwCreateWindow(1280,800,"SPC-009 Quantum Caravan | Base-X Engine",0,0);
    if(!win){glfwTerminate();return 1;}
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    glfwSetKeyCallback(win,keyCB);
    glfwSetMouseButtonCallback(win,mbtnCB);
    glfwSetCursorPosCallback(win,cursorCB);
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glEnable(GL_DEPTH_TEST);

    initPhysics();
    spawnAsteroids();
    spawnStations();
    spawnDrones();

    printf("SPC-009 Quantum Caravan Siege\n");
    printf("WASD: Thrust | Mouse: Look | SPACE: Quantum Fork | D: Dock | R: Reset | ESC: Quit\n");

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
        updatePhysics(dt);
        updateAsteroids(dt);
        render();
        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    // Cleanup Bullet
    delete g_world;
    delete g_solver;
    delete g_dispatcher;
    delete g_collisionConfig;
    delete g_broadphase;

    glfwTerminate();
    return 0;
}
