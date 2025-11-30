#include <windows.h>
#include <mmsystem.h>
#include <math.h>
#include "DemoPhysicsGravity.h"

#pragma comment(lib, "winmm.lib")

void DemoPhysicsGravity::Init(HWND hWnd) {
    demoInitialized = TRUE;
    particleCount = 0;
    centralMass = 50000.0f;
    spawnTimer = 0.0f;
    
    // Get window bounds
    GetClientRect(hWnd, &bounds);
    bounds.top = 130;    // Account for controls at top
    
    // Place central attractor in center
    centralX = (bounds.left + bounds.right) * 0.5f;
    centralY = (bounds.top + bounds.bottom) * 0.5f;
    
    // Initialize all particles as inactive
    for (int i = 0; i < MAX_GRAVITY_PARTICLES; i++) {
        particles[i].active = false;
    }
}

void DemoPhysicsGravity::Update(float deltaTime) {
    if (!demoInitialized) return;
    
    float dt = deltaTime / 1000.0f; // Convert to seconds
    
    // Spawn new particles periodically
    spawnTimer += deltaTime;
    if (spawnTimer >= 200.0f) { // Spawn every 200ms
        SpawnParticle();
        spawnTimer = 0.0f;
    }
    
    // Update all active particles
    for (int i = 0; i < MAX_GRAVITY_PARTICLES; i++) {
        if (particles[i].active) {
            UpdateParticle(particles[i], dt);
        }
    }
}

void DemoPhysicsGravity::Render(HDC hdc) {
    if (!demoInitialized) return;
    
    // Draw demo info
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(220, 220, 220));
    TextOut(hdc, bounds.left + 10, bounds.top + 10, L"Physics Demo - Gravity Simulation", 33);
    
    SetTextColor(hdc, RGB(180, 180, 180));
    TextOut(hdc, bounds.left + 10, bounds.top + 30, L"Particles attracted to central mass", 35);
    
    // Draw central attractor
    HBRUSH centralBrush = CreateSolidBrush(RGB(255, 255, 100));
    HPEN centralPen = CreatePen(PS_SOLID, 3, RGB(255, 255, 255));
    
    SelectObject(hdc, centralBrush);
    SelectObject(hdc, centralPen);
    
    int radius = 15;
    Ellipse(hdc, (int)(centralX - radius), (int)(centralY - radius),
                 (int)(centralX + radius), (int)(centralY + radius));
    
    DeleteObject(centralBrush);
    DeleteObject(centralPen);
    
    // Draw particles
    for (int i = 0; i < MAX_GRAVITY_PARTICLES; i++) {
        if (particles[i].active) {
            GravityParticle& p = particles[i];  // Fixed: use GravityParticle
            
            HBRUSH brush = CreateSolidBrush(p.color);
            SelectObject(hdc, brush);
            
            int size = 3;
            Ellipse(hdc, (int)(p.x - size), (int)(p.y - size),
                         (int)(p.x + size), (int)(p.y + size));
            
            DeleteObject(brush);
            
            // Draw trail line to show movement
            HPEN trailPen = CreatePen(PS_SOLID, 1, p.color);
            SelectObject(hdc, trailPen);
            
            int trailX = (int)(p.x - p.vx * 0.05f);
            int trailY = (int)(p.y - p.vy * 0.05f);
            
            MoveToEx(hdc, (int)p.x, (int)p.y, NULL);
            LineTo(hdc, trailX, trailY);
            
            DeleteObject(trailPen);
        }
    }
    
    // Display particle count
    WCHAR countText[64];
    wsprintf(countText, L"Active Particles: %d", particleCount);
    SetTextColor(hdc, RGB(160, 160, 160));
    TextOut(hdc, bounds.left + 10, bounds.top + 50, countText, lstrlen(countText));
}

void DemoPhysicsGravity::Shutdown() {
    demoInitialized = FALSE;
    particleCount = 0;
}

