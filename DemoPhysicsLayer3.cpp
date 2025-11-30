#include <windows.h>

// Undefine Windows min/max macros to prevent conflicts with std::min/std::max
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include "DemoPhysicsLayer3.h"
#include <cmath>
#include <algorithm>
#include <vector>

// Declare external global variables from main application
extern float globalGravity;
extern float globalBounce;
extern float globalFriction;

DemoPhysicsLayer3::DemoPhysicsLayer3() {
    objectCount = 0;
    springCount = 0;
    distanceConstraintCount = 0;
    angleConstraintCount = 0;
    selectedObjectId = -1;
    isDragging = false;
    showHelp = true;
    showConstraints = true;
    showVelocityVectors = false;
    showForces = false;
    currentMode = MODE_SANDBOX;
    demoInitialized = FALSE;
    gravityY = 500.0;
    airResistance = 0.01;
    timeStep = 1.0 / 60.0;
    constraintIterations = 3;
    
    memset(objects, 0, sizeof(objects));
    memset(springs, 0, sizeof(springs));
    memset(distanceConstraints, 0, sizeof(distanceConstraints));
    memset(angleConstraints, 0, sizeof(angleConstraints));
    memset(keyStates, 0, sizeof(keyStates));
    
    InitializeMaterials();
}

DemoPhysicsLayer3::~DemoPhysicsLayer3() {
    Shutdown();
}

void DemoPhysicsLayer3::InitializeMaterials() {
    // Wood: Medium density, good restitution, moderate friction
    materialWood = MaterialProperties(600.0, 0.6, 0.7, 0.5, 0.9, RGB(139, 69, 19));
    
    // Metal: High density, high restitution, low friction
    materialMetal = MaterialProperties(2700.0, 0.9, 0.3, 0.2, 0.95, RGB(192, 192, 192));
    
    // Rubber: Low density, very high restitution, high friction
    materialRubber = MaterialProperties(900.0, 0.95, 0.9, 0.8, 0.8, RGB(75, 0, 130));
    
    // Ice: Medium density, high restitution, very low friction
    materialIce = MaterialProperties(917.0, 0.85, 0.1, 0.05, 0.98, RGB(173, 216, 230));
}

MaterialProperties DemoPhysicsLayer3::GetRandomMaterial() {
    int choice = rand() % 4;
    switch (choice) {
        case 0: return materialWood;
        case 1: return materialMetal;
        case 2: return materialRubber;
        case 3: return materialIce;
        default: return materialWood;
    }
}

void DemoPhysicsLayer3::Init(HWND hWnd) {
    if (!hWnd || !IsWindow(hWnd)) return;
    
    GetClientRect(hWnd, &bounds);
    bounds.top = 130;
    bounds.left = 10;
    bounds.right -= 10;
    bounds.bottom -= 10;
    
    // Validate bounds
    if (bounds.right <= bounds.left || bounds.bottom <= bounds.top) {
        bounds = {10, 130, 800, 600};
    }
    
    gravityY = globalGravity;
    demoInitialized = TRUE;
    
    // Setup initial demo based on current mode
    SetupSandboxMode();
}

void DemoPhysicsLayer3::SetupSandboxMode() {
    // Clear everything
    objectCount = 0;
    springCount = 0;
    distanceConstraintCount = 0;
    angleConstraintCount = 0;
    collisions.clear();
    
    // Create a few basic objects for experimentation
    int ground = AddObject(100, bounds.bottom - 30, 600, 20, materialWood, true);
    int box1 = AddObject(200, 200, 40, 40, materialWood);
    int box2 = AddObject(300, 150, 30, 30, materialMetal);
    int ball = AddObject(400, 100, 25, 25, materialRubber);
    
    // Add a spring between two boxes
    if (objectCount >= 3) {
        AddSpring(1, 2, 100, 50, 0.3);
    }
}

void DemoPhysicsLayer3::SetupSpringDemo() {
    objectCount = 0;
    springCount = 0;
    distanceConstraintCount = 0;
    angleConstraintCount = 0;
    collisions.clear();
    
    // Create a spring system demonstration
    int anchor = AddObject(400, 150, 20, 20, materialMetal, true);
    int mass1 = AddObject(400, 250, 30, 30, materialWood);
    int mass2 = AddObject(400, 350, 25, 25, materialRubber);
    
    // Connect with springs
    AddSpring(0, 1, 100, 80, 0.2); // Anchor to mass1
    AddSpring(1, 2, 100, 60, 0.3); // Mass1 to mass2
    
    // Add side masses with springs
    int side1 = AddObject(300, 300, 20, 20, materialMetal);
    int side2 = AddObject(500, 300, 20, 20, materialMetal);
    AddSpring(1, 3, 120, 40, 0.4); // Connect to mass1
    AddSpring(1, 4, 120, 40, 0.4);
}

void DemoPhysicsLayer3::SetupBridgeDemo() {
    objectCount = 0;
    springCount = 0;
    distanceConstraintCount = 0;
    angleConstraintCount = 0;
    collisions.clear();
    
    // Create a suspension bridge
    double bridgeY = 300;
    double segmentWidth = 60;
    int numSegments = 8;
    
    // Create bridge segments
    std::vector<int> segments;
    for (int i = 0; i < numSegments; i++) {
        double x = 100 + i * segmentWidth;
        int segment = AddObject(x, bridgeY, segmentWidth - 5, 15, materialWood);
        segments.push_back(segment);
    }
    
    // Create anchor points
    int leftAnchor = AddObject(50, bridgeY - 100, 20, 20, materialMetal, true);
    int rightAnchor = AddObject(100 + numSegments * segmentWidth, bridgeY - 100, 20, 20, materialMetal, true);
    
    // Connect segments with distance constraints
    for (int i = 0; i < numSegments - 1; i++) {
        AddDistanceConstraint(segments[i], segments[i + 1], segmentWidth, 0.9);
    }
    
    // Add suspension cables
    AddSpring(leftAnchor, segments[0], 150, 30, 0.1);
    AddSpring(leftAnchor, segments[2], 200, 25, 0.1);
    AddSpring(rightAnchor, segments[numSegments - 1], 150, 30, 0.1);
    AddSpring(rightAnchor, segments[numSegments - 3], 200, 25, 0.1);
    
    // Add a test mass on the bridge
    AddObject(300, bridgeY - 50, 30, 30, materialMetal);
}

