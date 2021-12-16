#pragma once

#include <PxPhysicsAPI.h>
#include <PxPhysicsVersion.h>
#include <PxPhysics.h>
#include <extensions/PxExtensionsAPI.h>
#include <common/PxTolerancesScale.h>

#include <atomic>
#include <vector>
#include <mutex>
#include <bitset>
#include "../network/protocol/common.hpp"

using namespace physx;
using std::mutex;
using std::vector;
using std::bitset;
using std::atomic;
using std::scoped_lock;

class PhysXServer;

struct GameObject {
    atomic<bool> removing;
    uint16_t id;
    PxRigidActor* actor;

    virtual bool isPrimitive() = 0;
    virtual bool isPlayer() = 0;

    bool deferRemove() { 
        bool expected = false;
        return removing.compare_exchange_weak(expected, true);
    }

    GameObject(uint16_t id) : id(id), actor(nullptr), removing(false) {};
    GameObject(uint16_t id, PxRigidActor* actor) : id(id), actor(actor), removing(false) {
        actor->userData = this;
    }
    virtual ~GameObject() {};
};

struct PrimitiveObject : GameObject {
    virtual bool isPrimitive() { return true; };
    virtual bool isPlayer() { return false; };
    PrimitiveObject(uint16_t id, PxRigidActor* actor) : GameObject(id, actor) {};
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
    Player() : GameObject(0), ct(nullptr) {};
};

class World {
    friend PhysXServer;

    PxDefaultCpuDispatcher* dispatcher;
    PxScene* scene;
    PxControllerManager* ctm;
    
    uint64_t tick = 0;
    mutex object_mutex;
    vector<GameObject*> objects;
    vector<GameObject*> trashQ;
    vector<uint16_t> free_object_ids;
    bitset<65536> used_obj_masks;

    PxMaterial* shared_mat;

public:
    static int init();
    static void cleanup();

    World();
    ~World();

    void initScene();

    // Assign object ID
    uint16_t assignID();

    // Assign an ID and add it to the list
    template<typename T, bool lock = true>
    T* addObject(PxRigidActor* actor) {
        if constexpr (lock) {
            PxSceneWriteLock sl(*scene);
            scoped_lock ol(object_mutex);

            scene->addActor(*actor);
            auto id = assignID();
            if (!id) return nullptr;
            auto ptr = new T(id, actor);
            objects.push_back(ptr);
            return ptr;
        } else {
            scene->addActor(*actor);
            auto id = assignID();
            if (!id) return nullptr;
            auto ptr = new T(id, actor);
            objects.push_back(ptr);
            return ptr;
        }
    }

    void spawn(Player* player);
    void move(Player* player, float dt);
    void destroy(Player* player);

    void gc();
    void step(float dt, bool blocking = true);
    void syncSim();

    PxScene* getScene() { return scene; };
};