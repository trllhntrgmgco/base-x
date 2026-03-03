#include <iostream>
#include "BaseX/Engine.h"
#include <btBulletDynamicsCommon.h>

int main() {
    std::cout << "Physics Demo with Bullet3\n";

    // Initialize Bullet3
    btDefaultCollisionConfiguration* collisionConfiguration = new btDefaultCollisionConfiguration();
    btCollisionDispatcher* dispatcher = new btCollisionDispatcher(collisionConfiguration);
    btBroadphaseInterface* overlappingPairCache = new btDbvtBroadphase();
    btSequentialImpulseConstraintSolver* solver = new btSequentialImpulseConstraintSolver();
    btDiscreteDynamicsWorld* dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, overlappingPairCache, solver, collisionConfiguration);

    dynamicsWorld->setGravity(btVector3(0, -9.81, 0));

    // Create a ground plane
    btCollisionShape* groundShape = new btStaticPlaneShape(btVector3(0, 1, 0), 1);
    btDefaultMotionState* groundMotionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, -1, 0)));
    btRigidBody::btRigidBodyConstructionInfo groundRigidBodyCI(0, groundMotionState, groundShape, btVector3(0, 0, 0));
    btRigidBody* groundRigidBody = new btRigidBody(groundRigidBodyCI);
    dynamicsWorld->addRigidBody(groundRigidBody);

    // Create a falling sphere
    btCollisionShape* fallShape = new btSphereShape(1);
    btDefaultMotionState* fallMotionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, 50, 0)));
    btScalar mass = 1;
    btVector3 fallInertia(0, 0, 0);
    fallShape->calculateLocalInertia(mass, fallInertia);
    btRigidBody::btRigidBodyConstructionInfo fallRigidBodyCI(mass, fallMotionState, fallShape, fallInertia);
    btRigidBody* fallRigidBody = new btRigidBody(fallRigidBodyCI);
    dynamicsWorld->addRigidBody(fallRigidBody);

    basex::Engine engine;
    if (!engine.initialize()) {
        std::cerr << "Failed to initialize engine.\n";
        return 1;
    }

    // Simulate physics
    for (int i = 0; i < 100; ++i) {
        dynamicsWorld->stepSimulation(1 / 60.f, 10);
        btTransform trans;
        fallRigidBody->getMotionState()->getWorldTransform(trans);
        std::cout << "Ball position: " << trans.getOrigin().getY() << "\n";
        engine.update(0.016f);
    }

    engine.shutdown();

    // Cleanup Bullet
    dynamicsWorld->removeRigidBody(fallRigidBody);
    delete fallRigidBody->getMotionState();
    delete fallRigidBody;
    delete fallShape;

    dynamicsWorld->removeRigidBody(groundRigidBody);
    delete groundRigidBody->getMotionState();
    delete groundRigidBody;
    delete groundShape;

    delete dynamicsWorld;
    delete solver;
    delete overlappingPairCache;
    delete dispatcher;
    delete collisionConfiguration;

    std::cout << "Physics demo complete\n";
    return 0;
}