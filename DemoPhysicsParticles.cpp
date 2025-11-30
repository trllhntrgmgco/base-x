#include "DemoPhysicsParticles.h"
#include <windows.h>
#include <cstdlib>
#include <cmath>

void DemoPhysicsParticles::Init(HWND hWnd) {
    particles.clear();
    spawnAccumulator = 0.0f;
    running = true;
    RECT rc;
    GetClientRect(hWnd, &rc);
    emitterX = (rc.right - rc.left) / 2.0;
    emitterY = (rc.bottom - rc.top) / 2.0;
}

void DemoPhysicsParticles::SpawnParticle() {
    if ((int)particles.size() >= maxParticles) return;
    ParticlePS p;
    p.x = emitterX + ((rand() % 200) - 100) * 0.5;
    p.y = emitterY + ((rand() % 200) - 100) * 0.5;
    double angle = (rand() % 360) * 3.1415926 / 180.0;
    double speed = 20 + rand() % 200;
    p.vx = cos(angle) * speed;
    p.vy = sin(angle) * speed;
    p.life = 1.0f + (rand() % 100) / 100.0f;
    p.size = 2.0f + (rand() % 6);
    p.color = RGB(150 + rand() % 106, 150 + rand() % 106, 150 + rand() % 106);
    particles.push_back(p);
}

void DemoPhysicsParticles::Update(float deltaTime) {
    if (!running) return;
    float dt = deltaTime / 1000.0f; // convert ms to seconds
    spawnAccumulator += dt * spawnRate;
    int spawnCount = (int)spawnAccumulator;
    spawnAccumulator -= spawnCount;
    for (int i = 0; i < spawnCount; ++i) SpawnParticle();

    for (int i = (int)particles.size() - 1; i >= 0; --i) {
        ParticlePS &p = particles[i];
        // integrate
        p.vy += 200.0 * dt; // gravity
        p.vx *= 0.999;
        p.vy *= 0.999;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.life -= dt;
        if (p.life <= 0.0f) {
            particles[i] = particles.back();
            particles.pop_back();
        }
    }
}

void DemoPhysicsParticles::Render(HDC hdc) {
    // draw simple particles as filled ellipses
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    HPEN oldPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));

    for (const ParticlePS &p : particles) {
        HBRUSH b = CreateSolidBrush(p.color);
        HBRUSH ob = (HBRUSH)SelectObject(hdc, b);
        int r = (int)max(1.0, p.size);
        Ellipse(hdc, (int)(p.x - r), (int)(p.y - r), (int)(p.x + r), (int)(p.y + r));
        SelectObject(hdc, ob);
        DeleteObject(b);
    }

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
}

void DemoPhysicsParticles::Shutdown() {
    particles.clear();
}
