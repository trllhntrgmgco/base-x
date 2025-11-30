#pragma once

// Undefine Windows min/max macros to prevent conflicts with std::min/std::max
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include "DemoBase.h"
#include <vector>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Demo constants - move outside class to avoid static member issues
#define MAX_OBJECTS_LAYER3 50
#define MAX_SPRINGS_LAYER3 50
#define MAX_DISTANCE_CONSTRAINTS_LAYER3 50
#define MAX_ANGLE_CONSTRAINTS_LAYER3 20

// Forward declarations for physics objects
struct MaterialProperties;
struct AdvancedPhysicsObject;
struct SpringConstraint;
struct DistanceConstraint;
struct AngleConstraint;
struct CollisionPair;

// Material Properties for different object types
struct MaterialProperties {
    double density;         // kg/m³
    double restitution;     // Bounce factor (0-1)
    double staticFriction;  // Static friction coefficient
    double dynamicFriction; // Dynamic friction coefficient
    double damping;         // Velocity damping factor
    COLORREF color;         // Visual color

    MaterialProperties() : density(1000.0), restitution(0.5), staticFriction(0.5), 
                          dynamicFriction(0.3), damping(0.99), color(RGB(128, 128, 128)) {}
    
    MaterialProperties(double d, double r, double sf, double df, double dam, COLORREF c) 
        : density(d), restitution(r), staticFriction(sf), dynamicFriction(df), damping(dam), color(c) {}
};

// Advanced physics object with material properties
struct AdvancedPhysicsObject {
    // Position and dimensions
    double posX, posY;
    double width, height;
    
    // Motion properties
    double velX, velY;
    double angularVelocity;
    double rotation;
    
    // Forces and torques
    double forceX, forceY;
    double torque;
    
    // Mass properties
    double mass;
    double momentOfInertia;
    
    // Material properties
    MaterialProperties material;
    
    // Object state
    bool isStatic;
    bool isDragging;
    bool isSelected;
    int id;
    bool isVisible;
    
    AdvancedPhysicsObject() {
        posX = posY = 0.0;
        width = height = 30.0;
        velX = velY = 0.0;
        angularVelocity = rotation = 0.0;
        forceX = forceY = torque = 0.0;
        mass = 1.0;
        momentOfInertia = 1.0;
        isStatic = false;
        isDragging = false;
        isSelected = false;
        id = -1;
        isVisible = true;
        UpdateMomentOfInertia();
    }
    
    void UpdateMomentOfInertia() {
        // For rectangular objects: I = m * (w² + h²) / 12
        momentOfInertia = mass * (width * width + height * height) / 12.0;
    }
};

// Spring constraint between two objects
struct SpringConstraint {
    int objectA, objectB;
    double restLength;
    double stiffness;
    double damping;
    COLORREF color;
    bool isVisible;
    
    SpringConstraint() : objectA(-1), objectB(-1), restLength(100.0), stiffness(50.0), 
                        damping(0.1), color(RGB(255, 255, 0)), isVisible(true) {}
    
    SpringConstraint(int a, int b, double length, double k, double d) 
        : objectA(a), objectB(b), restLength(length), stiffness(k), damping(d), 
          color(RGB(255, 255, 0)), isVisible(true) {}
};

// Distance constraint (rigid connection)
struct DistanceConstraint {
    int objectA, objectB;
    double distance;
    double stiffness;
    COLORREF color;
    bool isVisible;
    
    DistanceConstraint() : objectA(-1), objectB(-1), distance(100.0), stiffness(1.0), 
                          color(RGB(0, 255, 255)), isVisible(true) {}
    
    DistanceConstraint(int a, int b, double d, double s) 
        : objectA(a), objectB(b), distance(d), stiffness(s), 
          color(RGB(0, 255, 255)), isVisible(true) {}
};

// Angle constraint between three objects
struct AngleConstraint {
    int objectA, objectB, objectC; // B is the center object
    double targetAngle;
    double stiffness;
    COLORREF color;
    bool isVisible;
    
    AngleConstraint() : objectA(-1), objectB(-1), objectC(-1), targetAngle(0.0), 
                       stiffness(0.1), color(RGB(255, 128, 0)), isVisible(true) {}
    
    AngleConstraint(int a, int b, int c, double angle, double s) 
        : objectA(a), objectB(b), objectC(c), targetAngle(angle), stiffness(s), 
          color(RGB(255, 128, 0)), isVisible(true) {}
};

// Collision information
struct CollisionPair {
    int objectA, objectB;
    double normalX, normalY;
    double penetration;
    double contactPointX, contactPointY;
    
    CollisionPair() : objectA(-1), objectB(-1), normalX(0.0), normalY(0.0), 
                     penetration(0.0), contactPointX(0.0), contactPointY(0.0) {}
};

