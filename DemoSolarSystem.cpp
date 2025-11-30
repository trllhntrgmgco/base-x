#include <windows.h>
#include <mmsystem.h>
#include <cmath>
#include <algorithm>
#include "DemoSolarSystem.h"
#include "physics.h"  // Add include for BaseX classes

#pragma comment(lib, "winmm.lib")

using namespace BaseXEngine;  // Add using statement for BaseX classes

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline double ToRad(double deg) { return deg * M_PI / 180.0; }
static inline double ToDeg(double rad) { return rad * 180.0 / M_PI; }

// Helper functions to replace std::min and std::max
template<typename T>
inline T MyMin(T a, T b) { return (a < b) ? a : b; }

template<typename T>
inline T MyMax(T a, T b) { return (a > b) ? a : b; }

// Enhanced Solar System Colors
#define SOLAR_NEON_YELLOW RGB(255, 255, 100)    // Enhanced Sun
#define SOLAR_BRIGHT_GRAY RGB(200, 200, 200)    // Enhanced Mercury
#define SOLAR_BRIGHT_ORANGE RGB(255, 180, 50)   // Enhanced Venus
#define SOLAR_BRIGHT_BLUE RGB(100, 180, 255)    // Enhanced Earth
#define SOLAR_BRIGHT_RED RGB(255, 100, 100)     // Enhanced Mars
#define SOLAR_BRIGHT_JUPITER RGB(255, 200, 120) // Enhanced Jupiter
#define SOLAR_BRIGHT_SATURN RGB(255, 240, 150)  // Enhanced Saturn
#define SOLAR_BRIGHT_URANUS RGB(150, 230, 255)  // Enhanced Uranus
#define SOLAR_BRIGHT_NEPTUNE RGB(120, 160, 255) // Enhanced Neptune

void DemoSolarSystem::Init(HWND hWnd) {
    hwnd = hWnd;
    BaseXConfig::setBase(16); // Use base 16 for solar system calculations
    
    enableTrails = true;
    enableEnhancedGlow = true;
    trailCount = 0;
    trailIndex = 0;
    
    BuildEnhancedSolarSystem();
    InitializeStarField();
    UpdateCamera();
}

void DemoSolarSystem::InitializeStarField() {
    for (int i = 0; i < MAX_STARS; i++) {
        CreateStar(i);
    }
}

void DemoSolarSystem::CreateStar(int index) {
    if (index >= MAX_STARS) return;
    
    starField[index].position = BaseXVector(
        -2000 + rand() % 4000,
        -2000 + rand() % 4000,
        -2000 + rand() % 4000
    );
    starField[index].brightness = 0.3f + (rand() % 70) / 100.0f;
    starField[index].size = 1 + rand() % 2;
    
    // Varied star colors
    COLORREF starColors[] = {
        RGB(255, 255, 255), RGB(255, 240, 200), RGB(200, 220, 255),
        RGB(255, 200, 200), RGB(200, 255, 200)
    };
    starField[index].color = starColors[rand() % 5];
}