void DemoPhysicsLayer3::SetupPendulumDemo() {
    objectCount = 0;
    springCount = 0;
    distanceConstraintCount = 0;
    angleConstraintCount = 0;
    collisions.clear();
    
    // Create multiple pendulums of different lengths
    int anchor = AddObject(400, 100, 15, 15, materialMetal, true);
    
    // Create pendulum bobs at different positions
    int bob1 = AddObject(300, 300, 25, 25, materialWood);
    int bob2 = AddObject(400, 350, 20, 20, materialMetal);
    int bob3 = AddObject(500, 250, 30, 30, materialRubber);
    
    // Connect with distance constraints (rigid pendulum rods)
    AddDistanceConstraint(0, 1, 224, 1.0); // sqrt((300-400)^2 + (300-100)^2)
    AddDistanceConstraint(0, 2, 250, 1.0);
    AddDistanceConstraint(0, 3, 180, 1.0);
    
    // Give them initial velocities for interesting motion
    objects[1].velX = 100;
    objects[2].velX = -80;
    objects[3].velX = 120;
}

void DemoPhysicsLayer3::SetupChainDemo() {
    objectCount = 0;
    springCount = 0;
    distanceConstraintCount = 0;
    angleConstraintCount = 0;
    collisions.clear();
    
    // Create a chain hanging from an anchor
    int anchor = AddObject(400, 100, 15, 15, materialMetal, true);
    
    double linkSize = 20;
    double chainLength = 25;
    std::vector<int> links;
    links.push_back(anchor);
    
    // Create chain links
    for (int i = 1; i <= 10; i++) {
        int link = AddObject(400, 100 + i * chainLength, linkSize, linkSize * 0.6, materialMetal);
        links.push_back(link);
        
        // Connect to previous link with distance constraint
        AddDistanceConstraint(links[i-1], links[i], chainLength, 0.95);
    }
    
    // Add a heavy weight at the end
    int weight = AddObject(400, 100 + 11 * chainLength, 40, 40, materialMetal);
    AddDistanceConstraint(links.back(), weight, chainLength, 0.95);
    
    // Give the chain some initial motion
    for (int i = 1; i < links.size(); i++) {
        objects[links[i]].velX = 50 * sin(i * 0.5);
    }
}

void DemoPhysicsLayer3::Update(float deltaTime) {
    if (!demoInitialized) return;
    
    // Clamp deltaTime to prevent instability
    deltaTime = ClampValue(deltaTime, 0.0f, 33.0f); // Max 33ms
    float physicsTimeStep = deltaTime / 1000.0f; // Convert to seconds
    
    // Update physics settings from globals
    gravityY = globalGravity;
    
    // Handle user input
    HandleKeyboardInput(physicsTimeStep);
    
    // Clear collision list for this frame
    collisions.clear();
    
    // Apply forces to all objects
    for (int i = 0; i < objectCount; i++) {
        if (!objects[i].isStatic && !objects[i].isDragging) {
            ApplyForces(objects[i], physicsTimeStep);
        }
    }
    
    // Apply spring forces
    ApplySpringForces(physicsTimeStep);
    
    // Solve constraints multiple times for stability
    for (int iter = 0; iter < constraintIterations; iter++) {
        SolveConstraints(physicsTimeStep);
    }
    
    // Handle collisions
    HandleCollisions(physicsTimeStep);
    
    // Integrate motion for all non-static objects
    for (int i = 0; i < objectCount; i++) {
        if (!objects[i].isStatic && !objects[i].isDragging) {
            IntegrateMotion(physicsTimeStep);
        }
    }
    
    // Handle boundary collisions
    for (int i = 0; i < objectCount; i++) {
        if (!objects[i].isStatic) {
            HandleBoundaryCollisions(objects[i]);
        }
    }
}

void DemoPhysicsLayer3::ApplyForces(AdvancedPhysicsObject& obj, float deltaTime) {
    // Apply gravity
    obj.forceY += gravityY * obj.mass;
    
    // Apply air resistance
    double velMag = Magnitude(obj.velX, obj.velY);
    if (velMag > 0.1) {
        double dragForce = airResistance * velMag * velMag;
        obj.forceX -= (obj.velX / velMag) * dragForce;
        obj.forceY -= (obj.velY / velMag) * dragForce;
    }
    
    // Apply material damping
    obj.velX *= obj.material.damping;
    obj.velY *= obj.material.damping;
    obj.angularVelocity *= obj.material.damping;
}

void DemoPhysicsLayer3::ApplySpringForces(float deltaTime) {
    for (int i = 0; i < springCount; i++) {
        SpringConstraint& spring = springs[i];
        
        if (spring.objectA >= objectCount || spring.objectB >= objectCount) continue;
        
        AdvancedPhysicsObject& objA = objects[spring.objectA];
        AdvancedPhysicsObject& objB = objects[spring.objectB];
        
        // Calculate spring vector
        double dx = objB.posX - objA.posX;
        double dy = objB.posY - objA.posY;
        double currentLength = Distance(objA.posX, objA.posY, objB.posX, objB.posY);
        
        if (currentLength < 0.001) continue; // Avoid division by zero
        
        // Spring force calculation: F = -k * (current_length - rest_length)
        double displacement = currentLength - spring.restLength;
        double springForce = spring.stiffness * displacement;
        
        // Normalize direction
        double dirX = dx / currentLength;
        double dirY = dy / currentLength;
        
        // Calculate relative velocity for damping
        double relVelX = objB.velX - objA.velX;
        double relVelY = objB.velY - objA.velY;
        double dampingForce = spring.damping * (relVelX * dirX + relVelY * dirY);
        
        // Total force
        double totalForce = springForce + dampingForce;
        double forceX = totalForce * dirX;
        double forceY = totalForce * dirY;
        
        // Apply forces (Newton's third law)
        if (!objA.isStatic && !objA.isDragging) {
            objA.forceX += forceX;
            objA.forceY += forceY;
        }
        if (!objB.isStatic && !objB.isDragging) {
            objB.forceX -= forceX;
            objB.forceY -= forceY;
        }
    }
}

void DemoPhysicsLayer3::SolveConstraints(float deltaTime) {
    SolveDistanceConstraints(deltaTime);
    SolveAngleConstraints(deltaTime);
}

