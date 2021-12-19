#include "world.hpp"
#include <thread>
#include <random>
#include <algorithm>

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

PxFilterFlags ContactReportFilterShader(
	PxFilterObjectAttributes attr0, PxFilterData fd0,
	PxFilterObjectAttributes attr1, PxFilterData fd1,
	PxPairFlags& flags, const void*, PxU32) {

	// let triggers through
	if (PxFilterObjectIsTrigger(attr0) || PxFilterObjectIsTrigger(attr1)) {
		flags = PxPairFlag::eTRIGGER_DEFAULT;
		return PxFilterFlag::eDEFAULT;
	}

	flags = PxPairFlag::eCONTACT_DEFAULT | PxPairFlag::eNOTIFY_TOUCH_FOUND;
	return PxFilterFlag::eDEFAULT;
}

void World::onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nbPairs) {
	// printf("Contacting pairs: %u\n", nbPairs);
	for (PxU32 i = 0; i < nbPairs; i++) {
		contacting++;

		auto& pair = pairs[i];
		auto obj0 = static_cast<WorldObject*>(pair.shapes[0]->getActor()->userData);
		auto obj1 = static_cast<WorldObject*>(pair.shapes[1]->getActor()->userData);
	}
}

World::World() {
    PxSceneDesc sceneDesc(physics->getTolerancesScale());
	sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);

	auto t = 4; // std::thread::hardware_concurrency();

	printf("[world] using %u threads\n", t);

	dispatcher = PxDefaultCpuDispatcherCreate(t);
	sceneDesc.flags = PxSceneFlag::eREQUIRE_RW_LOCK;
	sceneDesc.cpuDispatcher = dispatcher;
	// Report kin-kin & static-kin contacts with be reported
	sceneDesc.kineKineFilteringMode = PxPairFilteringMode::eKEEP; 
	sceneDesc.staticKineFilteringMode = PxPairFilteringMode::eKEEP;
	sceneDesc.simulationEventCallback = this;
	sceneDesc.filterShader = ContactReportFilterShader;

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
	addObject<PrimitiveObject>(PxCreatePlane(*physics, PxPlane(0, 1, 0, 0), *shared_mat));
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

	{
		scoped_lock lock(player_mutex);
		players.push_back(player);
	}

	printf("[world] spawned player 0x%p\n", player);
}

void World::updatePlayers(float dt) {
	scoped_lock lock(player_mutex);

	for (auto& player : players) {
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

		auto pos = player->ct->getPosition();
		state.position = PxVec3(pos.x, pos.y, pos.z);

		if (flags & PxControllerCollisionFlag::eCOLLISION_DOWN) {
			state.ground = true;
			state.velocity.y = 0;
			state.lastGroundTick = tick;
		} else {
			state.ground = false;
		}
	}
}

void World::updateNet(float) {
	scoped_lock pl(player_mutex);

	for (auto& player : players) {
		PxSceneReadLock sl(*scene);
		player->updateState(this);
	}
}

void World::destroy(Player* player) {
	player->release();

	scoped_lock lock(player_mutex);
	players.remove(player);

	printf("[world] destroyed player 0x%p\n", player);
}

static std::random_device rd;
static std::mt19937 gen(rd());
static std::uniform_real_distribution<float> dist(-15, 15);

static uint16_t objCount = 0;

void World::step(float dt, bool blocking) {
	// Add cubes
	{
		PxSceneWriteLock sl(*scene);
		scoped_lock ol(object_mutex);

		constexpr uint16_t TOTAL_OBJ = 5000;

		if (objCount < TOTAL_OBJ) {
			for (auto i = 0; i < std::max(1, TOTAL_OBJ / 200); i++) {
				auto box = PxCreateDynamic(*physics, PxTransform(
					PxVec3(dist(gen) * 2, 5.f, dist(gen) * 2)),
					PxBoxGeometry(0.49f, 0.49f, 0.49f), *shared_mat, 5.0f);
				box->setAngularDamping(0.2f);
				box->setLinearVelocity(PxVec3(dist(gen), dist(gen) * 2 + 40.f, dist(gen)));
				box->setMaxLinearVelocity(50.f);

				auto obj = addObject<PrimitiveObject, false>(box);

				objCount++;
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
				if (obj->release()) objCount--;
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
		delete ptr;
	}
	trashQ.clear();

	PxSceneWriteLock sl(*scene);

	objects.erase(std::remove_if(objects.begin(), objects.end(), [&](WorldObject*& obj) {
		if (obj->released.load()) {
			// requires scene lock
			if (obj->actor && obj->actor->isReleasable()) {
				obj->actor->release();
				obj->actor = nullptr;
			}

			used_obj_masks[obj->id] = 0;

			// Push to trash queue to clean up in next tick
			trashQ.push_back(obj);
			return true;
		} else return false;
	}), objects.end());
}

void World::syncSim() {
	PxSceneWriteLock sl(*scene);
	scene->fetchResults(true);

	// printf("%lu contacting pairs\n", contacting.load());
	contacting = 0;
}
