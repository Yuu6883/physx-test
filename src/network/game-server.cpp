#include "game-server.hpp"
#include "util/writer.hpp"
#include "util/bitmagic.hpp"

using namespace bitmagic;

PhysXServer::PhysXServer(uv_loop_t* loop) : QuicServer(), loop(loop), 
	world(new World()), running(false) {

	uv_timer_init(loop, &tick_timer);
	tick_timer.data = this;
}

PhysXServer::~PhysXServer() {
	running = false;
	if (world) delete world;
}

void PhysXServer::tick_timer_cb(uv_timer_t* handle) {
	auto server = static_cast<PhysXServer*>(handle->data);
	auto now = uv_hrtime();
	auto dt = now - server->last_tick;

	server->tick(now, dt / 1000000.f);

	auto busyNano = uv_hrtime() - now;
	server->last_tick = now;

	if (busyNano > server->tickIntervalNano) {
		// printf("Busy time: %.2f, interval: %.2f\n", busyNano / 1000000.f, server->intervalNano / 1000000.f);
		uv_timer_start(&server->tick_timer, PhysXServer::tick_timer_cb, 1, 0);
	} else {
		uint64_t timeLeft = (server->tickIntervalNano - busyNano) / 1000000.f;
		// printf("Time left: %lu\n", timeLeft);
		uv_timer_start(&server->tick_timer, PhysXServer::tick_timer_cb, timeLeft, 0);
	}
}

const int MS_TO_NANO = 1000000;

void PhysXServer::run(uint64_t msTickInterval, uint64_t msNetInterval) {
	if (running) return;
	running = true;

	tickIntervalNano = msTickInterval * MS_TO_NANO;
	netIntervalNano = msNetInterval * MS_TO_NANO;

	last_net = uv_hrtime();
	last_tick = uv_hrtime();

	printf("Tick: %lums, Net: %lums\n", msTickInterval, msNetInterval);
	uv_timer_start(&tick_timer, PhysXServer::tick_timer_cb, 0, 0);

	uv_run(loop, UV_RUN_DEFAULT);
}

void PhysXServer::tick(uint64_t now, float realDelay) {
	if (world) {
		if (!count()) return;

		world->step(tickIntervalNano / 1000000000.f, false);

		if (now > last_net + netIntervalNano) {
			last_net = now;
			broadcastState();
		}

		world->syncSim();
	}
	// printf("Real delay = %.2f\n", realDelay);
}

void PhysXServer::broadcastState() {
	if (!world) return;
	auto scene = world->getScene();
	if (!scene) return;

	auto QFLAGS = PxActorTypeFlag::eRIGID_DYNAMIC | PxActorTypeFlag::eRIGID_STATIC;

	auto nbActors = scene->getNbActors(QFLAGS);
	if (!nbActors) return;

	std::vector<PxRigidActor*> actors(nbActors);
	scene->getActors(QFLAGS, reinterpret_cast<PxActor**>(actors.data()), nbActors);

	unordered_set<PxRigidActor*> curr(actors.begin(), actors.end());
	vector<PxRigidActor*> toRemove;

	sync([&] (auto& connections) {
		for (auto conn : connections) {
			auto handle = static_cast<Handle*>(conn);
			toRemove.resize(0);
			// TODO: move this terribly long piece of code somewhere else
			Writer w;

			constexpr uint8_t PROTO_VER[3] = { 0, 0, 1 };

			w.write<uint8_t>(PROTO_VER[0]);
			w.write<uint8_t>(PROTO_VER[1]);
			w.write<uint8_t>(PROTO_VER[2]);

			w.write<uint64_t>(duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch()).count());

			w.write<uint32_t>(handle->cache.size());

			constexpr uint8_t ADD_OBJ_ST = 0 << 6;
			constexpr uint8_t ADD_OBJ_DY = 1 << 6;
			constexpr uint8_t UPD_OBJ    = 2 << 6;
			constexpr uint8_t OBJ_STATE  = 3 << 6;

			constexpr uint32_t OBJ_SLEEP = 1;
			constexpr uint32_t OBJ_REMOVE = 2;

			PxShape* shape = nullptr;
			for (auto& [obj, entry] : handle->cache) {
				auto& prevFlags = entry.flags;
				auto& prevPos = entry.pos;

				if (curr.find(obj) == curr.end()) {
					// remove
					toRemove.push_back(obj);
					w.write<uint8_t>(OBJ_STATE | OBJ_REMOVE);
				} else {
					// skip static object
					if (obj->is<PxRigidStatic>()) continue;
					uint32_t newFlags = 0;
					if (obj->is<PxRigidDynamic>() && 
						obj->is<PxRigidDynamic>()->isSleeping()) newFlags |= OBJ_SLEEP;

					bool sleepToggled = ((prevFlags ^ newFlags) & OBJ_SLEEP);
					// TODO: other flags?
					prevFlags = newFlags;

					auto t = obj->getGlobalPose();
					auto& currPos = t.p;

					if (sleepToggled) {
						// Obj sleep state toggled, send 'precise' coordinates
						w.write<uint8_t>(OBJ_STATE | OBJ_SLEEP);
						vec3_48_encode_wb(currPos, prevPos, w.ref<uint16_t>(), w.ref<uint16_t>(), w.ref<uint16_t>());
					} else {
						// update
						vec3_24_delta_encode(currPos, prevPos, w.ref<uint8_t>(UPD_OBJ), w.ref<uint8_t>(), w.ref<uint8_t>(), w.ref<uint8_t>());
					}

					// Smallest 3 quaterion
					w.write<uint32_t>(quat_sm3_encode(t.q));
				}
			}

			constexpr uint8_t BOX_T = 1;
			constexpr uint8_t SPH_T = 2;
			constexpr uint8_t PLN_T = 3;
			constexpr uint8_t UNK_T = 63;

			for (auto obj : curr) {
				if (handle->cache.find(obj) != handle->cache.end()) continue;
				if (obj->getShapes(&shape, 1) != 1) continue;

				uint8_t headerInit = ADD_OBJ_DY;
				if (obj->is<PxRigidStatic>()) headerInit = ADD_OBJ_ST;
				else if (!obj->is<PxRigidDynamic>()) continue; // ???

				// Write to buffer (shape contains the shape singleton)
				auto& header = w.ref<uint8_t>(headerInit);

				auto geo = shape->getGeometry();
				auto type = geo.getType();
				auto t = obj->getGlobalPose();

				if (type == PxGeometryType::eBOX) {
					header |= BOX_T;
					geo.box(); // extents
				} else if (type == PxGeometryType::ePLANE) {
					header |= PLN_T;
					// nothing
				} else if (type == PxGeometryType::eSPHERE) {
					header |= SPH_T;
					geo.sphere(); // radius
				} else {
					header |= UNK_T;
					// TODO: implement more shapes
					continue;
				}

				uint32_t newFlags = 0;
				if (obj->is<PxRigidDynamic>() &&
					obj->is<PxRigidDynamic>()->isSleeping()) newFlags |= OBJ_SLEEP;
				// Add to cache
				handle->cache.insert({ obj, { newFlags, obj->getGlobalPose().p } });
			}

			for (auto obj : toRemove) handle->cache.erase(obj);
			auto buf = w.finalize();
			printf("Buffer size: %u\n", buf.size());
			handle->send(buf, true);
		}
	});
}

void PhysXServer::Handle::onConnect() {

}

void PhysXServer::Handle::onData(string_view buffer) {
}

void PhysXServer::Handle::onDisconnect() {
}