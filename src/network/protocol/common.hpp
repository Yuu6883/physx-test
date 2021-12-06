#pragma once

#include <cinttypes>

// Flags
constexpr uint8_t PROTO_VER[3] = { 0, 0, 1 };
constexpr uint8_t ADD_OBJ_ST = 0 << 6;
constexpr uint8_t ADD_OBJ_DY = 1 << 6;
constexpr uint8_t UPD_OBJ = 2 << 6;
constexpr uint8_t UPD_STATE = 3 << 6;

constexpr uint32_t OBJ_SLEEP = 1;
constexpr uint32_t OBJ_REMOVE = 2;