void DemoPhysicsLayer3::SolveDistanceConstraints(float deltaTime) {
    for (int i = 0; i < distanceConstraintCount; i++) {
        DistanceConstraint& constraint = distanceConstraints[i];
        
        if (constraint.objectA >= objectCount || constraint.objectB >= objectCount) continue;
        
        AdvancedPhysicsObject& objA = objects[constraint.objectA];
        AdvancedPhysicsObject& objB = objects[constraint.objectB];
        
        // Calculate current distance
        double dx = objB.posX - objA.posX;
        double dy = objB.posY - objA.posY;
        double currentDistance = Distance(objA.posX, objA.posY, objB.posX, objB.posY);
        
        if (currentDistance < 0.001) continue;
        
        // Calculate constraint error
        double error = currentDistance - constraint.distance;
        
        // Calculate correction factor
        double correction = error * constraint.stiffness * 0.5; // Split correction between both objects
        
        // Normalize direction
        double dirX = dx / currentDistance;
        double dirY = dy / currentDistance;
        
        // Apply position corrections
        double correctionX = correction * dirX;
        double correctionY = correction * dirY;
        
        if (!objA.isStatic && !objA.isDragging) {
            objA.posX += correctionX;
            objA.posY += correctionY;
        }
        if (!objB.isStatic && !objB.isDragging) {
            objB.posX -= correctionX;
            objB.posY -= correctionY;
        }
    }
}

void DemoPhysicsLayer3::SolveAngleConstraints(float deltaTime) {
    for (int i = 0; i < angleConstraintCount; i++) {
        AngleConstraint& constraint = angleConstraints[i];
        
        if (constraint.objectA >= objectCount || 
            constraint.objectB >= objectCount || 
            constraint.objectC >= objectCount) continue;
        
        AdvancedPhysicsObject& objA = objects[constraint.objectA];
        AdvancedPhysicsObject& objB = objects[constraint.objectB]; // Center object
        AdvancedPhysicsObject& objC = objects[constraint.objectC];
        
        // Calculate current angle
        double currentAngle = CalculateAngle(objA.posX, objA.posY, objB.posX, objB.posY, objC.posX, objC.posY);
        
        // Calculate angle error
        double angleError = currentAngle - constraint.targetAngle;
        
        // Normalize angle error to [-?, ?]
        while (angleError > M_PI) angleError -= 2 * M_PI;
        while (angleError < -M_PI) angleError += 2 * M_PI;
        
        // Apply angular correction (simplified)
        double correction = angleError * constraint.stiffness * 0.1;
        
        if (!objA.isStatic && !objA.isDragging) {
            objA.angularVelocity -= correction;
        }
        if (!objC.isStatic && !objC.isDragging) {
            objC.angularVelocity += correction;
        }
    }
}

void DemoPhysicsLayer3::HandleCollisions(float deltaTime) {
    // Detect collisions
    for (int i = 0; i < objectCount; i++) {
        for (int j = i + 1; j < objectCount; j++) {
            CollisionPair collision;
            if (CheckCollision(objects[i], objects[j], collision)) {
                collision.objectA = i;
                collision.objectB = j;
                collisions.push_back(collision);
                ResolveCollision(objects[i], objects[j], collision);
            }
        }
    }
}

bool DemoPhysicsLayer3::CheckCollision(const AdvancedPhysicsObject& obj1, const AdvancedPhysicsObject& obj2, CollisionPair& collision) {
    // Simple AABB collision detection
    bool overlapping = (obj1.posX < obj2.posX + obj2.width &&
                       obj1.posX + obj1.width > obj2.posX &&
                       obj1.posY < obj2.posY + obj2.height &&
                       obj1.posY + obj1.height > obj2.posY);
                       
    if (!overlapping) return false;
    
    // Calculate collision normal and penetration
    double overlapX = std::min(obj1.posX + obj1.width - obj2.posX, obj2.posX + obj2.width - obj1.posX);
    double overlapY = std::min(obj1.posY + obj1.height - obj2.posY, obj2.posY + obj2.height - obj1.posY);
    
    if (overlapX < overlapY) {
        // Horizontal collision
        collision.normalX = (obj1.posX < obj2.posX) ? -1.0 : 1.0;
        collision.normalY = 0.0;
        collision.penetration = overlapX;
    } else {
        // Vertical collision
        collision.normalX = 0.0;
        collision.normalY = (obj1.posY < obj2.posY) ? -1.0 : 1.0;
        collision.penetration = overlapY;
    }
    
    return true;
}

void DemoPhysicsLayer3::ResolveCollision(AdvancedPhysicsObject& obj1, AdvancedPhysicsObject& obj2, const CollisionPair& collision) {
    // Separate objects
    double separation = collision.penetration * 0.5;
    
    if (!obj1.isStatic && !obj1.isDragging) {
        obj1.posX -= collision.normalX * separation;
        obj1.posY -= collision.normalY * separation;
    }
    if (!obj2.isStatic && !obj2.isDragging) {
        obj2.posX += collision.normalX * separation;
        obj2.posY += collision.normalY * separation;
    }
    
    // Calculate relative velocity
    double relVelX = obj2.velX - obj1.velX;
    double relVelY = obj2.velY - obj1.velY;
    double relativeSpeed = relVelX * collision.normalX + relVelY * collision.normalY;
    
    // Don't resolve if objects are separating
    if (relativeSpeed > 0) return;
    
    // Calculate restitution (bounce)
    double restitution = std::min(obj1.material.restitution, obj2.material.restitution);
    
    // Calculate impulse
    double impulse = -(1 + restitution) * relativeSpeed;
    if (!obj1.isStatic && !obj2.isStatic) {
        impulse /= (1.0 / obj1.mass + 1.0 / obj2.mass);
    } else if (obj1.isStatic) {
        impulse /= (1.0 / obj2.mass);
    } else if (obj2.isStatic) {
        impulse /= (1.0 / obj1.mass);
    }
    
    // Apply impulse
    double impulseX = impulse * collision.normalX;
    double impulseY = impulse * collision.normalY;
    
    if (!obj1.isStatic && !obj1.isDragging) {
        obj1.velX -= impulseX / obj1.mass;
        obj1.velY -= impulseY / obj1.mass;
    }
    if (!obj2.isStatic && !obj2.isDragging) {
        obj2.velX += impulseX / obj2.mass;
        obj2.velY += impulseY / obj2.mass;
    }
    
    // Apply friction
    double friction = std::max(obj1.material.dynamicFriction, obj2.material.dynamicFriction);
    double tangentVelX = relVelX - relativeSpeed * collision.normalX;
    double tangentVelY = relVelY - relativeSpeed * collision.normalY;
    double tangentSpeed = Magnitude(tangentVelX, tangentVelY);
    
    if (tangentSpeed > 0.001) {
        double frictionImpulse = friction * impulse;
        double frictionX = -(tangentVelX / tangentSpeed) * frictionImpulse;
        double frictionY = -(tangentVelY / tangentSpeed) * frictionImpulse;
        
        if (!obj1.isStatic && !obj1.isDragging) {
            obj1.velX += frictionX / obj1.mass;
            obj1.velY += frictionY / obj1.mass;
        }
        if (!obj2.isStatic && !obj2.isDragging) {
            obj2.velX -= frictionX / obj2.mass;
            obj2.velY -= frictionY / obj2.mass;
        }
    }
}

