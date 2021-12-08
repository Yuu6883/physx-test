#pragma once


#include <map>
#include <vector>
#include <chrono>
#include <unordered_set>
#include <uv.h>

#include "../world/world.hpp"
#include "../network/quic/server.hpp"

using std::vector;
using std::unordered_set;
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
		float query;
		float compression;
	} timing;

	struct Handle : Connection {
		struct CacheItem {
			PxRigidActor* obj;
			uint32_t flags;
			PxVec3 pos;
		};

		vector<CacheItem> cache;
		unordered_set<PxRigidActor*> cache_set;

		static const size_t cache_size = sizeof(CacheItem);

		// Implemented in network/protocol/server-tick.cpp
		virtual void onTick(unordered_set<PxRigidActor*>& actors);

		virtual void onConnect();
		virtual void onData(string_view buffer);
		virtual void onDisconnect();
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