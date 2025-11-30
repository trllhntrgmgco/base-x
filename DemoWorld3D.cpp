#include <windows.h>
#include <mmsystem.h>
#include <cmath>
#include "DemoWorld3D.h"

#pragma comment(lib, "winmm.lib")

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline double ToRad(double deg){ return deg * M_PI / 180.0; }

// Enhanced 3D World Colors
#define WORLD3D_NEON_RED RGB(255, 51, 51)
#define WORLD3D_NEON_GREEN RGB(51, 255, 51)
#define WORLD3D_NEON_BLUE RGB(51, 153, 255)
#define WORLD3D_NEON_YELLOW RGB(255, 255, 51)
#define WORLD3D_NEON_CYAN RGB(51, 255, 255)
#define WORLD3D_NEON_MAGENTA RGB(255, 51, 255)
#define WORLD3D_ELECTRIC_PURPLE RGB(186, 85, 211)
#define WORLD3D_LASER_ORANGE RGB(255, 140, 0)
#define WORLD3D_PLASMA_GREEN RGB(0, 255, 127)

// Sky and grass colors
#define WORLD3D_SKY_TOP RGB(25, 150, 255)
#define WORLD3D_SKY_BOTTOM RGB(135, 206, 250)
#define WORLD3D_GRASS_TOP RGB(60, 160, 60)
#define WORLD3D_GRASS_BOTTOM RGB(30, 120, 30)

void DemoWorld3D::Init(HWND hWnd) {
    hwnd = hWnd;
    BaseXConfig::setBase(32);
    enableWireframeGlow = true;
    enableParticleTrails = true;
    showDebugInfo = true;

    BuildEnhancedScene();
    InitializeParticleSystem();

    world = BaseXMatrix(); // identity
    UpdateView();
}

void DemoWorld3D::InitializeParticleSystem() {
    for (int i = 0; i < MAX_3D_PARTICLES; i++) {
        Create3DParticle(i);
    }
}

void DemoWorld3D::Create3DParticle(int index) {
    if (index >= MAX_3D_PARTICLES) return;
    
    world3DParticles[index].position = BaseXVector(
        -300 + rand() % 600,
        -200 + rand() % 400,
        -300 + rand() % 600
    );
    world3DParticles[index].velocity = BaseXVector(
        -20 + rand() % 40,
        -20 + rand() % 40,
        -20 + rand() % 40
    );
    world3DParticles[index].life = 5.0f + (rand() % 300) / 100.0f;
    world3DParticles[index].maxLife = world3DParticles[index].life;
    world3DParticles[index].size = 1 + rand() % 3;
    world3DParticles[index].active = true;
    
    // Vibrant 3D particle colors
    COLORREF colors[] = {
        WORLD3D_NEON_RED, WORLD3D_NEON_GREEN, WORLD3D_NEON_BLUE,
        WORLD3D_NEON_YELLOW, WORLD3D_NEON_CYAN, WORLD3D_NEON_MAGENTA,
        WORLD3D_ELECTRIC_PURPLE, WORLD3D_LASER_ORANGE, WORLD3D_PLASMA_GREEN
    };
    world3DParticles[index].color = colors[rand() % 9];
}