void DemoPhysicsLayer3::IntegrateMotion(float deltaTime) {
    for (int i = 0; i < objectCount; i++) {
        AdvancedPhysicsObject& obj = objects[i];
        
        if (obj.isStatic || obj.isDragging) continue;
        
        // Integrate linear motion using Verlet integration
        obj.velX += (obj.forceX / obj.mass) * deltaTime;
        obj.velY += (obj.forceY / obj.mass) * deltaTime;
        
        obj.posX += obj.velX * deltaTime;
        obj.posY += obj.velY * deltaTime;
        
        // Integrate angular motion
        obj.angularVelocity += (obj.torque / obj.momentOfInertia) * deltaTime;
        obj.rotation += obj.angularVelocity * deltaTime;
        
        // Clear forces for next frame
        obj.forceX = obj.forceY = 0.0;
        obj.torque = 0.0;
    }
}

void DemoPhysicsLayer3::HandleBoundaryCollisions(AdvancedPhysicsObject& obj) {
    bool bounced = false;
    
    // Left boundary
    if (obj.posX < bounds.left) {
        obj.posX = bounds.left;
        obj.velX = -obj.velX * obj.material.restitution;
        bounced = true;
    }
    
    // Right boundary
    if (obj.posX + obj.width > bounds.right) {
        obj.posX = bounds.right - obj.width;
        obj.velX = -obj.velX * obj.material.restitution;
        bounced = true;
    }
    
    // Top boundary
    if (obj.posY < bounds.top) {
        obj.posY = bounds.top;
        obj.velY = -obj.velY * obj.material.restitution;
        bounced = true;
    }
    
    // Bottom boundary
    if (obj.posY + obj.height > bounds.bottom) {
        obj.posY = bounds.bottom - obj.height;
        obj.velY = -obj.velY * obj.material.restitution;
        bounced = true;
        
        // Apply friction when hitting ground
        obj.velX *= obj.material.dynamicFriction;
    }
}

int DemoPhysicsLayer3::AddObject(double x, double y, double w, double h, const MaterialProperties& material, bool isStatic) {
    if (objectCount >= MAX_OBJECTS_LAYER3) return -1;
    
    AdvancedPhysicsObject& obj = objects[objectCount];
    obj.posX = x;
    obj.posY = y;
    obj.width = w;
    obj.height = h;
    obj.material = material;
    obj.isStatic = isStatic;
    obj.id = objectCount;
    
    // Calculate mass based on density and volume (area in 2D)
    if (!isStatic) {
        obj.mass = material.density * (w * h) / 10000.0; // Scale down for reasonable values
        obj.UpdateMomentOfInertia();
    } else {
        obj.mass = 1000000.0; // Very heavy for static objects
    }
    
    return objectCount++;
}

void DemoPhysicsLayer3::RemoveObject(int index) {
    if (index < 0 || index >= objectCount) return;
    
    // Remove any constraints involving this object
    RemoveConstraintsForObject(index);
    
    // Shift objects down
    for (int i = index; i < objectCount - 1; i++) {
        objects[i] = objects[i + 1];
        objects[i].id = i; // Update IDs
    }
    objectCount--;
    
    // Update constraint indices
    for (int i = 0; i < springCount; i++) {
        if (springs[i].objectA > index) springs[i].objectA--;
        if (springs[i].objectB > index) springs[i].objectB--;
    }
    
    for (int i = 0; i < distanceConstraintCount; i++) {
        if (distanceConstraints[i].objectA > index) distanceConstraints[i].objectA--;
        if (distanceConstraints[i].objectB > index) distanceConstraints[i].objectB--;
    }
}

int DemoPhysicsLayer3::FindObjectAtPosition(double x, double y) {
    for (int i = objectCount - 1; i >= 0; i--) {
        const AdvancedPhysicsObject& obj = objects[i];
        if (x >= obj.posX && x <= obj.posX + obj.width &&
            y >= obj.posY && y <= obj.posY + obj.height) {
            return i;
        }
    }
    return -1;
}

int DemoPhysicsLayer3::AddSpring(int objA, int objB, double restLength, double stiffness, double damping) {
    if (springCount >= MAX_SPRINGS_LAYER3 || objA >= objectCount || objB >= objectCount) return -1;
    
    springs[springCount] = SpringConstraint(objA, objB, restLength, stiffness, damping);
    return springCount++;
}

int DemoPhysicsLayer3::AddDistanceConstraint(int objA, int objB, double distance, double stiffness) {
    if (distanceConstraintCount >= MAX_DISTANCE_CONSTRAINTS_LAYER3 || objA >= objectCount || objB >= objectCount) return -1;
    
    distanceConstraints[distanceConstraintCount] = DistanceConstraint(objA, objB, distance, stiffness);
    return distanceConstraintCount++;
}

int DemoPhysicsLayer3::AddAngleConstraint(int objA, int objB, int objC, double angle, double stiffness) {
    if (angleConstraintCount >= MAX_ANGLE_CONSTRAINTS_LAYER3 || 
        objA >= objectCount || objB >= objectCount || objC >= objectCount) return -1;
    
    angleConstraints[angleConstraintCount] = AngleConstraint(objA, objB, objC, angle, stiffness);
    return angleConstraintCount++;
}

