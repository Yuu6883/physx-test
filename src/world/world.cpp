#include "world.hpp"
#include <thread>
#include <random>

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

	shared_mat = physics->createMaterial(0.5f, 0.5f, 0.1f);
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

	for (auto& obj : trashQ) delete obj;
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
	auto ground = PxCreatePlane(*physics, PxPlane(0, 1, 0, 0), *shared_mat);
	addObject<PrimitiveObject>(ground);
}

void World::spawn(Player* player) {
	PxCapsuleControllerDesc desc;

	desc.material = shared_mat;
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

	printf("Spawned player 0x%p\n", player);
}

void World::move(Player* player, float dt) {
	PlayerInput input;

	{
		scoped_lock lock(player->input_mutex);
		input = player->input;
	}

	if (!input.dir.isFinite()) input.dir = PxZero;
	PxVec3 dir(input.dir.x, 0, input.dir.y);

	auto len = dir.normalize();
	if (!len) dir = PxVec3(1, 0, 0);

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
}

void World::destroy(Player* player) {
	player->deferRemove();
}

static std::random_device rd;
static std::mt19937 gen(rd());
static std::uniform_real_distribution<float> dist(-10, 21);

static uint16_t cubes = 0;

void World::step(float dt, bool blocking) {
	// Add cubes
	{
		PxSceneWriteLock sl(*scene);
		scoped_lock ol(object_mutex);

		if (cubes < 10000) {
			for (auto i = 0; i < 25; i++) {
				auto box = PxCreateDynamic(*physics, PxTransform(
					PxVec3(dist(gen) * 2, 5.f, dist(gen) * 2)),
					PxBoxGeometry(0.49f, 0.49f, 0.49f), *shared_mat, 5.0f);
				box->setAngularDamping(0.2f);
				box->setLinearVelocity(PxVec3(dist(gen), dist(gen) * 2 + 40.f, dist(gen)));
				auto obj = addObject<PrimitiveObject, false>(box);
				// printf("[world] Added cube[0x%p] #%u\n", obj, obj->id);

				cubes++;
			}
		}
	}

	// Remove dead cubes
	{
		PxSceneReadLock sl(*scene);
		scoped_lock ol(object_mutex);

		for (auto& obj : objects) {
			if (obj->isPlayer()) continue;

			auto dynamic = obj->actor->is<PxRigidDynamic>();
			if (!dynamic) continue;
			if (dynamic->isSleeping()) {
				if (obj->deferRemove()) {
					// printf("[world] Removing cube[0x%p] #%u\n", obj, obj->id);
					cubes--;
				}
			}
		}
	}

	// Simulate
	{
		PxSceneWriteLock sl(*scene);
		scene->simulate(1 / 60.f); // ???
		tick++;
	}

	if (blocking) syncSim();
}

void World::gc() {

	scoped_lock ol(object_mutex);
	// Actual deallocation happens 1 tick after "gc", 
	// so player onTick function can still have access to the "freed" object
	for (auto& ptr : trashQ) {
		// Made sure this ID is not used immediated for the tick between remove & cleanup ((and not 0))
		if (ptr->id) free_object_ids.push_back(ptr->id);
		// printf("[world] Delete Obj[0x%p] #%u\n", ptr, ptr->id);
		delete ptr;
	}
	trashQ.clear();

	PxSceneWriteLock sl(*scene);

	objects.erase(std::remove_if(objects.begin(), objects.end(), [&](GameObject*& obj) {
		if (obj->removing.load()) {
			// requires scene lock
			if (obj->actor && obj->actor->isReleasable()) {
				obj->actor->release();
				obj->actor = nullptr;
			}

			used_obj_masks[obj->id] = 0;

			// Push to trash queue to clean up in next tick
			trashQ.push_back(obj);
			// printf("[world] Trash Obj[0x%p] #%u\n", obj, obj->id);
			return true;
		} else return false;
	}), objects.end());
}

void World::syncSim() {
	PxSceneWriteLock sl(*scene);
	scene->fetchResults(true);
}