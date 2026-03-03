/**
 * HLD-003: Holodeck Safety Protocols — Base-X Engine
 * Converted from html_demos/HLD-003-Safety-Protocols-BaseX-Demo.html
 *
 * Toggle safety ON/OFF. Enemies attack the player.
 * Safety ON:  hits blocked (green shield), no damage, Base-32
 * Safety OFF: real damage, injuries, Base-64 required for physics
 *
 * Controls:
 *   TAB        toggle safety on/off
 *   SPACE      spawn enemy         H  trigger hazard
 *   WASD       move player         1-4 scenario
 *   R          reset               ESC quit
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

// ── scenarios ──────────────────────────────────────────────────────
struct Scenario { const char*name; const char*hazard; float dmgMin,dmgMax; float col[3]; };
static const Scenario SCENES[] = {
    {"SWORD FIGHT","Laceration",5,20,{1,.6f,0}},
    {"PHASER RANGE","Plasma burn",10,30,{1,.2f,.27f}},
    {"ROCK CLIMB","Fracture",15,40,{.53f,.67f,.27f}},
    {"BOXING RING","Bruise",3,12,{1,.4f,0}},
};
static const char* BODY_PARTS[] = {"L.Arm","R.Arm","L.Leg","R.Leg","Torso","Head","Back","Shoulder"};
static const char* INJURIES[][4] = {
    {"Laceration","Cut","Slash","Puncture"},
    {"Plasma burn","Phaser burn","Energy trauma","Neural shock"},
    {"Fracture","Sprain","Contusion","Concussion"},
    {"Bruise","Swelling","Minor fracture","Abrasion"},
};

// ── entities ───────────────────────────────────────────────────────
struct Enemy { float x,y,hp,atkCD; };
struct Particle { float x,y,vx,vy,life,size; float col[3]; };
struct Injury { const char*name; const char*loc; const char*sev; };

// ── globals ────────────────────────────────────────────────────────
static BaseX::DROManager g_dro;
static std::mt19937 g_rng(42);
static std::uniform_real_distribution<float> g_r01(0,1);

static bool  g_safety = true;
static int   g_scene = 0;
static float g_health = 100;
static int   g_blocked = 0, g_totalDmg = 0;
static int   g_baseTransitions = 0;
static int   g_currentBase = 32;

static float g_px = 400, g_py = 350;  // player pos
static std::vector<Enemy> g_enemies;
static std::vector<Particle> g_parts;
static std::vector<Injury> g_injuries;

static float g_dmgFlash = 0;
static float g_shieldFlash = 0;
static float g_warnPulse = 0;
static float g_time = 0;
static int g_winW = 1024, g_winH = 720;
static int g_fps = 0;
static bool g_keys[512] = {};

// ── helpers ────────────────────────────────────────────────────────
static void drawRect(float x,float y,float w,float h){
    glBegin(GL_QUADS);glVertex2f(x,y);glVertex2f(x+w,y);glVertex2f(x+w,y+h);glVertex2f(x,y+h);glEnd();
}
static void drawCircle(float cx,float cy,float r,int s,bool fill){
    if(fill)glBegin(GL_TRIANGLE_FAN);else glBegin(GL_LINE_LOOP);
    if(fill)glVertex2f(cx,cy);
    for(int i=0;i<=s;i++){float a=2*PI*i/s;glVertex2f(cx+cosf(a)*r,cy+sinf(a)*r);}
    glEnd();
}

// ── spawn ──────────────────────────────────────────────────────────
static void spawnEnemy() {
    float side = g_r01(g_rng) > 0.5f ? (float)(g_winW-250) : 200.0f;
    g_enemies.push_back({side, 150+g_r01(g_rng)*300, 100, 0});
}

static void createParticles(float x, float y, float r, float g, float b, int n) {
    for (int i = 0; i < n; i++) {
        float a = g_r01(g_rng)*2*PI;
        float spd = 1+g_r01(g_rng)*4;
        g_parts.push_back({x,y, cosf(a)*spd, sinf(a)*spd, 1, 2+g_r01(g_rng)*4, {r,g,b}});
    }
}

static void createShieldRing(float x, float y) {
    for (int i = 0; i < 20; i++) {
        float a = 2*PI*i/20;
        g_parts.push_back({x+cosf(a)*30, y+sinf(a)*30, cosf(a)*2, sinf(a)*2, 1, 4, {0,1,0.5f}});
    }
}

static void applyDamage(float amount) {
    if (g_safety) {
        g_blocked++;
        g_shieldFlash = 1;
        createShieldRing(g_px, g_py);
        return;
    }
    float actual = std::min(amount, g_health);
    g_health -= actual;
    g_totalDmg += (int)actual;
    g_dmgFlash = 1;

    const char* part = BODY_PARTS[(int)(g_r01(g_rng)*8)%8];
    const char* inj = INJURIES[g_scene][(int)(g_r01(g_rng)*4)%4];
    const char* sev = amount > 15 ? "SEVERE" : amount > 8 ? "MODERATE" : "MINOR";
    g_injuries.push_back({inj, part, sev});
    if (g_injuries.size() > 8) g_injuries.erase(g_injuries.begin());

    createParticles(g_px, g_py, 1, 0.2f, 0.1f, 20);
}

// ── update DRO ─────────────────────────────────────────────────────
static void updateDRO() {
    int newBase = g_safety ? 32 : 64;
    if (!g_enemies.empty()) newBase = std::min(64, newBase + 16);
    if (newBase != g_currentBase) { g_baseTransitions++; g_currentBase = newBase; }
}

// ── update ─────────────────────────────────────────────────────────
static void update(float dt) {
    g_time += dt;
    g_warnPulse += dt * 3;

    // Player movement
    float spd = 200 * dt;
    if (g_keys[GLFW_KEY_W] || g_keys[GLFW_KEY_UP])    g_py += spd;
    if (g_keys[GLFW_KEY_S] || g_keys[GLFW_KEY_DOWN])   g_py -= spd;
    if (g_keys[GLFW_KEY_A] || g_keys[GLFW_KEY_LEFT])   g_px -= spd;
    if (g_keys[GLFW_KEY_D] || g_keys[GLFW_KEY_RIGHT])  g_px += spd;
    g_px = std::clamp(g_px, 200.0f, (float)g_winW - 50);
    g_py = std::clamp(g_py, 50.0f,  (float)g_winH - 50);

    // Enemies
    for (auto& e : g_enemies) {
        float dx = g_px - e.x, dy = g_py - e.y;
        float dist = sqrtf(dx*dx+dy*dy);
        if (dist > 1) { e.x += dx/dist*80*dt; e.y += dy/dist*80*dt; }

        e.atkCD -= dt;
        if (dist < 50 && e.atkCD <= 0) {
            float dmg = SCENES[g_scene].dmgMin +
                        g_r01(g_rng)*(SCENES[g_scene].dmgMax-SCENES[g_scene].dmgMin);
            applyDamage(dmg);
            e.atkCD = 1.5f;
        }
    }

    // Particles
    for (auto it=g_parts.begin();it!=g_parts.end();){
        it->x+=it->vx; it->y+=it->vy; it->vy-=0.1f; it->life-=dt*2;
        if(it->life<=0)it=g_parts.erase(it);else++it;
    }

    g_dmgFlash = std::max(0.0f, g_dmgFlash - dt*3);
    g_shieldFlash = std::max(0.0f, g_shieldFlash - dt*3);
    updateDRO();
}

// ── render ─────────────────────────────────────────────────────────
static void render() {
    glViewport(0,0,g_winW,g_winH);
    glClearColor(0.04f,0.04f,0.07f,1);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    float m[16]={}; m[0]=2.f/g_winW;m[5]=2.f/g_winH;m[10]=-1;m[12]=-1;m[13]=-1;m[15]=1;
    glLoadMatrixf(m); glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    float areaX=185, areaW=g_winW-185.0f;

    // Holodeck grid
    glColor4f(1,0.7f,0,0.06f);
    for(float gx=areaX;gx<(float)g_winW;gx+=40){
        glBegin(GL_LINES);glVertex2f(gx,0);glVertex2f(gx,(float)g_winH);glEnd();
    }
    for(float gy=0;gy<(float)g_winH;gy+=40){
        glBegin(GL_LINES);glVertex2f(areaX,gy);glVertex2f((float)g_winW,gy);glEnd();
    }

    // Damage flash overlay
    if (g_dmgFlash > 0) {
        glColor4f(1,0,0,g_dmgFlash*0.3f);
        drawRect(areaX,0,areaW,(float)g_winH);
    }
    if (g_shieldFlash > 0) {
        glColor4f(0,1,0.5f,g_shieldFlash*0.2f);
        drawRect(areaX,0,areaW,(float)g_winH);
    }

    // Player
    // Shield ring if safety on
    if (g_safety) {
        float pulse = 0.3f + sinf(g_time*2)*0.1f;
        glColor4f(0,1,0.5f,pulse);
        drawCircle(g_px,g_py,35,24,false);
        drawCircle(g_px,g_py,38,24,false);
    }
    // Body
    glColor4f(0.2f,0.6f,1,1);
    drawCircle(g_px,g_py+12,10,12,true); // head
    drawRect(g_px-10,g_py-15,20,25);     // torso
    glBegin(GL_LINES);
    glVertex2f(g_px-10,g_py-15);glVertex2f(g_px-16,g_py-30); // legs
    glVertex2f(g_px+10,g_py-15);glVertex2f(g_px+16,g_py-30);
    glVertex2f(g_px-10,g_py+5);glVertex2f(g_px-20,g_py-5);  // arms
    glVertex2f(g_px+10,g_py+5);glVertex2f(g_px+20,g_py-5);
    glEnd();

    // Enemies
    for (auto& e : g_enemies) {
        const float* sc = SCENES[g_scene].col;
        glColor4f(sc[0],sc[1],sc[2],0.9f);
        drawCircle(e.x,e.y+10,9,10,true);
        drawRect(e.x-9,e.y-12,18,20);
        // HP bar
        glColor4f(0.3f,0,0,1);
        drawRect(e.x-15,e.y+22,30,4);
        glColor4f(1,0.2f,0.1f,1);
        drawRect(e.x-15,e.y+22,30*e.hp/100,4);
        // Attack indicator
        if (e.atkCD > 0.8f) {
            glColor4f(1,0,0,0.5f);
            glLineWidth(2);
            glBegin(GL_LINES);glVertex2f(e.x,e.y);glVertex2f(g_px,g_py);glEnd();
        }
    }

    // Particles
    for (auto& p : g_parts) {
        glColor4f(p.col[0],p.col[1],p.col[2],p.life);
        drawCircle(p.x,p.y,p.size,6,true);
    }

    // Safety warning banner (when off)
    if (!g_safety) {
        float alpha = 0.5f + sinf(g_warnPulse)*0.3f;
        glColor4f(1,0,0,alpha*0.15f);
        drawRect(areaX,g_winH-35.0f,areaW,35);
        glColor4f(1,0.2f,0.1f,alpha);
        gltext::drawCentered("WARNING: SAFETY PROTOCOLS OFFLINE", areaX+areaW/2, g_winH-22.0f, 1.0f);
    }

    // Health bar (bottom)
    float hbX=areaX+10, hbY=8, hbW=areaW-20, hbH=14;
    glColor4f(0.15f,0.15f,0.2f,0.8f);
    drawRect(hbX,hbY,hbW,hbH);
    float hpPct = g_health/100;
    glColor4f(hpPct>.5f?0:1, hpPct>.5f?0.8f:0.3f, 0.2f, 0.9f);
    drawRect(hbX,hbY,hbW*hpPct,hbH);
    glColor4f(1,1,1,0.9f);
    char buf[64]; snprintf(buf,64,"HEALTH %.0f%%", g_health);
    gltext::drawCentered(buf, areaX+areaW/2, hbY+2, 0.7f);

    // HUD top-right
    glColor4f(0,0,0,0.5f);
    drawRect((float)g_winW-170, g_winH-80.0f, 165, 75);
    float hy = g_winH - 15.0f;
    glColor4f(.6f,.6f,.7f,1); gltext::draw("SAFETY:", g_winW-165.0f, hy, 0.7f);
    if(g_safety){glColor4f(0,1,.5f,1);gltext::draw("ACTIVE",g_winW-95.0f,hy,0.7f);}
    else{glColor4f(1,.2f,.1f,1);gltext::draw("OFFLINE",g_winW-95.0f,hy,0.7f);}
    hy-=14;
    glColor4f(.6f,.6f,.7f,1); gltext::draw("SCENE:", g_winW-165.0f, hy, 0.7f);
    glColor4f(SCENES[g_scene].col[0],SCENES[g_scene].col[1],SCENES[g_scene].col[2],1);
    gltext::draw(SCENES[g_scene].name, g_winW-100.0f, hy, 0.6f);
    hy-=14;
    glColor4f(.6f,.6f,.7f,1); gltext::draw("THREATS:", g_winW-165.0f, hy, 0.7f);
    glColor4f(.8f,.8f,.9f,1); snprintf(buf,64,"%d",(int)g_enemies.size());
    gltext::draw(buf, g_winW-90.0f, hy, 0.7f);
    hy-=14;
    glColor4f(.6f,.6f,.7f,1); gltext::draw("BASE-X:", g_winW-165.0f, hy, 0.7f);
    glColor4f(0,.87f,.93f,1); snprintf(buf,64,"B-%d",g_currentBase);
    gltext::draw(buf, g_winW-90.0f, hy, 0.7f);

    // ── Side panel ────────────────────────────────────────────
    glColor4f(0.02f,0.03f,0.06f,0.92f);
    drawRect(0,0,183,(float)g_winH);
    // LCARS-style curved border
    glColor4f(1,0.6f,0,0.4f);
    glLineWidth(3);
    glBegin(GL_LINES);glVertex2f(183,0);glVertex2f(183,(float)g_winH);glEnd();
    // Top accent bar
    glColor4f(1,0.6f,0,0.6f);
    drawRect(0,g_winH-6.0f,183,6);
    glColor4f(0.6f,0.3f,1,0.6f);
    drawRect(0,g_winH-10.0f,90,4);

    float py = g_winH - 22.0f;

    glColor4f(1,0.6f,0,1);
    gltext::draw("HOLODECK", 8, py, 1.3f); py-=14;
    glColor4f(.5f,.4f,.6f,1);
    gltext::draw("SAFETY CONTROL", 8, py, 0.7f); py-=18;

    // Safety toggle
    glColor4f(g_safety?0:.3f, g_safety?0.4f:0, g_safety?0.2f:.1f, 1);
    drawRect(8,py-2,165,20);
    if(g_safety){glColor4f(0,1,.5f,1);gltext::draw("PROTOCOLS: ACTIVE",12,py+2,0.8f);}
    else{glColor4f(1,.2f,.1f,1);gltext::draw("PROTOCOLS: OFFLINE",12,py+2,0.8f);}
    py -= 28;

    // Divider
    glColor4f(1,0.6f,0,0.2f);
    glBegin(GL_LINES);glVertex2f(8,py);glVertex2f(175,py);glEnd(); py-=8;

    // Stats
    glColor4f(1,0.6f,0,0.8f);
    gltext::draw("DAMAGE STATS", 8, py, 0.8f); py-=16;

    auto stat = [&](const char* label, const char* val, float r, float g, float b) {
        glColor4f(.5f,.5f,.6f,1); gltext::draw(label, 8, py, 0.7f);
        glColor4f(r,g,b,1); gltext::draw(val, 100, py, 0.7f);
        py -= 13;
    };

    snprintf(buf,64,"%.0f%%", g_health);
    stat("HEALTH:", buf, g_health>50?0:1, g_health>50?0.8f:0.3f, 0.2f);
    snprintf(buf,64,"%d", g_totalDmg);
    stat("TOTAL DMG:", buf, .8f,.8f,.9f);
    snprintf(buf,64,"%d", (int)g_injuries.size());
    stat("INJURIES:", buf, g_injuries.empty()?0:1, g_injuries.empty()?0.8f:0.3f, 0.2f);
    snprintf(buf,64,"%d", g_blocked);
    stat("BLOCKED:", buf, 0,1,0.5f);

    py -= 6;
    glColor4f(1,0.6f,0,0.2f);
    glBegin(GL_LINES);glVertex2f(8,py);glVertex2f(175,py);glEnd(); py-=8;

    // DRO
    glColor4f(1,0.6f,0,0.8f);
    gltext::draw("DRO STATUS", 8, py, 0.8f); py-=14;

    snprintf(buf,64,"B-%d", g_currentBase);
    stat("CONTACT:", buf, 0,.87f,.93f);
    snprintf(buf,64,"B-%d", g_safety?24:32);
    stat("ENV:", buf, 0,.87f,.93f);
    snprintf(buf,64,"%s", g_safety?"1.00":"0.00");
    stat("SAFETY:", buf, g_safety?0:1, g_safety?1:0.3f, g_safety?0.5f:0.1f);
    snprintf(buf,64,"%d", g_baseTransitions);
    stat("SHIFTS:", buf, .8f,.8f,.9f);

    py -= 6;
    glColor4f(1,0.6f,0,0.2f);
    glBegin(GL_LINES);glVertex2f(8,py);glVertex2f(175,py);glEnd(); py-=8;

    // Injury log
    glColor4f(1,0.6f,0,0.8f);
    gltext::draw("INJURY LOG", 8, py, 0.8f); py-=4;

    for (int i = (int)g_injuries.size()-1; i >= 0 && py > 30; i--) {
        py -= 12;
        glColor4f(1,0.3f,0.2f,0.8f);
        gltext::draw(g_injuries[i].name, 8, py, 0.55f);
        glColor4f(.5f,.5f,.6f,1);
        gltext::draw(g_injuries[i].loc, 100, py, 0.55f);
    }

    // Controls
    glColor4f(.4f,.4f,.5f,1);
    gltext::draw("TAB SAFETY", 8, 52, 0.6f);
    gltext::draw("SPACE ENEMY", 8, 42, 0.6f);
    gltext::draw("WASD MOVE", 8, 32, 0.6f);
    gltext::draw("1-4 SCENE", 8, 22, 0.6f);
    gltext::draw("R RESET", 8, 12, 0.6f);

    glDisable(GL_BLEND);
}

// ── callbacks ──────────────────────────────────────────────────────
static void keyCB(GLFWwindow*w,int key,int,int act,int){
    if(key>=0&&key<512){
        if(act==GLFW_PRESS)g_keys[key]=true;
        if(act==GLFW_RELEASE)g_keys[key]=false;
    }
    if(act==GLFW_PRESS){
        if(key==GLFW_KEY_ESCAPE)glfwSetWindowShouldClose(w,1);
        if(key==GLFW_KEY_TAB) g_safety=!g_safety;
        if(key==GLFW_KEY_SPACE) spawnEnemy();
        if(key>=GLFW_KEY_1&&key<=GLFW_KEY_4) g_scene=key-GLFW_KEY_1;
        if(key==GLFW_KEY_H){
            float dmg=SCENES[g_scene].dmgMin+g_r01(g_rng)*(SCENES[g_scene].dmgMax-SCENES[g_scene].dmgMin);
            applyDamage(dmg);
        }
        if(key==GLFW_KEY_R){
            g_health=100;g_totalDmg=0;g_blocked=0;g_baseTransitions=0;
            g_injuries.clear();g_enemies.clear();g_parts.clear();
            g_safety=true;g_currentBase=32;
        }
    }
}

int main(){
    if(!glfwInit())return 1;
    GLFWwindow*win=glfwCreateWindow(1024,720,"HLD-003 Holodeck Safety Protocols | Base-X Engine",0,0);
    if(!win){glfwTerminate();return 1;}
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    glfwSetKeyCallback(win,keyCB);
    g_dro.initializeVRProfile();

    printf("HLD-003 Holodeck Safety Protocols\n");
    printf("TAB=safety | SPACE=spawn | WASD=move | H=hazard | 1-4=scenario | R=reset\n");

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
