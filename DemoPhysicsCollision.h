#pragma once
#define NOMINMAX
#include "DemoBase.h"
#include "physics.h"
#include <windows.h>
#include <vector>
#include <cmath>
#include <algorithm>

// Use the BaseXEngine namespace
using namespace BaseXEngine;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_OBJECTS 50
#define MAX_TRAIL_POINTS 500
#define MAX_COLLISION_PARTICLES 100

// Enhanced Trail System
struct TrailPoint {
    BaseXVector position;
    COLORREF color;
    float life;
};

// Enhanced Collision Particle System
struct CollisionParticle {
    BaseXVector position;
    BaseXVector velocity;
    float life;
    float maxLife;
    int size;
    COLORREF color;
};

// Enhanced Physics Object with BaseX integration
struct EnhancedPhysicsObject {
    BaseXVector position;
    BaseXVector velocity;
    BaseXVector size;
    BaseXVector forces;
    COLORREF color;
    bool isStatic;
    bool isSelected;
    bool isDragging;
    int id;
    double mass;
    double rotation;
    double angularVelocity;
    BaseXMatrix transform;
    
    // Enhanced visual properties
    float glowIntensity;
    bool hasTrail;
};

// Main Enhanced Physics Demo Class
class DemoPhysicsCollision : public DemoBase {
private:
    EnhancedPhysicsObject objects[MAX_OBJECTS];
    int objectCount;
    
    // Enhanced visual effects
    TrailPoint trails[MAX_TRAIL_POINTS];
    int trailCount;
    int trailIndex;
    
    CollisionParticle collisionParticles[MAX_COLLISION_PARTICLES];
    int collisionParticleCount;
    
    // Physics world properties
    BaseXVector gravity;
    double bounce;
    double friction;
    double airResistance;
    RECT bounds;
    
    // Enhanced quantum effects
    bool enableQuantumEffects;
    double quantumPhaseTime;
    
    // Interaction state
    int selectedObjectId;
    bool isDragging;
    BaseXVector lastMousePos;
    BaseXVector dragOffset;
    bool showHelp;
    
    // Input handling
    bool keyStates[256];
    HWND parentWindow;
    BOOL demoInitialized;
    
    // Collision highlighting with enhanced effects
    int lastCollisionId1, lastCollisionId2;
    float collisionHighlightTime;

public:
    void Init(HWND hWnd) override;
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;
    void Shutdown() override;
    
    // Input handling
    void HandleMouseDown(int x, int y, bool leftButton = true);
    void HandleMouseUp(int x, int y, bool leftButton = true);
    void HandleMouseMove(int x, int y);
    void HandleKeyDown(int keyCode);
    void HandleKeyUp(int keyCode);
    
    // Configuration
    void SetPhysicsSettings(float gravity, float bounce, float friction);
    void GetPhysicsSettings(float& gravity, float& bounce, float& friction);
    void SetQuantumEffects(bool enabled);

private:
    // Object management
    void AddObject(const BaseXVector& position, const BaseXVector& velocity, 
                   const BaseXVector& size, COLORREF color, bool isStatic = false, double mass = 1.0);
    int FindObjectAtPosition(const BaseXVector& pos);
    bool IsPointInObject(const BaseXVector& point, const EnhancedPhysicsObject& obj);
    void SelectPhysicsObject(int objectId);
    void DeselectAll();
    void SpawnObjectAtMouse();
    void DeleteSelectedObject();
    
    // Enhanced physics simulation
    bool CheckCollision(const EnhancedPhysicsObject& obj1, const EnhancedPhysicsObject& obj2);
    void ResolveCollision(EnhancedPhysicsObject& obj1, EnhancedPhysicsObject& obj2);
    void HandleBoundaryCollisions(EnhancedPhysicsObject& obj);
    void ApplyForces(EnhancedPhysicsObject& obj, float deltaTime);
    void ApplyQuantumEffects(EnhancedPhysicsObject& obj, float deltaTime);
    void UpdateTransform(EnhancedPhysicsObject& obj);
    
    // Enhanced visual effects
    void AddTrail(const BaseXVector& position, COLORREF color);
    void CreateCollisionEffect(const BaseXVector& pos1, const BaseXVector& pos2);
    void UpdateCollisionParticles(float deltaTime);
    
    // User interaction
    void HandleKeyboardInput(float deltaTime);
    BaseXVector ScreenToWorld(int screenX, int screenY);
    
    // Enhanced rendering
    void DrawEnhancedObject(HDC hdc, const EnhancedPhysicsObject& obj, bool highlight = false);
    void DrawTrails(HDC hdc);
    void DrawCollisionParticles(HDC hdc);
    void DrawEnhancedUserInterface(HDC hdc);
    void DrawEnhancedHelp(HDC hdc);
    
    // Legacy method for compatibility (renamed to avoid conflict)
    void DrawObject(HDC hdc, const EnhancedPhysicsObject& obj, bool highlight = false) {
        DrawEnhancedObject(hdc, obj, highlight);
    }
    void DrawUserInterface(HDC hdc) {
        DrawEnhancedUserInterface(hdc);
    }
    void DrawHelp(HDC hdc) {
        DrawEnhancedHelp(hdc);
    }

    // Utility
    bool IsValidObjectId(int id) const { return id >= 0 && id < objectCount; }
};