void DemoPhysicsLayer3::RemoveConstraintsForObject(int objectId) {
    // Remove springs involving this object
    for (int i = springCount - 1; i >= 0; i--) {
        if (springs[i].objectA == objectId || springs[i].objectB == objectId) {
            // Shift springs down
            for (int j = i; j < springCount - 1; j++) {
                springs[j] = springs[j + 1];
            }
            springCount--;
        }
    }
    
    // Remove distance constraints involving this object
    for (int i = distanceConstraintCount - 1; i >= 0; i--) {
        if (distanceConstraints[i].objectA == objectId || distanceConstraints[i].objectB == objectId) {
            for (int j = i; j < distanceConstraintCount - 1; j++) {
                distanceConstraints[j] = distanceConstraints[j + 1];
            }
            distanceConstraintCount--;
        }
    }
    
    // Remove angle constraints involving this object
    for (int i = angleConstraintCount - 1; i >= 0; i--) {
        if (angleConstraints[i].objectA == objectId || 
            angleConstraints[i].objectB == objectId || 
            angleConstraints[i].objectC == objectId) {
            for (int j = i; j < angleConstraintCount - 1; j++) {
                angleConstraints[j] = angleConstraints[j + 1];
            }
            angleConstraintCount--;
        }
    }
}

void DemoPhysicsLayer3::Render(HDC hdc) {
    if (!demoInitialized || !hdc) return;
    
    SetBkMode(hdc, TRANSPARENT);
    
    // Draw title
    SetTextColor(hdc, RGB(255, 255, 255));
    RECT titleRect = { bounds.left, bounds.top - 25, bounds.right, bounds.top };
    WCHAR title[256];
    const WCHAR* modeNames[] = { L"Sandbox", L"Springs", L"Bridge", L"Pendulum", L"Chain" };
    wsprintf(title, L"Physics Layer #3 - %s Mode (Press M to cycle modes)", modeNames[currentMode]);
    DrawText(hdc, title, -1, &titleRect, DT_LEFT);
    
    // Draw bounds
    HPEN boundsPen = CreatePen(PS_SOLID, 2, RGB(100, 100, 100));
    HPEN oldPen = (HPEN)SelectObject(hdc, boundsPen);
    Rectangle(hdc, bounds.left, bounds.top, bounds.right, bounds.bottom);
    SelectObject(hdc, oldPen);
    DeleteObject(boundsPen);
    
    // Draw constraints first (so they appear behind objects)
    if (showConstraints) {
        for (int i = 0; i < springCount; i++) {
            if (springs[i].isVisible) {
                DrawSpring(hdc, springs[i]);
            }
        }
        
        for (int i = 0; i < distanceConstraintCount; i++) {
            if (distanceConstraints[i].isVisible) {
                DrawDistanceConstraint(hdc, distanceConstraints[i]);
            }
        }
        
        for (int i = 0; i < angleConstraintCount; i++) {
            if (angleConstraints[i].isVisible) {
                DrawAngleConstraint(hdc, angleConstraints[i]);
            }
        }
    }
    
    // Draw objects
    for (int i = 0; i < objectCount; i++) {
        DrawObject(hdc, objects[i]);
        
        if (showVelocityVectors) {
            DrawVelocityVector(hdc, objects[i]);
        }
        
        if (showForces) {
            DrawForceVector(hdc, objects[i]);
        }
    }
    
    // Draw UI
    DrawUserInterface(hdc);
    
    if (showHelp) {
        DrawHelp(hdc);
    }
}

void DemoPhysicsLayer3::DrawObject(HDC hdc, const AdvancedPhysicsObject& obj) {
    RECT rect = {
        (LONG)obj.posX,
        (LONG)obj.posY,
        (LONG)(obj.posX + obj.width),
        (LONG)(obj.posY + obj.height)
    };
    
    COLORREF color = obj.material.color;
    
    // Highlight selected object
    if (obj.id == selectedObjectId) {
        int r = std::min(255, GetRValue(color) + 100);
        int g = std::min(255, GetGValue(color) + 100);
        int b = std::min(255, GetBValue(color) + 50);
        color = RGB(r, g, b);
    }
    
    HBRUSH brush = CreateSolidBrush(color);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    
    HPEN pen;
    if (obj.isStatic) {
        pen = CreatePen(PS_SOLID, 3, RGB(80, 80, 80));
    } else if (obj.id == selectedObjectId) {
        pen = CreatePen(PS_SOLID, 3, RGB(255, 255, 0));
    } else {
        pen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
    }
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    
    // Draw based on object type
    if (obj.isStatic) {
        Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    } else {
        // Draw as rotated rectangle (simplified)
        if (abs(obj.rotation) > 0.1) {
            // For now, just draw as ellipse to show rotation
            Ellipse(hdc, rect.left, rect.top, rect.right, rect.bottom);
            
            // Draw rotation indicator
            HPEN rotPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 0));
            HPEN oldRotPen = (HPEN)SelectObject(hdc, rotPen);
            
            int centerX = (int)(obj.posX + obj.width / 2);
            int centerY = (int)(obj.posY + obj.height / 2);
            int radius = (int)(obj.width / 3);
            
            int lineX = centerX + (int)(cos(obj.rotation) * radius);
            int lineY = centerY + (int)(sin(obj.rotation) * radius);
            
            MoveToEx(hdc, centerX, centerY, NULL);
            LineTo(hdc, lineX, lineY);
            
            SelectObject(hdc, oldRotPen);
            DeleteObject(rotPen);
        } else {
            Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
        }
    }
    
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
    
    // Draw mass indicator
    if (!obj.isStatic) {
        SetTextColor(hdc, RGB(255, 255, 255));
        WCHAR massText[16];
        wsprintf(massText, L"%.1f", obj.mass);
        TextOut(hdc, (int)obj.posX, (int)obj.posY - 15, massText, wcslen(massText));
    }
}

