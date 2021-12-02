#pragma once

#include <PxPhysicsAPI.h>
#include <PxPhysicsVersion.h>
#include <PxPhysics.h>
#include <extensions/PxExtensionsAPI.h>
#include <pvd/PxPvd.h>
#include <common/PxTolerancesScale.h>

using namespace physx;

class World {
    PxDefaultCpuDispatcher* dispatcher;
    PxScene* scene;
    PxPvdSceneClient* pvdClient;
    
public:
    static int init();
    static void cleanup();

    World();
    ~World();

    void initScene();
    void step(float dt);

    PxScene* getScene() { return scene; };
};