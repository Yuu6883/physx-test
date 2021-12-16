#pragma once

#include <map>
#include <vector>
#include <chrono>
#include <bitset>
#include <uv.h>

#include <PxPhysicsAPI.h>
#include <PxPhysicsVersion.h>
#include <PxPhysics.h>

#include "../world/world.hpp"
#include "../network/quic/server.hpp"

using std::vector;
using std::bitset;
using std::scoped_lock;
using namespace std::chrono;

class PhysXServer : public QuicServer {
	bool running;

	uv_loop_t* loop;
	uint64_t last_net = 0;
	uint64_t last_tick = 0;

	uint64_t tickIntervalNano = 0;
	uint64_t netIntervalNano = 0;

	uv_timer_t tick_timer;

	void broadcastState();

	struct {
		float compression;
	} timing;

	class Handle : public Connection, Player {
		friend PhysXServer;

		struct CacheItem {
			GameObject* obj;
			uint32_t flags;
			PxVec3 pos;
		};

		vector<CacheItem> cache;
		bitset<65536> cache_set;

		static const size_t cache_size = sizeof(CacheItem);

		// Implemented in network/protocol/server-tick.cpp
		virtual void onTick(bitset<65536>& masks, vector<GameObject*>& objects);

		virtual void onConnect();
		virtual void onData(string_view buffer);
		virtual void onDisconnect();

		virtual void move(float dt);
		PhysXServer* getServer() { return static_cast<PhysXServer*>(server);  };
		World* getWorld() { return getServer()->world; }
	};

	static void tick_timer_cb(uv_timer_t* handle);
public:
	World* world;

	PhysXServer(uv_loop_t* loop = uv_default_loop());
	~PhysXServer();

	void run(uint64_t tickInterval, uint64_t netInterval);
	void tick(uint64_t now, float realDelay);
	Connection* client() { return new Handle(); };
};