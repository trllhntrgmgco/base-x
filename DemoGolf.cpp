#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <windows.h>
#include <cmath>
#include <algorithm>
#include <random>
#include "DemoGolf.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline double ToRad(double d){ return d * M_PI / 180.0; }
static inline double clampd(double v, double lo, double hi){ return (v < lo) ? lo : (v > hi) ? hi : v; }

// Enhanced Color Palette for Golf Demo
#define GOLF_SKY_DAY RGB(135, 206, 250)
#define GOLF_SKY_DAWN RGB(255, 165, 100)
#define GOLF_SKY_DUSK RGB(255, 100, 150)
#define GOLF_GRASS_FAIRWAY RGB(34, 139, 34)
#define GOLF_GRASS_ROUGH RGB(107, 142, 35)
#define GOLF_WATER RGB(30, 144, 255)
#define GOLF_SAND RGB(238, 203, 173)
#define GOLF_BALL_WHITE RGB(255, 255, 255)
#define GOLF_BALL_GLOW RGB(255, 255, 200)
#define GOLF_HOLE_BLACK RGB(20, 20, 20)
#define GOLF_TEE_BROWN RGB(139, 69, 19)
#define GOLF_TRAIL RGB(255, 215, 0)

void DemoGolf::Init(HWND hWnd) {
    hwnd = hWnd;
    BaseXConfig::setBase(32);
    
    // Initialize course
    InitializeCourse();
    
    // Reset game state
    strokes = 0;
    totalScore = 0;
    currentHole = 0;
    inHole = false;
    ballInMotion = false;
    ballInWater = false;
    ballInSand = false;
    
    // Initialize ball at first tee
    if (!course.empty()) {
        ballPos = course[0].teePos;
        ballPos.setY(ballRadius);
    } else {
        ballPos = BaseXVector(0.0, ballRadius, -200.0);
    }
    ballVel = BaseXVector(0.0, 0.0, 0.0);
    
    // Initialize camera
    camPos = BaseXVector(0.0, 120.0, 300.0);
    camYaw = 0.0; 
    camPitch = -0.35;
    cameraFollowsBall = true;
    
    // Initialize environment
    timeOfDay = 14.0; // 2 PM
    grassWavePhase = 0.0;
    
    // Initialize clouds
    cloudPositions.clear();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> distX(-1000, 1000);
    std::uniform_real_distribution<double> distZ(-2000, 1000);
    std::uniform_real_distribution<double> distY(200, 400);
    
    for (int i = 0; i < 8; ++i) {
        cloudPositions.push_back(BaseXVector(distX(gen), distY(gen), distZ(gen)));
    }
    
    UpdateView();
}

void DemoGolf::Shutdown() {
    ballTrails.clear();
    course.clear();
    holeScores.clear();
    cloudPositions.clear();
    treePositions.clear();
    hazardPositions.clear();
}

void DemoGolf::InitializeCourse() {
    course.clear();
    holeScores.clear();
    totalPar = 0;
    
    for (int i = 0; i < totalHoles; ++i) {
        GolfHole hole = CreateHole(i + 1);
        course.push_back(hole);
        holeScores.push_back(0);
        totalPar += hole.par;
    }
}