void DemoPhysicsLayer3::DrawSpring(HDC hdc, const SpringConstraint& spring) {
    if (spring.objectA >= objectCount || spring.objectB >= objectCount) return;
    
    const AdvancedPhysicsObject& objA = objects[spring.objectA];
    const AdvancedPhysicsObject& objB = objects[spring.objectB];
    
    int x1 = (int)(objA.posX + objA.width / 2);
    int y1 = (int)(objA.posY + objA.height / 2);
    int x2 = (int)(objB.posX + objB.width / 2);
    int y2 = (int)(objB.posY + objB.height / 2);
    
    HPEN springPen = CreatePen(PS_SOLID, 2, spring.color);
    HPEN oldPen = (HPEN)SelectObject(hdc, springPen);
    
    // Draw zigzag pattern for spring
    int segments = 8;
    double totalLength = Distance(x1, y1, x2, y2);
    double segmentLength = totalLength / segments;
    
    double dx = (x2 - x1) / segments;
    double dy = (y2 - y1) / segments;
    
    // Perpendicular vector for zigzag
    double perpX = -dy / segments * 0.3;
    double perpY = dx / segments * 0.3;
    
    MoveToEx(hdc, x1, y1, NULL);
    
    for (int i = 1; i < segments; i++) {
        int x = x1 + (int)(dx * i);
        int y = y1 + (int)(dy * i);
        
        if (i % 2 == 1) {
            x += (int)perpX;
            y += (int)perpY;
        } else {
            x -= (int)perpX;
            y -= (int)perpY;
        }
        
        LineTo(hdc, x, y);
    }
    
    LineTo(hdc, x2, y2);
    
    SelectObject(hdc, oldPen);
    DeleteObject(springPen);
}

void DemoPhysicsLayer3::DrawDistanceConstraint(HDC hdc, const DistanceConstraint& constraint) {
    if (constraint.objectA >= objectCount || constraint.objectB >= objectCount) return;
    
    const AdvancedPhysicsObject& objA = objects[constraint.objectA];
    const AdvancedPhysicsObject& objB = objects[constraint.objectB];
    
    int x1 = (int)(objA.posX + objA.width / 2);
    int y1 = (int)(objA.posY + objA.height / 2);
    int x2 = (int)(objB.posX + objB.width / 2);
    int y2 = (int)(objB.posY + objB.height / 2);
    
    HPEN constraintPen = CreatePen(PS_SOLID, 3, constraint.color);
    HPEN oldPen = (HPEN)SelectObject(hdc, constraintPen);
    
    MoveToEx(hdc, x1, y1, NULL);
    LineTo(hdc, x2, y2);
    
    SelectObject(hdc, oldPen);
    DeleteObject(constraintPen);
}

void DemoPhysicsLayer3::DrawAngleConstraint(HDC hdc, const AngleConstraint& constraint) {
    if (constraint.objectA >= objectCount || 
        constraint.objectB >= objectCount || 
        constraint.objectC >= objectCount) return;
    
    const AdvancedPhysicsObject& objA = objects[constraint.objectA];
    const AdvancedPhysicsObject& objB = objects[constraint.objectB];
    const AdvancedPhysicsObject& objC = objects[constraint.objectC];
    
    int x1 = (int)(objA.posX + objA.width / 2);
    int y1 = (int)(objA.posY + objA.height / 2);
    int x2 = (int)(objB.posX + objB.width / 2);
    int y2 = (int)(objB.posY + objB.height / 2);
    int x3 = (int)(objC.posX + objC.width / 2);
    int y3 = (int)(objC.posY + objC.height / 2);
    
    HPEN anglePen = CreatePen(PS_DOT, 1, RGB(255, 128, 0));
    HPEN oldPen = (HPEN)SelectObject(hdc, anglePen);
    
    // Draw lines from B to A and B to C
    MoveToEx(hdc, x2, y2, NULL);
    LineTo(hdc, x1, y1);
    MoveToEx(hdc, x2, y2, NULL);
    LineTo(hdc, x3, y3);
    
    SelectObject(hdc, oldPen);
    DeleteObject(anglePen);
}

void DemoPhysicsLayer3::DrawVelocityVector(HDC hdc, const AdvancedPhysicsObject& obj) {
    if (obj.isStatic) return;
    
    double scale = 0.1;
    int centerX = (int)(obj.posX + obj.width / 2);
    int centerY = (int)(obj.posY + obj.height / 2);
    int endX = centerX + (int)(obj.velX * scale);
    int endY = centerY + (int)(obj.velY * scale);
    
    HPEN velPen = CreatePen(PS_SOLID, 2, RGB(0, 255, 0));
    HPEN oldPen = (HPEN)SelectObject(hdc, velPen);
    
    MoveToEx(hdc, centerX, centerY, NULL);
    LineTo(hdc, endX, endY);
    
    // Draw arrowhead
    if (Magnitude(obj.velX, obj.velY) > 10) {
        double angle = atan2(obj.velY, obj.velX);
        int arrowSize = 5;
        int arrow1X = endX - (int)(cos(angle - 0.5) * arrowSize);
        int arrow1Y = endY - (int)(sin(angle - 0.5) * arrowSize);
        int arrow2X = endX - (int)(cos(angle + 0.5) * arrowSize);
        int arrow2Y = endY - (int)(sin(angle + 0.5) * arrowSize);
        
        LineTo(hdc, arrow1X, arrow1Y);
        MoveToEx(hdc, endX, endY, NULL);
        LineTo(hdc, arrow2X, arrow2Y);
    }
    
    SelectObject(hdc, oldPen);
    DeleteObject(velPen);
}

void DemoPhysicsLayer3::DrawForceVector(HDC hdc, const AdvancedPhysicsObject& obj) {
    if (obj.isStatic) return;
    
    double scale = 0.01;
    int centerX = (int)(obj.posX + obj.width / 2);
    int centerY = (int)(obj.posY + obj.height / 2);
    int endX = centerX + (int)(obj.forceX * scale);
    int endY = centerY + (int)(obj.forceY * scale);
    
    HPEN forcePen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
    HPEN oldPen = (HPEN)SelectObject(hdc, forcePen);
    
    MoveToEx(hdc, centerX, centerY, NULL);
    LineTo(hdc, endX, endY);
    
    SelectObject(hdc, oldPen);
    DeleteObject(forcePen);
}

