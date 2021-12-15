#pragma once

#include <mutex>
#include <vector>

#include <PxPhysics.h>
#include <PxPhysicsAPI.h>

#include "../network/util/reader.hpp"
#include "../network/quic/client.hpp"
#include "../network/protocol/common.hpp"

using std::mutex;
using std::vector;
using namespace physx;

class BaseClient : public QuicClient {
	// Implemented in network/protocol/client-tick.cpp
	void onData(string_view buffer);

	uint64_t last_packet;
	mutex m;

protected:
	class NetworkedObject;
private:
	struct NetworkData {
		uint16_t type;
		uint16_t state;
		uint16_t flags;
		PxVec3 pos;
		PxQuat quat;
		NetworkedObject* ctx;
	};

	static const size_t client_data_size = sizeof(BaseClient::NetworkData);

protected:
	class NetworkedObject {
		friend BaseClient;
	protected:
		virtual ~NetworkedObject() {};
		virtual void onWake() {};
		virtual void onSleep() {};
		virtual void onAdd(const PxVec3& pos, const PxQuat& quat) {};
		virtual void onUpdate(const PxVec3& pos, const PxQuat& quat) {};
		virtual void onRemove() {};
	};
private:
	// Compact data array
	vector<NetworkData> data;
public:
	template<typename SyncCallback>
	inline void sync(const SyncCallback& cb) {
		m.lock();
		for (auto& d : data) cb(d.ctx);
		m.unlock();
	}

	// Override this function
	virtual NetworkedObject* addObj(uint16_t type, uint16_t state, uint16_t flags, Reader& r) {
		if (type == BOX_T) {
			auto halfExtents = r.read<PxVec3>();
		} else if (type == SPH_T) {
			auto radius = r.read<float>();
		} else if (type == PLN_T) {
			// 
		} else if (type == CPS_T) {
			auto radius = r.read<float>();
			auto halfHeight = r.read<float>();
		} else {
			printf("Unknown network data type: %u\n", type);
		}

		return nullptr;
	}

	uint64_t lastPacketTime() { return last_packet; }
};