GolfHole DemoGolf::CreateHole(int holeNumber) {
    GolfHole hole;
    
    // Varied hole designs based on hole number
    switch (holeNumber) {
        case 1: // Easy starter hole
            hole.teePos = BaseXVector(0.0, 0.0, -200.0);
            hole.holePos = BaseXVector(0.0, 0.0, -600.0);
            hole.par = 3;
            hole.fairwayWidth = 120.0;
            hole.description = L"Hole 1: Straight fairway, perfect for warming up";
            break;
        case 2: // Slight dogleg left
            hole.teePos = BaseXVector(0.0, 0.0, -800.0);
            hole.holePos = BaseXVector(-150.0, 0.0, -1200.0);
            hole.par = 4;
            hole.fairwayWidth = 100.0;
            hole.description = L"Hole 2: Dogleg left, watch for the sand trap";
            // Add sand trap
            hole.hazards.push_back(BaseXVector(-80.0, 0.0, -1000.0));
            break;
        case 3: // Water hazard hole
            hole.teePos = BaseXVector(-200.0, 0.0, -1400.0);
            hole.holePos = BaseXVector(100.0, 0.0, -1700.0);
            hole.par = 3;
            hole.fairwayWidth = 80.0;
            hole.description = L"Hole 3: Island green - precision required!";
            // Add water hazards
            hole.hazards.push_back(BaseXVector(0.0, 0.0, -1650.0));
            hole.hazards.push_back(BaseXVector(50.0, 0.0, -1650.0));
            break;
        case 4: // Long par 5
            hole.teePos = BaseXVector(150.0, 0.0, -1900.0);
            hole.holePos = BaseXVector(-100.0, 0.0, -2400.0);
            hole.par = 5;
            hole.fairwayWidth = 110.0;
            hole.description = L"Hole 4: Long par 5 with elevated green";
            break;
        case 5: // Narrow fairway
            hole.teePos = BaseXVector(-150.0, 0.0, -2600.0);
            hole.holePos = BaseXVector(0.0, 0.0, -2900.0);
            hole.par = 4;
            hole.fairwayWidth = 60.0;
            hole.description = L"Hole 5: Narrow fairway, accuracy over power";
            // Add trees
            hole.trees.push_back(BaseXVector(-40.0, 0.0, -2750.0));
            hole.trees.push_back(BaseXVector(40.0, 0.0, -2750.0));
            break;
        case 6: // Uphill hole
            hole.teePos = BaseXVector(0.0, -20.0, -3100.0);
            hole.holePos = BaseXVector(80.0, 30.0, -3350.0);
            hole.par = 3;
            hole.fairwayWidth = 90.0;
            hole.description = L"Hole 6: Uphill approach to elevated green";
            break;
        case 7: // S-curve hole
            hole.teePos = BaseXVector(100.0, 0.0, -3550.0);
            hole.holePos = BaseXVector(-80.0, 0.0, -3900.0);
            hole.par = 4;
            hole.fairwayWidth = 85.0;
            hole.description = L"Hole 7: S-curve with multiple hazards";
            hole.hazards.push_back(BaseXVector(0.0, 0.0, -3700.0));
            hole.trees.push_back(BaseXVector(50.0, 0.0, -3650.0));
            break;
        case 8: // Risk/reward hole
            hole.teePos = BaseXVector(-100.0, 0.0, -4100.0);
            hole.holePos = BaseXVector(120.0, 0.0, -4350.0);
            hole.par = 4;
            hole.fairwayWidth = 70.0;
            hole.description = L"Hole 8: Risk/reward - carry the water or play safe";
            hole.hazards.push_back(BaseXVector(0.0, 0.0, -4200.0));
            hole.hazards.push_back(BaseXVector(20.0, 0.0, -4220.0));
            break;
        case 9: // Championship finishing hole
            hole.teePos = BaseXVector(150.0, 0.0, -4550.0);
            hole.holePos = BaseXVector(0.0, 0.0, -4900.0);
            hole.par = 5;
            hole.fairwayWidth = 100.0;
            hole.description = L"Hole 9: Championship finish with bunkers guarding green";
            hole.hazards.push_back(BaseXVector(-30.0, 0.0, -4850.0));
            hole.hazards.push_back(BaseXVector(30.0, 0.0, -4850.0));
            hole.trees.push_back(BaseXVector(-80.0, 0.0, -4800.0));
            hole.trees.push_back(BaseXVector(80.0, 0.0, -4800.0));
            break;
        default:
            hole.teePos = BaseXVector(0.0, 0.0, -200.0 - (holeNumber * 400.0));
            hole.holePos = BaseXVector(0.0, 0.0, -400.0 - (holeNumber * 400.0));
            hole.par = 4;
            hole.fairwayWidth = 100.0;
            hole.description = L"Standard hole";
            break;
    }
    
    hole.holeRadius = 11.0;
    hole.fairwayColor = GOLF_GRASS_FAIRWAY;
    hole.roughColor = GOLF_GRASS_ROUGH;
    
    return hole;
}

// ... existing helper methods remain the same (UpdateView, ToCameraSpace, ProjectToScreen, etc.)

void DemoGolf::UpdateView() {
    BaseXMatrix rotY = BaseXMatrix::createRotationY(camYaw);
    BaseXMatrix rotX = BaseXMatrix::createRotationX(camPitch);
    view = rotY * rotX;
}

BaseXVector DemoGolf::ToCameraSpace(const BaseXVector& world) const {
    return view.transform(world - camPos);
}

bool DemoGolf::ProjectToScreen(const BaseXVector& vCam, POINT& outPt, const RECT& rc) const {
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return false;
    double aspect = w / (double)h;
    double f = 1.0 / tan(ToRad(fovDeg) * 0.5);

    double x = vCam.x();
    double y = vCam.y();
    double z = vCam.z();

    if (z > -nearZ || z < -farZ) return false;

    double nx = (x * f / aspect) / -z;
    double ny = (y * f) / -z;

    outPt.x = rc.left + (int)((nx * 0.5 + 0.5) * w);
    outPt.y = rc.top  + (int)((-ny * 0.5 + 0.5) * h);
    return true;
}

bool DemoGolf::WorldToScreen(const BaseXVector& world, POINT& outPt, const RECT& rc) const {
    BaseXVector cam = ToCameraSpace(world);
    return ProjectToScreen(cam, outPt, rc);
}

