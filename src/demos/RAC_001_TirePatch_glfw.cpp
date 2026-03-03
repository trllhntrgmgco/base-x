/**
 * RAC-001: Tire Contact Patch — Base-X Engine
 * Converted from html_demos/RAC-001_Tire_Contact_Patch.html
 *
 * Split-screen tire simulation. Left = high-base, Right = Base-16.
 * Pacejka grip model with precision error from DRO quantization.
 * Shows how low base causes grip jitter → spin-outs.
 *
 * Controls:
 *   UP/DOWN    throttle/brake      LEFT/RIGHT  steering
 *   1-4        tire compound       Q/E  change base
 *   R          reset               ESC  quit
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

// ── tire compounds ─────────────────────────────────────────────────
struct Compound { const char*name; float grip,wear,optTemp; float col[3]; };
static const Compound COMPOUNDS[] = {
    {"Soft",   1.15f, 1.5f, 95, {0.87f,0.13f,0.13f}},
    {"Medium", 1.00f, 1.0f, 90, {0.87f,0.87f,0.13f}},
    {"Hard",   0.90f, 0.7f, 85, {0.9f, 0.9f, 0.9f}},
    {"Wet",    0.80f, 0.5f, 60, {0.13f,0.67f,0.87f}},
};

// ── sim state for one side ─────────────────────────────────────────
struct TireSim {
    float grip = 1.0f;
    float slipAngle = 0, slipRatio = 0;
    float tireTemp = 80;
    float wheelAngle = 0;      // visual rotation
    // History for grip graph
    float gripHist[120] = {};
    int   histIdx = 0;
};

// ── globals ────────────────────────────────────────────────────────
static BaseX::DROManager g_dro;
static std::mt19937 g_rng(42);
static std::uniform_real_distribution<float> g_r01(0,1);

static int   g_compound = 0;
static int   g_base = 32;
static float g_speed = 100;
static float g_steer = 0;
static float g_throttle = 0, g_brake = 0;
static float g_wear = 0;
static float g_time = 0;

static TireSim g_hi, g_lo;
static int g_winW = 1100, g_winH = 720;
static int g_fps = 0;

static float precError(int base) {
    switch(base){
        case 64: return 0.001f; case 48: return 0.005f;
        case 32: return 0.02f;  case 24: return 0.05f;
        case 16: return 0.10f;  case 8:  return 0.25f;
        default: return 0.50f;
    }
}

// ── Pacejka-style grip ─────────────────────────────────────────────
static float calcGrip(float slipA, float slipR, int comp, float temp,
                      float wear, float prec) {
    const Compound& c = COMPOUNDS[comp];
    float tempDiff = fabsf(temp - c.optTemp);
    float tempF = std::max(0.5f, 1-tempDiff*0.01f);
    float wearF = std::max(0.3f, 1-wear*0.007f);

    float combined = sqrtf(slipA*slipA + slipR*slipR);
    float peak = 8.0f;
    float curve;
    if (combined < peak)
        curve = combined / peak;
    else
        curve = 1.0f - (combined-peak)*0.03f;
    curve = std::clamp(curve, 0.1f, 1.0f);

    float error = (g_r01(g_rng)-0.5f) * prec * 0.5f;
    float grip = c.grip * tempF * wearF * curve + error;
    return std::clamp(grip, 0.0f, 1.3f);
}

static float calcSlipAngle(float steer, float speed, float prec) {
    float base = fabsf(steer)*0.5f*(1+speed/200);
    float err = (g_r01(g_rng)-0.5f)*prec*2;
    return roundf((base+err)/prec)*prec;
}

static float calcSlipRatio(float throttle, float brake, float prec) {
    float ratio = 0;
    if (throttle > 0) ratio = (throttle/100)*20;
    if (brake > 0) ratio = -(brake/100)*30;
    float err = (g_r01(g_rng)-0.5f)*prec*5;
    return roundf((ratio+err)/prec)*prec;
}

static void updateSide(TireSim& s, float prec) {
    s.slipAngle = calcSlipAngle(g_steer, g_speed, prec);
    s.slipRatio = calcSlipRatio(g_throttle, g_brake, prec);

    // Temperature
    float heatGen = (fabsf(s.slipAngle)+fabsf(s.slipRatio))*0.1f;
    float cooling = (s.tireTemp - 30)*0.005f;
    s.tireTemp += heatGen - cooling;
    s.tireTemp = std::clamp(s.tireTemp, 30.0f, 150.0f);

    s.grip = calcGrip(s.slipAngle, s.slipRatio, g_compound,
                      s.tireTemp, g_wear, prec);
    s.wheelAngle += g_speed * 0.01f;

    // History
    s.gripHist[s.histIdx % 120] = s.grip;
    s.histIdx++;
}

static void update(float dt) {
    g_time += dt;
    g_wear += COMPOUNDS[g_compound].wear * 0.002f * dt;
    g_wear = std::min(g_wear, 100.0f);

    updateSide(g_hi, precError(g_base));
    updateSide(g_lo, precError(16));
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

// ── render one tire panel ──────────────────────────────────────────
static void renderPanel(float px,float py,float pw,float ph,
                        TireSim& sim, int base, const char* label) {
    glEnable(GL_SCISSOR_TEST);
    glScissor((int)px,0,(int)pw,g_winH);

    // Background
    glColor4f(0.05f,0.05f,0.08f,1);
    drawRect(px,py,pw,ph);

    float cx = px + pw/2, cy = py + ph*0.45f;

    // ── Tire top-down view ─────────────────────────────────────
    float tireW = pw*0.35f, tireH = pw*0.5f;

    // Tire outline
    glColor4f(0.3f,0.3f,0.3f,1);
    glLineWidth(3);
    glBegin(GL_LINE_LOOP);
    // Rounded rectangle
    int segs = 12;
    float rr = tireW*0.3f;
    for(int c=0;c<4;c++){
        float ox = (c==0||c==3)?cx-tireW/2+rr:cx+tireW/2-rr;
        float oy = (c<2)?cy+tireH/2-rr:cy-tireH/2+rr;
        float sa = PI/2*c;
        for(int i=0;i<=segs;i++){
            float a=sa+PI/2*i/segs;
            glVertex2f(ox+cosf(a)*rr, oy+sinf(a)*rr);
        }
    }
    glEnd();

    // Contact patch (ellipse at bottom of tire)
    float patchW = tireW * 0.7f;
    float patchH = tireH * 0.18f * (1 + sim.grip*0.3f);
    float patchY = cy;

    // Slip vector offset
    float slipOff = sim.slipAngle * 0.8f;

    // Patch color based on grip
    float gr = sim.grip;
    float pr,pg,pb;
    if (gr > 0.8f) { pr=0.1f; pg=0.8f; pb=0.2f; }       // green = grip
    else if (gr > 0.5f) { pr=0.9f; pg=0.8f; pb=0.1f; }   // yellow = slip
    else { pr=0.9f; pg=0.2f; pb=0.1f; }                   // red = spin

    // Glow
    glColor4f(pr,pg,pb,0.15f);
    drawCircle(cx+slipOff, patchY, patchW*1.2f, 32, true);

    // Contact patch
    glColor4f(pr,pg,pb,0.6f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx+slipOff, patchY);
    for(int i=0;i<=32;i++){
        float a=2*PI*i/32;
        glVertex2f(cx+slipOff+cosf(a)*patchW/2, patchY+sinf(a)*patchH/2);
    }
    glEnd();

    // Patch outline
    glColor4f(pr,pg,pb,0.9f);
    glLineWidth(2);
    glBegin(GL_LINE_LOOP);
    for(int i=0;i<32;i++){
        float a=2*PI*i/32;
        glVertex2f(cx+slipOff+cosf(a)*patchW/2, patchY+sinf(a)*patchH/2);
    }
    glEnd();

    // Tread lines (rotating)
    glColor4f(0.4f,0.4f,0.4f,0.3f);
    glLineWidth(1);
    for(int i=0;i<8;i++){
        float ty = cy - tireH/2 + fmodf(sim.wheelAngle*3 + i*tireH/8, tireH);
        glBegin(GL_LINES);
        glVertex2f(cx-tireW/2+5, ty);
        glVertex2f(cx+tireW/2-5, ty);
        glEnd();
    }

    // ── Grip graph ─────────────────────────────────────────────
    float graphX = px+10, graphY = py+10, graphW = pw-20, graphH = ph*0.2f;

    // Graph background
    glColor4f(0,0,0,0.4f);
    drawRect(graphX, graphY, graphW, graphH);

    // 1.0 grip line
    float oneY = graphY + graphH * (1 - 1.0f/1.3f);
    glColor4f(1,1,1,0.15f);
    glBegin(GL_LINES);glVertex2f(graphX,oneY);glVertex2f(graphX+graphW,oneY);glEnd();

    // Grip trace
    glLineWidth(2);
    glBegin(GL_LINE_STRIP);
    for(int i=0;i<120;i++){
        int idx = (sim.histIdx - 120 + i + 12000) % 120;
        float v = sim.gripHist[idx];
        float gx = graphX + (float)i/120 * graphW;
        float gy = graphY + graphH * (1 - v/1.3f);
        if(v>0.8f) glColor4f(0.1f,0.8f,0.2f,0.9f);
        else if(v>0.5f) glColor4f(0.9f,0.8f,0.1f,0.9f);
        else glColor4f(0.9f,0.2f,0.1f,0.9f);
        glVertex2f(gx,gy);
    }
    glEnd();

    // Graph border
    glColor4f(1,1,1,0.2f);
    glLineWidth(1);
    glBegin(GL_LINE_LOOP);
    glVertex2f(graphX,graphY);glVertex2f(graphX+graphW,graphY);
    glVertex2f(graphX+graphW,graphY+graphH);glVertex2f(graphX,graphY+graphH);
    glEnd();

    // ── Readouts ───────────────────────────────────────────────
    char buf[64];
    float ry = py + ph - 14;

    // Label + base
    float bc[3];
    switch(base){case 64:bc[0]=0;bc[1]=1;bc[2]=.5f;break;case 48:bc[0]=.5f;bc[1]=1;bc[2]=.3f;break;
        case 32:bc[0]=1;bc[1]=1;bc[2]=.2f;break;default:bc[0]=1;bc[1]=.4f;bc[2]=.2f;break;}
    glColor4f(bc[0],bc[1],bc[2],1);
    snprintf(buf,64,"%s BASE-%d",label,base);
    gltext::draw(buf, px+4, ry, 0.9f);

    // Grip value
    glColor4f(pr,pg,pb,1);
    snprintf(buf,64,"GRIP %.0f%%", sim.grip*100);
    gltext::drawRight(buf, px+pw-4, ry, 0.9f);

    // Slip info
    ry -= 14;
    glColor4f(.6f,.6f,.7f,1);
    snprintf(buf,64,"SLIP %.1f* RATIO %.1f", sim.slipAngle, sim.slipRatio);
    gltext::draw(buf, px+4, ry, 0.6f);

    // Temp
    snprintf(buf,64,"%.0fC", sim.tireTemp);
    gltext::drawRight(buf, px+pw-4, ry, 0.6f);

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

    // Left = high precision
    renderPanel(185, 0, hw-2, (float)g_winH, g_hi, g_base, "HIGH");

    // Right = Base-16
    renderPanel(185+hw+2, 0, hw-2, (float)g_winH, g_lo, 16, "LOW");

    // Divider
    glColor4f(0.87f,0.13f,0.13f,0.4f);
    drawRect(185+hw-1, 0, 2, (float)g_winH);

    // ── Side panel ────────────────────────────────────────────
    glColor4f(0.02f,0.03f,0.06f,0.9f);
    drawRect(0,0,183,(float)g_winH);
    glColor4f(0.87f,0.13f,0.13f,0.3f);
    glBegin(GL_LINES);glVertex2f(183,0);glVertex2f(183,(float)g_winH);glEnd();

    float py = g_winH - 18.0f;
    char buf[128];

    glColor4f(0.87f,0.13f,0.13f,1);
    gltext::draw("TIRE CONTACT", 8, py, 1.1f); py-=16;

    // Compound
    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("COMPOUND:", 8, py, 0.7f);
    const Compound& c = COMPOUNDS[g_compound];
    glColor4f(c.col[0],c.col[1],c.col[2],1);
    gltext::draw(c.name, 85, py, 0.7f); py-=14;

    // Speed
    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("SPEED:", 8, py, 0.7f);
    glColor4f(.8f,.8f,.9f,1);
    snprintf(buf,128,"%.0f KPH", g_speed);
    gltext::draw(buf, 55, py, 0.7f); py-=14;

    // Steering
    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("STEER:", 8, py, 0.7f);
    glColor4f(.8f,.8f,.9f,1);
    snprintf(buf,128,"%.0f*", g_steer);
    gltext::draw(buf, 55, py, 0.7f); py-=14;

    // Throttle/Brake bar
    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("THROTTLE:", 8, py, 0.7f);
    glColor4f(0,.8f,.3f,0.7f);
    drawRect(85, py, g_throttle*0.8f, 8);
    py -= 12;
    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("BRAKE:", 8, py, 0.7f);
    glColor4f(.9f,.2f,.1f,0.7f);
    drawRect(55, py, g_brake*0.8f, 8);
    py -= 14;

    // Wear
    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("WEAR:", 8, py, 0.7f);
    glColor4f(.8f,.8f,.9f,1);
    snprintf(buf,128,"%.0f%%", g_wear);
    gltext::draw(buf, 50, py, 0.7f);
    // Wear bar
    glColor4f(.3f,.3f,.35f,1);
    drawRect(85,py,80,8);
    glColor4f(g_wear>60?1:.5f, g_wear>60?.3f:.8f, .2f, 0.8f);
    drawRect(85,py,g_wear*0.8f,8);
    py -= 18;

    // Divider
    glColor4f(0.87f,0.13f,0.13f,0.2f);
    glBegin(GL_LINES);glVertex2f(8,py);glVertex2f(175,py);glEnd(); py-=8;

    // Precision
    glColor4f(0.87f,0.13f,0.13f,0.8f);
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
    gltext::draw("BASE-16", 50, py, 0.7f); py-=12;

    glColor4f(.5f,.5f,.6f,1);
    gltext::draw("ERROR HI:", 8, py, 0.7f);
    glColor4f(.8f,.8f,.9f,1);
    snprintf(buf,128,"%.0f um", precError(g_base)*1e6f);
    gltext::draw(buf, 80, py, 0.7f); py-=18;

    // Controls
    glColor4f(.4f,.4f,.5f,1);
    gltext::draw("UP/DN ACCEL", 8, 64, 0.6f);
    gltext::draw("LEFT/RIGHT STEER", 8, 54, 0.6f);
    gltext::draw("1-4 COMPOUND", 8, 44, 0.6f);
    gltext::draw("Q/E BASE", 8, 34, 0.6f);
    gltext::draw("R RESET", 8, 24, 0.6f);

    glColor4f(.4f,.4f,.5f,1);
    snprintf(buf,128,"FPS %d",g_fps);
    gltext::draw(buf, 8, 8, 0.6f);

    glDisable(GL_BLEND);
}

// ── callbacks ──────────────────────────────────────────────────────
static void keyCB(GLFWwindow*w,int key,int,int act,int){
    if(act==GLFW_PRESS){
        if(key==GLFW_KEY_ESCAPE)glfwSetWindowShouldClose(w,1);
        if(key==GLFW_KEY_R){g_wear=0;g_hi=TireSim();g_lo=TireSim();g_throttle=g_brake=g_steer=0;}
        if(key>=GLFW_KEY_1&&key<=GLFW_KEY_4)g_compound=key-GLFW_KEY_1;
    }
    if(act==GLFW_PRESS||act==GLFW_REPEAT){
        int bases[]={4,8,16,24,32,48,64};
        if(key==GLFW_KEY_Q){for(int i=6;i>=0;i--)if(bases[i]<g_base){g_base=bases[i];break;}}
        if(key==GLFW_KEY_E){for(int i=0;i<7;i++)if(bases[i]>g_base){g_base=bases[i];break;}}
        if(key==GLFW_KEY_UP){g_throttle=std::min(100.f,g_throttle+10);g_brake=0;}
        if(key==GLFW_KEY_DOWN){g_brake=std::min(100.f,g_brake+10);g_throttle=0;}
        if(key==GLFW_KEY_LEFT)g_steer=std::max(-30.f,g_steer-5);
        if(key==GLFW_KEY_RIGHT)g_steer=std::min(30.f,g_steer+5);
    }
    if(act==GLFW_RELEASE){
        if(key==GLFW_KEY_UP)g_throttle=0;
        if(key==GLFW_KEY_DOWN)g_brake=0;
        if(key==GLFW_KEY_LEFT||key==GLFW_KEY_RIGHT)g_steer=0;
    }
}

int main(){
    if(!glfwInit())return 1;
    GLFWwindow*win=glfwCreateWindow(1100,720,"RAC-001 Tire Contact Patch | Base-X Engine",0,0);
    if(!win){glfwTerminate();return 1;}
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    glfwSetKeyCallback(win,keyCB);
    g_dro.initializeVRProfile();

    printf("RAC-001 Tire Contact Patch\n");
    printf("UP/DOWN throttle/brake | LEFT/RIGHT steer | 1-4 compound | Q/E base | R reset\n");

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
