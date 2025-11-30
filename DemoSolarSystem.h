// DemoSolarSystem.h : Enhanced Solar System Simulation Demo Header
#pragma once

#include "DemoBase.h"
#include "physics.h"  // Add include for BaseX classes
#include <windows.h>
#include <vector>
#include <string>

using namespace BaseXEngine;  // Add using statement for BaseX classes

#define MAX_STARS 200
#define MAX_TRAILS 1000

// Enhanced celestial body structure
struct CelestialBody {
    std::string name;
    double orbitRadius;      // Distance from sun (AU scaled)
    double orbitSpeed;       // Orbital velocity (radians per year)
    double radius;           // Physical radius (km scaled)
    double mass;             // Mass (kg)
    COLORREF color;          // Enhanced display color
    bool isSun;
    bool hasRings;
    float glowIntensity;     // Enhanced glow effect intensity
    
    // Dynamic properties
    BaseXVector position;
    double currentAngle;     // Current orbital position
    double currentRotation;  // Current rotation angle
    double rotationSpeed;    // Rotation velocity (radians per day)
    
    CelestialBody(const std::string& n, double orbit, double period, double r, double m, COLORREF c, bool sun = false)
        : name(n), orbitRadius(orbit), radius(r), mass(m), color(c), isSun(sun), hasRings(false)
        , glowIntensity(1.0f), position(0, 0, 0), currentAngle(0), currentRotation(0), rotationSpeed(0)
    {
        // Calculate orbital speed (Kepler's 3rd law approximation)
        orbitSpeed = (period > 0) ? (2.0 * 3.14159265359 / (period * 365.25 * 24.0 * 3600.0)) : 0.0;
    }
};

// Enhanced star field
struct Star {
    BaseXVector position;
    float brightness;
    int size;
    COLORREF color;
};

// Enhanced orbital trails
struct OrbitTrail {
    BaseXVector position;
    COLORREF color;
    float life;
    float maxLife;
    int size;
};

class DemoSolarSystem : public DemoBase {
private:
    HWND hwnd;
    
    // Enhanced visual features
    bool enableTrails;
    bool enableEnhancedGlow;
    Star starField[MAX_STARS];
    OrbitTrail orbitTrails[MAX_TRAILS];
    int trailCount;
    int trailIndex;
    
    // Solar system data
    std::vector<CelestialBody> bodies;
    
    // Simulation constants
    static constexpr double AU = 149597870.7;        // Astronomical Unit in km
    static constexpr double SCALE_FACTOR = 0.00001;  // Scale down for display
    static constexpr double TIME_SCALE = 1000000.0;  // Speed up time
    
    // Camera system
    BaseXVector camPos = BaseXVector(0, 500, -1000);
    double camYaw = 0.0;
    double camPitch = -0.3;
    double camDistance = 800.0;
    BaseXMatrix view;
    
    // View projection
    double fovDeg = 45.0;
    double nearZ = 1.0;
    double farZ = 50000.0;
    
    // Simulation control
    bool isPaused = false;
    double timeAcceleration = 1.0;
    bool followBody = false;
    size_t selectedBody = 0;
    
    // Display options
    bool showOrbits = true;
    bool showLabels = true;
    bool showStats = false;
    
    // Helper methods
    void BuildEnhancedSolarSystem();
    void InitializeStarField();
    void CreateStar(int index);
    void UpdateOrbits(float deltaTime);
    void UpdateTrails(float deltaTime);
    void AddOrbitTrail(const CelestialBody& body);
    void UpdateCamera();
    bool ProjectToScreen(const BaseXVector& worldPos, POINT& outPt, const RECT& rect) const;
    BaseXVector GetScaledPosition(const CelestialBody& body) const;
    double GetScaledRadius(const CelestialBody& body) const;
    
    // Enhanced rendering methods
    void RenderEnhancedBody(HDC hdc, const CelestialBody& body, const RECT& rect);
    void RenderStarField(HDC hdc, const RECT& rect);
    void RenderTrails(HDC hdc, const RECT& rect);
    void RenderOrbit(HDC hdc, const CelestialBody& body, const RECT& rect);
    void RenderEnhancedUI(HDC hdc, const RECT& rect);
    void RenderStats(HDC hdc, const RECT& rect);
    
    // Legacy methods for compatibility
    void RenderBody(HDC hdc, const CelestialBody& body, const RECT& rect) {
        RenderEnhancedBody(hdc, body, rect);
    }
    void RenderUI(HDC hdc, const RECT& rect) {
        RenderEnhancedUI(hdc, rect);
    }

public:
    void Init(HWND hWnd) override;
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;
    void Shutdown() override;
    
    // Input handling
    void HandleKeyDown(int keyCode);
    void HandleKeyUp(int keyCode);
    void HandleMouseDown(int x, int y, bool leftButton = true);
    void HandleMouseMove(int x, int y);
    void HandleMouseUp(int x, int y, bool leftButton = true);
};