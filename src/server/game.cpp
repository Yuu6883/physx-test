#include "game.hpp"

using std::scoped_lock;

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

	printf("[game] tick: %lums, net: %lums\n", msTickInterval, msNetInterval);
	uv_timer_start(&tick_timer, PhysXServer::tick_timer_cb, 0, 0);

	uv_run(loop, UV_RUN_DEFAULT);
}

void PhysXServer::tick(uint64_t now, float realDelay) {
	if (!world) return;

	world->updatePlayers(realDelay * 0.001f);

	auto start = high_resolution_clock::now();
	world->step(tickIntervalNano / 1000000000.f, false);

	if (now > last_net + netIntervalNano) {
		last_net = last_net + netIntervalNano;

		world->updateNet(realDelay);
		world->syncSim();
		world->gc();
		world->timing.update.store(duration<float, std::milli>(high_resolution_clock::now() - start).count());
	}
	else {
		world->syncSim();
	}
	
	world->timing.sim.store(duration<float, std::milli>(high_resolution_clock::now() - start).count());

	// printf("Sim    : %4.4fms\n", world->timing.sim);
	// printf("Update : %4.4fms\n", world->timing.update);
	// printf("Objects: %u\n", world->objects->size());
}


void PhysXServer::addHandle(Handle* handle) {
	scoped_lock lock(handle_mutex);
	uint32_t i = 1;
	while (allHandles.find(i) != allHandles.end()) i++;
	handle->pid = i;
	allHandles.insert({ i, handle });

	printf("[server] added handle#%u\n", handle->pid);
}

void PhysXServer::removeHandle(Handle* handle) {
	scoped_lock lock(handle_mutex);
	allHandles.erase(handle->pid);

	printf("[server] removed handle#%u\n", handle->pid);
}

void PhysXServer::Handle::onConnect() {
	getServer()->addHandle(this);

	scoped_lock lock(world_mutex);
	getWorld()->spawn(this);
}

void PhysXServer::Handle::onDisconnect() {
	getServer()->removeHandle(this);

	scoped_lock lock(world_mutex);
	getWorld()->destroy(this);
}