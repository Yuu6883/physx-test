#pragma once
#include "../../gl/base.hpp"
#include <vector>

using std::vector;

#include <PxPhysicsAPI.h>
#include <PxPhysicsVersion.h>
#include <PxPhysics.h>

using namespace physx;

class World;

class ServerDebugRenderer : public BaseRenderer {
    World* currentWorld;
    void renderGeometry(const PxGeometryHolder& obj);
    void renderActors(const vector<PxRigidActor*>& actors, bool shadow = false, const PxVec3& color = PxVec3(0.75f, 0.75f, 0.75f));
public:
    ServerDebugRenderer(World* world);
    void render();
};