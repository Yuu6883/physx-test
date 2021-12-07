#pragma once

#include <uv.h>

#include "../../client/base.hpp"
#include "../util/reader.hpp"

#include <chrono>
using namespace std::chrono;

template<typename T>
void BaseClient<T>::onData(string_view buffer) {

	uint64_t now = uv_hrtime();
	auto dt = (now - last_packet) / 1000000.f;
	// printf("dt = %5.5f ms, %lu bytes\n", dt, buffer.size());
	last_packet = now;

	bool error = false;
	Reader r(buffer, error);

	uint8_t remote_ver[3] = { r.read<uint8_t>(), r.read<uint8_t>(), r.read<uint8_t>() };

	int64_t remote_now = r.read<int64_t>();

	int64_t local = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	printf("%lu bytes | ping %li ms\n", buffer.size(), local - remote_now);
}