bool DemoGolf::ScreenRayToGround(int sx, int sy, BaseXVector& outPoint) const {
    RECT rc; GetClientRect(hwnd, &rc);
    if (rc.top < 130) rc.top = 130;
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    double aspect = w / (double)h;
    double f = 1.0 / tan(ToRad(fovDeg) * 0.5);

    // NDC
    double nx = ( (sx - rc.left) / (double)w ) * 2.0 - 1.0;
    double ny = -(((sy - rc.top) / (double)h) * 2.0 - 1.0);

    // Ray in camera space (z = -1)
    BaseXVector dirCam(nx / (f / aspect), ny / f, -1.0);
    // Normalize
    double mag = dirCam.magnitude();
    if (mag > 1e-9) dirCam = dirCam / mag;

    // Convert to world: inverse of view rotation is transpose for rotation matrices
    BaseXMatrix rotY = BaseXMatrix::createRotationY(-camYaw);
    BaseXMatrix rotX = BaseXMatrix::createRotationX(-camPitch);
    BaseXMatrix invRot = rotX * rotY; // inverse of (rotY*rotX)

    BaseXVector dirWorld = invRot.transform(dirCam);

    // Parametric ray: P(t) = camPos + t * dirWorld, intersect ground plane y=0
    double dy = dirWorld.y();
    if (fabs(dy) < 1e-6) return false;
    double t = (0.0 - camPos.y()) / dy;
    if (t <= 0) return false;

    BaseXVector hit = camPos + dirWorld * t;
    outPoint = hit;
    return true;
}

void DemoGolf::UpdateCamera(float deltaTime) {
    if (cameraFollowsBall && ballInMotion) {
        // Smooth camera follow
        BaseXVector targetPos = ballPos + BaseXVector(0.0, 100.0, 200.0);
        BaseXVector diff = targetPos - camPos;
        camPos = camPos + diff * (deltaTime / 1000.0 * 2.0);
        
        // Look at ball
        BaseXVector dir = ballPos - camPos;
        camYaw = atan2(dir.x(), dir.z());
        double horizDist = sqrt(dir.x() * dir.x() + dir.z() * dir.z());
        camPitch = atan2(dir.y(), horizDist);
    }
}

void DemoGolf::UpdateTrails(float deltaTime) {
    // Update existing trails
    for (auto it = ballTrails.begin(); it != ballTrails.end();) {
        it->life -= deltaTime / 1000.0f;
        if (it->life <= 0.0f) {
            it = ballTrails.erase(it);
        } else {
            ++it;
        }
    }
    
    // Add new trail if ball is moving
    if (ballVel.magnitude() > 10.0) {
        BallTrail trail;
        trail.position = ballPos;
        trail.life = 2.0f;
        trail.maxLife = 2.0f;
        trail.color = GOLF_TRAIL;
        ballTrails.push_back(trail);
    }
}

void DemoGolf::UpdateEnvironment(float deltaTime) {
    grassWavePhase += deltaTime / 1000.0;
    if (grassWavePhase > 2.0 * M_PI) grassWavePhase -= 2.0 * M_PI;
    
    // Slowly move clouds
    for (auto& cloud : cloudPositions) {
        cloud.setX(cloud.x() + windSpeed * deltaTime / 1000.0);
        if (cloud.x() > 1500.0) cloud.setX(-1500.0);
    }
}

void DemoGolf::Simulate(double dt) {
    if (inHole || ballInWater) return;
    
    ballInMotion = (ballVel.magnitude() > 5.0);
    
    // Apply gravity
    ballVel.setY(ballVel.y() + gravity * dt);

    // Apply wind effects
    if (ballPos.y() > ballRadius) {
        BaseXVector windForce = windDirection * (windSpeed * dt * 0.5);
        ballVel = ballVel + windForce;
    }

    // Air drag (stronger at higher speeds)
    double speed = ballVel.magnitude();
    double dragFactor = airDrag * (1.0 + speed * 0.001) * dt;
    ballVel *= (1.0 - dragFactor);

    // Integrate position
    ballPos = ballPos + ballVel * dt;

    // Enhanced ground collision with terrain variation
    double terrainHeight = GetTerrainHeight(ballPos);
    double groundY = terrainHeight + ballRadius;
    
    if (ballPos.y() < groundY) {
        ballPos.setY(groundY);
        if (ballVel.y() < 0) {
            // Different bounce based on surface
            double bounceFactor = ballInSand ? bounce * 0.3 : bounce;
            ballVel.setY(-ballVel.y() * bounceFactor);
        }
        
        // Apply surface friction
        BaseXVector vHoriz(ballVel.x(), 0.0, ballVel.z());
        double hSpeed = vHoriz.magnitude();
        if (hSpeed > 1e-3) {
            double frictionFactor = ballInSand ? rollingFriction * 3.0 : rollingFriction;
            double decel = frictionFactor * 9.8;
            double newSpeed = std::max(0.0, hSpeed - decel * dt * 60.0);
            if (hSpeed > 0) {
                vHoriz = vHoriz * (newSpeed / hSpeed);
                ballVel.setX(vHoriz.x());
                ballVel.setZ(vHoriz.z());
            }
        }
    }

    // Enhanced bounds checking
    if (ballPos.x() - ballRadius < boundLeft) {
        ballPos.setX(boundLeft + ballRadius);
        ballVel.setX(-ballVel.x() * 0.5);
    }
    if (ballPos.x() + ballRadius > boundRight) {
        ballPos.setX(boundRight - ballRadius);
        ballVel.setX(-ballVel.x() * 0.5);
    }
    if (ballPos.z() + ballRadius > boundNear) {
        ballPos.setZ(boundNear - ballRadius);
        ballVel.setZ(-ballVel.z() * 0.5);
    }
    if (ballPos.z() - ballRadius < boundFar) {
        ballPos.setZ(boundFar + ballRadius);
        ballVel.setZ(-ballVel.z() * 0.5);
    }

    // Check hazards
    ballInSand = false;
    ballInWater = false;
    
    if (currentHole < course.size()) {
        const GolfHole& hole = course[currentHole];
        
        for (const auto& hazard : hole.hazards) {
            double dist = ballPos.distanceTo(hazard);
            if (dist < 40.0) {
                if (hazard.y() < -5.0) { // Water hazard
                    ballInWater = true;
                    ballVel = BaseXVector(0, 0, 0);
                    // Reset to last position or tee
                    ballPos = hole.teePos;
                    ballPos.setY(ballRadius);
                    strokes += 1; // Penalty stroke
                } else { // Sand trap
                    ballInSand = true;
                }
            }
        }
        
        // Check hole capture
        BaseXVector flat = BaseXVector(ballPos.x(), 0.0, ballPos.z());
        double dist = flat.distanceTo(BaseXVector(hole.holePos.x(), 0.0, hole.holePos.z()));
        if (dist < hole.holeRadius * 0.9 && ballPos.y() <= ballRadius + 2.0 && ballVel.magnitude() < 30.0) {
            inHole = true;
            ballPos = BaseXVector(hole.holePos.x(), ballRadius * 0.2, hole.holePos.z());
            ballVel = BaseXVector(0, 0, 0);
            holeScores[currentHole] = strokes;
            totalScore += strokes;
        }
    }
}