void DemoSolarSystem::BuildEnhancedSolarSystem() {
    bodies.clear();
    
    // Build our solar system with enhanced colors and realistic orbital data
    
    // Enhanced Sun with bright glow
    bodies.emplace_back("Sol", 0.0, 0.0, 696000 * SCALE_FACTOR, 1.989e30, SOLAR_NEON_YELLOW, true);
    bodies.back().rotationSpeed = (2.0 * M_PI) / (25.4 * 24.0 * 3600.0); // 25.4 day rotation
    bodies.back().glowIntensity = 3.0f; // Bright glow for sun
    
    // Enhanced Inner planets
    bodies.emplace_back("Mercury", 0.387 * AU * SCALE_FACTOR, 0.241, 2440 * SCALE_FACTOR, 3.301e23, SOLAR_BRIGHT_GRAY);
    bodies.back().glowIntensity = 1.2f;
    
    bodies.emplace_back("Venus", 0.723 * AU * SCALE_FACTOR, 0.615, 6052 * SCALE_FACTOR, 4.867e24, SOLAR_BRIGHT_ORANGE);
    bodies.back().glowIntensity = 1.5f;
    
    bodies.emplace_back("Earth", 1.0 * AU * SCALE_FACTOR, 1.0, 6371 * SCALE_FACTOR, 5.972e24, SOLAR_BRIGHT_BLUE);
    bodies.back().glowIntensity = 1.8f;
    
    bodies.emplace_back("Mars", 1.524 * AU * SCALE_FACTOR, 1.881, 3390 * SCALE_FACTOR, 6.417e23, SOLAR_BRIGHT_RED);
    bodies.back().glowIntensity = 1.3f;
    
    // Enhanced Outer planets
    bodies.emplace_back("Jupiter", 5.203 * AU * SCALE_FACTOR, 11.86, 69911 * SCALE_FACTOR * 0.3, 1.898e27, SOLAR_BRIGHT_JUPITER);
    bodies.back().glowIntensity = 2.0f;
    
    bodies.emplace_back("Saturn", 9.537 * AU * SCALE_FACTOR, 29.46, 58232 * SCALE_FACTOR * 0.3, 5.683e26, SOLAR_BRIGHT_SATURN);
    bodies.back().hasRings = true;
    bodies.back().glowIntensity = 1.8f;
    
    bodies.emplace_back("Uranus", 19.19 * AU * SCALE_FACTOR, 84.01, 25362 * SCALE_FACTOR * 0.5, 8.681e25, SOLAR_BRIGHT_URANUS);
    bodies.back().glowIntensity = 1.4f;
    
    bodies.emplace_back("Neptune", 30.07 * AU * SCALE_FACTOR, 164.8, 24622 * SCALE_FACTOR * 0.5, 1.024e26, SOLAR_BRIGHT_NEPTUNE);
    bodies.back().glowIntensity = 1.5f;
    
    // Set initial positions and velocities
    for (auto& body : bodies) {
        if (!body.isSun) {
            // Start at random position in orbit
            body.currentAngle = (rand() % 360) * M_PI / 180.0;
            body.position.setX(body.orbitRadius * cos(body.currentAngle));
            body.position.setZ(body.orbitRadius * sin(body.currentAngle));
            body.position.setY(0);
        } else {
            body.position = BaseXVector(0, 0, 0);
        }
    }
}

void DemoSolarSystem::UpdateOrbits(float deltaTime) {
    if (isPaused) return;
    
    double dt = deltaTime * 0.001 * timeAcceleration; // Convert to seconds and apply time acceleration
    
    for (auto& body : bodies) {
        if (!body.isSun) {
            // Update orbital position
            body.currentAngle += body.orbitSpeed * dt * TIME_SCALE;
            if (body.currentAngle > 2.0 * M_PI) {
                body.currentAngle -= 2.0 * M_PI;
            }
            
            // Calculate new position
            body.position.setX(body.orbitRadius * cos(body.currentAngle));
            body.position.setZ(body.orbitRadius * sin(body.currentAngle));
            
            // Add orbital trails
            if (enableTrails) {
                AddOrbitTrail(body);
            }
        }
        
        // Update rotation
        body.currentRotation += body.rotationSpeed * dt * TIME_SCALE;
        if (body.currentRotation > 2.0 * M_PI) {
            body.currentRotation -= 2.0 * M_PI;
        }
    }
}

void DemoSolarSystem::AddOrbitTrail(const CelestialBody& body) {
    if (trailCount >= MAX_TRAILS) {
        // Circular buffer - overwrite oldest trail
        trailIndex = (trailIndex + 1) % MAX_TRAILS;
    } else {
        trailCount++;
    }
    
    int index = (trailCount < MAX_TRAILS) ? trailCount - 1 : trailIndex;
    orbitTrails[index].position = body.position;
    orbitTrails[index].color = body.color;
    orbitTrails[index].life = 5000.0f; // 5 seconds
    orbitTrails[index].maxLife = orbitTrails[index].life;
    orbitTrails[index].size = 1;
}