void DemoPhysicsLayer3::DrawUserInterface(HDC hdc) {
    WCHAR infoText[512];
    wsprintf(infoText, L"Objects: %d | Springs: %d | Constraints: %d | Gravity: %.0f | Mode: %s",
             objectCount, springCount, distanceConstraintCount, gravityY,
             (currentMode == MODE_SANDBOX) ? L"Sandbox" :
             (currentMode == MODE_SPRING_DEMO) ? L"Springs" :
             (currentMode == MODE_BRIDGE_DEMO) ? L"Bridge" :
             (currentMode == MODE_PENDULUM_DEMO) ? L"Pendulum" : L"Chain");
    
    SetTextColor(hdc, RGB(200, 200, 200));
    RECT infoRect = { bounds.left, bounds.bottom + 5, bounds.right, bounds.bottom + 25 };
    DrawText(hdc, infoText, -1, &infoRect, DT_LEFT);
    
    if (selectedObjectId >= 0 && selectedObjectId < objectCount) {
        const AdvancedPhysicsObject& obj = objects[selectedObjectId];
        WCHAR objText[256];
        wsprintf(objText, L"Selected: ID:%d | Mass:%.1f | Vel:(%.1f,%.1f) | Material: %s",
                 obj.id, obj.mass, obj.velX, obj.velY,
                 (obj.material.color == materialWood.color) ? L"Wood" :
                 (obj.material.color == materialMetal.color) ? L"Metal" :
                 (obj.material.color == materialRubber.color) ? L"Rubber" : L"Ice");
        
        SetTextColor(hdc, RGB(255, 255, 100));
        RECT objRect = { bounds.left, bounds.bottom + 25, bounds.right, bounds.bottom + 45 };
        DrawText(hdc, objText, -1, &objRect, DT_LEFT);
    }
}

void DemoPhysicsLayer3::DrawHelp(HDC hdc) {
    RECT helpRect = { bounds.right - 350, bounds.top + 10, bounds.right - 10, bounds.top + 300 };
    
    // Draw background
    HBRUSH helpBrush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &helpRect, helpBrush);
    DeleteObject(helpBrush);
    
    // Draw border
    HPEN helpPen = CreatePen(PS_SOLID, 1, RGB(150, 150, 150));
    HPEN oldPen = (HPEN)SelectObject(hdc, helpPen);
    Rectangle(hdc, helpRect.left, helpRect.top, helpRect.right, helpRect.bottom);
    SelectObject(hdc, oldPen);
    DeleteObject(helpPen);
    
    // Draw help text
    SetTextColor(hdc, RGB(255, 255, 255));
    RECT textRect = helpRect;
    textRect.left += 10;
    textRect.top += 10;
    textRect.right -= 10;
    
    const WCHAR* helpText = 
        L"PHYSICS LAYER #3 CONTROLS:\n"
        L"Left Click: Select/Drag object\n"
        L"Right Click: Create object at cursor\n"
        L"M: Cycle demo modes\n"
        L"C: Connect selected objects\n"
        L"Del/X: Delete selected object\n"
        L"V: Toggle velocity vectors\n"
        L"F: Toggle force vectors\n"
        L"T: Toggle constraint display\n"
        L"1-4: Switch materials\n"
        L"Space: Spawn random object\n"
        L"R: Reset current demo\n"
        L"H: Toggle this help\n\n"
        L"Features:\n"
        L"• Advanced materials\n"
        L"• Spring systems\n"
        L"• Distance constraints\n"
        L"• Angle constraints\n"
        L"• Realistic collisions";
    
    DrawText(hdc, helpText, -1, &textRect, DT_LEFT | DT_WORDBREAK);
}

void DemoPhysicsLayer3::HandleMouseDown(int x, int y, bool leftButton) {
    if (!demoInitialized) return;
    
    if (leftButton) {
        // Find object at position
        int objectId = FindObjectAtPosition((double)x, (double)y);
        
        if (objectId >= 0) {
            SelectPhysicsObject(objectId);
            objects[selectedObjectId].isDragging = true;
            isDragging = true;
            dragOffsetX = x - objects[selectedObjectId].posX;
            dragOffsetY = y - objects[selectedObjectId].posY;
        } else {
            DeselectAll();
        }
    } else {
        // Right click - create new object
        MaterialProperties material = GetRandomMaterial();
        AddObject(x - 15, y - 15, 30, 30, material);
    }
    
    lastMouseX = x;
    lastMouseY = y;
}

void DemoPhysicsLayer3::HandleMouseUp(int x, int y, bool leftButton) {
    if (!demoInitialized) return;
    
    if (leftButton && isDragging) {
        isDragging = false;
        for (int i = 0; i < objectCount; i++) {
            objects[i].isDragging = false;
        }
    }
    
    lastMouseX = x;
    lastMouseY = y;
}

void DemoPhysicsLayer3::HandleMouseMove(int x, int y) {
    if (!demoInitialized) return;
    
    if (isDragging && selectedObjectId >= 0 && selectedObjectId < objectCount) {
        double newPosX = x - dragOffsetX;
        double newPosY = y - dragOffsetY;
        
        // Keep within bounds
        newPosX = ClampValue(newPosX, bounds.left, bounds.right - objects[selectedObjectId].width);
        newPosY = ClampValue(newPosY, bounds.top, bounds.bottom - objects[selectedObjectId].height);
        
        objects[selectedObjectId].posX = newPosX;
        objects[selectedObjectId].posY = newPosY;
        objects[selectedObjectId].velX = objects[selectedObjectId].velY = 0.0; // Stop motion while dragging
    }
    
    lastMouseX = x;
    lastMouseY = y;
}

