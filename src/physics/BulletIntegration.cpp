/**
 * @file BulletIntegration.cpp
 * @brief Base-X Game Engine — Bullet3 Physics Implementation
 */

#include <BaseX/BulletIntegration.h>
#include <BaseX/DRO.h>

#ifdef BASEX_HAVE_BULLET3
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#endif

#include <cstring>
#include <cmath>

namespace BaseX {

// ═══════════════════════════════════════════════════════════════════
//  PhysicsWorld Implementation
// ═══════════════════════════════════════════════════════════════════

PhysicsWorld::PhysicsWorld()
    : m_dynamicsWorld(nullptr)
    , m_collisionConfig(nullptr)
    , m_dispatcher(nullptr)
    , m_broadphase(nullptr)
    , m_solver(nullptr)
    , m_droManager(nullptr)
    , m_lodManager(nullptr)
    , m_initialized(false)
{
}

PhysicsWorld::~PhysicsWorld() {
    shutdown();
}

bool PhysicsWorld::initialize(const PhysicsConfig& config) {
#ifdef BASEX_HAVE_BULLET3
    if (m_initialized) return true;

    m_config = config;

    // Create Bullet3 components
    m_collisionConfig = new btDefaultCollisionConfiguration();
    m_dispatcher = new btCollisionDispatcher(m_collisionConfig);
    m_broadphase = new btDbvtBroadphase();
    m_solver = new btSequentialImpulseConstraintSolver();
    m_dynamicsWorld = new btDiscreteDynamicsWorld(
        m_dispatcher, m_broadphase, m_solver, m_collisionConfig
    );

    // Set gravity
    m_dynamicsWorld->setGravity(btVector3(
        config.gravity[0], config.gravity[1], config.gravity[2]
    ));

    m_initialized = true;
    return true;
#else
    return false;
#endif
}

void PhysicsWorld::shutdown() {
#ifdef BASEX_HAVE_BULLET3
    if (!m_initialized) return;

    // Remove all rigid bodies
    if (m_dynamicsWorld) {
        for (int i = m_dynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i--) {
            btCollisionObject* obj = m_dynamicsWorld->getCollisionObjectArray()[i];
            btRigidBody* body = btRigidBody::upcast(obj);
            if (body && body->getMotionState()) {
                delete body->getMotionState();
            }
            m_dynamicsWorld->removeCollisionObject(obj);
            delete obj;
        }
    }

    delete m_dynamicsWorld;
    delete m_solver;
    delete m_broadphase;
    delete m_dispatcher;
    delete m_collisionConfig;

    m_dynamicsWorld = nullptr;
    m_initialized = false;
#endif
}

void PhysicsWorld::stepSimulation(float deltaTime) {
#ifdef BASEX_HAVE_BULLET3
    if (!m_initialized) return;

    // Update precision based on DRO if enabled
    if (m_config.enableDRO && m_droManager) {
        updateAllPrecisions();
    }

    m_dynamicsWorld->stepSimulation(deltaTime, m_config.maxSubSteps, m_config.timeStep);
#endif
}

void PhysicsWorld::attachDROManager(DROManager* droManager) {
    m_droManager = droManager;
}

void PhysicsWorld::attachLODManager(LODManager* lodManager) {
    m_lodManager = lodManager;
}

void* PhysicsWorld::createRigidBody(CollisionShapeType shapeType, 
                                     const float shapeParams[3], 
                                     const RigidBodyParams& params) {
#ifdef BASEX_HAVE_BULLET3
    if (!m_initialized) return nullptr;

    // Create collision shape
    btCollisionShape* shape = nullptr;
    switch (shapeType) {
        case CollisionShapeType::Box:
            shape = new btBoxShape(btVector3(
                shapeParams[0], shapeParams[1], shapeParams[2]
            ));
            break;
        case CollisionShapeType::Sphere:
            shape = new btSphereShape(shapeParams[0]);
            break;
        case CollisionShapeType::Capsule:
            shape = new btCapsuleShape(shapeParams[0], shapeParams[1]);
            break;
        case CollisionShapeType::Cylinder:
            shape = new btCylinderShape(btVector3(
                shapeParams[0], shapeParams[1], shapeParams[2]
            ));
            break;
        default:
            shape = new btBoxShape(btVector3(1, 1, 1));
            break;
    }

    // Create transform
    btTransform startTransform;
    startTransform.setIdentity();
    startTransform.setOrigin(btVector3(
        params.position[0], params.position[1], params.position[2]
    ));
    startTransform.setRotation(btQuaternion(
        params.rotation[0], params.rotation[1], params.rotation[2], params.rotation[3]
    ));

    // Create motion state
    btDefaultMotionState* motionState = new btDefaultMotionState(startTransform);

    // Calculate local inertia
    btVector3 localInertia(0, 0, 0);
    if (params.mass > 0.0f) {
        shape->calculateLocalInertia(params.mass, localInertia);
    }

    // Create rigid body
    btRigidBody::btRigidBodyConstructionInfo rbInfo(
        params.mass, motionState, shape, localInertia
    );
    rbInfo.m_friction = params.friction;
    rbInfo.m_restitution = params.restitution;
    rbInfo.m_linearDamping = params.linearDamping;
    rbInfo.m_angularDamping = params.angularDamping;

    btRigidBody* body = new btRigidBody(rbInfo);
    body->setUserPointer(params.userData);

    if (params.isKinematic) {
        body->setCollisionFlags(
            body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT
        );
        body->setActivationState(DISABLE_DEACTIVATION);
    }

    // Enable CCD if configured
    if (m_config.enableCCD) {
        body->setCcdMotionThreshold(0.001f);
        body->setCcdSweptSphereRadius(0.01f);
    }

    m_dynamicsWorld->addRigidBody(body);
    return body;
#else
    return nullptr;
#endif
}

void PhysicsWorld::removeRigidBody(void* body) {
#ifdef BASEX_HAVE_BULLET3
    if (!m_initialized || !body) return;
    
    btRigidBody* rb = static_cast<btRigidBody*>(body);
    m_dynamicsWorld->removeRigidBody(rb);
    
    if (rb->getMotionState()) {
        delete rb->getMotionState();
    }
    if (rb->getCollisionShape()) {
        delete rb->getCollisionShape();
    }
    delete rb;
#endif
}

void PhysicsWorld::updateBodyPrecision(void* body, float distance) {
#ifdef BASEX_HAVE_BULLET3
    if (!m_droManager || !body) return;

    btRigidBody* rb = static_cast<btRigidBody*>(body);
    
    // Get optimal radix for distance
    RadixBase radix = m_droManager->getOptimalRadix(distance);
    
    // Adjust simulation parameters based on radix
    switch (radix) {
        case RadixBase::BASE_64: // Maximum precision
            rb->setActivationState(ACTIVE_TAG);
            rb->setSleepingThresholds(0.001f, 0.001f);
            break;
        case RadixBase::BASE_32: // High precision
            rb->setSleepingThresholds(0.01f, 0.01f);
            break;
        case RadixBase::BASE_16: // Medium precision
            rb->setSleepingThresholds(0.1f, 0.1f);
            break;
        case RadixBase::BASE_8: // Low precision
            rb->setSleepingThresholds(0.5f, 0.5f);
            break;
        case RadixBase::BASE_4: // Minimal precision - allow sleep
            rb->setActivationState(WANTS_DEACTIVATION);
            break;
    }
#endif
}

void PhysicsWorld::getBodyPosition(void* body, float outPos[3]) {
#ifdef BASEX_HAVE_BULLET3
    if (!body) return;
    btRigidBody* rb = static_cast<btRigidBody*>(body);
    btTransform trans;
    rb->getMotionState()->getWorldTransform(trans);
    btVector3 pos = trans.getOrigin();
    outPos[0] = pos.x();
    outPos[1] = pos.y();
    outPos[2] = pos.z();
#endif
}

void PhysicsWorld::getBodyRotation(void* body, float outQuat[4]) {
#ifdef BASEX_HAVE_BULLET3
    if (!body) return;
    btRigidBody* rb = static_cast<btRigidBody*>(body);
    btTransform trans;
    rb->getMotionState()->getWorldTransform(trans);
    btQuaternion rot = trans.getRotation();
    outQuat[0] = rot.x();
    outQuat[1] = rot.y();
    outQuat[2] = rot.z();
    outQuat[3] = rot.w();
#endif
}

void PhysicsWorld::setBodyPosition(void* body, const float pos[3]) {
#ifdef BASEX_HAVE_BULLET3
    if (!body) return;
    btRigidBody* rb = static_cast<btRigidBody*>(body);
    btTransform trans = rb->getWorldTransform();
    trans.setOrigin(btVector3(pos[0], pos[1], pos[2]));
    rb->setWorldTransform(trans);
    rb->activate(true);
#endif
}

void PhysicsWorld::applyForce(void* body, const float force[3]) {
#ifdef BASEX_HAVE_BULLET3
    if (!body) return;
    btRigidBody* rb = static_cast<btRigidBody*>(body);
    rb->applyCentralForce(btVector3(force[0], force[1], force[2]));
    rb->activate(true);
#endif
}

void PhysicsWorld::applyImpulse(void* body, const float impulse[3]) {
#ifdef BASEX_HAVE_BULLET3
    if (!body) return;
    btRigidBody* rb = static_cast<btRigidBody*>(body);
    rb->applyCentralImpulse(btVector3(impulse[0], impulse[1], impulse[2]));
    rb->activate(true);
#endif
}

bool PhysicsWorld::raycast(const float from[3], const float to[3], 
                            float outHitPos[3], float outHitNormal[3], 
                            void** outHitBody) {
#ifdef BASEX_HAVE_BULLET3
    if (!m_initialized) return false;

    btVector3 rayFrom(from[0], from[1], from[2]);
    btVector3 rayTo(to[0], to[1], to[2]);
    
    btCollisionWorld::ClosestRayResultCallback rayCallback(rayFrom, rayTo);
    m_dynamicsWorld->rayTest(rayFrom, rayTo, rayCallback);
    
    if (rayCallback.hasHit()) {
        btVector3 hitPos = rayCallback.m_hitPointWorld;
        btVector3 hitNorm = rayCallback.m_hitNormalWorld;
        
        outHitPos[0] = hitPos.x();
        outHitPos[1] = hitPos.y();
        outHitPos[2] = hitPos.z();
        
        outHitNormal[0] = hitNorm.x();
        outHitNormal[1] = hitNorm.y();
        outHitNormal[2] = hitNorm.z();
        
        if (outHitBody) {
            *outHitBody = const_cast<btCollisionObject*>(rayCallback.m_collisionObject);
        }
        
        return true;
    }
#endif
    return false;
}

void PhysicsWorld::setBodyActive(void* body, bool active) {
#ifdef BASEX_HAVE_BULLET3
    if (!body) return;
    btRigidBody* rb = static_cast<btRigidBody*>(body);
    if (active) {
        rb->activate(true);
    } else {
        rb->setActivationState(WANTS_DEACTIVATION);
    }
#endif
}

void PhysicsWorld::updateAllPrecisions() {
#ifdef BASEX_HAVE_BULLET3
    // This would iterate all bodies and update precision based on camera distance
    // Implementation depends on your entity/scene system
#endif
}

// ═══════════════════════════════════════════════════════════════════
//  VRHandPhysics Implementation
// ═══════════════════════════════════════════════════════════════════

VRHandPhysics::VRHandPhysics(PhysicsWorld* world)
    : m_world(world)
    , m_leftHandBody(nullptr)
    , m_rightHandBody(nullptr)
{
}

VRHandPhysics::~VRHandPhysics() {
    if (m_leftHandBody) {
        m_world->removeRigidBody(m_leftHandBody);
    }
    if (m_rightHandBody) {
        m_world->removeRigidBody(m_rightHandBody);
    }
}

void VRHandPhysics::createHandColliders(float radius) {
    float sphereParams[3] = {radius, 0, 0};
    
    RigidBodyParams params;
    params.mass = 0.0f; // Kinematic
    params.isKinematic = true;
    params.friction = 1.0f;
    
    // Left hand
    params.userData = (void*)1; // Tag as left hand
    m_leftHandBody = m_world->createRigidBody(
        CollisionShapeType::Sphere, sphereParams, params
    );
    
    // Right hand
    params.userData = (void*)2; // Tag as right hand
    m_rightHandBody = m_world->createRigidBody(
        CollisionShapeType::Sphere, sphereParams, params
    );
}

void VRHandPhysics::updateHandPose(bool isLeftHand, const float pos[3], const float rot[4]) {
    void* body = isLeftHand ? m_leftHandBody : m_rightHandBody;
    if (body) {
        m_world->setBodyPosition(body, pos);
    }
}

void* VRHandPhysics::getHandBody(bool isLeftHand) {
    return isLeftHand ? m_leftHandBody : m_rightHandBody;
}

} // namespace BaseX