// Demo modes
enum DemoMode {
    MODE_SANDBOX = 0,
    MODE_SPRING_DEMO,
    MODE_BRIDGE_DEMO,
    MODE_PENDULUM_DEMO,
    MODE_CHAIN_DEMO
};

// Main Physics Layer 3 Demo Class - Advanced Constraint-Based Physics
class DemoPhysicsLayer3 : public DemoBase {
private:
    // Physics objects and constraints using the new defines
    AdvancedPhysicsObject objects[MAX_OBJECTS_LAYER3];
    SpringConstraint springs[MAX_SPRINGS_LAYER3];
    DistanceConstraint distanceConstraints[MAX_DISTANCE_CONSTRAINTS_LAYER3];
    AngleConstraint angleConstraints[MAX_ANGLE_CONSTRAINTS_LAYER3];
    
    int objectCount;
    int springCount;
    int distanceConstraintCount;
    int angleConstraintCount;
    
    // Collision detection
    std::vector<CollisionPair> collisions;
    
    // Simulation parameters
    double gravityY;
    double airResistance;
    double timeStep;
    int constraintIterations;
    
    // Material presets
    MaterialProperties materialWood;
    MaterialProperties materialMetal;
    MaterialProperties materialRubber;
    MaterialProperties materialIce;
    
    // Demo state
    DemoMode currentMode;
    int selectedObjectId;
    bool isDragging;
    double dragOffsetX, dragOffsetY;
    int lastMouseX, lastMouseY;
    
    // UI state
    bool showHelp;
    bool showConstraints;
    bool showVelocityVectors;
    bool showForces;
    bool keyStates[256];
    
    // Demo bounds
    RECT bounds;
    BOOL demoInitialized;
    
public:
    DemoPhysicsLayer3();
    virtual ~DemoPhysicsLayer3();
    
    // DemoBase interface
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
    
private:
    // Initialization
    void InitializeMaterials();
    MaterialProperties GetRandomMaterial();
    
    // Demo setup methods
    void SetupSandboxMode();
    void SetupSpringDemo();
    void SetupBridgeDemo();
    void SetupPendulumDemo();
    void SetupChainDemo();
    
    // Physics simulation
    void ApplyForces(AdvancedPhysicsObject& obj, float deltaTime);
    void ApplySpringForces(float deltaTime);
    void SolveConstraints(float deltaTime);
    void SolveDistanceConstraints(float deltaTime);
    void SolveAngleConstraints(float deltaTime);
    void HandleCollisions(float deltaTime);
    void IntegrateMotion(float deltaTime);
    void HandleBoundaryCollisions(AdvancedPhysicsObject& obj);
    
    // Collision detection and resolution
    bool CheckCollision(const AdvancedPhysicsObject& obj1, const AdvancedPhysicsObject& obj2, CollisionPair& collision);
    void ResolveCollision(AdvancedPhysicsObject& obj1, AdvancedPhysicsObject& obj2, const CollisionPair& collision);
    
    // Object management
    int AddObject(double x, double y, double w, double h, const MaterialProperties& material, bool isStatic = false);
    void RemoveObject(int index);
    int FindObjectAtPosition(double x, double y);
    
    // Constraint management
    int AddSpring(int objA, int objB, double restLength, double stiffness, double damping);
    int AddDistanceConstraint(int objA, int objB, double distance, double stiffness);
    int AddAngleConstraint(int objA, int objB, int objC, double angle, double stiffness);
    void RemoveConstraintsForObject(int objectId);
    
    // Rendering
    void DrawObject(HDC hdc, const AdvancedPhysicsObject& obj);
    void DrawSpring(HDC hdc, const SpringConstraint& spring);
    void DrawDistanceConstraint(HDC hdc, const DistanceConstraint& constraint);
    void DrawAngleConstraint(HDC hdc, const AngleConstraint& constraint);
    void DrawVelocityVector(HDC hdc, const AdvancedPhysicsObject& obj);
    void DrawForceVector(HDC hdc, const AdvancedPhysicsObject& obj);
    void DrawUserInterface(HDC hdc);
    void DrawHelp(HDC hdc);
    
    // User input handling
    void HandleKeyboardInput(float deltaTime);
    void SelectPhysicsObject(int objectId);
    void DeselectAll();
    void SpawnObjectAtMouse();
    void DeleteSelectedObject();
    void ConnectSelectedObjects();
    void CycleDemoMode();
    
    // Utility functions
    double Distance(double x1, double y1, double x2, double y2);
    double Magnitude(double x, double y);
    void Normalize(double& x, double& y);
    double ClampValue(double value, double minVal, double maxVal);
    double CalculateAngle(double x1, double y1, double x2, double y2, double x3, double y3);
};