void DemoSolarSystem::UpdateTrails(float deltaTime) {
    for (int i = 0; i < trailCount && i < MAX_TRAILS; i++) {
        orbitTrails[i].life -= deltaTime;
        if (orbitTrails[i].life <= 0) {
            // Remove expired trail by shifting array
            for (int j = i; j < trailCount - 1; j++) {
                orbitTrails[j] = orbitTrails[j + 1];
            }
            trailCount--;
            i--; // Adjust index
        }
    }
}

void DemoSolarSystem::UpdateCamera() {
    // Camera follows selected body if enabled
    if (followBody && selectedBody < bodies.size()) {
        BaseXVector targetPos = GetScaledPosition(bodies[selectedBody]);
        camPos = targetPos + BaseXVector(0, 100, -camDistance);
    }
    
    // Build view matrix
    BaseXMatrix rotY = BaseXMatrix::createRotationY(camYaw);
    BaseXMatrix rotX = BaseXMatrix::createRotationX(camPitch);
    view = rotY * rotX;
}

void DemoSolarSystem::Update(float deltaTime) {
    // Handle camera controls
    double moveSpeed = 50.0;
    double rotSpeed = 1.0;
    double dt = deltaTime * 0.001;
    
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) moveSpeed *= 3.0;
    
    // Camera rotation
    if (GetAsyncKeyState(VK_LEFT) & 0x8000) camYaw -= rotSpeed * dt;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) camYaw += rotSpeed * dt;
    if (GetAsyncKeyState(VK_UP) & 0x8000) camPitch -= rotSpeed * dt;
    if (GetAsyncKeyState(VK_DOWN) & 0x8000) camPitch += rotSpeed * dt;
    
    // Clamp pitch
    if (camPitch > 1.5) camPitch = 1.5;
    if (camPitch < -1.5) camPitch = -1.5;
    
    // Camera movement
    if (!followBody) {
        double forward = 0.0, right = 0.0, up = 0.0;
        if (GetAsyncKeyState('W') & 0x8000) forward += 1.0;
        if (GetAsyncKeyState('S') & 0x8000) forward -= 1.0;
        if (GetAsyncKeyState('D') & 0x8000) right += 1.0;
        if (GetAsyncKeyState('A') & 0x8000) right -= 1.0;
        if (GetAsyncKeyState('E') & 0x8000) up += 1.0;
        if (GetAsyncKeyState('Q') & 0x8000) up -= 1.0;
        
        if (forward != 0.0 || right != 0.0 || up != 0.0) {
            double cy = cos(camYaw), sy = sin(camYaw);
            BaseXVector fwd(cy, 0.0, sy);
            BaseXVector strafe(-sy, 0.0, cy);
            BaseXVector move = fwd * forward + strafe * right + BaseXVector(0.0, -1.0, 0.0) * up;
            move *= (moveSpeed * dt);
            camPos = camPos + move;
        }
    }
    
    // Zoom control
    if (GetAsyncKeyState('Z') & 0x8000) camDistance *= (1.0 - dt);
    if (GetAsyncKeyState('X') & 0x8000) camDistance *= (1.0 + dt);
    if (camDistance < 50.0) camDistance = 50.0;
    if (camDistance > 2000.0) camDistance = 2000.0;
    
    UpdateOrbits(deltaTime);
    if (enableTrails) {
        UpdateTrails(deltaTime);
    }
    UpdateCamera();
}

bool DemoSolarSystem::ProjectToScreen(const BaseXVector& worldPos, POINT& outPt, const RECT& rect) const {
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    if (h <= 0 || w <= 0) return false;
    
    double aspect = w / (double)h;
    double f = 1.0 / tan(ToRad(fovDeg) * 0.5);
    
    // Transform to camera space
    BaseXVector camSpace = view.transform(worldPos - camPos);
    
    double x = camSpace.x();
    double y = camSpace.y();
    double z = camSpace.z();
    
    if (z > -nearZ || z < -farZ) return false;
    
    double nx = (x * f / aspect) / -z;
    double ny = (y * f) / -z;
    
    outPt.x = rect.left + (int)((nx * 0.5 + 0.5) * w);
    outPt.y = rect.top + (int)((-ny * 0.5 + 0.5) * h);
    return true;
}