void DemoWorld3D::BuildEnhancedScene() {
    vertices.clear();
    edges.clear();

    auto addLine = [&](const BaseXVector& a, const BaseXVector& b, COLORREF c){
        int ia = (int)vertices.size();
        vertices.push_back(a);
        int ib = (int)vertices.size();
        vertices.push_back(b);
        edges.push_back({ia, ib, c});
    };

    // Enhanced Axes with brighter colors (X=red, Y=green, Z=blue)
    addLine(BaseXVector(-500, 0, 0), BaseXVector(500, 0, 0), WORLD3D_NEON_RED);     // X
    addLine(BaseXVector(0, -500, 0), BaseXVector(0, 500, 0), WORLD3D_NEON_GREEN);   // Y
    addLine(BaseXVector(0, 0, -500), BaseXVector(0, 0, 500), WORLD3D_NEON_BLUE);    // Z

    // Enhanced ground grid with green colors for grass feel
    for (int i = -500; i <= 500; i += 50) {
        COLORREF c = (i == 0) ? WORLD3D_NEON_CYAN : RGB(34, 139, 34); // center axis cyan, others forest green
        addLine(BaseXVector(i, 0, -500), BaseXVector(i, 0, 500), c);
        addLine(BaseXVector(-500, 0, i), BaseXVector(500, 0, i), c);
    }

    // Build an enhanced wireframe cube with vibrant colors
    double s = 100.0;
    std::vector<BaseXVector> cube = {
        BaseXVector(-s,-s,-s), BaseXVector(s,-s,-s), BaseXVector(s,s,-s), BaseXVector(-s,s,-s),
        BaseXVector(-s,-s, s), BaseXVector(s,-s, s), BaseXVector(s,s, s), BaseXVector(-s,s, s)
    };
    int base = (int)vertices.size();
    vertices.insert(vertices.end(), cube.begin(), cube.end());

    auto edge = [&](int a,int b,COLORREF c){ edges.push_back({base + a, base + b, c}); };
    
    // Enhanced cube with neon colors
    // back square
    edge(0,1, WORLD3D_NEON_MAGENTA); edge(1,2, WORLD3D_NEON_MAGENTA); 
    edge(2,3, WORLD3D_NEON_MAGENTA); edge(3,0, WORLD3D_NEON_MAGENTA);
    // front square
    edge(4,5, WORLD3D_NEON_YELLOW); edge(5,6, WORLD3D_NEON_YELLOW); 
    edge(6,7, WORLD3D_NEON_YELLOW); edge(7,4, WORLD3D_NEON_YELLOW);
    // connections
    edge(0,4, WORLD3D_NEON_CYAN); edge(1,5, WORLD3D_NEON_CYAN); 
    edge(2,6, WORLD3D_NEON_CYAN); edge(3,7, WORLD3D_NEON_CYAN);

    // Add enhanced pyramid with bright colors
    BaseXVector apex(0.0, -s-80.0, 200.0);
    int baseStart = (int)vertices.size();
    vertices.push_back(BaseXVector(-s,-s, s+200));   // 8
    vertices.push_back(BaseXVector(s,-s, s+200));    // 9
    vertices.push_back(BaseXVector(s,s, s+200));     // 10
    vertices.push_back(BaseXVector(-s,s, s+200));    // 11
    int apexIdx = (int)vertices.size();
    vertices.push_back(apex);                        // 12

    edges.push_back({baseStart+0, baseStart+1, WORLD3D_ELECTRIC_PURPLE});
    edges.push_back({baseStart+1, baseStart+2, WORLD3D_ELECTRIC_PURPLE});
    edges.push_back({baseStart+2, baseStart+3, WORLD3D_ELECTRIC_PURPLE});
    edges.push_back({baseStart+3, baseStart+0, WORLD3D_ELECTRIC_PURPLE});
    edges.push_back({baseStart+0, apexIdx, WORLD3D_LASER_ORANGE});
    edges.push_back({baseStart+1, apexIdx, WORLD3D_LASER_ORANGE});
    edges.push_back({baseStart+2, apexIdx, WORLD3D_LASER_ORANGE});
    edges.push_back({baseStart+3, apexIdx, WORLD3D_LASER_ORANGE});
    
    // Add additional geometric shapes for visual richness
    BuildAdditionalShapes();
}

