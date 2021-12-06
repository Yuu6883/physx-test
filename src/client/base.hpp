#pragma once

#include <mutex>
#include <vector>
#include <PxPhysics.h>

#include "../network/quic/client.hpp"

using std::mutex;
using std::vector;
using namespace physx;

template<typename T = void>
class BaseClient : public QuicClient {
	// Implemented in network/protocol/client-tick.cpp
	void onData(string_view buffer);

	uint64_t last_packet;
	mutex m;
public:
	template<typename SyncCallback>
	inline void sync(const SyncCallback& cb) {
		m.lock();
		cb();
		m.unlock();
	}

	struct NetworkedObject {
		uint32_t flags;
		PxVec3 prevPos;
		PxVec3 currPos;
		PxQuat prevQuat;
		PxQuat currQuat;
		T* ctx;
	};

	// Compact object array
	vector<NetworkedObject> objects;
};

constexpr size_t obj_size = sizeof(BaseClient<>::NetworkedObject);

#include "../network/protocol/client-tick.hpp"