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

	auto start = high_resolution_clock::now();

	auto QFLAGS = PxActorTypeFlag::eRIGID_DYNAMIC | PxActorTypeFlag::eRIGID_STATIC;

	auto nbActors = scene->getNbActors(QFLAGS);
	if (!nbActors) return;

	std::vector<PxRigidActor*> actors(nbActors);
	scene->getActors(QFLAGS, reinterpret_cast<PxActor**>(actors.data()), nbActors);

	unordered_set<PxRigidActor*> curr(actors.begin(), actors.end());

	auto dt = high_resolution_clock::now() - start;
	timing.query = duration<float, std::milli>(dt).count();

	sync([&] (auto& connections) {
		for (auto conn : connections) {
			static_cast<Handle*>(conn)->onTick(curr);
		}
	});

	dt = high_resolution_clock::now() - start;
	timing.compression = duration<float, std::milli>(dt).count();

	// printf("Query      : %4.4fms\n", timing.query);
	// printf("Compression: %4.4fms\n", timing.compression);
}

void PhysXServer::Handle::onConnect() {

}

void PhysXServer::Handle::onData(string_view buffer) {
}

void PhysXServer::Handle::onDisconnect() {
}