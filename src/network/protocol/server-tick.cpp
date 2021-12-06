#include "common.hpp"

#include "../../server/game.hpp"
#include "../util/writer.hpp"
#include "../util/bitmagic.hpp"

using namespace bitmagic;

void PhysXServer::Handle::onTick(unordered_set<PxRigidActor*>& curr) {
	static thread_local vector<PxRigidActor*> toRemove;

	toRemove.resize(0);
	Writer w;

	w.write<uint8_t>(PROTO_VER[0]);
	w.write<uint8_t>(PROTO_VER[1]);
	w.write<uint8_t>(PROTO_VER[2]);

	int64_t timestamp = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	w.write<int64_t>(timestamp);

	w.write<uint32_t>(cache.size());

	PxShape* shape = nullptr;
	for (auto& [obj, entry] : cache) {
		auto& prevFlags = entry.flags;
		auto& prevPos = entry.pos;

		if (curr.find(obj) == curr.end()) {
			// remove
			toRemove.push_back(obj);
			w.write<uint8_t>(UPD_STATE | OBJ_REMOVE);
		} else {
			// skip static object
			if (obj->is<PxRigidStatic>()) continue;
			uint32_t newFlags = 0;
			if (obj->is<PxRigidDynamic>() &&
				obj->is<PxRigidDynamic>()->isSleeping()) newFlags |= OBJ_SLEEP;

			bool sleepToggled = ((prevFlags ^ newFlags) & OBJ_SLEEP);
			// TODO: other flags?
			prevFlags = newFlags;

			if (sleepToggled) {
				auto t = obj->getGlobalPose();
				const auto& currPos = t.p;

				// Obj goes to sleep
				if (newFlags & OBJ_SLEEP) {
					// Loseless encode and cache update
					w.write<uint8_t>(UPD_STATE | OBJ_SLEEP);
					w.write<float>(currPos.x);
					w.write<float>(currPos.y);
					w.write<float>(currPos.z);
					w.write<uint32_t>(quat_sm3_encode(t.q));
					prevPos = currPos;
				} else {
					// Obj wake up ((no flags but no OBJ_SLEEP indicate wake up
					w.write<uint8_t>(UPD_STATE);
					vec3_24_delta_encode(prevPos, currPos, w.ref<uint8_t>(UPD_OBJ), w.ref<uint8_t>(), w.ref<uint8_t>(), w.ref<uint8_t>());
					w.write<uint32_t>(quat_sm3_encode(t.q));
				}
				// sleep state did not update
			} else {
				// currently sleeping, skip
				if (newFlags & OBJ_SLEEP) {
					continue;
				} else {
					// normal update
					auto t = obj->getGlobalPose();
					const auto& currPos = t.p;
					vec3_24_delta_encode(prevPos, currPos, w.ref<uint8_t>(UPD_OBJ), w.ref<uint8_t>(), w.ref<uint8_t>(), w.ref<uint8_t>());
					w.write<uint32_t>(quat_sm3_encode(t.q));
				}
			}
		}
	}

	constexpr uint8_t BOX_T = 1;
	constexpr uint8_t SPH_T = 2;
	constexpr uint8_t PLN_T = 3;
	constexpr uint8_t UNK_T = 63;

	for (auto obj : curr) {
		if (cache.find(obj) != cache.end()) continue;
		if (obj->getShapes(&shape, 1) != 1) continue;

		uint8_t headerInit = ADD_OBJ_DY;
		if (obj->is<PxRigidStatic>()) headerInit = ADD_OBJ_ST;
		else if (!obj->is<PxRigidDynamic>()) continue; // ???

		// Write to buffer (shape contains the shape singleton)
		auto& header = w.ref<uint8_t>(headerInit);

		auto geo = shape->getGeometry();
		auto type = geo.getType();
		auto t = obj->getGlobalPose();

		PxVec3 toCache;
		vec3_48_encode_wb(t.p, toCache, w.ref<uint16_t>(), w.ref<uint16_t>(), w.ref<uint16_t>());
		w.write<uint32_t>(quat_sm3_encode(t.q));

		if (type == PxGeometryType::eBOX) {
			header |= BOX_T;
			auto& box = geo.box();
			w.write<float>(box.halfExtents.x);
			w.write<float>(box.halfExtents.y);
			w.write<float>(box.halfExtents.z);
		} else if (type == PxGeometryType::ePLANE) {
			header |= PLN_T;
			// nothing
		} else if (type == PxGeometryType::eSPHERE) {
			header |= SPH_T;
			w.write<float>(geo.sphere().radius);
		} else {
			header |= UNK_T;
			// TODO: implement more shapes
			continue;
		}

		// Add to cache
		cache.insert({ obj, { 0, toCache } });
	}

	for (auto obj : toRemove) cache.erase(obj);
	auto buf = w.finalize();

	// printf("Buffer size: %u\n", buf.size());

	send(buf, true);
}