void DemoWorld3D::BuildAdditionalShapes() {
    auto addLine = [&](const BaseXVector& a, const BaseXVector& b, COLORREF c){
        int ia = (int)vertices.size();
        vertices.push_back(a);
        int ib = (int)vertices.size();
        vertices.push_back(b);
        edges.push_back({ia, ib, c});
    };
    
    // Add a tetrahedron
    double t = 80.0;
    BaseXVector tetraCenter(200, -50, 0);
    std::vector<BaseXVector> tetra = {
        tetraCenter + BaseXVector(t, t, t),
        tetraCenter + BaseXVector(-t, -t, t),
        tetraCenter + BaseXVector(-t, t, -t),
        tetraCenter + BaseXVector(t, -t, -t)
    };
    
    int tetraBase = (int)vertices.size();
    vertices.insert(vertices.end(), tetra.begin(), tetra.end());
    
    // Connect tetrahedron edges with bright colors
    edges.push_back({tetraBase+0, tetraBase+1, WORLD3D_PLASMA_GREEN});
    edges.push_back({tetraBase+0, tetraBase+2, WORLD3D_PLASMA_GREEN});
    edges.push_back({tetraBase+0, tetraBase+3, WORLD3D_PLASMA_GREEN});
    edges.push_back({tetraBase+1, tetraBase+2, WORLD3D_NEON_RED});
    edges.push_back({tetraBase+1, tetraBase+3, WORLD3D_NEON_RED});
    edges.push_back({tetraBase+2, tetraBase+3, WORLD3D_NEON_RED});
    
    // Add an octahedron
    double o = 60.0;
    BaseXVector octaCenter(-200, -100, 100);
    std::vector<BaseXVector> octa = {
        octaCenter + BaseXVector(0, o, 0),    // top
        octaCenter + BaseXVector(0, -o, 0),   // bottom
        octaCenter + BaseXVector(o, 0, 0),    // right
        octaCenter + BaseXVector(-o, 0, 0),   // left
        octaCenter + BaseXVector(0, 0, o),    // front
        octaCenter + BaseXVector(0, 0, -o)    // back
    };
    
    int octaBase = (int)vertices.size();
    vertices.insert(vertices.end(), octa.begin(), octa.end());
    
    // Connect octahedron edges explicitly to avoid index overflow
    COLORREF octaColors[] = {WORLD3D_NEON_BLUE, WORLD3D_NEON_YELLOW, WORLD3D_NEON_MAGENTA};
    // Top connections (top index = octaBase+0)
    edges.push_back({octaBase+0, octaBase+2, octaColors[0]}); // top-right
    edges.push_back({octaBase+0, octaBase+3, octaColors[0]}); // top-left
    edges.push_back({octaBase+0, octaBase+4, octaColors[1]}); // top-front
    edges.push_back({octaBase+0, octaBase+5, octaColors[1]}); // top-back
    // Bottom connections (bottom index = octaBase+1)
    edges.push_back({octaBase+1, octaBase+2, octaColors[2]}); // bottom-right
    edges.push_back({octaBase+1, octaBase+3, octaColors[2]}); // bottom-left
    edges.push_back({octaBase+1, octaBase+4, octaColors[0]}); // bottom-front
    edges.push_back({octaBase+1, octaBase+5, octaColors[1]}); // bottom-back
    // Middle square
    edges.push_back({octaBase+2, octaBase+4, WORLD3D_ELECTRIC_PURPLE});
    edges.push_back({octaBase+4, octaBase+3, WORLD3D_ELECTRIC_PURPLE});
    edges.push_back({octaBase+3, octaBase+5, WORLD3D_ELECTRIC_PURPLE});
    edges.push_back({octaBase+5, octaBase+2, WORLD3D_ELECTRIC_PURPLE});
}

void DemoWorld3D::UpdateView() {
    // Simple yaw/pitch camera rotation matrix (view is rotation only)
    BaseXMatrix rotY = BaseXMatrix::createRotationY(camYaw);
    BaseXMatrix rotX = BaseXMatrix::createRotationX(camPitch);
    BaseXMatrix rot = rotY * rotX;
    view = rot;
}