void DemoGolf::Update(float deltaTime) {
    double dt = clampd(deltaTime / 1000.0, 0.0, 0.05); // seconds

    // Camera controls (WASD/QE, arrows)
    if (!cameraFollowsBall) {
        double move = 200.0 * dt;
        if (GetAsyncKeyState(VK_LEFT) & 0x8000)  camYaw -= 0.9 * dt;
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) camYaw += 0.9 * dt;
        if (GetAsyncKeyState(VK_UP) & 0x8000)    camPitch -= 0.7 * dt;
        if (GetAsyncKeyState(VK_DOWN) & 0x8000)  camPitch += 0.7 * dt;
        camPitch = clampd(camPitch, -1.3, 0.8);

        double cy = cos(camYaw), sy = sin(camYaw);
        BaseXVector fwd(cy, 0.0, sy);
        BaseXVector strafe(-sy, 0.0, cy);
        if (GetAsyncKeyState('W') & 0x8000) camPos = camPos + fwd * move;
        if (GetAsyncKeyState('S') & 0x8000) camPos = camPos - fwd * move;
        if (GetAsyncKeyState('D') & 0x8000) camPos = camPos + strafe * move;
        if (GetAsyncKeyState('A') & 0x8000) camPos = camPos - strafe * move;
        if (GetAsyncKeyState('E') & 0x8000) camPos = camPos + BaseXVector(0, move, 0);
        if (GetAsyncKeyState('Q') & 0x8000) camPos = camPos - BaseXVector(0, move, 0);
    }

    UpdateView();
    UpdateCamera(deltaTime);
    UpdateTrails(deltaTime);
    UpdateEnvironment(deltaTime);
    Simulate(dt);
}

// Enhanced rendering methods...
void DemoGolf::DrawGlow(HDC hdc, int x, int y, int radius, COLORREF color, int intensity) {
    for (int i = intensity; i > 0; --i) {
        int alpha = 255 / (i + 1);
        int r = GetRValue(color);
        int g = GetGValue(color);
        int b = GetBValue(color);
        
        COLORREF glowColor = RGB(
            std::min(255, r + alpha/4),
            std::min(255, g + alpha/4),
            std::min(255, b + alpha/4)
        );
        
        HBRUSH glowBrush = CreateSolidBrush(glowColor);
        HPEN glowPen = CreatePen(PS_SOLID, 1, glowColor);
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, glowBrush);
        HPEN oldPen = (HPEN)SelectObject(hdc, glowPen);
        
        int glowRadius = radius + i * 2;
        Ellipse(hdc, x - glowRadius, y - glowRadius, x + glowRadius, y + glowRadius);
        
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(glowBrush);
        DeleteObject(glowPen);
    }
}

COLORREF DemoGolf::GetSkyColor() const {
    // Dynamic sky color based on time of day
    if (timeOfDay >= 6.0 && timeOfDay <= 18.0) {
        return GOLF_SKY_DAY;
    } else if (timeOfDay >= 5.0 && timeOfDay < 6.0) {
        return GOLF_SKY_DAWN;
    } else if (timeOfDay > 18.0 && timeOfDay <= 19.0) {
        return GOLF_SKY_DUSK;
    } else {
        return RGB(25, 25, 112); // Night sky
    }
}

