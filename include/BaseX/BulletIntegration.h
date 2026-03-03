/**
 * @file BulletIntegration.h
 * @brief Base-X Game Engine — Bullet3 Physics Integration with DRO
 * 
 * Copyright (c) 2025-2026 Keenan Wallace Dunham / Troll Hunter Gaming LLC
 * All Rights Reserved. Patent Pending.
 * 
 * This integrates Bullet3 physics with the Base-X DRO system:
 * - Per-object precision control based on distance
 * - VR hand collision with Base-64 precision
 * - LOD-based physics activation/deactivation
 * - Custom collision callbacks for DRO precision boosting
 */

#pragma once

#include <BaseX/DRO.h>
#include <cstdint>
#include <memory>

#ifdef BASEX_HAVE_BULLET3
// Forward declare Bullet3 types to avoid header pollution
class btDiscreteDynamicsWorld;
class btRigidBody;
class btCollisionShape;
class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btBroadphaseInterface;
class btSequentialImpulseConstraintSolver;
class btTransform;
class btVector3;
#endif

namespace BaseX {

// Forward declarations
class DROManager;
class LODManager;

/**
 * @brief Physics world configuration
 */
struct PhysicsConfig {
    float gravity[3] = {0.0f, -9.81f, 0.0f};  // Earth gravity
    float timeStep   = 1.0f / 90.0f;           // 90 Hz for VR
    int   maxSubSteps = 10;
    bool  enableCCD   = true;                  // Continuous collision detection
    bool  enableDRO   = true;                  // Use DRO for precision control
};

/**
 * @brief Rigid body creation parameters
 */
struct RigidBodyParams {
    float position[3]   = {0, 0, 0};
    float rotation[4]   = {0, 0, 0, 1};  // quaternion
    float mass          = 1.0f;           // 0 = static
    float friction      = 0.5f;
    float restitution   = 0.3f;           // bounciness
    float linearDamping = 0.0f;
    float angularDamping = 0.0f;
    bool  isKinematic   = false;
    void* userData      = nullptr;        // For entity association
};

/**
 * @brief Collision shape types
 */
enum class CollisionShapeType {
    Box,
    Sphere,
    Capsule,
    Cylinder,
    Cone,
    ConvexHull,
    TriangleMesh
};

/**
 * @brief Base-X Physics World (Bullet3 wrapper with DRO integration)
 */
class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    // Non-copyable
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    /**
     * @brief Initialize physics world
     */
    bool initialize(const PhysicsConfig& config);
    
    /**
     * @brief Shutdown and cleanup
     */
    void shutdown();

    /**
     * @brief Step simulation forward
     */
    void stepSimulation(float deltaTime);

    /**
     * @brief Attach DRO manager for precision control
     */
    void attachDROManager(DROManager* droManager);

    /**
     * @brief Attach LOD manager for distance-based activation
     */
    void attachLODManager(LODManager* lodManager);

    /**
     * @brief Create a rigid body
     * @return Opaque handle to the rigid body (btRigidBody*)
     */
    void* createRigidBody(CollisionShapeType shapeType, 
                          const float shapeParams[3], 
                          const RigidBodyParams& params);

    /**
     * @brief Remove rigid body from world
     */
    void removeRigidBody(void* body);

    /**
     * @brief Update body's DRO precision based on distance
     */
    void updateBodyPrecision(void* body, float distance);

    /**
     * @brief Get body position
     */
    void getBodyPosition(void* body, float outPos[3]);

    /**
     * @brief Get body rotation (quaternion)
     */
    void getBodyRotation(void* body, float outQuat[4]);

    /**
     * @brief Set body position
     */
    void setBodyPosition(void* body, const float pos[3]);

    /**
     * @brief Apply force at center of mass
     */
    void applyForce(void* body, const float force[3]);

    /**
     * @brief Apply impulse at center of mass
     */
    void applyImpulse(void* body, const float impulse[3]);

    /**
     * @brief Raycast against world
     * @return true if hit, fills outHitPos and outHitNormal
     */
    bool raycast(const float from[3], const float to[3], 
                 float outHitPos[3], float outHitNormal[3], 
                 void** outHitBody = nullptr);

    /**
     * @brief Enable/disable physics for a body (LOD optimization)
     */
    void setBodyActive(void* body, bool active);

private:
#ifdef BASEX_HAVE_BULLET3
    btDiscreteDynamicsWorld*                m_dynamicsWorld;
    btDefaultCollisionConfiguration*        m_collisionConfig;
    btCollisionDispatcher*                  m_dispatcher;
    btBroadphaseInterface*                  m_broadphase;
    btSequentialImpulseConstraintSolver*    m_solver;
#else
    void* m_dynamicsWorld;
    void* m_collisionConfig;
    void* m_dispatcher;
    void* m_broadphase;
    void* m_solver;
#endif

    PhysicsConfig m_config;
    DROManager*   m_droManager;
    LODManager*   m_lodManager;
    bool          m_initialized;

    void updateAllPrecisions();
};

/**
 * @brief VR hand physics helper
 */
class VRHandPhysics {
public:
    VRHandPhysics(PhysicsWorld* world);
    ~VRHandPhysics();

    /**
     * @brief Create physics representation of VR hands
     */
    void createHandColliders(float radius = 0.05f);

    /**
     * @brief Update hand positions from VR tracking
     */
    void updateHandPose(bool isLeftHand, const float pos[3], const float rot[4]);

    /**
     * @brief Get hand collision body
     */
    void* getHandBody(bool isLeftHand);

private:
    PhysicsWorld* m_world;
    void*         m_leftHandBody;
    void*         m_rightHandBody;
};

} // namespace BaseX
