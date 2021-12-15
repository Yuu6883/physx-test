#include "game.hpp"

PhysXServer::PhysXServer(uv_loop_t* loop) : QuicServer(), loop(loop), 
	world(new World()), running(false), timing({ 0.f, 0.f }) {

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

		float delaySec = realDelay * 0.001f;
		syncPerConn([delaySec](auto& conn) {
			static_cast<Handle*>(conn)->move(delaySec);
		});

		world->step(tickIntervalNano / 1000000000.f, false);

		if (now > last_net + netIntervalNano) {
			last_net = last_net + netIntervalNano;
			broadcastState();
		}

		world->syncSim();
	}
}

void PhysXServer::broadcastState() {
	if (!world) return;
	auto scene = world->getScene();
	if (!scene) return;

	auto start = high_resolution_clock::now();

	auto QFLAGS = PxActorTypeFlag::eRIGID_DYNAMIC | PxActorTypeFlag::eRIGID_STATIC;

	auto nbActors = scene->getNbActors(QFLAGS);
	if (!nbActors) return;

	std::vector<PxRigidActor*> actors(nbActors);
	scene->getActors(QFLAGS, reinterpret_cast<PxActor**>(actors.data()), nbActors);

	unordered_set<PxRigidActor*> curr(actors.begin(), actors.end());

	auto dt = high_resolution_clock::now() - start;
	timing.query = duration<float, std::milli>(dt).count();

	syncPerConn([&] (auto& conn) {
		static_cast<Handle*>(conn)->onTick(curr);
	});

	dt = high_resolution_clock::now() - start;
	timing.compression = duration<float, std::milli>(dt).count();

	// printf("Query      : %4.4fms\n", timing.query);
	// printf("Compression: %4.4fms\n", timing.compression);
	// printf("Num Actors : %u\n", nbActors);
}

void PhysXServer::Handle::onConnect() {
	auto server = getServer();
	server->sync([&] {
		server->world->spawn(this);
	});
}

void PhysXServer::Handle::onDisconnect() {
	auto server = getServer();
	server->sync([&] {
		ct->release();
		ct = nullptr;
	});
}

static const PxControllerFilters ctFilters;

void PhysXServer::Handle::move(float dt) {
	if (!ct) return;

	PlayerInput input;

	{
		std::scoped_lock lock(input_mutex);
		input = this->input;
	}

	PxVec3 dir(input.target.x, input.target.y, 0);
	auto len = dir.normalize();

	if (len <= 0) return;
	auto flags = ct->move(dir, 0.001f, dt, ctFilters);
}