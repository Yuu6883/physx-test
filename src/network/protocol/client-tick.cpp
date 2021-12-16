#include <uv.h>

#include "../../client/base.hpp"
#include "../util/bitmagic.hpp"

#include <chrono>
using namespace std::chrono;

using namespace bitmagic;

void BaseClient::onData(string_view buffer) {
	uint64_t now = uv_hrtime();
	auto dt = (now - last_packet) / 1000000.f;
	// printf("dt = %5.5f ms, %lu bytes\n", dt, buffer.size());
	last_packet = now;

	bool error = false;
	Reader r(buffer, error);

	uint8_t remote_ver[3] = { r.read<uint8_t>(), r.read<uint8_t>(), r.read<uint8_t>() };
	if (remote_ver[0] != PROTO_VER[0] ||
		remote_ver[1] != PROTO_VER[1] ||
		remote_ver[2] != PROTO_VER[2]) {
		printf("Version mismatch: remote[%d,%d,%d] != local[%d,%d,%d]\n", 
			remote_ver[0], remote_ver[1], remote_ver[2],
			PROTO_VER[0], PROTO_VER[1], PROTO_VER[2]);
		disconnect();
		return;
	}

	int64_t remote_now = r.read<int64_t>();

	int64_t local = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	// printf("%lu bytes | ping %li ms\n", buffer.size(), local - remote_now);

	uint64_t cacheSize = r.read<uint32_t>();
	if (data.size() != cacheSize) {
		printf("Cache size mismatch: %lu != %lu\n", data.size(), cacheSize);
		disconnect();
		exit(1);
		return;
	}

	// Deserialize lock
	m.lock();

	// auto start = uv_hrtime(); // around 0.3ms for 4k objects

	// Update object loop -> update flags, position, and quaternion
	size_t write_id = 0;
	for (uint32_t i = 0; i < cacheSize; i++) {
		if (write_id < i) memcpy(&data[write_id], &data[i], sizeof(NetworkData));

		NetworkData& obj = data[write_id];

		auto header = r.read<uint8_t>();
		auto subop = header & SUBOP_BITS;

		// TODO: only 2 subop is required in this loop, why use 2 bits? same problem with the add loop below
		if (subop == UPD_STATE) {
			auto newFlags = header & STATE_BITS;
			// Remote this obj
			if (newFlags & OBJ_REMOVE) {
				// By not incrementing write_id so it gets overwriten in next step in loop
				obj.ctx->onRemove();
				delete obj.ctx;
				continue;
			}

			if (obj.flags & OBJ_SLEEP) {
				if (newFlags & OBJ_SLEEP) {
					// Does nothing, object has been zzz
				} else {
					// Object wakes up
					PxVec3 prev = obj.pos;
					// Read new header,x,y,z for vec3 delta decode
					vec3_24_delta_decode(prev, obj.pos, r.read<uint8_t>(), r.read<uint8_t>(), r.read<uint8_t>(), r.read<uint8_t>());
					quat_sm3_decode(obj.quat, r.read<uint32_t>());

					obj.ctx->onWake();
					obj.ctx->onUpdate(obj.pos, obj.quat);
				}
			} else {
				// Obj goes to sleep
				if (newFlags & OBJ_SLEEP) {
					// Read full precision coordinates
					obj.pos = r.read<PxVec3>();

					obj.quat = r.read<PxQuat>();

					obj.ctx->onUpdate(obj.pos, obj.quat);
					obj.ctx->onSleep();
				} else {
					// Not possible??? subop should be UPD_OBJ for this
				}
			}
			obj.flags = newFlags;

		} else if (subop == UPD_OBJ) {
			const PxVec3 prevPos = obj.pos;
			auto x = r.read<uint8_t>();
			auto y = r.read<uint8_t>();
			auto z = r.read<uint8_t>();
			vec3_24_delta_decode(prevPos, obj.pos, header, x, y, z);
			quat_sm3_decode(obj.quat, r.read<uint32_t>());

			obj.ctx->onUpdate(obj.pos, obj.quat);			
		} else {
			printf("Unexpected subop code in update loop: %i\n", subop);
		}

		write_id++;
	}

	uint64_t adding = r.read<uint32_t>();

	auto newSize = write_id + adding;
	data.reserve(newSize);
	data.resize(newSize);

	if (adding) {
		if (adding > 65536) {
			printf("Adding %lu objects???\n", adding);
			// Something must have gone very wrong
			m.unlock();
			disconnect();
			exit(1);
		}
	}

	// Add object loop -> add new objects static/dynamic
	for (uint32_t i = 0; i < adding; i++) {
		auto header = r.read<uint8_t>();
		auto& obj = data[write_id + i];

		auto subop = header & SUBOP_BITS;
		if (subop == ADD_OBJ_ST) {
			obj.state = STATIC_OBJ;
		} else if (subop == ADD_OBJ_DY) {
			obj.state = 0;
		} else {
			obj.state = 0;
			printf("Unexpected subop code in add loop: %i\n", subop);
		}

		obj.type = header & STATE_BITS;
		obj.flags = 0;

		vec3_48_decode(obj.pos, r.read<uint16_t>(), r.read<uint16_t>(), r.read<uint16_t>());
		quat_sm3_decode(obj.quat, r.read<uint32_t>());

		obj.ctx = addObj(obj.type, obj.state, obj.flags, r);
		if (!obj.ctx) obj.ctx = new NetworkedObject();

		obj.ctx->onAdd(obj.pos, obj.quat);
	}

	auto expectedCacheSize = r.read<uint32_t>();
	
	// Strict integrity check
	if (data.size() != expectedCacheSize || !r.eof() || error) {
		printf("Data integrity check failed\n");
		printf("data.size() = %u, expected = %u\n", data.size(), expectedCacheSize);
		printf("eof = %s\n", r.eof() ? "true" : "false");
		printf("error = %s\n", error ? "true" : "false");
		disconnect();
		exit(1);
	}

	// Done writing to data array
	m.unlock();

	/*
		auto end = uv_hrtime();
		auto d = (end - start) / 1000000.f;
		printf("Deserialize time: %.5f\n", d);
	*/
}