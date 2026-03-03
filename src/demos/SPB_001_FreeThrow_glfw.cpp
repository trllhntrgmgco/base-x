/**
 * SPB-001: Free Throw Precision — Base-X Engine
 * Converted from html_demos/SPB-001_Free_Throw_Precision.html
 *
 * Split-screen basketball shot. Left = high-base, Right = Base-16.
 * Precision error affects launch velocity + angle → misses.
 *
 * Controls:
 *   SPACE       shoot             W/S  angle up/down
 *   A/D         aim left/right    UP/DOWN power
 *   1-3         free throw / mid / three    Q/E base
 *   R           reset stats       ESC quit
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

static constexpr float PI  = 3.14159265f;
static constexpr float G   = 32.2f;      // ft/s²
static constexpr float RIM_H = 10.0f;    // ft
static constexpr float REL_H = 7.0f;     // ft
static constexpr float RIM_D = 18.0f;    // inches
static constexpr float BALL_D = 9.43f;   // inches

// ── shot types ─────────────────────────────────────────────────────
struct ShotDef { const char*name; float dist; };
static const ShotDef SHOTS[] = {
    {"Free Throw", 15.0f},
    {"Mid-Range",  17.0f},
    {"Three Point",23.75f},
};

// ── ball sim ───────────────────────────────────────────────────────
struct Ball {
    float x,y,vx,vy;
    std::vector<std::pair<float,float>> trail;
    bool active = false;
    int result = 0;  // 0=flying 1=made 2=miss 3=swish
    float time = 0;
    float prec;
};

// ── globals ────────────────────────────────────────────────────────
static BaseX::DROManager g_dro;
static std::mt19937 g_rng(42);
static std::uniform_real_distribution<float> g_r01(0,1);

static int   g_shot = 0;
static int   g_base = 32;
static float g_angle = 52;
static float g_power = 75;
static float g_aim = 0;

struct Stats { int made,att,swish; };
static Stats g_sHi={}, g_sLo={};

static Ball g_hi, g_lo;
static int g_winW=1100, g_winH=720;
static int g_fps=0;
static float g_time=0;

static float precError(int base) {
    switch(base){
        case 64: return 0.001f; case 48: return 0.005f;
        case 32: return 0.02f;  case 24: return 0.05f;
        case 16: return 0.10f;  case 8:  return 0.25f;
        default: return 0.50f;
    }
}

// ── shoot ──────────────────────────────────────────────────────────
static Ball createShot(float prec) {
    Ball b{};
    b.prec = prec;
    b.active = true;
    b.x = 0;
    b.y = REL_H;

    float dist = SHOTS[g_shot].dist;
    float dy = RIM_H - REL_H;
    float ang = g_angle * PI / 180;
    float cosA = cosf(ang), tanA = tanf(ang);

    // Required velocity from projectile eq
    float denom = 2*cosA*cosA*(dist*tanA - dy);
    float baseV = denom > 0 ? sqrtf(G*dist*dist / denom) : 20;
    float vel = baseV * (g_power / 75.0f);

    // Add precision error
    float velErr = (g_r01(g_rng)-0.5f) * prec * 3;
    float angErr = (g_r01(g_rng)-0.5f) * prec * 0.03f;

    b.vx = (vel + velErr) * cosf(ang + angErr);
    b.vy = (vel + velErr) * sinf(ang + angErr);
    return b;
}

static void shoot() {
    g_hi = createShot(precError(g_base));
    g_lo = createShot(precError(16));
    g_sHi.att++; g_sLo.att++;
}

// ── update ball ────────────────────────────────────────────────────
static void updateBall(Ball& b, Stats& s, float dt) {
    if (!b.active) return;
    b.time += dt;

    b.trail.push_back({b.x, b.y});
    if (b.trail.size() > 200) b.trail.erase(b.trail.begin());

    b.vy -= G * dt;

    // Quantize velocity
    float q = std::max(0.001f, b.prec * 0.1f);
    b.vx = roundf(b.vx/q)*q;
    b.vy = roundf(b.vy/q)*q;

    b.x += b.vx * dt;
    b.y += b.vy * dt;

    // Rim collision check
    float dist = SHOTS[g_shot].dist;
    float rimLeft = dist - RIM_D/24;     // in feet
    float rimRight = dist;
    float ballR = BALL_D/24;

    if (b.y <= RIM_H + ballR && b.y >= RIM_H - ballR*2) {
        if (b.x >= rimLeft && b.x <= rimRight) {
            // Ball is at rim height and within rim width
            float centerX = (rimLeft + rimRight) / 2;
            float offCenter = fabsf(b.x - centerX);
            float clearance = (RIM_D/12 - BALL_D/12) / 2;

            if (offCenter < clearance * 0.3f && b.vy < 0) {
                // Swish - clean through center
                b.result = 3; s.made++; s.swish++;
                b.active = false;
            } else if (offCenter < clearance) {
                // Made it
                b.result = 1; s.made++;
                b.active = false;
            } else if (b.x > rimRight - 0.2f || b.x < rimLeft + 0.2f) {
                // Hit rim
                b.vy = fabsf(b.vy) * 0.4f;
                b.vx *= -0.3f;
            }
        }
    }

    // Ground / out of bounds
    if (b.y < 0 || b.x > dist + 5 || b.time > 5) {
        if (b.result == 0) b.result = 2; // miss
        b.active = false;
    }
}

static void update(float dt) {
    g_time += dt;
    updateBall(g_hi, g_sHi, dt);
    updateBall(g_lo, g_sLo, dt);
}

// ── draw helpers ───────────────────────────────────────────────────
static void drawRect(float x,float y,float w,float h){
    glBegin(GL_QUADS);
    glVertex2f(x,y);glVertex2f(x+w,y);glVertex2f(x+w,y+h);glVertex2f(x,y+h);
    glEnd();
}
static void drawCircle(float cx,float cy,float r,int s,bool fill){
    if(fill)glBegin(GL_TRIANGLE_FAN);else glBegin(GL_LINE_LOOP);
    if(fill)glVertex2f(cx,cy);
    for(int i=0;i<=s;i++){float a=2*PI*i/s;glVertex2f(cx+cosf(a)*r,cy+sinf(a)*r);}
    glEnd();
}

// ── render one court panel ─────────────────────────────────────────
static void renderPanel(float px, float py, float pw, float ph,
                        Ball& ball, Stats& st, int base, const char* label) {
    glEnable(GL_SCISSOR_TEST);
    glScissor((int)px, 0, (int)pw, g_winH);

    // Background
    glColor4f(0.1f,0.08f,0.07f,1);
    drawRect(px, py, pw, ph);

    // Court floor
    glColor4f(0.8f,0.53f,0.27f,1);
    drawRect(px, py, pw, 60);

    // Court line
    glColor4f(1,1,1,0.25f);
    glLineWidth(2);
    glBegin(GL_LINES);glVertex2f(px,py+60);glVertex2f(px+pw,py+60);glEnd();

    // Scale
    float dist = SHOTS[g_shot].dist;
    float scale = (pw - 100) / (dist + 5);
    float gndY = py + 60;

    // Backboard
    float bbX = px + 50 + dist * scale;
    float bbY = gndY + RIM_H * scale * 0.9f;
    glColor4f(0.9f,0.9f,0.87f,0.8f);
    drawRect(bbX - 3, bbY - 30, 6, 45);

    // Pole
    glColor4f(0.4f,0.4f,0.4f,1);
    drawRect(bbX + 3, gndY, 6, bbY - gndY + 15);

    // Rim
    float rimY = gndY + RIM_H * scale;
    float rimEndX = bbX - 3 - (RIM_D/12) * scale;
    glColor4f(0.8f,0.2f,0.13f,1);
    glLineWidth(4);
    glBegin(GL_LINES);
    glVertex2f(bbX - 3, rimY);
    glVertex2f(rimEndX, rimY);
    glEnd();

    // Net
    glColor4f(0.87f,0.8f,0.73f,0.3f);
    glLineWidth(1);
    for (int i = 0; i < 6; i++) {
        float nx = rimEndX + i * (bbX-3-rimEndX)/6;
        glBegin(GL_LINES);
        glVertex2f(nx, rimY);
        glVertex2f(nx + (g_r01(g_rng)-0.5f)*3, rimY - 20);
        glEnd();
    }

    // Free throw line
    float ftX = px + 50 + 15 * scale;
    glColor4f(1,1,1,0.3f);
    glLineWidth(2);
    glBegin(GL_LINES);glVertex2f(ftX, gndY);glVertex2f(ftX, gndY+15);glEnd();

    // Player silhouette
    float plX = px + 50;
    glColor4f(0.35f,0.35f,0.35f,1);
    drawCircle(plX, gndY + 55, 10, 16, true);
    drawRect(plX-7, gndY+10, 14, 40);
    drawRect(plX-9, gndY, 8, 12);
    drawRect(plX+1, gndY, 8, 12);

    // Launch angle indicator
    float angRad = g_angle * PI / 180;
    glColor4f(1,1,1,0.2f);
    glLineWidth(1);
    glBegin(GL_LINES);
    glVertex2f(plX, gndY + REL_H * scale * 0.9f);
    glVertex2f(plX + cosf(angRad)*40, gndY + REL_H*scale*0.9f + sinf(angRad)*40);
    glEnd();

    // Trajectory
    if (ball.trail.size() > 1) {
        bool isHi = (base != 16);
        glColor4f(isHi?0:1, isHi?0.87f:0.27f, isHi?0.53f:0.33f, 0.7f);
        glLineWidth(2);
        glBegin(GL_LINE_STRIP);
        for (auto& p : ball.trail) {
            glVertex2f(px + 50 + p.first * scale, gndY + p.second * scale);
        }
        glEnd();
    }

    // Ball
    float ballSX = px + 50 + ball.x * scale;
    float ballSY = gndY + ball.y * scale;
    float ballR = (BALL_D/12) * scale / 2;
    if (ballR < 4) ballR = 4;

    // Shadow
    glColor4f(0,0,0,0.3f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(ballSX, gndY+2);
    for(int i=0;i<=16;i++){float a=2*PI*i/16;glVertex2f(ballSX+cosf(a)*ballR,gndY+2+sinf(a)*ballR*0.3f);}
    glEnd();

    // Ball body
    glColor4f(0.9f,0.5f,0.15f,1);
    drawCircle(ballSX, ballSY, ballR, 16, true);
    glColor4f(0.3f,0.15f,0.05f,0.5f);
    drawCircle(ballSX, ballSY, ballR, 16, false);
    // Seam lines
    glColor4f(0,0,0,0.3f);
    glLineWidth(1);
    glBegin(GL_LINES);
    glVertex2f(ballSX-ballR*0.8f,ballSY);glVertex2f(ballSX+ballR*0.8f,ballSY);
    glVertex2f(ballSX,ballSY-ballR*0.8f);glVertex2f(ballSX,ballSY+ballR*0.8f);
    glEnd();

    // Result text
    if (!ball.active && ball.result > 0) {
        if (ball.result == 3) {
            glColor4f(0,1,0.5f,0.9f);
            gltext::drawCentered("SWISH!", px+pw/2, gndY+ph*0.6f, 1.5f);
        } else if (ball.result == 1) {
            glColor4f(0.2f,0.9f,0.3f,0.9f);
            gltext::drawCentered("MADE IT", px+pw/2, gndY+ph*0.6f, 1.2f);
        } else {
            glColor4f(1,0.3f,0.2f,0.9f);
            gltext::drawCentered("MISS", px+pw/2, gndY+ph*0.6f, 1.2f);
        }
    }

    // Label
    float bc[3];
    switch(base){case 64:bc[0]=0;bc[1]=1;bc[2]=.5f;break;case 48:bc[0]=.5f;bc[1]=1;bc[2]=.3f;break;
        case 32:bc[0]=1;bc[1]=1;bc[2]=.2f;break;default:bc[0]=1;bc[1]=.4f;bc[2]=.2f;break;}
    glColor4f(bc[0],bc[1],bc[2],1);
    char buf[64];
    snprintf(buf,64,"%s BASE-%d", label, base);
    gltext::draw(buf, px+4, py+ph-14, 0.9f);

    // Stats
    float pct = st.att>0 ? st.made*100.f/st.att : 0;
    glColor4f(.8f,.8f,.9f,1);
    snprintf(buf,64,"%d/%d  %.0f%%", st.made, st.att, pct);
    gltext::drawRight(buf, px+pw-4, py+ph-14, 0.9f);

    glDisable(GL_SCISSOR_TEST);
}

static void render() {
    glViewport(0,0,g_winW,g_winH);
    glClearColor(0.04f,0.04f,0.06f,1);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    float m[16]={}; m[0]=2.f/g_winW;m[5]=2.f/g_winH;m[10]=-1;m[12]=-1;m[13]=-1;m[15]=1;
    glLoadMatrixf(m);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    float hw = (g_winW - 185) / 2.0f;

    renderPanel(185, 0, hw-2, (float)g_winH, g_hi, g_sHi, g_base, "HIGH");
    renderPanel(185+hw+2, 0, hw-2, (float)g_winH, g_lo, g_sLo, 16, "LOW");

    // Divider
    glColor4f(0.9f,0.5f,0.15f,0.4f);
    drawRect(185+hw-1, 0, 2, (float)g_winH);

    // ── Side panel ────────────────────────────────────────────
    glColor4f(0.02f,0.03f,0.06f,0.9f);
    drawRect(0,0,183,(float)g_winH);
    glColor4f(0.9f,0.5f,0.15f,0.3f);
    glBegin(GL_LINES);glVertex2f(183,0);glVertex2f(183,(float)g_winH);glEnd();

    float py = g_winH - 18.0f;
    char buf[128];

    glColor4f(0.9f,0.5f,0.15f,1);
    gltext::draw("FREE THROW", 8, py, 1.2f); py-=18;

    // Shot type
    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("SHOT:", 8, py, 0.7f);
    glColor4f(1,0.85f,0.2f,1);
    gltext::draw(SHOTS[g_shot].name, 48, py, 0.7f); py-=14;

    // Angle
    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("ANGLE:", 8, py, 0.7f);
    glColor4f(.8f,.8f,.9f,1);
    snprintf(buf,128,"%.0f*", g_angle);
    gltext::draw(buf, 58, py, 0.7f); py-=12;

    // Power
    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("POWER:", 8, py, 0.7f);
    glColor4f(.8f,.8f,.9f,1);
    snprintf(buf,128,"%.0f%%", g_power);
    gltext::draw(buf, 58, py, 0.7f);
    // Power bar
    glColor4f(.2f,.2f,.25f,1);
    drawRect(100, py, 70, 8);
    glColor4f(0.9f,0.5f,0.15f,0.8f);
    drawRect(100, py, g_power*0.7f, 8);
    py -= 18;

    // Divider
    glColor4f(0.9f,0.5f,0.15f,0.2f);
    glBegin(GL_LINES);glVertex2f(8,py);glVertex2f(175,py);glEnd(); py-=8;

    // Precision
    glColor4f(0.9f,0.5f,0.15f,0.8f);
    gltext::draw("PRECISION", 8, py, 0.8f); py-=14;

    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("HIGH:", 8, py, 0.7f);
    float bc[3];
    switch(g_base){case 64:bc[0]=0;bc[1]=1;bc[2]=.5f;break;case 48:bc[0]=.5f;bc[1]=1;bc[2]=.3f;break;
        case 32:bc[0]=1;bc[1]=1;bc[2]=.2f;break;default:bc[0]=1;bc[1]=.4f;bc[2]=.2f;break;}
    glColor4f(bc[0],bc[1],bc[2],1);
    snprintf(buf,128,"BASE-%d",g_base);
    gltext::draw(buf, 50, py, 0.7f); py-=12;

    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("LOW:", 8, py, 0.7f);
    glColor4f(1,.4f,.2f,1);
    gltext::draw("BASE-16", 50, py, 0.7f); py-=18;

    // Divider
    glColor4f(0.9f,0.5f,0.15f,0.2f);
    glBegin(GL_LINES);glVertex2f(8,py);glVertex2f(175,py);glEnd(); py-=8;

    // Shooting stats
    glColor4f(0.9f,0.5f,0.15f,0.8f);
    gltext::draw("HIGH STATS", 8, py, 0.8f); py-=14;

    float pctHi = g_sHi.att>0 ? g_sHi.made*100.f/g_sHi.att : 0;
    glColor4f(.5f,.5f,.6f,1); gltext::draw("MADE:", 8, py, 0.7f);
    glColor4f(.2f,.9f,.3f,1); snprintf(buf,128,"%d/%d",g_sHi.made,g_sHi.att);
    gltext::draw(buf, 50, py, 0.7f); py-=12;

    glColor4f(.5f,.5f,.6f,1); gltext::draw("PCT:", 8, py, 0.7f);
    glColor4f(pctHi>60?.2f:1, pctHi>60?1:.3f, .2f, 1);
    snprintf(buf,128,"%.0f%%", pctHi);
    gltext::draw(buf, 40, py, 0.7f); py-=12;

    glColor4f(.5f,.5f,.6f,1); gltext::draw("SWISH:", 8, py, 0.7f);
    glColor4f(0,.9f,.5f,1); snprintf(buf,128,"%d",g_sHi.swish);
    gltext::draw(buf, 58, py, 0.7f); py-=16;

    glColor4f(0.9f,0.5f,0.15f,0.8f);
    gltext::draw("LOW STATS", 8, py, 0.8f); py-=14;

    float pctLo = g_sLo.att>0 ? g_sLo.made*100.f/g_sLo.att : 0;
    glColor4f(.5f,.5f,.6f,1); gltext::draw("MADE:", 8, py, 0.7f);
    glColor4f(1,.4f,.2f,1); snprintf(buf,128,"%d/%d",g_sLo.made,g_sLo.att);
    gltext::draw(buf, 50, py, 0.7f); py-=12;

    glColor4f(.5f,.5f,.6f,1); gltext::draw("PCT:", 8, py, 0.7f);
    glColor4f(pctLo>60?.2f:1, pctLo>60?1:.3f, .2f, 1);
    snprintf(buf,128,"%.0f%%", pctLo);
    gltext::draw(buf, 40, py, 0.7f); py-=12;

    glColor4f(.5f,.5f,.6f,1); gltext::draw("SWISH:", 8, py, 0.7f);
    glColor4f(0,.9f,.5f,1); snprintf(buf,128,"%d",g_sLo.swish);
    gltext::draw(buf, 58, py, 0.7f); py-=18;

    // Controls
    glColor4f(.4f,.4f,.5f,1);
    gltext::draw("SPACE SHOOT", 8, 64, 0.6f);
    gltext::draw("W/S ANGLE", 8, 54, 0.6f);
    gltext::draw("UP/DN POWER", 8, 44, 0.6f);
    gltext::draw("1-3 SHOT TYPE", 8, 34, 0.6f);
    gltext::draw("Q/E BASE", 8, 24, 0.6f);
    gltext::draw("R RESET", 8, 14, 0.6f);

    glDisable(GL_BLEND);
}

// ── callbacks ──────────────────────────────────────────────────────
static void keyCB(GLFWwindow*w,int key,int,int act,int){
    if(act==GLFW_PRESS){
        if(key==GLFW_KEY_ESCAPE)glfwSetWindowShouldClose(w,1);
        if(key==GLFW_KEY_SPACE) shoot();
        if(key==GLFW_KEY_R){g_sHi={}; g_sLo={}; g_hi=Ball{}; g_lo=Ball{};}
        if(key>=GLFW_KEY_1&&key<=GLFW_KEY_3) g_shot=key-GLFW_KEY_1;
    }
    if(act==GLFW_PRESS||act==GLFW_REPEAT){
        int bases[]={4,8,16,24,32,48,64};
        if(key==GLFW_KEY_Q){for(int i=6;i>=0;i--)if(bases[i]<g_base){g_base=bases[i];break;}}
        if(key==GLFW_KEY_E){for(int i=0;i<7;i++)if(bases[i]>g_base){g_base=bases[i];break;}}
        if(key==GLFW_KEY_W) g_angle=std::min(75.f,g_angle+1);
        if(key==GLFW_KEY_S) g_angle=std::max(30.f,g_angle-1);
        if(key==GLFW_KEY_UP) g_power=std::min(100.f,g_power+2);
        if(key==GLFW_KEY_DOWN) g_power=std::max(30.f,g_power-2);
    }
}

int main(){
    if(!glfwInit())return 1;
    GLFWwindow*win=glfwCreateWindow(1100,720,"SPB-001 Free Throw Precision | Base-X Engine",0,0);
    if(!win){glfwTerminate();return 1;}
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    glfwSetKeyCallback(win,keyCB);
    g_dro.initializeVRProfile();

    printf("SPB-001 Free Throw Precision\n");
    printf("SPACE shoot | W/S angle | UP/DN power | 1-3 shot type | Q/E base | R reset\n");

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