COLORREF DemoGolf::GetGrassColor(const BaseXVector& pos) const {
    // Vary grass color based on position and wave effect
    double wave = sin(grassWavePhase + pos.x() * 0.01 + pos.z() * 0.01) * 0.1 + 0.9;
    int r = (int)(GetRValue(GOLF_GRASS_FAIRWAY) * wave);
    int g = (int)(GetGValue(GOLF_GRASS_FAIRWAY) * wave);
    int b = (int)(GetBValue(GOLF_GRASS_FAIRWAY) * wave);
    return RGB(std::min(255, r), std::min(255, g), std::min(255, b));
}

double DemoGolf::GetTerrainHeight(const BaseXVector& pos) const {
    // Simple terrain variation
    return sin(pos.x() * 0.01) * 5.0 + cos(pos.z() * 0.01) * 3.0;
}

bool DemoGolf::IsInHazard(const BaseXVector& pos) const {
    if (currentHole >= course.size()) return false;
    
    const GolfHole& hole = course[currentHole];
    for (const auto& hazard : hole.hazards) {
        if (pos.distanceTo(hazard) < 30.0) return true;
    }
    return false;
}

void DemoGolf::Render(HDC hdc) {
    RECT rc; 
    GetClientRect(hwnd, &rc);
    if (rc.top < 130) rc.top = 130;

    RenderSkybox(hdc, rc);
    RenderCourse(hdc, rc);
    RenderEnvironment(hdc, rc);
    RenderBall(hdc, rc);
    
    if (showTrajectoryPreview && aiming) {
        RenderTrajectoryPreview(hdc, rc);
    }
    
    RenderHUD(hdc, rc);
}

void DemoGolf::RenderSkybox(HDC hdc, const RECT& rc) {
    // Enhanced sky with gradient
    COLORREF skyColor = GetSkyColor();
    TRIVERTEX vertex[2];
    vertex[0].x = rc.left;
    vertex[0].y = rc.top;
    vertex[0].Red = GetRValue(skyColor) << 8;
    vertex[0].Green = GetGValue(skyColor) << 8;
    vertex[0].Blue = GetBValue(skyColor) << 8;
    vertex[0].Alpha = 0x0000;

    vertex[1].x = rc.right;
    vertex[1].y = rc.bottom;
    // Slightly darker toward the horizon
    vertex[1].Red = (int)(GetRValue(skyColor) * 0.7) << 8;
    vertex[1].Green = (int)(GetGValue(skyColor) * 0.7) << 8;
    vertex[1].Blue = (int)(GetBValue(skyColor) * 0.7) << 8;
    vertex[1].Alpha = 0x0000;

    GRADIENT_RECT gRect = {0, 1};
    GradientFill(hdc, vertex, 2, &gRect, 1, GRADIENT_FILL_RECT_V);

    // Render clouds
    for (const auto& cloud : cloudPositions) {
        POINT cloudPt;
        if (WorldToScreen(cloud, cloudPt, rc)) {
            DrawGlow(hdc, cloudPt.x, cloudPt.y, 25, RGB(255, 255, 255), 2);
        }
    }
}

