#include "world.hpp"
#include <thread>

static PxDefaultErrorCallback defaultErrorCallback;
static PxDefaultAllocator defaultAllocatorCallback;

static PxFoundation* foundation;
static PxPhysics* physics;

int World::init() {
    foundation = PxCreateFoundation(PX_PHYSICS_VERSION, defaultAllocatorCallback,
        defaultErrorCallback);

    if (!foundation) {
        printf("PxCreateFoundation failed!");
        return 1;
    }

    bool recordMemoryAllocations = true;

    physics = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation,
        PxTolerancesScale(), recordMemoryAllocations, nullptr);

    if (!physics) {
        printf("PxCreatePhysics failed!");
        return 1;
    }

    return 0;
}

void World::cleanup() {
    physics->release();
    foundation->release();
}

World::World() {
    PxSceneDesc sceneDesc(physics->getTolerancesScale());
	sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);

	auto t = 4; // std::thread::hardware_concurrency();

	printf("Using %u threads\n", t);

	dispatcher = PxDefaultCpuDispatcherCreate(t);
	sceneDesc.cpuDispatcher = dispatcher;
	sceneDesc.filterShader = PxDefaultSimulationFilterShader;

#ifdef PHYSX_USE_CUDA
	PxCudaContextManagerDesc cudaContextManagerDesc;

	auto cudaCtxManager = PxCreateCudaContextManager(*foundation, cudaContextManagerDesc, PxGetProfilerCallback());
	if (cudaCtxManager) {
		sceneDesc.cudaContextManager = cudaCtxManager;
		sceneDesc.flags |= PxSceneFlag::eENABLE_GPU_DYNAMICS;
		sceneDesc.broadPhaseType = PxBroadPhaseType::eGPU;
	} else {
		printf("Failed to create CUDA context\n");
	}
#endif

	scene = physics->createScene(sceneDesc);
}

World::~World() {
    dispatcher->release();
    scene->release();
}

void World::initScene() {
    auto material = physics->createMaterial(0.5f, 0.5f, 0.1f);
    
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


	for (int stack = 0; stack < 10; stack++) {
		for (int i = -10; i <= 10; i++) {
			for (int j = -10; j <= 10; j++) {
				auto box = PxCreateDynamic(*physics, PxTransform(PxVec3(i, stack + 0.49f * 0.5f, j)),
					PxBoxGeometry(0.49f, 0.49f, 0.49f), *material, 5.0f);
				box->setAngularDamping(0.2f);
				scene->addActor(*box);
			}
		}
	}

	/*
	auto panel = PxCreateDynamic(*physics, PxTransform(PxVec3(0.f, 20.f, 0.f)),
		PxBoxGeometry(10.f, 0.48f, 10.f), *material, 10.0f);
	scene->addActor(*panel);
	*/

	// Destroyer ball
	auto ball = PxCreateDynamic(*physics, PxTransform(PxVec3(0, 50, 0)), PxSphereGeometry(10), *material, 10.0f);
	ball->setAngularDamping(0.5f);
	scene->addActor(*ball);

	// ball->setLinearVelocity(PxVec3(0, -25, 0));
}

void World::step(float dt, bool blocking) {
	scene->simulate(1 / 60.f); // ???
	if (blocking) scene->fetchResults(true);
}

void World::syncSim() {
	scene->fetchResults(true);
}