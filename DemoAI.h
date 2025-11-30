#pragma once
#include "DemoBase.h"
#include <windows.h>
#include <vector>

class DemoAI : public DemoBase {
public:
    void Init(HWND hWnd) override;
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;
    void Shutdown() override;

private:
    struct Agent {
        float x, y;
        float vx, vy;
    };

    std::vector<Agent> agents;
    float targetX = 0.0f;
    float targetY = 0.0f;
    bool running = false;

    // parameters
    int agentCount = 30;
    float maxSpeed = 80.0f; // units per second
    float neighborRadius = 30.0f;
    float seekWeight = 1.0f;
    float avoidWeight = 1.5f;
    float alignWeight = 0.8f;
    float cohesionWeight = 0.6f;
    float maxAccel = 200.0f; // limit acceleration
    bool debugRender = false;
};