BaseXVector DemoSolarSystem::GetScaledPosition(const CelestialBody& body) const {
    return body.position;
}

double DemoSolarSystem::GetScaledRadius(const CelestialBody& body) const {
    double baseRadius = body.radius;
    if (body.isSun) {
        baseRadius *= 0.1; // Scale down sun for better visibility
    } else {
        baseRadius = MyMax(baseRadius, 3.0); // Minimum size for planets
    }
    return baseRadius;
}

void DemoSolarSystem::RenderEnhancedBody(HDC hdc, const CelestialBody& body, const RECT& rect) {
    BaseXVector pos = GetScaledPosition(body);
    POINT screenPos;
    
    if (!ProjectToScreen(pos, screenPos, rect)) return;
    
    double radius = GetScaledRadius(body);
    int screenRadius = MyMax(3, (int)(radius * 0.8));
    
    // Enhanced glow effect for all bodies
    if (enableEnhancedGlow) {
        for (int glow = 3; glow >= 0; glow--) {
            COLORREF glowColor = body.color;
            int r = (int)(GetRValue(glowColor) * body.glowIntensity / (glow + 1));
            int g = (int)(GetGValue(glowColor) * body.glowIntensity / (glow + 1));
            int b = (int)(GetBValue(glowColor) * body.glowIntensity / (glow + 1));
            
            // Clamp values
            r = MyMin(255, MyMax(0, r));
            g = MyMin(255, MyMax(0, g));
            b = MyMin(255, MyMax(0, b));
            
            HBRUSH glowBrush = CreateSolidBrush(RGB(r, g, b));
            HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, glowBrush);
            
            int glowRadius = screenRadius + glow * 4;
            Ellipse(hdc, screenPos.x - glowRadius, screenPos.y - glowRadius,
                    screenPos.x + glowRadius, screenPos.y + glowRadius);
            
            SelectObject(hdc, oldBrush);
            DeleteObject(glowBrush);
        }
    }
    
    // Create main body brush
    HBRUSH bodyBrush = CreateSolidBrush(body.color);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, bodyBrush);
    
    // Draw main body as circle
    Ellipse(hdc, screenPos.x - screenRadius, screenPos.y - screenRadius,
            screenPos.x + screenRadius, screenPos.y + screenRadius);
    
    // Draw enhanced rings for Saturn
    if (body.hasRings) {
        HPEN ringPen = CreatePen(PS_SOLID, 2, RGB(255, 220, 120));
        HPEN oldPen = (HPEN)SelectObject(hdc, ringPen);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        
        int ringRadius1 = screenRadius + 4;
        int ringRadius2 = screenRadius + 8;
        int ringRadius3 = screenRadius + 12;
        
        Ellipse(hdc, screenPos.x - ringRadius1, screenPos.y - ringRadius1,
                screenPos.x + ringRadius1, screenPos.y + ringRadius1);
        Ellipse(hdc, screenPos.x - ringRadius2, screenPos.y - ringRadius2,
                screenPos.x + ringRadius2, screenPos.y + ringRadius2);
        Ellipse(hdc, screenPos.x - ringRadius3, screenPos.y - ringRadius3,
                screenPos.x + ringRadius3, screenPos.y + ringRadius3);
        
        SelectObject(hdc, oldPen);
        DeleteObject(ringPen);
    }
    
    SelectObject(hdc, oldBrush);
    DeleteObject(bodyBrush);
    
    // Draw label if enabled
    if (showLabels) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        std::wstring wname(body.name.begin(), body.name.end());
        TextOutW(hdc, screenPos.x + screenRadius + 8, screenPos.y - 8, 
                wname.c_str(), (int)wname.length());
    }
}

