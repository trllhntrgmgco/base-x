#pragma once
#include "DemoBase.h"

// Use define instead of static const to avoid C++ initialization issues
#define MAX_GRAVITY_PARTICLES 50

struct GravityParticle {
    float x, y;           // Position
    float vx, vy;         // Velocity
    float mass;           // Mass
    COLORREF color;       // Color
    float life;           // Lifetime
    bool active;          // Is particle active
};

class DemoPhysicsGravity : public DemoBase {
private:
    GravityParticle particles[MAX_GRAVITY_PARTICLES];
    int particleCount;
    float centralMass;    // Mass of central attractor
    float centralX, centralY; // Position of central attractor
    float spawnTimer;
    RECT bounds;
    BOOL demoInitialized;

public:
    void Init(HWND hWnd) override;
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;
    void Shutdown() override;

private:
    void SpawnParticle();
    void UpdateParticle(GravityParticle& p, float deltaTime);
    COLORREF GetParticleColor(float life);
};