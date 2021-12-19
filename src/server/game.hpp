#pragma once

#include <map>
#include <mutex>
#include <vector>
#include <chrono>
#include <bitset>
#include <unordered_map>
#include <uv.h>

#include <PxPhysicsAPI.h>
#include <PxPhysicsVersion.h>
#include <PxPhysics.h>

#include "../world/world.hpp"
#include "../network/quic/server.hpp"

using std::mutex;
using std::vector;
using std::bitset;
using std::scoped_lock;
using std::unordered_map;
using namespace std::chrono;

class PhysXServer : public QuicServer {
	bool running;

	uv_loop_t* loop;
	uint64_t last_net = 0;
	uint64_t last_tick = 0;

	uint64_t tickIntervalNano = 0;
	uint64_t netIntervalNano = 0;

	uv_timer_t tick_timer;

	class Handle : public Connection, Player {
		friend PhysXServer;

		struct CacheItem {
			WorldObject* obj;
			uint32_t flags;
			PxVec3 pos;
		};

		vector<CacheItem> cache;
		bitset<65536> cache_set;

		static const size_t cache_size = sizeof(CacheItem);

		// Implemented in network/protocol/server-tick.cpp
		virtual void updateState(World* world);

		virtual void onConnect();
		virtual void onData(string_view buffer);
		virtual void onDisconnect();

		PhysXServer* getServer() { return static_cast<PhysXServer*>(server);  };
		World* getWorld() { return getServer()->world; }
	};

	static void tick_timer_cb(uv_timer_t* handle);

	mutex handle_mutex;
	unordered_map<uint32_t, Handle*> allHandles;
public:
	World* world; // TODO: multi world

	PhysXServer(uv_loop_t* loop = uv_default_loop());
	~PhysXServer();

	void run(uint64_t tickInterval, uint64_t netInterval);
	void tick(uint64_t now, float realDelay);

	void addHandle(Handle* handle);
	void removeHandle(Handle* handle);

	Connection* client() { return new Handle(); };
};