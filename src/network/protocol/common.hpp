#pragma once

#include <cinttypes>

#include <PxPhysicsAPI.h>

using namespace physx;

// Flags
constexpr uint8_t PROTO_VER[3] = { 0, 0, 2 };

constexpr uint8_t ADD_OBJ_ST = 0 << 6;
constexpr uint8_t ADD_OBJ_DY = 1 << 6;
constexpr uint8_t UPD_OBJ = 2 << 6;
constexpr uint8_t UPD_STATE = 3 << 6;

constexpr uint8_t SUBOP_BITS = 3 << 6;
constexpr uint8_t STATE_BITS = (1 << 6) - 1;

constexpr uint16_t OBJ_SLEEP = 1;
constexpr uint16_t OBJ_REMOVE = 2;

constexpr uint16_t STATIC_OBJ = 1 << 15;

constexpr uint8_t BOX_T = 1;
constexpr uint8_t SPH_T = 2;
constexpr uint8_t PLN_T = 3;
constexpr uint8_t CPS_T = 4;
constexpr uint8_t UNK_T = 63;

struct PlayerState {
	bool ground : 1;
	PlayerState() : ground(true) {};
};

struct PlayerInput {
	bool jump : 1;
	bool movF : 1;
	bool movB : 1;
	bool movL : 1;
	bool movR : 1;
	PxVec3 target;
	PlayerInput() : jump(false), movF(false), movB(false), movL(false), movR(false), target(PxZero) {};
};
