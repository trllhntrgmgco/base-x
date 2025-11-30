#pragma once
#include "DemoBase.h"
#include "physics.h"

using namespace BaseXEngine;

struct EnhancedPhysicsObject {
    BaseXVector position;         // Position using BaseXVector
    BaseXVector velocity;         // Velocity using BaseXVector
    BaseXVector size;            // Width, height, depth using BaseXVector
    BaseXMatrix transform;       // Transformation matrix for rotation/scaling
    COLORREF color;              // Color
    bool isStatic;               // Static objects don't move
    int id;                      // Unique identifier
    double mass;                 // Mass for enhanced physics
    double rotation;             // Current rotation angle
    double angularVelocity;      // Rotational speed
    BaseXVector forces;          // Accumulated forces
    bool isSelected;             // Selection state for user interaction
    bool isDragging;             // Dragging state
};

class DemoPhysicsCollision : public DemoBase {
private:
    static const int MAX_OBJECTS = 15;
    EnhancedPhysicsObject objects[MAX_OBJECTS];
    int objectCount;
    BaseXVector gravity;         // Gravity as a vector
    double bounce;
    double friction;
    RECT bounds;
    BOOL demoInitialized;
    int lastCollisionId1, lastCollisionId2;
    float collisionHighlightTime;
    
    // Enhanced physics parameters
    double airResistance;
    bool enableQuantumEffects;
    double quantumPhaseTime;
    
    // User interaction state
    int selectedObjectId;
    bool isDragging;
    BaseXVector lastMousePos;
    BaseXVector dragOffset;
    bool showHelp;
    
    // Input state tracking
    bool keyStates[256];
    HWND parentWindow;

public:
    void Init(HWND hWnd) override;
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;
    void Shutdown() override;
    
    // Input handling methods
    void HandleMouseDown(int x, int y, bool leftButton = true);
    void HandleMouseUp(int x, int y, bool leftButton = true);
    void HandleMouseMove(int x, int y);
    void HandleKeyDown(int keyCode);
    void HandleKeyUp(int keyCode);
    
    // Public methods for settings
    void SetPhysicsSettings(float gravity, float bounce, float friction);
    void GetPhysicsSettings(float& gravity, float& bounce, float& friction);
    void SetQuantumEffects(bool enabled);

private:
    void AddObject(const BaseXVector& pos, const BaseXVector& vel, const BaseXVector& size, 
                   COLORREF color, bool isStatic = false, double mass = 1.0);
    bool CheckCollision(const EnhancedPhysicsObject& obj1, const EnhancedPhysicsObject& obj2);
    void ResolveCollision(EnhancedPhysicsObject& obj1, EnhancedPhysicsObject& obj2);
    void HandleBoundaryCollisions(EnhancedPhysicsObject& obj);
    void DrawObject(HDC hdc, const EnhancedPhysicsObject& obj, bool highlight = false);
    void ApplyForces(EnhancedPhysicsObject& obj, float deltaTime);
    void UpdateTransform(EnhancedPhysicsObject& obj);
    
    // BaseX-specific physics
    void ApplyQuantumEffects(EnhancedPhysicsObject& obj, float deltaTime);
    BaseXVector CalculateCollisionImpulse(const EnhancedPhysicsObject& obj1, 
                                          const EnhancedPhysicsObject& obj2,
                                          const BaseXVector& contactPoint);
    
    // User interaction helpers
    int FindObjectAtPosition(const BaseXVector& pos);
    void SelectObject(int objectId);
    void DeselectAll();
    void HandleKeyboardInput(float deltaTime);
    void SpawnObjectAtMouse();
    void DeleteSelectedObject();
    void DrawUserInterface(HDC hdc);
    void DrawHelp(HDC hdc);
    bool IsPointInObject(const BaseXVector& point, const EnhancedPhysicsObject& obj);
    BaseXVector ScreenToWorld(int screenX, int screenY);
};