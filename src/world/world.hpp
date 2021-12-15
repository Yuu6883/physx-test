#pragma once

#include <PxPhysicsAPI.h>
#include <PxPhysicsVersion.h>
#include <PxPhysics.h>
#include <extensions/PxExtensionsAPI.h>
#include <common/PxTolerancesScale.h>
#include <mutex>
#include "../network/protocol/common.hpp"

using namespace physx;
using std::mutex;

class PhysXServer;

class World {
    friend PhysXServer;

    PxDefaultCpuDispatcher* dispatcher;
    PxScene* scene;
    PxControllerManager* ctm;

    struct Player {
        mutex input_mutex;
        PlayerInput input;
        PxController* ct;

        // Need to be called from the world thread
        virtual void move(float dt) {};
    };
    
public:
    static int init();
    static void cleanup();

    World();
    ~World();

    void initScene();
    void spawn(Player* player);
    void step(float dt, bool blocking = true);
    void syncSim();

    PxScene* getScene() { return scene; };
};