void DemoGolf::RenderCourse(HDC hdc, const RECT& rc) {
    if (currentHole >= course.size()) return;
    
    const GolfHole& hole = course[currentHole];
    
    // Enhanced fairway rendering with perspective
    double expandedLeft = boundLeft - 200.0;
    double expandedRight = boundRight + 200.0;
    double expandedNear = boundNear + 100.0;
    double expandedFar = boundFar - 500.0;
    
    POINT p0, p1, p2, p3;
    BaseXVector wl(expandedLeft, 0.0, expandedNear);
    BaseXVector wr(expandedRight, 0.0, expandedNear);
    BaseXVector fr(expandedRight, 0.0, expandedFar);
    BaseXVector fl(expandedLeft, 0.0, expandedFar);

    bool courseVisible = WorldToScreen(wl, p0, rc) && WorldToScreen(wr, p1, rc) && 
                        WorldToScreen(fr, p2, rc) && WorldToScreen(fl, p3, rc);
    
    if (courseVisible) {
        // Base course area
        HBRUSH courseBrush = CreateSolidBrush(hole.roughColor);
        HPEN oldPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, courseBrush);
        POINT poly[4] = {p0, p1, p2, p3};
        Polygon(hdc, poly, 4);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(courseBrush);
        
        // Enhanced fairway with dynamic grass texture
        for (int x = (int)boundLeft; x <= (int)boundRight; x += 30) {
            for (int z = (int)boundFar; z <= (int)boundNear; z += 30) {
                BaseXVector grassPos((double)x, 0.0, (double)z);
                POINT grassPt;
                if (WorldToScreen(grassPos, grassPt, rc)) {
                    COLORREF grassColor = GetGrassColor(grassPos);
                    HBRUSH grassBrush = CreateSolidBrush(grassColor);
                    HBRUSH oldB = (HBRUSH)SelectObject(hdc, grassBrush);
                    Rectangle(hdc, grassPt.x - 2, grassPt.y - 2, grassPt.x + 2, grassPt.y + 2);
                    SelectObject(hdc, oldB);
                    DeleteObject(grassBrush);
                }
            }
        }
    }
    
    // Enhanced hole rendering with glow
    POINT holePt;
    if (WorldToScreen(hole.holePos, holePt, rc)) {
        DrawGlow(hdc, holePt.x, holePt.y, 8, GOLF_HOLE_BLACK, 2);
        
        // Flag
        HPEN flagPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
        HPEN oldP = (HPEN)SelectObject(hdc, flagPen);
        MoveToEx(hdc, holePt.x, holePt.y, nullptr);
        LineTo(hdc, holePt.x, holePt.y - 30);
        
        // Flag banner
        POINT flagPoints[3] = {{holePt.x, holePt.y - 30}, {holePt.x + 15, holePt.y - 25}, {holePt.x, holePt.y - 20}};
        HBRUSH flagBrush = CreateSolidBrush(RGB(255, 0, 0));
        HBRUSH oldFB = (HBRUSH)SelectObject(hdc, flagBrush);
        Polygon(hdc, flagPoints, 3);
        SelectObject(hdc, oldFB);
        DeleteObject(flagBrush);
        
        SelectObject(hdc, oldP);
        DeleteObject(flagPen);
    }
    
    // Render hazards with enhanced visuals
    for (const auto& hazard : hole.hazards) {
        POINT hazardPt;
        if (WorldToScreen(hazard, hazardPt, rc)) {
            if (hazard.y() < -5.0) {
                // Water hazard with ripple effect
                DrawGlow(hdc, hazardPt.x, hazardPt.y, 25, GOLF_WATER, 3);
            } else {
                // Sand trap
                DrawGlow(hdc, hazardPt.x, hazardPt.y, 20, GOLF_SAND, 2);
            }
        }
    }
    
    // Render trees
    for (const auto& tree : hole.trees) {
        POINT treePt;
        if (WorldToScreen(tree, treePt, rc)) {
            // Tree trunk
            HPEN trunkPen = CreatePen(PS_SOLID, 4, RGB(101, 67, 33));
            HPEN oldTP = (HPEN)SelectObject(hdc, trunkPen);
            MoveToEx(hdc, treePt.x, treePt.y, nullptr);
            LineTo(hdc, treePt.x, treePt.y - 25);
            SelectObject(hdc, oldTP);
            DeleteObject(trunkPen);
            
            // Tree foliage
            DrawGlow(hdc, treePt.x, treePt.y - 30, 15, RGB(34, 139, 34), 2);
        }
    }
}

void DemoGolf::RenderBall(HDC hdc, const RECT& rc) {
    // Render ball trails first
    for (const auto& trail : ballTrails) {
        POINT trailPt;
        if (WorldToScreen(trail.position, trailPt, rc)) {
            float alpha = trail.life / trail.maxLife;
            int intensity = (int)(alpha * 255);
            COLORREF trailColor = RGB(
                std::min(255, GetRValue(trail.color) + intensity/2),
                std::min(255, GetGValue(trail.color) + intensity/2),
                std::min(255, GetBValue(trail.color) + intensity/4)
            );
            
            HBRUSH trailBrush = CreateSolidBrush(trailColor);
            HBRUSH oldTB = (HBRUSH)SelectObject(hdc, trailBrush);
            int size = (int)(alpha * 3) + 1;
            Ellipse(hdc, trailPt.x - size, trailPt.y - size, trailPt.x + size, trailPt.y + size);
            SelectObject(hdc, oldTB);
            DeleteObject(trailBrush);
        }
    }
    
    // Enhanced ball rendering with glow and shadow
    POINT ballPt;
    if (WorldToScreen(ballPos, ballPt, rc)) {
        // Shadow
        POINT shadowPt;
        BaseXVector shadowPos = ballPos + BaseXVector(2.0, -ballRadius + 1.0, 2.0);
        if (WorldToScreen(shadowPos, shadowPt, rc)) {
            DrawGlow(hdc, shadowPt.x, shadowPt.y, 6, RGB(50, 50, 50), 1);
        }
        
        // Ball with enhanced glow
        int ballSize = (int)clampd(8.0 + (ballPos.y() - ballRadius) * 0.02, 6.0, 15.0);
        DrawGlow(hdc, ballPt.x, ballPt.y, ballSize, GOLF_BALL_GLOW, 3);
        
        // Ball core
        HBRUSH ballBrush = CreateSolidBrush(GOLF_BALL_WHITE);
        HPEN ballPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
        HBRUSH oldBB = (HBRUSH)SelectObject(hdc, ballBrush);
        HPEN oldBP = (HPEN)SelectObject(hdc, ballPen);
        Ellipse(hdc, ballPt.x - ballSize/2, ballPt.y - ballSize/2, 
                ballPt.x + ballSize/2, ballPt.y + ballSize/2);
        SelectObject(hdc, oldBB);
        SelectObject(hdc, oldBP);
        DeleteObject(ballBrush);
        DeleteObject(ballPen);
    }
}