void DemoSolarSystem::RenderStarField(HDC hdc, const RECT& rect) {
    for (int i = 0; i < MAX_STARS; i++) {
        const Star& star = starField[i];
        POINT starPt;
        
        if (!ProjectToScreen(star.position, starPt, rect)) continue;
        
        // Draw star with brightness variation
        COLORREF starColor = star.color;
        int r = (int)(GetRValue(starColor) * star.brightness);
        int g = (int)(GetGValue(starColor) * star.brightness);
        int b = (int)(GetBValue(starColor) * star.brightness);
        
        HBRUSH starBrush = CreateSolidBrush(RGB(r, g, b));
        
        RECT starRect = { 
            starPt.x - star.size, starPt.y - star.size, 
            starPt.x + star.size, starPt.y + star.size 
        };
        FillRect(hdc, &starRect, starBrush);
        
        DeleteObject(starBrush);
    }
}

void DemoSolarSystem::RenderTrails(HDC hdc, const RECT& rect) {
    if (!enableTrails) return;
    
    for (int i = 0; i < trailCount && i < MAX_TRAILS; i++) {
        const OrbitTrail& trail = orbitTrails[i];
        if (trail.life <= 0) continue;
        
        POINT trailPt;
        if (!ProjectToScreen(trail.position, trailPt, rect)) continue;
        
        float alpha = trail.life / trail.maxLife;
        COLORREF trailColor = trail.color;
        int r = (int)(GetRValue(trailColor) * alpha * 0.6);
        int g = (int)(GetGValue(trailColor) * alpha * 0.6);
        int b = (int)(GetBValue(trailColor) * alpha * 0.6);
        
        HBRUSH trailBrush = CreateSolidBrush(RGB(r, g, b));
        
        int size = (int)(trail.size * alpha);
        RECT trailRect = { 
            trailPt.x - size, trailPt.y - size, 
            trailPt.x + size, trailPt.y + size 
        };
        FillRect(hdc, &trailRect, trailBrush);
        
        DeleteObject(trailBrush);
    }
}

void DemoSolarSystem::RenderOrbit(HDC hdc, const CelestialBody& body, const RECT& rect) {
    if (body.isSun || !showOrbits) return;
    
    HPEN orbitPen = CreatePen(PS_DOT, 1, RGB(100, 120, 150));
    HPEN oldPen = (HPEN)SelectObject(hdc, orbitPen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    
    // Draw orbit as circle centered at origin
    BaseXVector center(0, 0, 0);
    POINT centerPt;
    if (ProjectToScreen(center, centerPt, rect)) {
        // Calculate orbit radius in screen space
        BaseXVector orbitPoint(body.orbitRadius, 0, 0);
        POINT orbitPt;
        if (ProjectToScreen(orbitPoint, orbitPt, rect)) {
            int orbitRadius = abs(orbitPt.x - centerPt.x);
            if (orbitRadius > 5 && orbitRadius < 2000) {
                Ellipse(hdc, centerPt.x - orbitRadius, centerPt.y - orbitRadius,
                        centerPt.x + orbitRadius, centerPt.y + orbitRadius);
            }
        }
    }
    
    SelectObject(hdc, oldPen);
    DeleteObject(orbitPen);
}

void DemoSolarSystem::RenderEnhancedUI(HDC hdc, const RECT& rect) {
    SetBkMode(hdc, TRANSPARENT);
    
    // Enhanced title with glow
    SetTextColor(hdc, RGB(255, 255, 100));
    const wchar_t* title = L"Enhanced Solar System Simulation";
    TextOut(hdc, rect.left + 10, rect.top + 10, title, lstrlenW(title));
    
    // Enhanced controls
    SetTextColor(hdc, RGB(150, 200, 255));
    const wchar_t* controls = L"Arrows: Look | WASD: Move | Z/X: Zoom | Space: Pause | 1-9: Follow | O: Orbits | L: Labels | T: Trails";
    TextOut(hdc, rect.left + 10, rect.top + 28, controls, lstrlenW(controls));
    
    // Enhanced status
    wchar_t status[256];
    const wchar_t* pauseText = isPaused ? L"PAUSED" : L"RUNNING";
    const wchar_t* followText = followBody && selectedBody < bodies.size() ? 
        std::wstring(bodies[selectedBody].name.begin(), bodies[selectedBody].name.end()).c_str() : L"FREE";
    
    swprintf_s(status, L"Status: %s | Time: %.1fx | Following: %s | Trails: %s", 
               pauseText, timeAcceleration, followText, enableTrails ? L"ON" : L"OFF");
    SetTextColor(hdc, RGB(100, 255, 150));
    TextOut(hdc, rect.left + 10, rect.top + 46, status, lstrlenW(status));
}

void DemoSolarSystem::RenderStats(HDC hdc, const RECT& rect) {
    if (!showStats) return;
    
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(200, 220, 255));
    
    int y = rect.top + 80;
    for (size_t i = 0; i < bodies.size() && i < 5; ++i) {
        const auto& body = bodies[i];
        wchar_t info[256];
        if (body.isSun) {
            swprintf_s(info, L"%S: Radius %.0f km | Glow: %.1f", 
                      body.name.c_str(), body.radius / SCALE_FACTOR, body.glowIntensity);
        } else {
            double orbitalPeriod = 2.0 * M_PI / body.orbitSpeed / (365.25 * 24.0 * 3600.0);
            swprintf_s(info, L"%S: %.2f AU, %.1f years | Glow: %.1f", 
                      body.name.c_str(), body.orbitRadius / (AU * SCALE_FACTOR), 
                      orbitalPeriod, body.glowIntensity);
        }
        TextOut(hdc, rect.right - 350, y, info, lstrlenW(info));
        y += 16;
    }
}

