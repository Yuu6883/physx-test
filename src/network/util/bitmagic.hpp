#pragma once

#include <cinttypes>
#include <PxPhysicsAPI.h>
#include <PxPhysicsVersion.h>
#include <PxPhysics.h>

#undef min
#include <algorithm>
#include <random>
#include <chrono>

using namespace std::chrono;

namespace bitmagic {
	namespace {
		inline uint16_t fixed_16fe(const float& in) {
			//        1 sign bit    |  15 fixed float bits
			return ((in < 0) << 15) | (int(roundf(std::min(fabsf(in), 511.f) * 64.f)) & 32767);
		}

		inline float fixed_16fd(const uint16_t& in) {
			//        1 sign bit               |  15 fixed float bits
			return ((in >> 15) ? -1 : 1) * ((in & 32767) * 0.015625f);
		}

		template<uint8_t shift>
		inline void fixed_delta_encode_wb(const float d, uint8_t& header, uint8_t& out, float& wb) {
			out |= ((d < 0) << 7); // sign bit
			float ab = std::min(fabsf(d), 7.5f);
			constexpr uint8_t c = (1 << 7) - 1; // 127 or b01111111
			if (ab < 0.5f) {
				header |= (0 << shift);
				out |= (uint8_t(roundf((ab - 0.f) * 255.f)) & c);
				wb += ((out >> 7) ? -1 : 1) * ((out & c) + 0.f) * (1 / 255.f);
			} else if (ab < 1.5f) {
				header |= (1 << shift);
				out |= (uint8_t(roundf((ab - 0.5f) * 127.f)) & c);
				wb += ((out >> 7) ? -1 : 1) * ((out & c) + 0.5f) * (1 / 127.f);
			} else if (ab < 3.5f) {
				header |= (2 << shift);
				out |= (uint8_t(roundf((ab - 1.5f) * 63.f)) & c);
				wb += ((out >> 7) ? -1 : 1) * ((out & c) + 1.5f) * (1 / 63.f);
			} else {
				header |= (3 << shift);
				out |= (uint8_t(roundf((ab - 3.5f) * 31.f)) & c);
				wb += ((out >> 7) ? -1 : 1) * ((out & c) + 3.5f) * (1 / 31.f);
			}
		}

		template<uint8_t shift>
		inline float fixed_delta_decode(const uint8_t& header, const uint8_t& in) {
			static const float offset[4] = { 0.f, 0.5, 1.5f, 3.5f };
			static const float multi[4]  = { 1 / 255.f, 1 / 127.f, 1 / 63.f, 1 / 31.f };

			constexpr uint8_t c = (1 << 7) - 1; // 127 or b01111111
			
			uint8_t i = (header >> shift) & 3;
			return ((in >> 7) ? -1 : 1) * ((in & c) + offset[i]) * multi[i];
		}
	}

	static inline void vec3_48_encode(const PxVec3& p, uint16_t& x_out, uint16_t& y_out, uint16_t& z_out) {
		x_out = fixed_16fe(p.x);
		y_out = fixed_16fe(p.y);
		z_out = fixed_16fe(p.z);
		// is this SIMD-able hmm
	}

	static inline void vec3_48_decode(PxVec3& out, const uint16_t& x_in, const uint16_t& y_in, const uint16_t& z_in) {
		out.x = fixed_16fd(x_in);
		out.y = fixed_16fd(y_in);
		out.z = fixed_16fd(z_in);
		// is this SIMD-able hmm
	}

	static inline void vec3_48_encode_wb(PxVec3& prev, const PxVec3& curr, uint16_t& x_out, uint16_t& y_out, uint16_t& z_out) {
		vec3_48_encode(curr, x_out, y_out, z_out);
		vec3_48_decode(prev, x_out, y_out, z_out);
	}

	static inline void vec3_24_delta_encode(PxVec3& prev, const PxVec3& curr, uint8_t& header, uint8_t& x_out, uint8_t& y_out, uint8_t& z_out) {
		fixed_delta_encode_wb<4>(curr.x - prev.x, header, x_out, prev.x);
		fixed_delta_encode_wb<2>(curr.y - prev.y, header, y_out, prev.y);
		fixed_delta_encode_wb<0>(curr.z - prev.z, header, z_out, prev.z);
	}

	static inline void vec3_24_delta_decode(const PxVec3& prev, PxVec3& curr, const uint8_t& header, const uint8_t& x_in, const uint8_t& y_in, const uint8_t& z_in) {
		curr.x = prev.x + fixed_delta_decode<4>(header, x_in);
		curr.y = prev.y + fixed_delta_decode<2>(header, y_in);
		curr.z = prev.z + fixed_delta_decode<0>(header, z_in);
	}

	constexpr uint32_t c = (1 << 9) - 1;
	constexpr float m_encode = c * 1.41421356237309504880f;
	constexpr float m_decode = 1.f / m_encode;

