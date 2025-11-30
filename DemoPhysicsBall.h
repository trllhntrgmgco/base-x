#pragma once
#include "DemoBase.h"

struct Ball {
    float x, y;           // Position
    float vx, vy;         // Velocity
    float radius;         // Size
    COLORREF color;       // Color
    float mass;           // Mass for physics calculations
};

class DemoPhysicsBall : public DemoBase {
private:
    static const int MAX_BALLS = 5;
    Ball balls[MAX_BALLS];
    int ballCount;
    float gravity;
    float bounce;
    float friction;
    RECT bounds;
    BOOL demoInitialized;

public:
    void Init(HWND hWnd) override;
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;
    void Shutdown() override;

private:
    void AddBall(float x, float y, float vx, float vy, float radius, COLORREF color);
    void HandleCollisions();
    void HandleBoundaryCollisions(Ball& ball);
    bool CheckBallCollision(Ball& ball1, Ball& ball2);
    void ResolveBallCollision(Ball& ball1, Ball& ball2);
};