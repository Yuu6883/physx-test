#include "world.hpp"
#include <thread>

using std::scoped_lock;

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
	sceneDesc.flags = PxSceneFlag::eREQUIRE_RW_LOCK;
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
	ctm = PxCreateControllerManager(*scene);

	ctm->setOverlapRecoveryModule(true);

	free_object_ids.reserve(65535);
	for (int i = 65535; i > 0; i--) {
		free_object_ids.push_back(uint16_t(i));
	}
}

World::~World() {
    dispatcher->release();
    scene->release();

	for (auto& obj : objects) delete obj;
}

uint16_t World::assignID() {
	if (free_object_ids.empty()) return 0;

	auto id = free_object_ids.back();
	free_object_ids.pop_back();
	used_obj_masks[id] = 1;
	return id;
}

void World::initScene() {
	PxSceneWriteLock sl(*scene);
	scoped_lock ol(object_mutex);

    auto material = physics->createMaterial(0.5f, 0.5f, 0.1f);
    
	auto ground = PxCreatePlane(*physics, PxPlane(0, 1, 0, 0), *material);
	addObject<PrimitiveObject>(ground, false);

	for (int stack = 0; stack < 10; stack++) {
		for (int i = -10; i <= 10; i++) {
			for (int j = -10; j <= 10; j++) {
				auto box = PxCreateDynamic(*physics, PxTransform(PxVec3(i, stack + 0.49f * 0.5f, j)),
					PxBoxGeometry(0.49f, 0.49f, 0.49f), *material, 5.0f);
				box->setAngularDamping(0.2f);
				addObject<PrimitiveObject>(box, false);
			}
		}
	}

	// Destroyer ball
	auto ball = PxCreateDynamic(*physics, PxTransform(PxVec3(0, 25, 0)), PxSphereGeometry(10), *material, 10.0f);
	ball->setAngularDamping(0.5f);
	ball->setLinearVelocity(PxVec3(0, 25, 0));

	addObject<PrimitiveObject>(ball, false);
}

void World::spawn(Player* player) {

	PxCapsuleControllerDesc desc;

	desc.material = physics->createMaterial(0.5f, 0.5f, 0.1f);
	desc.height = 1.2f;
	desc.radius = 0.5f;
	desc.slopeLimit = 0.f;
	desc.contactOffset = 0.1f;
	desc.stepOffset = 0.02f;
	desc.userData = player;
	desc.position = PxExtendedVec3(25.f, 25.f, 25.f);

	{
		PxSceneWriteLock lock(*scene);
		player->ct = ctm->createController(desc);
		player->actor = player->ct->getActor();
		player->actor->userData = player;
	}

	player->state.ground = false;
	player->state.lastGroundTick = tick;
	player->state.velocity = PxZero;

	{
		scoped_lock lock(object_mutex);
		objects.push_back(player);
	}
}

void World::move(Player* player, float dt) {

	PlayerInput input;

	{
		std::scoped_lock lock(player->input_mutex);
		input = player->input;
	}

	if (!input.dir.isFinite()) input.dir = PxZero;
	PxVec3 dir(input.dir.x, 0, input.dir.y);

	auto len = dir.normalize();
	PxVec3 side = dir.cross(PxVec3(0, 1, 0));

	float ws = 0.f, ad = 0.f;
	if (input.movL) ad = -1.f;
	if (input.movR) ad = 1.f;
	if (input.movB) ws = -1.f;
	if (input.movF) ws = 1.f;

	auto& state = player->state;
	if (input.jump && !state.ground) state.velocity.y = 5.f;

	constexpr float moveSpeed = 2.5f;
	PxVec3 movement = (ws * dir + ad * side).getNormalized() * moveSpeed;

	auto airTick = tick - state.lastGroundTick;
	auto fallV = airTick * (1.f / 60) * -9.81f;

	movement.y = player->state.velocity.y + fallV;

	static const PxControllerFilters ctFilters(0);

	PxSceneWriteLock lock(*scene);
	auto flags = player->ct->move(movement * dt, 0.f, dt, ctFilters);

	if (flags & PxControllerCollisionFlag::eCOLLISION_DOWN) {
		state.ground = true;
		state.velocity.y = 0;
		state.lastGroundTick = tick;
	} else {
		state.ground = false;
	}

	if (flags & PxControllerCollisionFlag::eCOLLISION_UP) {
	}
	if (flags & PxControllerCollisionFlag::eCOLLISION_SIDES) {
	}
}

void World::destroy(Player* player) {
	player->deferRemove();
}

void World::step(float dt, bool blocking) {
	PxSceneWriteLock lock(*scene);

	scene->simulate(1 / 60.f); // ???
	tick++;

	if (blocking) scene->fetchResults(true);
}

void World::gc() {
	// Actual deallocation happens 1 tick after "gc", 
	// so player onTick function can still have access to the "freed" object
	for (auto& ptr : trashQ) delete ptr;
	if (trashQ.size()) printf("Cleaned up %u objects\n", trashQ.size());
	trashQ.clear();

	scoped_lock ol(object_mutex);
	PxSceneWriteLock sl(*scene);

	objects.erase(std::remove_if(objects.begin(), objects.end(), [&](GameObject*& obj) {
		if (obj->removing) {

			// requires scene lock
			if (obj->actor && obj->actor->isReleasable()) {
				obj->actor->release();
				obj->actor = nullptr;
			}

			// requires object lock
			free_object_ids.push_back(obj->id);
			used_obj_masks[obj->id] = 0;

			// Push to trash queue to clean up in next tick
			trashQ.push_back(obj);
			return true;
		} else return false;
	}), objects.end());
}

void World::syncSim() {
	scene->fetchResults(true);
}