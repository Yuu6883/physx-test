#pragma once

#include <cinttypes>

// Flags
constexpr uint8_t PROTO_VER[3] = { 0, 0, 1 };

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
constexpr uint8_t UNK_T = 63;