void DemoGolf::RenderHUD(HDC hdc, const RECT& rc) {
    SetBkMode(hdc, TRANSPARENT);
    
    // Enhanced header with neon colors
    SetTextColor(hdc, RGB(0, 255, 255));
    HFONT headerFont = CreateFont(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(hdc, headerFont);
    
    wchar_t header[256];
    if (currentHole < course.size()) {
        const GolfHole& hole = course[currentHole];
        wsprintf(header, L"BaseX Golf 3D - Hole %d (Par %d) | Strokes: %d%s%s", 
                currentHole + 1, hole.par, strokes, 
                inHole ? L" (HOLED!)" : L"",
                ballInSand ? L" [SAND]" : ballInWater ? L" [WATER]" : L"");
    } else {
        wsprintf(header, L"BaseX Golf 3D - Course Complete!");
    }
    TextOut(hdc, rc.left + 10, rc.top + 10, header, lstrlenW(header));
    
    // Score display
    SetTextColor(hdc, RGB(255, 255, 0));
    wchar_t scoreText[128];
    wsprintf(scoreText, L"Total Score: %d (Par: %d) | Round: %d/%d", 
            totalScore, totalPar, currentHole + 1, totalHoles);
    TextOut(hdc, rc.left + 10, rc.top + 35, scoreText, lstrlenW(scoreText));
    
    // Enhanced help text
    if (showHelp) {
        SetTextColor(hdc, RGB(150, 255, 150));
        HFONT helpFont = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        SelectObject(hdc, helpFont);
        
        const wchar_t* controls[] = {
            L"Click+Drag: Aim & Power | Space: Reset Aim | R: Reset Hole",
            L"Arrows: Camera | WASD: Move | C: Toggle Camera Follow",
            L"N: Next Hole | H: Toggle Help | T: Toggle Trajectory Preview"
        };
        
        for (int i = 0; i < 3; ++i) {
            TextOut(hdc, rc.left + 10, rc.top + 60 + i * 18, controls[i], lstrlenW(controls[i]));
        }
        
        DeleteObject(helpFont);
    }
    
    // Current hole description
    if (currentHole < course.size()) {
        SetTextColor(hdc, RGB(255, 200, 100));
        const GolfHole& hole = course[currentHole];
        TextOut(hdc, rc.left + 10, rc.bottom - 40, hole.description.c_str(), (int)hole.description.length());
    }
    
    // Status indicators
    SetTextColor(hdc, RGB(255, 100, 100));
    wchar_t statusText[64];
    wsprintf(statusText, L"Wind: %.1f | Time: %.1f:00 | Camera: %s", 
            windSpeed, timeOfDay, cameraFollowsBall ? L"Follow" : L"Free");
    TextOut(hdc, rc.left + 10, rc.bottom - 20, statusText, lstrlenW(statusText));
    
    SelectObject(hdc, oldFont);
    DeleteObject(headerFont);
    
    // Aiming line with enhanced visualization
    if (aiming) {
        HPEN aimPen = CreatePen(PS_DOT, 2, RGB(255, 255, 0));
        HPEN oldAP = (HPEN)SelectObject(hdc, aimPen);
        
        // Power indicator
        int dx = aimCur.x - aimStart.x;
        int dy = aimCur.y - aimStart.y;
        double power = sqrt(dx*dx + dy*dy) / 250.0;
        power = std::min(1.0, power);
        
        // Show power percent near the aim start
        wchar_t powerText[32];
        int percent = (int)(power * 100.0 + 0.5);
        wsprintf(powerText, L"Power: %d%%", percent);
        SetTextColor(hdc, RGB(255, 220, 0));
        TextOut(hdc, aimStart.x + 12, aimStart.y - 22, powerText, lstrlenW(powerText));
        
        // Power arc
        for (int i = 0; i < (int)(power * 50); ++i) {
            double angle = (i / 50.0) * M_PI;
            int px = aimStart.x + (int)(cos(angle) * 30);
            int py = aimStart.y - (int)(sin(angle) * 15);
            SetPixel(hdc, px, py, RGB(255, (int)(255 * (1.0 - power)), 0));
        }
        
        MoveToEx(hdc, aimStart.x, aimStart.y, nullptr);
        LineTo(hdc, aimCur.x, aimCur.y);
        SelectObject(hdc, oldAP);
        DeleteObject(aimPen);
    }
}

void DemoGolf::RenderTrajectoryPreview(HDC hdc, const RECT& rc) {
    if (!aiming) return;
    
    // Calculate predicted trajectory
    BaseXVector pStart, pEnd;
    if (!ScreenRayToGround(aimStart.x, aimStart.y, pStart) || 
        !ScreenRayToGround(aimCur.x, aimCur.y, pEnd)) return;
    
    BaseXVector dir = pStart - pEnd;
    dir.setY(0.0);
    double len = dir.magnitude();
    if (len < 1.0) return;
    
    dir = dir / len;
    double power = std::min(1.0, len / 250.0);
    double speed = 500.0 * power;
    double elev = ToRad(aimElevDeg * power);
    
    BaseXVector vel(dir.x() * cos(elev) * speed, sin(elev) * speed, dir.z() * cos(elev) * speed);
    BaseXVector pos = ballPos;
    
    // Draw trajectory arc
    HPEN trajPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 0));
    HPEN oldTP = (HPEN)SelectObject(hdc, trajPen);
    
    bool firstPoint = true;
    for (int i = 0; i < 100; ++i) {
        double dt = 0.05;
        vel.setY(vel.y() + gravity * dt);
        pos = pos + vel * dt;
        
        if (pos.y() < ballRadius) break;
        
        POINT trajPt;
        if (WorldToScreen(pos, trajPt, rc)) {
            if (firstPoint) {
                MoveToEx(hdc, trajPt.x, trajPt.y, nullptr);
                firstPoint = false;
            } else {
                LineTo(hdc, trajPt.x, trajPt.y);
            }
        }
    }
    
    SelectObject(hdc, oldTP);
    DeleteObject(trajPen);
}

