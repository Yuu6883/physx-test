#pragma once

#include <PxPhysicsAPI.h>
#include <PxPhysicsVersion.h>
#include <PxPhysics.h>
#include <extensions/PxExtensionsAPI.h>
#include <common/PxTolerancesScale.h>

#include <vector>
#include <mutex>
#include "../network/protocol/common.hpp"

using namespace physx;
using std::mutex;
using std::vector;
using std::scoped_lock;

class PhysXServer;

struct GameObject {
    bool removing;
    PxRigidActor* actor;
    virtual bool isPrimitive() = 0;
    virtual bool isPlayer() = 0;
    virtual void remove() {
        if (actor->isReleasable()) actor->release();
    }
    void deferRemove() { removing = true; }
    GameObject(PxRigidActor* actor) : actor(actor), removing(false) {
        if (actor) actor->userData = this;
    }
    virtual ~GameObject() {};
};

struct PrimitiveObject : GameObject {
    virtual bool isPrimitive() { return true; };
    virtual bool isPlayer() { return false; };
    PrimitiveObject(PxRigidActor* actor) : GameObject(actor) {};
};

struct Player : GameObject {
    mutex world_mutex;
    mutex input_mutex;

    PlayerInput input;
    PlayerState state;
    PxController* ct;

    virtual bool isPrimitive() { return false; };
    virtual bool isPlayer() { return true; };

    virtual void remove() {
        scoped_lock lock(world_mutex);
        ct->release();
    }

    // Need to be called from the world thread
    virtual void move(float dt) {};
    Player() : GameObject(nullptr), ct(nullptr) {};
};

class World {
    friend PhysXServer;

    PxDefaultCpuDispatcher* dispatcher;
    PxScene* scene;
    PxControllerManager* ctm;
    
    uint64_t tick = 0;
    mutex object_mutex;
    vector<GameObject*> objects;

public:
    static int init();
    static void cleanup();

    World();
    ~World();

    void initScene();

    void spawn(Player* player);
    void move(Player* player, float dt);
    void destroy(Player* player);

    void gc();
    void step(float dt, bool blocking = true);
    void syncSim();

    PxScene* getScene() { return scene; };
};