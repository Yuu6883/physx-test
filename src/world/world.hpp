#pragma once

#include <PxPhysicsAPI.h>
#include <PxPhysicsVersion.h>
#include <PxPhysics.h>
#include <extensions/PxExtensionsAPI.h>
#include <common/PxTolerancesScale.h>

#include <list>
#include <atomic>
#include <vector>
#include <mutex>
#include <bitset>
#include "../network/protocol/common.hpp"

using namespace physx;

using std::list;
using std::mutex;
using std::vector;
using std::bitset;
using std::atomic;
using std::scoped_lock;

class PhysXServer;
class World;

struct WorldObject {
    atomic<bool> released;
    uint16_t id;
    PxRigidActor* actor;

    virtual bool isPrimitive() = 0;
    virtual bool isPlayer() = 0;

    bool release() { 
        bool expected = false;
        return released.compare_exchange_weak(expected, true);
    }

    WorldObject(uint16_t id) : id(id), actor(nullptr), released(false) {};
    WorldObject(uint16_t id, PxRigidActor* actor) : id(id), actor(actor), released(false) {
        actor->userData = this;
    }
    virtual ~WorldObject() {};
};

struct PrimitiveObject : WorldObject {
    virtual bool isPrimitive() { return true; };
    virtual bool isPlayer() { return false; };
    PrimitiveObject(uint16_t id, PxRigidActor* actor) : WorldObject(id, actor) {};
};

struct Player : WorldObject {
    uint32_t pid = 0;

    mutex world_mutex;
    mutex input_mutex;

    PlayerInput input;
    PlayerState state;
    PxController* ct;

    bool isPrimitive() { return false; };
    bool isPlayer() { return true; };

    void remove() {
        scoped_lock lock(world_mutex);
        ct->release();
        ct = nullptr;
    }

    // Need to be called from the world thread
    virtual void updateState(World* world) = 0;

    Player() : WorldObject(0), ct(nullptr) {};
};

class World : public PxSimulationEventCallback {
    friend PhysXServer;

    PxDefaultCpuDispatcher* dispatcher;
    PxScene* scene;
    PxControllerManager* ctm;
    
    uint32_t id = 0;
    uint64_t tick = 0;

    mutex player_mutex;
    mutex object_mutex;

    list<Player*> players;
    vector<WorldObject*> objects;
    vector<WorldObject*> trashQ;

    vector<uint16_t> free_object_ids;
    bitset<65536> used_obj_masks;

    PxMaterial* shared_mat;

    atomic<uint64_t> contacting = 0;

    void onConstraintBreak(PxConstraintInfo* , PxU32) override {}
    void onWake(PxActor**, PxU32) override {}
    void onSleep(PxActor**, PxU32) override {}
    void onTrigger(PxTriggerPair*, PxU32) override {}
    void onAdvance(const PxRigidBody* const*, const PxTransform*, const PxU32) override {}
    void onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nbPairs) override;
public:
    struct {
        atomic<float> update = 0.f;
        atomic<float> sim = 0.f;
    } timing;

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
    void destroy(Player* player);

    void updateNet(float dt);
    void updatePlayers(float dt);

    void gc();
    void step(float dt, bool blocking = true);
    void syncSim();

    PxScene* getScene() { return scene; };
};