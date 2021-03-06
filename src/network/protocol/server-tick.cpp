#include "common.hpp"

#include "../../server/game.hpp"
#include "../util/reader.hpp"
#include "../util/writer.hpp"
#include "../util/bitmagic.hpp"

using namespace bitmagic;

void PhysXServer::Handle::onData(string_view buffer) {
	bool error = false;
	Reader r(buffer, error);

	{
		scoped_lock lock(input_mutex);
		r.read<PlayerInput>(input);
	}

	if (error) {
		printf("[handle] input error\n");
	}
}

void PhysXServer::Handle::updateState(World* world) {
	auto& players = world->players;
	auto& curr = world->objects;
	auto& masks = world->used_obj_masks;

	Writer w;

	w.write<uint8_t>(PROTO_VER[0]);
	w.write<uint8_t>(PROTO_VER[1]);
	w.write<uint8_t>(PROTO_VER[2]);

	int64_t timestamp = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	w.write<int64_t>(timestamp);

	w.write<uint32_t>(players.size());

	w.write<uint32_t>(pid);
	w.write<PlayerState>(state);

	for (auto& p : players) {
		if (p == this) continue;

		w.write<uint32_t>(p->pid);
		w.write<PlayerState>(p->state);
	}

	uint32_t cacheSize = cache.size();
	w.write<uint32_t>(cacheSize);

	PxShape* shape = nullptr;

	uint32_t write_id = 0;

	for (uint32_t i = 0; i < cacheSize; i++) {
		auto entry = &cache[i];

		if (write_id < i) {
			cache[write_id] = cache[i]; // move cache item 
			entry = &cache[write_id];
		}

		auto& prevFlags = entry->flags;
		auto& prevPos = entry->pos;

		// We need to make sure cached obj is still in memory at this point
		if (!masks[entry->obj->id]) {
			// remove
			w.write<uint8_t>(UPD_STATE | OBJ_REMOVE);
			cache_set[entry->obj->id] = 0;
		} else {
			write_id++; // Keep current cache in the array

			auto actor = entry->obj->actor;

			uint32_t newFlags = 0;
			if (actor->is<PxRigidStatic>() ||
				actor->is<PxRigidDynamic>() &&
				actor->is<PxRigidDynamic>()->isSleeping()) newFlags |= OBJ_SLEEP;

			bool sleepToggled = ((prevFlags ^ newFlags) & OBJ_SLEEP);
			// TODO: other flags?
			prevFlags = newFlags;

			if (sleepToggled) {
				auto t = actor->getGlobalPose();
				const auto& currPos = t.p;

				// Obj goes to sleep
				if (newFlags & OBJ_SLEEP) {
					// Loseless encode and cache update
					w.write<uint8_t>(UPD_STATE | OBJ_SLEEP);
					w.write<PxVec3>(currPos);
					w.write<PxQuat>(t.q);
					prevPos = currPos;
				} else {
					// Obj wake up ((no flags but no OBJ_SLEEP indicate wake up
					w.write<uint8_t>(UPD_STATE);
					vec3_24_delta_encode(prevPos, currPos, w.ref<uint8_t>(UPD_OBJ), w.ref<uint8_t>(), w.ref<uint8_t>(), w.ref<uint8_t>());
					w.write<uint32_t>(quat_sm3_encode(t.q));
				}
				// sleep state did not update
			} else {
				// currently sleeping, only write sleep state
				if (newFlags & OBJ_SLEEP) {
					w.write<uint8_t>(UPD_STATE | OBJ_SLEEP);
				} else {
					// normal update
					auto& header = w.ref<uint8_t>(UPD_OBJ);

					auto t = actor->getGlobalPose();
					const auto& currPos = t.p;
					auto& x = w.ref<uint8_t>();
					auto& y = w.ref<uint8_t>();
					auto& z = w.ref<uint8_t>();
					vec3_24_delta_encode(prevPos, currPos, header, x, y, z);
					w.write<uint32_t>(quat_sm3_encode(t.q));

					// printf("encoded: %u, %u, %u\n", x, y, z);
					// printf("cache [%.4f,%.4f,%.4f]\n", prevPos.x, prevPos.y, prevPos.z);
				}
			}
		}
	}

	cache.resize(write_id);

	auto& adding = w.ref<uint32_t>();

	for (auto& obj : curr) {
		// already cached or does not have an assigned ID
		if (!obj->id || cache_set[obj->id]) continue;

		auto actor = obj->actor;
		if (actor->getShapes(&shape, 1) != 1) continue;

		uint8_t headerInit = ADD_OBJ_DY;
		if (actor->is<PxRigidStatic>()) headerInit = ADD_OBJ_ST;
		else if (!actor->is<PxRigidDynamic>()) continue; // ???

		// Write to buffer (shape contains the shape singleton)
		auto& header = w.ref<uint8_t>(headerInit);

		auto geo = shape->getGeometry();
		auto type = geo.getType();
		const auto t = actor->getGlobalPose();

		PxVec3 toCache;
		vec3_48_encode_wb(toCache, t.p, w.ref<uint16_t>(), w.ref<uint16_t>(), w.ref<uint16_t>());
		w.write<uint32_t>(quat_sm3_encode(t.q));

		if (type == PxGeometryType::eBOX) {
			header |= BOX_T;
			w.write<PxVec3>(geo.box().halfExtents);
		} else if (type == PxGeometryType::eSPHERE) {
			header |= SPH_T;
			w.write<float>(geo.sphere().radius);
		} else if (type == PxGeometryType::ePLANE) {
			header |= PLN_T;
			// nothing
		} else if (type == PxGeometryType::eCAPSULE) {
			header |= CPS_T;
			auto& cap = geo.capsule();
			w.write<float>(cap.halfHeight);
			w.write<float>(cap.radius);
		} else {
			header |= UNK_T;
			// TODO: implement more shapes
			printf("Unknown shape\n");
		}

		// Add to cache
		cache.push_back({ obj, 0, toCache });
		cache_set[obj->id] = 1;

		adding++;
	}

	w.write<uint32_t>(cache.size());

	auto og = w.offset();

	auto start = high_resolution_clock::now();
	auto buf = w.lz4();
	auto dt = duration<float, std::milli>(high_resolution_clock::now() - start).count();

	// LZ4 95%-99% but only takes ~0.05-0.07ms
	// printf("Compression rate: %.2f%%, dt = %.10f\n", 100.f * buf.size() / og, dt);
	send(buf, true, COMP_LZ4);
}