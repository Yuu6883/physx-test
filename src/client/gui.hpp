#pragma once

#include <chrono>

#include "base.hpp"
#include "../gl/base.hpp"

using namespace std::chrono;

static const PxQuat PLAYER_QUAT = PxQuat(0.f, 0.f, 1.f, 1.f).getNormalized();

// Boom cam
struct ThirdPersonCamera : AbstractCamera {
	int mouseX = 0;
	int mouseY = 0;
	float speed = 1;
	float dist = 10.f;

	ThirdPersonCamera() : AbstractCamera(PxZero, PxVec3(1.f, 0.f, 0.f)) {};

	const PxVec3 getEye() override {
		return eye - dir * dist;
	};

	const PxVec3 getTarget() override {
		return eye;
	};
};

class GUIClient : public BaseClient, public BaseRenderer {

	system_clock::time_point lastBandwidthUpdate = system_clock::now();
	uint64_t lastTotalBytes = 0;
	uint64_t bandwidth = 0;

	bool roam = false;
	ThirdPersonCamera tpc;

	AbstractCamera* cam() {
		if (roam) return &DefaultCamera;
		else return &tpc;
	};

	class RenderableObject : public NetworkedObject {
		bool sleeping = false;

		PxVec3 prevPos;
		PxVec3 netPos;

		PxQuat prevQuat;
		PxQuat netQuat;

	protected:
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

	class RenderablePlayer : public NetworkedPlayer, public Capsule {
	public:
		RenderablePlayer(uint32_t pid, const PlayerState& state) : 
			NetworkedPlayer(pid, state), Capsule(0.3f, 0.75f) {
			onAdd(state.position, PLAYER_QUAT);
		};

		void onState(const PlayerState& state) {
			onUpdate(state.position, PLAYER_QUAT);
		};

		PxVec3 position() { return currPos; }
	};

	virtual void onMouseButton(int button, int mode, int x, int y);
	virtual void onMouseMove(int x, int y);

	virtual void render();
	virtual void postRender();

	virtual NetworkedObject* addObj(uint16_t type, uint16_t state, uint16_t flags, Reader& r);
	virtual NetworkedPlayer* addPlayer(uint32_t pid, const PlayerState& state) { return new RenderablePlayer(pid, state); }

	PlayerInput input;

	void onKeyUp(unsigned char k);
	void onKeyDown(unsigned char k);
	void onSpecialKeyUp(int k);
	void onSpecialKeyDown(int k);
	void onKeyHold(unsigned char k);

	void sendInput();

	void render(RenderableObject* obj);

public:
	GUIClient::GUIClient() : BaseClient(), BaseRenderer("Client Renderer") {
		glutSetCursor(roam ? GLUT_CURSOR_INHERIT : GLUT_CURSOR_NONE);
	};
};
