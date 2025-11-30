#include <windows.h>
#include <mmsystem.h>
#include <math.h>
#include "DemoPhysicsBall.h"

#pragma comment(lib, "winmm.lib")

void DemoPhysicsBall::Init(HWND hWnd) {
    demoInitialized = TRUE;
    ballCount = 0;
    gravity = 350.0f;    // Pixels per second squared
    bounce = 0.8f;       // Energy retained after bounce
    friction = 0.99f;    // Air resistance
    
    // Get window bounds for physics simulation
    GetClientRect(hWnd, &bounds);
    bounds.top = 130;    // Account for controls at top
    
    // Add some initial balls
    AddBall(100, 200, 150, -100, 20, RGB(255, 100, 100));  // Red ball
    AddBall(200, 180, -120, -80, 15, RGB(100, 255, 100));   // Green ball
    AddBall(300, 220, 80, -150, 25, RGB(100, 100, 255));    // Blue ball
    AddBall(400, 160, -90, -60, 18, RGB(255, 255, 100));    // Yellow ball
    AddBall(150, 300, 200, -200, 22, RGB(255, 100, 255));   // Magenta ball
}

void DemoPhysicsBall::Update(float deltaTime) {
    if (!demoInitialized) return;
    
    float dt = deltaTime / 1000.0f; // Convert to seconds
    
    // Update each ball's physics
    for (int i = 0; i < ballCount; i++) {
        Ball& ball = balls[i];
        
        // Apply gravity
        ball.vy += gravity * dt;
        
        // Apply friction
        ball.vx *= friction;
        ball.vy *= friction;
        
        // Update position
        ball.x += ball.vx * dt;
        ball.y += ball.vy * dt;
        
        // Handle boundary collisions
        HandleBoundaryCollisions(ball);
    }
    
    // Handle ball-to-ball collisions
    HandleCollisions();
}

void DemoPhysicsBall::Render(HDC hdc) {
    if (!demoInitialized) return;
    
    // Draw physics info
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(220, 220, 220));
    TextOut(hdc, bounds.left + 10, bounds.top + 10, L"Physics Demo - Bouncing Balls", 29);
    
    WCHAR infoText[128];
    wsprintf(infoText, L"Gravity: %.0f  Bounce: %.2f  Friction: %.3f", gravity, bounce, friction);
    SetTextColor(hdc, RGB(180, 180, 180));
    TextOut(hdc, bounds.left + 10, bounds.top + 30, infoText, lstrlen(infoText));
    
    // Draw each ball
    for (int i = 0; i < ballCount; i++) {
        Ball& ball = balls[i];
        
        HBRUSH brush = CreateSolidBrush(ball.color);
        HPEN pen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
        
        SelectObject(hdc, brush);
        SelectObject(hdc, pen);
        
        int left = (int)(ball.x - ball.radius);
        int top = (int)(ball.y - ball.radius);
        int right = (int)(ball.x + ball.radius);
        int bottom = (int)(ball.y + ball.radius);
        
        Ellipse(hdc, left, top, right, bottom);
        
        DeleteObject(brush);
        DeleteObject(pen);
        
        // Draw velocity vector (scaled down for visibility)
        HPEN vectorPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
        SelectObject(hdc, vectorPen);
        
        int endX = (int)(ball.x + ball.vx * 0.1f);
        int endY = (int)(ball.y + ball.vy * 0.1f);
        
        MoveToEx(hdc, (int)ball.x, (int)ball.y, NULL);
        LineTo(hdc, endX, endY);
        
        DeleteObject(vectorPen);
    }
}

void DemoPhysicsBall::Shutdown() {
    demoInitialized = FALSE;
    ballCount = 0;
}

void DemoPhysicsBall::AddBall(float x, float y, float vx, float vy, float radius, COLORREF color) {
    if (ballCount < MAX_BALLS) {
        Ball& ball = balls[ballCount];
        ball.x = x;
        ball.y = y;
        ball.vx = vx;
        ball.vy = vy;
        ball.radius = radius;
        ball.color = color;
        ball.mass = radius * radius * 0.01f; // Mass proportional to area
        ballCount++;
    }
}

void DemoPhysicsBall::HandleBoundaryCollisions(Ball& ball) {
    // Left and right boundaries
    if (ball.x - ball.radius <= bounds.left) {
        ball.x = bounds.left + ball.radius;
        ball.vx = -ball.vx * bounce;
    } else if (ball.x + ball.radius >= bounds.right) {
        ball.x = bounds.right - ball.radius;
        ball.vx = -ball.vx * bounce;
    }
    
    // Top and bottom boundaries
    if (ball.y - ball.radius <= bounds.top) {
        ball.y = bounds.top + ball.radius;
        ball.vy = -ball.vy * bounce;
    } else if (ball.y + ball.radius >= bounds.bottom) {
        ball.y = bounds.bottom - ball.radius;
        ball.vy = -ball.vy * bounce;
    }
}

void DemoPhysicsBall::HandleCollisions() {
    for (int i = 0; i < ballCount; i++) {
        for (int j = i + 1; j < ballCount; j++) {
            if (CheckBallCollision(balls[i], balls[j])) {
                ResolveBallCollision(balls[i], balls[j]);
            }
        }
    }
}

bool DemoPhysicsBall::CheckBallCollision(Ball& ball1, Ball& ball2) {
    float dx = ball2.x - ball1.x;
    float dy = ball2.y - ball1.y;
    float distance = sqrt(dx * dx + dy * dy);
    return distance < (ball1.radius + ball2.radius);
}

void DemoPhysicsBall::ResolveBallCollision(Ball& ball1, Ball& ball2) {
    // Calculate collision normal
    float dx = ball2.x - ball1.x;
    float dy = ball2.y - ball1.y;
    float distance = sqrt(dx * dx + dy * dy);
    
    if (distance == 0) return; // Prevent division by zero
    
    // Normalize collision vector
    float nx = dx / distance;
    float ny = dy / distance;
    
    // Separate balls if overlapping
    float overlap = (ball1.radius + ball2.radius) - distance;
    if (overlap > 0) {
        float separation = overlap * 0.5f;
        ball1.x -= nx * separation;
        ball1.y -= ny * separation;
        ball2.x += nx * separation;
        ball2.y += ny * separation;
    }
    
    // Calculate relative velocity
    float rvx = ball2.vx - ball1.vx;
    float rvy = ball2.vy - ball1.vy;
    
    // Calculate relative velocity along collision normal
    float speed = rvx * nx + rvy * ny;
    
    // Do not resolve if velocities are separating
    if (speed > 0) return;
    
    // Calculate restitution (combined bounce)
    float restitution = bounce;
    
    // Calculate impulse scalar
    float impulse = -(1 + restitution) * speed;
    impulse /= (1.0f / ball1.mass + 1.0f / ball2.mass);
    
    // Apply impulse
    float impulsex = impulse * nx;
    float impulsey = impulse * ny;
    
    ball1.vx -= impulsex / ball1.mass;
    ball1.vy -= impulsey / ball1.mass;
    ball2.vx += impulsex / ball2.mass;
    ball2.vy += impulsey / ball2.mass;
}