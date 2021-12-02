#include "world.hpp"
#include <chrono>
#include <thread>

using namespace std::chrono;

static PxDefaultErrorCallback defaultErrorCallback;
static PxDefaultAllocator defaultAllocatorCallback;

static PxFoundation* foundation;
static PxPvd* pvd;
static PxPvdTransport* transport;
static PxPhysics* physics;

int World::init() {
    foundation = PxCreateFoundation(PX_PHYSICS_VERSION, defaultAllocatorCallback,
        defaultErrorCallback);

    if (!foundation) {
        printf("PxCreateFoundation failed!");
        return 1;
    }

    bool recordMemoryAllocations = true;

    pvd = PxCreatePvd(*foundation);
    transport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
    pvd->connect(*transport, PxPvdInstrumentationFlag::eALL);

    physics = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation,
        PxTolerancesScale(), recordMemoryAllocations, pvd);

    if (!physics) {
        printf("PxCreatePhysics failed!");
        return 1;
    }

    return 0;
}

void World::cleanup() {
    
    physics->release();

    pvd->release();
    transport->release();

    foundation->release();
}

World::World() {
    PxSceneDesc sceneDesc(physics->getTolerancesScale());
	sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);

	auto bitconnect = std::thread::hardware_concurrency();

	printf("Hardware threads: %u\n", bitconnect);

	dispatcher = PxDefaultCpuDispatcherCreate(bitconnect);
	sceneDesc.cpuDispatcher = dispatcher;
	sceneDesc.filterShader = PxDefaultSimulationFilterShader;
	scene = physics->createScene(sceneDesc);

    pvdClient = scene->getScenePvdClient();
	if (pvdClient) {
		pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
		pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
		pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
	}
}

World::~World() {
    dispatcher->release();
    scene->release();
}

void World::initScene() {
    auto material = physics->createMaterial(0.1f, 0.1f, 0.5f);
    
	auto ground = PxCreatePlane(*physics, PxPlane(0, 1, 0, 0), *material);
	scene->addActor(*ground);
    
	/*
		auto shape = physics->createShape(PxBoxGeometry(1.0f, 1.0f, 1.0f), *material);
		PxTransform localTm(PxVec3(-3.0f, 5.0f, -15.f));
		auto body = physics->createRigidDynamic(localTm);
		body->attachShape(*shape);
		PxRigidBodyExt::updateMassAndInertia(*body, 10.0f);
		scene->addActor(*body);

		shape->release();
    
		shape = physics->createShape(PxSphereGeometry(1.0f), *material);
		PxTransform localTmS(PxVec3(3.0f, 5.0f, -15.f));
		body = physics->createRigidDynamic(localTmS);
		body->attachShape(*shape);
		PxRigidBodyExt::updateMassAndInertia(*body, 10.0f);
		scene->addActor(*body);
		shape->release();
	*/

	for (int stack = 0; stack < 20; stack++) {
		for (int i = 1; i < 20; i++) {
			for (int j = 1; j < 20; j++) {
				auto dynamic = PxCreateDynamic(*physics, PxTransform(PxVec3(i, stack + 0.0001f, j)), PxBoxGeometry(0.5f, 0.5f, 0.5f), *material, 10.0f);
				dynamic->setAngularDamping(0.5f);
				scene->addActor(*dynamic);
			}
		}
	}

	auto dynamic = PxCreateDynamic(*physics, PxTransform(PxVec3(10, 50, 10)), PxSphereGeometry(10), *material, 10.0f);
	dynamic->setAngularDamping(0.5f);
	// dynamic->setLinearVelocity(PxVec3(0, -5, -10));
	scene->addActor(*dynamic);
}

void World::step(float dt) {
	auto start = high_resolution_clock::now();

	scene->simulate(1.f / 60);
	scene->fetchResults(true);

	auto end = high_resolution_clock::now();

	printf("dt = %.2fms\n", duration_cast<microseconds>(end - start).count() / 1000.f);
}