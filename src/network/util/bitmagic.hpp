#pragma once

#include <cinttypes>
#include <PxPhysicsAPI.h>
#include <PxPhysicsVersion.h>
#include <PxPhysics.h>

#undef min
#include <algorithm>
#include <random>

namespace bitmagic {
	namespace {
		inline uint16_t&& fixed_16fe(const float& in) {
			//        1 sign bit    |  15 fixed float bits
			return ((in < 0) << 15) | (int(roundf(std::min(fabsf(in), 511.f) * 64.f)) & 32767);
		}

		inline float&& fixed_16fd(const uint16_t& in) {
			//        1 sign bit               |  15 fixed float bits
			return ((in & (1 << 15)) ? -1 : 1) * ((in & 32767) * 0.015625f);
		}
	}

	static inline void vec3_48_encode(const PxVec3& p, uint16_t& x_out, uint16_t& y_out, uint16_t& z_out) {
		x_out = fixed_16fe(p.x);
		y_out = fixed_16fe(p.y);
		z_out = fixed_16fe(p.z);
		// is this SIMD-able hmm
	}

	static inline void vec3_48_decode(PxVec3& p, const uint16_t& x_in, const uint16_t& y_in, const uint16_t& z_in) {
		p.x = fixed_16fd(x_in);
		p.y = fixed_16fd(y_in);
		p.z = fixed_16fd(z_in);
		// is this SIMD-able hmm
	}

	static inline void vec3_48_encode_wb(const PxVec3& p1, PxVec3& p2, uint16_t& x_out, uint16_t& y_out, uint16_t& z_out) {
		vec3_48_encode(p1, x_out, y_out, z_out);
		vec3_48_decode(p2, x_out, y_out, z_out);
	}

	static inline void vec3_24_delta_encode(PxVec3& prev, PxVec3& curr, uint8_t& header, uint8_t& x_out, uint8_t& y_out, uint8_t& z_out) {
		// TODO: impl
	}

	static inline void vec3_24_delta_decode(PxVec3& prev, PxVec3& curr, uint8_t& header, uint8_t& x_in, uint8_t& y_in, uint8_t& z_in) {
		// TODO: impl
	}

	constexpr uint32_t c = (1 << 9) - 1;
	constexpr float m_encode = c * 1.41421356237309504880;
	constexpr float m_decode = 1.f / m_encode;

	static inline uint32_t quat_sm3_encode(PxQuat& q) {
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
		bool s = ptr[index] > 0;

#define sign_bits(n1, n2, n3) (((n1 < 0) << 29) | ((n2 < 0) << 19) | ((n3 < 0) << 9))
#define truc(f) (uint32_t(roundf(m_encode * fabsf(f))) & c)
#define value_bits(n1, n2, n3) ((truc(n1) << 20) | (truc(n2) << 10) | truc(n3))
#define pack(n1, n2, n3) (sign_bits(n1, n2, n3) | value_bits(n1, n2, n3))

		if (index == 0) {
			return out | pack(q.y, q.z, q.w);
		} else if (index == 1) {
			return out | pack(q.x, q.z, q.w);
		} else if (index == 2) {
			return out | pack(q.x, q.y, q.w);
		} else {
			return out | pack(q.x, q.y, q.z);
		}

#undef sign_bits
#undef truc
#undef value_bits
#undef pack

	}

	static inline void quat_sm3_decode(uint32_t& value, PxQuat& q) {
		uint32_t index = value >> 30;

		float sum = 0;
		float t[3];

		// Hopefully unrolled
		for (int i = 0; i < 3; i++) {
			t[2 - i] = ((value >> (i * 10)) & c) * m_decode;
			t[2 - i] *= (value & (1 << (i * 10 - 1))) ? -1 : 1;
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
		gen.seed(0);

		/*
		for (int i = 0; i < 100; i++) {
			float f = dist(gen);
			uint16_t encoded = fixed_16fe(f);
			float decoded = fixed_16fd(encoded);
			printf("%.10f -> %u -> %.10f\n", f, encoded, decoded);
		}
		*/

		for (int i = 0; i < 100; i++) {
			PxQuat q;
			q.x = dist(gen);
			q.y = dist(gen);
			q.z = dist(gen);
			q.w = dist(gen);
			q.normalize();

			uint32_t encode = quat_sm3_encode(q);
			PxQuat o;
			quat_sm3_decode(encode, o);

			printf("In: %+.8f, %+.8f, %+.8f, %+.8f | out: %+.8f, %+.8f, %+.8f, %+.8f\n", 
				q.x, q.y, q.z, q.w, o.x, o.y, o.z, o.w);
		}

	}
}