#pragma once
#include "DemoBase.h"
#include <vector>

struct ParticlePS {
    double x, y;
    double vx, vy;
    float life;   // seconds
    float size;
    COLORREF color;
};

class DemoPhysicsParticles : public DemoBase {
public:
    void Init(HWND hWnd) override;
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;
    void Shutdown() override;

private:
    std::vector<ParticlePS> particles;
    int maxParticles = 1000;
    double emitterX = 400.0;
    double emitterY = 300.0;
    float spawnAccumulator = 0.0f;
    float spawnRate = 200.0f; // particles per second
    bool running = true;

    void SpawnParticle();
};