void DemoSolarSystem::Render(HDC hdc) {
    RECT rect;
    GetClientRect(hwnd, &rect);
    if (rect.top < 130) rect.top = 130;
    
    // Enhanced background
    HBRUSH bg = CreateSolidBrush(RGB(5, 5, 15));
    FillRect(hdc, &rect, bg);
    DeleteObject(bg);
    
    // Render star field first
    RenderStarField(hdc, rect);
    
    // Render orbit trails
    if (enableTrails) {
        RenderTrails(hdc, rect);
    }
    
    // Render orbits
    for (const auto& body : bodies) {
        RenderOrbit(hdc, body, rect);
    }
    
    // Render enhanced bodies
    for (const auto& body : bodies) {
        RenderEnhancedBody(hdc, body, rect);
    }
    
    // Render enhanced UI
    RenderEnhancedUI(hdc, rect);
    RenderStats(hdc, rect);
}

// Enhanced input handling methods remain the same with additional trail toggle
void DemoSolarSystem::HandleKeyDown(int keyCode) {
    switch (keyCode) {
        case VK_SPACE:
            isPaused = !isPaused;
            break;
        case 'O':
            showOrbits = !showOrbits;
            break;
        case 'L':
            showLabels = !showLabels;
            break;
        case 'T':
            if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                showStats = !showStats;
            } else {
                enableTrails = !enableTrails;
            }
            break;
        case 'G':
            enableEnhancedGlow = !enableEnhancedGlow;
            break;
        case '1': case '2': case '3': case '4': case '5':
        case '6': case '7': case '8': case '9':
            selectedBody = keyCode - '1' + 1; // Skip sun (index 0)
            if (selectedBody < bodies.size()) {
                followBody = true;
            }
            break;
        case '0':
            followBody = false;
            selectedBody = 0;
            break;
        case VK_ADD: case VK_OEM_PLUS:
            timeAcceleration = MyMin(timeAcceleration * 2.0, 1000.0);
            break;
        case VK_SUBTRACT: case VK_OEM_MINUS:
            timeAcceleration = MyMax(timeAcceleration * 0.5, 0.1);
            break;
    }
}

void DemoSolarSystem::HandleKeyUp(int keyCode) {
    // Not needed for this demo
}

void DemoSolarSystem::HandleMouseDown(int x, int y, bool leftButton) {
    // Could implement clicking on bodies to select them
}

void DemoSolarSystem::HandleMouseMove(int x, int y) {
    // Could implement mouse look
}

void DemoSolarSystem::HandleMouseUp(int x, int y, bool leftButton) {
    // Not needed for this demo
}

void DemoSolarSystem::Shutdown() {
    bodies.clear();
}