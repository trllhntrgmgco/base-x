// DemoWorld3D.h : Enhanced BaseX 3D World Demo Header
#pragma once

#include "DemoBase.h"
#include "physics.h"
#include <windows.h>
#include <vector>

using namespace BaseXEngine;

#define MAX_3D_PARTICLES 25

// Enhanced 3D Particle System
struct World3DParticle {
    BaseXVector position;
    BaseXVector velocity;
    float life;
    float maxLife;
    int size;
    COLORREF color;
    bool active;
};

// Enhanced 3D Edge Structure
struct Edge3D {
    int a, b;           // vertex indices
    COLORREF color;     // enhanced edge color
};

class DemoWorld3D : public DemoBase {
private:
    HWND hwnd;
    
    // Enhanced visual options
    bool enableWireframeGlow;
    bool enableParticleTrails;
    bool showDebugInfo;
    
    // 3D particle system
    World3DParticle world3DParticles[MAX_3D_PARTICLES];
    
    // 3D scene data
    std::vector<BaseXVector> vertices;
    std::vector<Edge3D> edges;
    
    // Camera system
    BaseXVector camPos = BaseXVector(0, 0, -300);
    double camYaw = 0.0;
    double camPitch = 0.0;
    double angle = 0.0; // for idle animation
    
    // View projection
    BaseXMatrix world, view;
    double fovDeg = 60.0;
    double nearZ = 1.0;
    double farZ = 1000.0;
    
    // Helper methods
    void BuildEnhancedScene();
    void BuildAdditionalShapes();
    void InitializeParticleSystem();
    void Create3DParticle(int index);
    void Update3DParticles(float deltaTime);
    void UpdateView();
    bool ProjectToScreen(const BaseXVector& vCam, POINT& outPt, const RECT& rc) const;
    
    // Enhanced rendering methods
    void DrawEnhancedBackground(HDC hdc, const RECT& rc);
    void DrawEnhancedUI(HDC hdc, const RECT& rc);
    void Render3DParticles(HDC hdc, const RECT& rc);
    void RenderEnhancedWireframe(HDC hdc, const RECT& rc);

public:
    void Init(HWND hWnd) override;
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;
    void Shutdown() override;
};