void DemoPhysicsLayer3::HandleKeyDown(int keyCode) {
    if (!demoInitialized) return;
    
    if (keyCode >= 0 && keyCode < 256) {
        keyStates[keyCode] = true;
    }
    
    switch (keyCode) {
        case 'H': // Toggle help
            showHelp = !showHelp;
            break;
            
        case 'M': // Cycle demo modes
            CycleDemoMode();
            break;
            
        case 'V': // Toggle velocity vectors
            showVelocityVectors = !showVelocityVectors;
            break;
            
        case 'F': // Toggle force vectors
            showForces = !showForces;
            break;
            
        case 'T': // Toggle constraint display
            showConstraints = !showConstraints;
            break;
            
        case 'C': // Connect selected objects
            ConnectSelectedObjects();
            break;
            
        case 'R': // Reset current demo
            switch (currentMode) {
                case MODE_SANDBOX: SetupSandboxMode(); break;
                case MODE_SPRING_DEMO: SetupSpringDemo(); break;
                case MODE_BRIDGE_DEMO: SetupBridgeDemo(); break;
                case MODE_PENDULUM_DEMO: SetupPendulumDemo(); break;
                case MODE_CHAIN_DEMO: SetupChainDemo(); break;
            }
            break;
            
        case VK_SPACE: // Spawn random object
            SpawnObjectAtMouse();
            break;
            
        case VK_DELETE:
        case 'X': // Delete selected object
            DeleteSelectedObject();
            break;
            
        case '1': // Switch to wood material
            if (selectedObjectId >= 0 && selectedObjectId < objectCount) {
                objects[selectedObjectId].material = materialWood;
            }
            break;
            
        case '2': // Switch to metal material
            if (selectedObjectId >= 0 && selectedObjectId < objectCount) {
                objects[selectedObjectId].material = materialMetal;
            }
            break;
            
        case '3': // Switch to rubber material
            if (selectedObjectId >= 0 && selectedObjectId < objectCount) {
                objects[selectedObjectId].material = materialRubber;
            }
            break;
            
        case '4': // Switch to ice material
            if (selectedObjectId >= 0 && selectedObjectId < objectCount) {
                objects[selectedObjectId].material = materialIce;
            }
            break;
    }
}

void DemoPhysicsLayer3::HandleKeyUp(int keyCode) {
    if (!demoInitialized) return;
    
    if (keyCode >= 0 && keyCode < 256) {
        keyStates[keyCode] = false;
    }
}

void DemoPhysicsLayer3::HandleKeyboardInput(float deltaTime) {
    if (selectedObjectId < 0 || selectedObjectId >= objectCount) return;
    
    AdvancedPhysicsObject& obj = objects[selectedObjectId];
    const double forceStrength = 1000.0;
    
    // Apply forces with WASD/Arrow keys
    if (keyStates[VK_LEFT] || keyStates['A']) {
        obj.forceX -= forceStrength;
    }
    if (keyStates[VK_RIGHT] || keyStates['D']) {
        obj.forceX += forceStrength;
    }
    if (keyStates[VK_UP] || keyStates['W']) {
        obj.forceY -= forceStrength;
    }
    if (keyStates[VK_DOWN] || keyStates['S']) {
        obj.forceY += forceStrength;
    }
    
    // Apply torque with Q/E
    if (keyStates['Q']) {
        obj.torque -= 500.0;
    }
    if (keyStates['E']) {
        obj.torque += 500.0;
    }
}

void DemoPhysicsLayer3::SelectPhysicsObject(int objectId) {
    DeselectAll();
    if (objectId >= 0 && objectId < objectCount) {
        selectedObjectId = objectId;
        objects[objectId].isSelected = true;
    }
}

void DemoPhysicsLayer3::DeselectAll() {
    selectedObjectId = -1;
    for (int i = 0; i < objectCount; i++) {
        objects[i].isSelected = false;
    }
    isDragging = false;
}

void DemoPhysicsLayer3::SpawnObjectAtMouse() {
    MaterialProperties material = GetRandomMaterial();
    double size = 20 + (rand() % 30); // 20-50 pixels
    AddObject(lastMouseX - size/2, lastMouseY - size/2, size, size, material);
}

void DemoPhysicsLayer3::DeleteSelectedObject() {
    if (selectedObjectId >= 0 && selectedObjectId < objectCount) {
        RemoveObject(selectedObjectId);
        DeselectAll();
    }
}

void DemoPhysicsLayer3::ConnectSelectedObjects() {
    static int lastSelected = -1;
    
    if (selectedObjectId >= 0 && selectedObjectId < objectCount) {
        if (lastSelected >= 0 && lastSelected < objectCount && lastSelected != selectedObjectId) {
            // Connect the two objects with a spring
            double distance = Distance(objects[lastSelected].posX + objects[lastSelected].width/2,
                                     objects[lastSelected].posY + objects[lastSelected].height/2,
                                     objects[selectedObjectId].posX + objects[selectedObjectId].width/2,
                                     objects[selectedObjectId].posY + objects[selectedObjectId].height/2);
            
            AddSpring(lastSelected, selectedObjectId, distance, 50, 0.2);
            lastSelected = -1;
        } else {
            lastSelected = selectedObjectId;
        }
    }
}

void DemoPhysicsLayer3::CycleDemoMode() {
    currentMode = (DemoMode)((currentMode + 1) % 5);
    
    switch (currentMode) {
        case MODE_SANDBOX: SetupSandboxMode(); break;
        case MODE_SPRING_DEMO: SetupSpringDemo(); break;
        case MODE_BRIDGE_DEMO: SetupBridgeDemo(); break;
        case MODE_PENDULUM_DEMO: SetupPendulumDemo(); break;
        case MODE_CHAIN_DEMO: SetupChainDemo(); break;
    }
}

void DemoPhysicsLayer3::Shutdown() {
    demoInitialized = FALSE;
    objectCount = 0;
    springCount = 0;
    distanceConstraintCount = 0;
    angleConstraintCount = 0;
    selectedObjectId = -1;
    isDragging = false;
    collisions.clear();
}

// Utility functions
double DemoPhysicsLayer3::Distance(double x1, double y1, double x2, double y2) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    return sqrt(dx * dx + dy * dy);
}

double DemoPhysicsLayer3::Magnitude(double x, double y) {
    return sqrt(x * x + y * y);
}

void DemoPhysicsLayer3::Normalize(double& x, double& y) {
    double mag = Magnitude(x, y);
    if (mag > 0.001) {
        x /= mag;
        y /= mag;
    }
}

double DemoPhysicsLayer3::ClampValue(double value, double minVal, double maxVal) {
    return std::max(minVal, std::min(maxVal, value));
}

double DemoPhysicsLayer3::CalculateAngle(double x1, double y1, double x2, double y2, double x3, double y3) {
    // Calculate angle ABC where B is the center point
    double vec1X = x1 - x2;
    double vec1Y = y1 - y2;
    double vec2X = x3 - x2;
    double vec2Y = y3 - y2;
    
    double dot = vec1X * vec2X + vec1Y * vec2Y;
    double cross = vec1X * vec2Y - vec1Y * vec2X;
    
    return atan2(cross, dot);
}