void DemoWorld3D::Update(float deltaTime) {
    // delta in seconds
    double dt = deltaTime * 0.001;

    // Camera controls
    double moveSpeed = 200.0; // units per second
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) moveSpeed *= 2.0;

    // Rotate camera with arrow keys
    double rotSpeed = 1.0; // radians per second
    if (GetAsyncKeyState(VK_LEFT) & 0x8000)  camYaw   -= rotSpeed * dt;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) camYaw   += rotSpeed * dt;
    if (GetAsyncKeyState(VK_UP) & 0x8000)    camPitch -= rotSpeed * dt;
    if (GetAsyncKeyState(VK_DOWN) & 0x8000)  camPitch += rotSpeed * dt;
    // Clamp pitch to avoid flip
    if (camPitch > 1.5) camPitch = 1.5;
    if (camPitch < -1.5) camPitch = -1.5;

    // Move camera with WASD and Q/E (up/down)
    double forward = 0.0, right = 0.0, up = 0.0;
    if (GetAsyncKeyState('W') & 0x8000) forward += 1.0;
    if (GetAsyncKeyState('S') & 0x8000) forward -= 1.0;
    if (GetAsyncKeyState('D') & 0x8000) right   += 1.0;
    if (GetAsyncKeyState('A') & 0x8000) right   -= 1.0;
    if (GetAsyncKeyState('E') & 0x8000) up      += 1.0;
    if (GetAsyncKeyState('Q') & 0x8000) up      -= 1.0;

    if (forward != 0.0 || right != 0.0 || up != 0.0) {
        // Build direction vectors from yaw
        double cy = cos(camYaw), sy = sin(camYaw);
        BaseXVector fwd(cy, 0.0, sy);
        BaseXVector strafe(-sy, 0.0, cy);
        BaseXVector move = fwd * forward + strafe * right + BaseXVector(0.0, -1.0, 0.0) * up;
        move *= (moveSpeed * dt);
        camPos = camPos + move;
    } else {
        // Enhanced idle animation: more dynamic movement
        angle += dt * 0.8;
        camPos.setY(-50.0 + 80.0 * sin(angle) + 20.0 * cos(angle * 2.0));
    }
    
    // Update particle system
    if (enableParticleTrails) {
        Update3DParticles(dt);
    }

    UpdateView();
}

void DemoWorld3D::Update3DParticles(float deltaTime) {
    for (int i = 0; i < MAX_3D_PARTICLES; i++) {
        if (!world3DParticles[i].active) continue;
        
        // Update particle physics
        world3DParticles[i].position += world3DParticles[i].velocity * deltaTime;
        world3DParticles[i].life -= deltaTime;
        
        // Apply some gravitational attraction to center
        BaseXVector toCenter = BaseXVector(0, 0, 0) - world3DParticles[i].position;
        double dist = toCenter.magnitude();
        if (dist > 50.0) {
            BaseXVector force = toCenter.normalized() * (30.0 / dist);
            world3DParticles[i].velocity += force * deltaTime;
        }
        
        // Respawn particle if life expired
        if (world3DParticles[i].life <= 0) {
            Create3DParticle(i);
        }
    }
}

bool DemoWorld3D::ProjectToScreen(const BaseXVector& vCam, POINT& outPt, const RECT& rc) const {
    // perspective divide: simple pinhole camera
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (h <= 0 || w <= 0) return false;
    double aspect = w / (double)h;
    double f = 1.0 / tan(ToRad(fovDeg) * 0.5);

    double x = vCam.x();
    double y = vCam.y();
    double z = vCam.z();

    if (z > -nearZ || z < -farZ) return false; // behind near plane or beyond far plane

    double nx = (x * f / aspect) / -z;
    double ny = (y * f) / -z;

    outPt.x = rc.left + (int)((nx * 0.5 + 0.5) * w);
    outPt.y = rc.top  + (int)((-ny * 0.5 + 0.5) * h);
    return true;
}

void DemoWorld3D::Render(HDC hdc) {
    RECT rc; GetClientRect(hwnd, &rc);
    if (rc.top < 130) rc.top = 130; // align with UI like other demos

    // Enhanced background with gradient effect
    DrawEnhancedBackground(hdc, rc);

    // Draw enhanced title and controls
    DrawEnhancedUI(hdc, rc);
    
    // Render 3D particles first (behind wireframe)
    if (enableParticleTrails) {
        Render3DParticles(hdc, rc);
    }

    // Render enhanced edges with glow effects
    RenderEnhancedWireframe(hdc, rc);
}