	static inline uint32_t quat_sm3_encode(const PxQuat& q) {
		float* ptr = (float*) &q;
		int index = 0;
		float maxV = 0.f;
		// Unroll loop pls
		for (int i = 0; i < 4; i++) {
			auto f = ptr[i];
			auto ab = fabsf(f);
			if (ab > maxV) {
				index = i;
				maxV = ab;
			}
		}

		uint32_t out = index << 30;
		bool s = ptr[index] <= 0;

#define sign_bits(n1, n2, n3) (((n1 < 0) << 29) | ((n2 < 0) << 19) | ((n3 < 0) << 9))
#define truc(f) (uint32_t(roundf(m_encode * fabsf(f))) & c)
#define value_bits(n1, n2, n3) ((truc(n1) << 20) | (truc(n2) << 10) | truc(n3))
#define pack(n1, n2, n3) (sign_bits(n1, n2, n3) | value_bits(n1, n2, n3))

		float n1 = index == 0 ? q.y : q.x;
		float n2 = index <= 1 ? q.z : q.y;
		float n3 = index <= 2 ? q.w : q.z;
		
		// return (out | pack(n1, n2, n3));
		return s ? (out | pack(-n1, -n2, -n3)) : (out | pack(n1, n2, n3));
	
#undef sign_bits
#undef truc
#undef value_bits
#undef pack

	}

	static inline void quat_sm3_decode(PxQuat& q, const uint32_t& value) {
		uint32_t index = value >> 30;

		float sum = 0;
		float t[3];

		// Hopefully unrolled
		for (int i = 0; i < 3; i++) {
			t[2 - i] = ((value >> (i * 10)) & c) * m_decode;
			t[2 - i] *= (value & (1 << (i * 10 + 9))) ? -1 : 1;
			sum += (t[2 - i] * t[2 - i]);
		}

		float l = sqrtf(1 - sum);

#define fill(n1, n2, n3) n1 = t[0]; n2 = t[1]; n3 = t[2]
		if (index == 0) {
			q.x = l;
			fill(q.y, q.z, q.w);
		} else if (index == 1) {
			q.y = l;
			fill(q.x, q.z, q.w);
		} else if (index == 2) {
			q.z = l;
			fill(q.x, q.y, q.w);
		} else {
			q.w = l;
			fill(q.x, q.y, q.z);
		}
#undef fill

	}

	static inline void test() {
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<float> dist(-1, 1);
		// gen.seed(0);

		// fixed point uint16_t  test
		/*
		for (int i = 0; i < 100; i++) {
			float f = dist(gen);
			uint16_t encoded = fixed_16fe(f);
			float decoded = fixed_16fd(encoded);
			printf("%.10f -> %u -> %.10f\n", f, encoded, decoded);
		}
		*/

		// Quat uint32_t test

		constexpr size_t TEST_SIZE = 100;
		PxQuat* arr = new PxQuat[TEST_SIZE];

		for (int t = 0; t < 10; t++) {
			// Randomize
			for (int i = 0; i < TEST_SIZE; i++) {
				PxQuat& q = arr[i];
				q.x = dist(gen);
				q.y = dist(gen);
				q.z = dist(gen);
				q.w = dist(gen);
				q.normalize();
			}

			auto start = high_resolution_clock::now();

			for (int i = 0; i < TEST_SIZE; i++) {
				auto& q = arr[i];
				uint32_t encode = quat_sm3_encode(q);
				PxQuat o;
				quat_sm3_decode(o, encode);

				printf("In: %+.8f, %+.8f, %+.8f, %+.8f | out: %+.8f, %+.8f, %+.8f, %+.8f\n", 
					q.x, q.y, q.z, q.w, o.x, o.y, o.z, o.w);
			}

			auto dt = high_resolution_clock::now() - start;
			printf("dt = %.4fms\n", duration<float, std::milli>(dt).count());
		}

		delete[] arr;

		// Delta uint8_t test
		/*
		for (int i = 0; i < 5; i++) {
			int a = 10;

			PxVec3 prev;
			PxVec3 curr;

			prev.x = dist(gen);
			prev.y = dist(gen);
			prev.z = dist(gen);

			curr.x = prev.x + 9 * dist(gen);
			curr.y = prev.y + 9 * dist(gen);
			curr.z = prev.z + 9 * dist(gen);

			uint8_t header;
			do {
				uint8_t x_out = 0, y_out = 0, z_out = 0;
				header = 0;
				vec3_24_delta_encode(prev, curr, header, x_out, y_out, z_out);
				printf("prev: %+.6f, %+.6f, %+.6f | curr: %+.6f, %+.6f, %+.6f\n", 
					prev.x, prev.y, prev.z, curr.x, curr.y, curr.z);
			} while (header && a-- > 0);
		}
		*/
	}
}