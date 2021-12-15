#pragma once

#include "base.hpp"
#include "../gl/base.hpp"

class GUIClient : public BaseClient, public BaseRenderer {
	class RenderableObject : public NetworkedObject {
		bool sleeping = false;

		PxVec3 prevPos;
		PxVec3 netPos;

		PxQuat prevQuat;
		PxQuat netQuat;

		void onWake() { 
			sleeping = false; 
		};

		void onSleep() { 
			sleeping = true; 
		};

		void onAdd(const PxVec3& pos, const PxQuat& quat) {
			prevPos = currPos = netPos = pos;
			prevQuat = currQuat = netQuat = quat;

			// printf("init pos: [%.4f,%.4f,%.4f]\n", pos.x, pos.y, pos.z);
		};

		void onUpdate(const PxVec3& pos, const PxQuat& quat) {
			prevPos = currPos;
			netPos = pos;
			prevQuat = currQuat;
			netQuat = quat;

			// printf("update pos: [%.4f,%.4f,%.4f]\n", pos.x, pos.y, pos.z);
		};

		// TODO onRemove -> call subclass
	public:
		PxVec3 currPos;
		PxQuat currQuat;

		bool isSleeping() { return sleeping; };

		// Called from render thread, remember to wrap this in sync to prevent racing with quic thread
		virtual void update(float dt, float lerp) {
			if (sleeping) {
				currPos = netPos;
				currQuat = netQuat;
			} else {
				currPos = prevPos + (netPos - prevPos) * lerp;

				auto& q1 = prevQuat;
				auto& q2 = netQuat;
				auto& out = currQuat;

				// slerp
				float dot = q1.dot(q2);
				float cosom = fabsf(dot);
				float s0, s1;

				if (cosom < 0.9999f) {
					const float omega = acosf(cosom);
					const float invsin = 1.f / sinf(omega);
					s0 = sinf((1.f - lerp) * omega) * invsin;
					s1 = sin(lerp * omega) * invsin;
				} else {
					s0 = 1 - lerp;
					s1 = lerp;
				}

				s1 = dot >= 0 ? s1 : -s1;

				out.x = s0 * q1.x + s1 * q2.x;
				out.y = s0 * q1.y + s1 * q2.y;
				out.z = s0 * q1.z + s1 * q2.z;
				out.w = s0 * q1.w + s1 * q2.w;
				out.normalize();
			}
		};

		virtual void render() {};
	};

	class Cube : public RenderableObject {
	public:
		PxVec3 halfExtents;
		Cube(PxVec3 halfExtents) : halfExtents(halfExtents) {};
		void render();
	};

	class Sphere : public RenderableObject {
	public:
		float radius;
		Sphere(float radius) : radius(radius) {};
		void render();
	};

	class Plane : public RenderableObject {
		void render();
	};

	class Capsule : public RenderableObject {
	public:
		float r, hh;
		Capsule(float r, float hh) : r(r), hh(hh) {};
		void render();
	};

	virtual void render();
	virtual NetworkedObject* addObj(uint16_t type, uint16_t state, uint16_t flags, Reader& r);

public:
	GUIClient::GUIClient() : BaseClient(), BaseRenderer("Client Renderer") {};
};