void DemoWorld3D::DrawEnhancedBackground(HDC hdc, const RECT& rc) {
    // Draw sky as top gradient and grass as bottom gradient with a horizon
    int midY = rc.top + (rc.bottom - rc.top) / 2;

    // Sky gradient (top -> horizon)
    TRIVERTEX skyVerts[2];
    skyVerts[0].x = rc.left; skyVerts[0].y = rc.top;
    skyVerts[0].Red = GetRValue(WORLD3D_SKY_TOP) << 8;
    skyVerts[0].Green = GetGValue(WORLD3D_SKY_TOP) << 8;
    skyVerts[0].Blue = GetBValue(WORLD3D_SKY_TOP) << 8;
    skyVerts[0].Alpha = 0x0000;

    skyVerts[1].x = rc.right; skyVerts[1].y = midY;
    skyVerts[1].Red = GetRValue(WORLD3D_SKY_BOTTOM) << 8;
    skyVerts[1].Green = GetGValue(WORLD3D_SKY_BOTTOM) << 8;
    skyVerts[1].Blue = GetBValue(WORLD3D_SKY_BOTTOM) << 8;
    skyVerts[1].Alpha = 0x0000;

    GRADIENT_RECT skyRect = {0, 1};
    GradientFill(hdc, skyVerts, 2, &skyRect, 1, GRADIENT_FILL_RECT_V);

    // Grass gradient (horizon -> bottom)
    TRIVERTEX grassVerts[2];
    grassVerts[0].x = rc.left; grassVerts[0].y = midY;
    grassVerts[0].Red = GetRValue(WORLD3D_GRASS_TOP) << 8;
    grassVerts[0].Green = GetGValue(WORLD3D_GRASS_TOP) << 8;
    grassVerts[0].Blue = GetBValue(WORLD3D_GRASS_TOP) << 8;
    grassVerts[0].Alpha = 0x0000;

    grassVerts[1].x = rc.right; grassVerts[1].y = rc.bottom;
    grassVerts[1].Red = GetRValue(WORLD3D_GRASS_BOTTOM) << 8;
    grassVerts[1].Green = GetGValue(WORLD3D_GRASS_BOTTOM) << 8;
    grassVerts[1].Blue = GetBValue(WORLD3D_GRASS_BOTTOM) << 8;
    grassVerts[1].Alpha = 0x0000;

    GRADIENT_RECT grassRect = {0, 1};
    GradientFill(hdc, grassVerts, 2, &grassRect, 1, GRADIENT_FILL_RECT_V);

    // Draw subtle horizon line
    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(210, 240, 255));
    HPEN old = (HPEN)SelectObject(hdc, hPen);
    MoveToEx(hdc, rc.left, midY, nullptr);
    LineTo(hdc, rc.right, midY);
    SelectObject(hdc, old);
    DeleteObject(hPen);
}

void DemoWorld3D::DrawEnhancedUI(HDC hdc, const RECT& rc) {
    SetBkMode(hdc, TRANSPARENT);
    
    // Enhanced title with glow effect
    SetTextColor(hdc, WORLD3D_NEON_CYAN);
    const wchar_t* title = L"Enhanced 3D World (Wireframe + Particles)";
    TextOut(hdc, rc.left+10, rc.top+10, title, lstrlenW(title));
    
    // Enhanced controls info
    SetTextColor(hdc, WORLD3D_NEON_YELLOW);
    const wchar_t* controls = L"Arrows: Look  |  WASD: Move  |  Q/E: Down/Up  |  Shift: Fast  |  G: Toggle Glow";
    TextOut(hdc, rc.left+10, rc.top+28, controls, lstrlenW(controls));
    
    // Enhanced status info
    if (showDebugInfo) {
        SetTextColor(hdc, WORLD3D_PLASMA_GREEN);
        wchar_t statusText[256];
        swprintf_s(statusText, L"Pos: (%.1f, %.1f, %.1f) | Yaw: %.1f° | Pitch: %.1f° | Particles: %d",
                   camPos.x(), camPos.y(), camPos.z(), 
                   camYaw * 180.0 / M_PI, camPitch * 180.0 / M_PI,
                   MAX_3D_PARTICLES);
        TextOut(hdc, rc.left+10, rc.top+46, statusText, lstrlenW(statusText));
    }
}