void DemoGolf::RenderEnvironment(HDC hdc, const RECT& rc) {
    // Additional environmental effects can be added here
    // such as animated grass, particle effects, etc.
}

void DemoGolf::RenderScorecard(HDC hdc, const RECT& rc) {
    // If no HDC provided, skip rendering (caller should provide valid HDC via Render)
    if (!hdc) return;

    // Render a detailed scorecard when requested
    if (currentHole >= totalHoles || showScorecard) {
        SetTextColor(hdc, RGB(255, 255, 255));
        HFONT scorecardFont = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HFONT oldFont = (HFONT)SelectObject(hdc, scorecardFont);

        wchar_t finalScore[128];
        wsprintf(finalScore, L"-- ROUND COMPLETE! Final Score: %d (Par: %d)", totalScore, totalPar);
        TextOut(hdc, rc.left + 50, rc.top + 100, finalScore, lstrlenW(finalScore));

        // Show individual hole scores
        for (int i = 0; i < totalHoles && i < holeScores.size(); ++i) {
            wchar_t holeScore[32];
            wsprintf(holeScore, L"Hole %d: %d", i + 1, holeScores[i]);
            TextOut(hdc, rc.left + 50, rc.top + 130 + i * 20, holeScore, lstrlenW(holeScore));
        }

        SelectObject(hdc, oldFont);
        DeleteObject(scorecardFont);
    }
}

// Input handling methods remain mostly the same but with enhancements...

void DemoGolf::HandleMouseDown(int x, int y, bool leftButton) {
    if (!leftButton || inHole) return;
    aiming = true;
    aimStart = {x, y};
    aimCur = aimStart;
}

void DemoGolf::HandleMouseMove(int x, int y) {
    if (aiming) {
        aimCur = {x, y};
    }
}

void DemoGolf::HandleMouseUp(int x, int y, bool leftButton) {
    if (!leftButton || !aiming) return;
    aimCur = {x, y};
    StrikeBallFromDrag();
    aiming = false;
}

void DemoGolf::HandleKeyDown(int keyCode) {
    switch (keyCode) {
        case 'R':
            ResetHole();
            break;
        case 'N':
            if (inHole) NextHole();
            break;
        case 'C':
            cameraFollowsBall = !cameraFollowsBall;
            break;
        case 'H':
            showHelp = !showHelp;
            break;
        case 'T':
            showTrajectoryPreview = !showTrajectoryPreview;
            break;
        case VK_SPACE:
            spaceHeld = true;
            break;
    }
}

void DemoGolf::HandleKeyUp(int keyCode) {
    if (keyCode == VK_SPACE && spaceHeld) {
        aiming = false;
        spaceHeld = false;
    }
}

void DemoGolf::StrikeBallFromDrag() {
    if (ballVel.magnitude() > 5.0) return; // Ball still in motion
    
    BaseXVector pStart, pEnd;
    if (!ScreenRayToGround(aimStart.x, aimStart.y, pStart) || 
        !ScreenRayToGround(aimCur.x, aimCur.y, pEnd)) return;

    BaseXVector dir = pStart - pEnd;
    dir.setY(0.0);
    double len = dir.magnitude();
    if (len < 1.0) return;

    dir = dir / len;

    double power = std::min(1.0, len / 250.0);
    double speed = 600.0 * power; // Slightly increased power

    double elev = ToRad(aimElevDeg * power);
    BaseXVector horiz = dir * (cos(elev) * speed);
    double vy = sin(elev) * speed;

    ballVel = BaseXVector(horiz.x(), vy, horiz.z());
    strokes++;
    ballInMotion = true;
}

void DemoGolf::ResetHole() {
    if (currentHole < course.size()) {
        ballPos = course[currentHole].teePos;
        ballPos.setY(ballRadius);
    } else {
        ballPos = BaseXVector(0.0, ballRadius, -200.0);
    }
    ballVel = BaseXVector(0, 0, 0);
    strokes = 0;
    inHole = false;
    ballInMotion = false;
    ballInWater = false;
    ballInSand = false;
    ballTrails.clear();

    // Hide scorecard when resetting
    showScorecard = false;
}

void DemoGolf::NextHole() {
    if (currentHole < totalHoles - 1) {
        currentHole++;
        ResetHole();
    } else {
        // Course complete - show final scorecard in the main render loop instead of calling render directly
        showScorecard = true;
    }
}