void DemoPhysicsGravity::SpawnParticle() {
    // Find an inactive particle slot
    for (int i = 0; i < MAX_GRAVITY_PARTICLES; i++) {
        if (!particles[i].active) {
            GravityParticle& p = particles[i];  // Fixed: use GravityParticle
            
            // Spawn from random edge of screen
            int edge = rand() % 4;
            switch (edge) {
                case 0: // Top
                    p.x = (float)(bounds.left + rand() % (bounds.right - bounds.left));
                    p.y = (float)bounds.top;
                    break;
                case 1: // Right
                    p.x = (float)bounds.right;
                    p.y = (float)(bounds.top + rand() % (bounds.bottom - bounds.top));
                    break;
                case 2: // Bottom
                    p.x = (float)(bounds.left + rand() % (bounds.right - bounds.left));
                    p.y = (float)bounds.bottom;
                    break;
                case 3: // Left
                    p.x = (float)bounds.left;
                    p.y = (float)(bounds.top + rand() % (bounds.bottom - bounds.top));
                    break;
            }
            
            // Give initial velocity towards center with some randomness
            float dx = centralX - p.x;
            float dy = centralY - p.y;
            float distance = sqrt(dx * dx + dy * dy);
            
            if (distance > 0) {
                float speed = 50.0f + (rand() % 100); // Random speed 50-150
                p.vx = (dx / distance) * speed;
                p.vy = (dy / distance) * speed;
                
                // Add some perpendicular velocity for orbital motion
                float perpVx = -dy / distance * (rand() % 50);
                float perpVy = dx / distance * (rand() % 50);
                p.vx += perpVx;
                p.vy += perpVy;
            } else {
                p.vx = p.vy = 0;
            }
            
            p.mass = 1.0f;
            p.life = 1.0f;
            p.color = GetParticleColor(p.life);
            p.active = true;
            
            particleCount++;
            break;
        }
    }
}

void DemoPhysicsGravity::UpdateParticle(GravityParticle& p, float deltaTime) {  // Fixed: use GravityParticle
    // Calculate gravitational force
    float dx = centralX - p.x;
    float dy = centralY - p.y;
    float distanceSquared = dx * dx + dy * dy;
    float distance = sqrt(distanceSquared);
    
    if (distance > 1.0f) { // Avoid division by zero
        // F = G * m1 * m2 / r^2, simplified as we don't need real physics constants
        float force = centralMass * p.mass / distanceSquared;
        
        // Calculate acceleration (F = ma, so a = F/m)
        float ax = (dx / distance) * force / p.mass;
        float ay = (dy / distance) * force / p.mass;
        
        // Update velocity
        p.vx += ax * deltaTime;
        p.vy += ay * deltaTime;
    }
    
    // Update position
    p.x += p.vx * deltaTime;
    p.y += p.vy * deltaTime;
    
    // Update life and color
    p.life -= deltaTime * 0.2f; // Particles live for 5 seconds
    p.color = GetParticleColor(p.life);
    
    // Check if particle should be destroyed
    if (p.life <= 0 || distance < 20.0f || // Too close to center
        p.x < bounds.left - 50 || p.x > bounds.right + 50 ||
        p.y < bounds.top - 50 || p.y > bounds.bottom + 50) { // Out of bounds
        p.active = false;
        particleCount--;
    }
}

COLORREF DemoPhysicsGravity::GetParticleColor(float life) {
    // Color changes from white to red to black as life decreases
    if (life > 0.8f) {
        // White to yellow
        int intensity = (int)(255 * life);
        return RGB(255, 255, intensity);
    } else if (life > 0.4f) {
        // Yellow to red
        int green = (int)(255 * (life - 0.4f) / 0.4f);
        return RGB(255, green, 0);
    } else if (life > 0.2f) {
        // Red to dark red
        int intensity = (int)(255 * (life - 0.2f) / 0.2f);
        return RGB(intensity, 0, 0);
    } else {
        // Dark red to black
        int intensity = (int)(128 * life / 0.2f);
        return RGB(intensity, 0, 0);
    }
}