void DemoWorld3D::Render3DParticles(HDC hdc, const RECT& rc) {
    for (int i = 0; i < MAX_3D_PARTICLES; i++) {
        if (!world3DParticles[i].active) continue;
        
        // Transform to camera space
        BaseXVector vCam = view.transform(world3DParticles[i].position - camPos);
        POINT pt;
        if (!ProjectToScreen(vCam, pt, rc)) continue;
        
        float alpha = world3DParticles[i].life / world3DParticles[i].maxLife;
        if (alpha <= 0) continue;
        
        // Enhanced particle rendering with depth-based sizing
        double depth = -vCam.z();
        int size = (int)(world3DParticles[i].size * (1000.0 / (depth + 100.0)) * alpha);
        size = max(1, min(8, size));
        
        // Draw glow effect
        for (int glow = 2; glow >= 0; glow--) {
            COLORREF glowColor = world3DParticles[i].color;
            int r = (int)(GetRValue(glowColor) * alpha / (glow + 1));
            int g = (int)(GetGValue(glowColor) * alpha / (glow + 1));
            int b = (int)(GetBValue(glowColor) * alpha / (glow + 1));
            
            HBRUSH glowBrush = CreateSolidBrush(RGB(r, g, b));
            
            int glowSize = size + glow * 2;
            RECT glowRect = { pt.x - glowSize, pt.y - glowSize, pt.x + glowSize, pt.y + glowSize };
            FillRect(hdc, &glowRect, glowBrush);
            
            DeleteObject(glowBrush);
        }
    }
}

void DemoWorld3D::RenderEnhancedWireframe(HDC hdc, const RECT& rc) {
    HPEN oldPen = nullptr;
    
    for (const auto& e : edges) {
        // Transform to camera space: rotate then translate by -camPos
        BaseXVector vAWorld = vertices[e.a];
        BaseXVector vBWorld = vertices[e.b];
        BaseXVector vACam = view.transform(vAWorld - camPos);
        BaseXVector vBCam = view.transform(vBWorld - camPos);
        POINT a, b;
        if (!ProjectToScreen(vACam, a, rc)) continue;
        if (!ProjectToScreen(vBCam, b, rc)) continue;
        
        // Enhanced wireframe with glow effects
        if (enableWireframeGlow) {
            // Draw glow effect
            for (int glow = 2; glow >= 0; glow--) {
                COLORREF glowColor = e.color;
                int r = GetRValue(glowColor) / (glow + 1);
                int g = GetGValue(glowColor) / (glow + 1);
                int b2 = GetBValue(glowColor) / (glow + 1);
                
                HPEN pen = CreatePen(PS_SOLID, 1 + glow * 2, RGB(r, g, b2));
                HPEN prev = (HPEN)SelectObject(hdc, pen);
                if (!oldPen) oldPen = prev;
                
                MoveToEx(hdc, a.x, a.y, nullptr);
                LineTo(hdc, b.x, b.y);
                
                SelectObject(hdc, prev);
                DeleteObject(pen);
            }
        } else {
            // Standard wireframe
            HPEN pen = CreatePen(PS_SOLID, 2, e.color);
            HPEN prev = (HPEN)SelectObject(hdc, pen);
            if (!oldPen) oldPen = prev;
            
            MoveToEx(hdc, a.x, a.y, nullptr);
            LineTo(hdc, b.x, b.y);
            
            SelectObject(hdc, prev);
            DeleteObject(pen);
        }
    }
    
    if (oldPen) SelectObject(hdc, oldPen);
}

void DemoWorld3D::Shutdown() {
    vertices.clear();
    edges.clear();
}
