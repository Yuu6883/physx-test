#pragma once

#include <cinttypes>
#include <memory>

#include <thread>
#include <atomic>
#include <string_view>
#include <lz4.h>

using std::atomic;
using std::string_view;

constexpr uint8_t COMP_NONE = 0;
constexpr uint8_t COMP_LZ4  = 1;

constexpr uint8_t COMP_PROFILE_BITS = 1;

class MessageProtocol {
	uint64_t cursor;

	uint64_t maxRecv;
	uint64_t maxDecomp;
	
	char* pool;
	char* decomp_pool;

	union header_t {
		uint64_t value;
		uint8_t bytes[8];

		inline uint8_t compressionMethod() {
			return (value >> (64 - COMP_PROFILE_BITS)) & ((1 << COMP_PROFILE_BITS) - 1);
		}

		inline uint64_t getLength() {
			return (value << COMP_PROFILE_BITS) >> COMP_PROFILE_BITS;
		}
	};
	
	header_t temp_header;
	header_t header;

	uint64_t temp_header_offset;

public:
	atomic<uint64_t> received_bytes;

	MessageProtocol(uint64_t maxRecv, uint64_t maxDecomp = 0) : maxRecv(maxRecv), maxDecomp(maxDecomp) {
		cursor = 0;
		header.value = 0;
		temp_header_offset = 0;
		temp_header.value = 0;
		received_bytes = 0;

		pool = (char*) malloc(maxRecv);
		decomp_pool = maxDecomp ? (char*) malloc(maxDecomp) : nullptr;
	}

	virtual ~MessageProtocol() {
		free(pool);
		if (decomp_pool) free(decomp_pool);
	}

	void recv(uint8_t* buf, uint64_t buf_len) {
		if (!buf_len) return;

		if (header.value) {
			uint64_t required = header.getLength() - cursor;

			// Received more than enough for current call back
			if (buf_len >= required) {
				// Nothing in pool to concat so there's no need to memcpy
				if (!cursor) {
					auto comp = header.compressionMethod();
					if (comp == COMP_NONE) {
						onData(string_view((char*) buf, header.getLength()));
					} else if (comp == COMP_LZ4) {
						int decomp_size = LZ4_decompress_safe((char*) buf, decomp_pool, header.getLength(), maxDecomp);
						if (decomp_size < 0) onDecompressionFailed();
						else {
							onData(string_view(decomp_pool, decomp_size));
						}
					}

				} else {
					// Concat with buffer from the pool
					memcpy(&pool[cursor], buf, required);

					auto comp = header.compressionMethod();
					// Now we have a complete message
					if (comp == COMP_NONE) {
						onData(string_view(pool, header.getLength()));
					} else if (comp == COMP_LZ4) {
						int decomp_size = LZ4_decompress_safe(pool, decomp_pool, header.getLength(), maxDecomp);
						if (decomp_size < 0) onDecompressionFailed();
						else {
							onData(string_view(decomp_pool, decomp_size));
						}
					}
				}

				// Reset target and cursor
				cursor = 0;
				header.value = 0;
				// Recursively call recv to process rest of the data
				recv(buf + required, buf_len - required);
			} else {
				// printf("[recv] append buffer\n");

				// Append buffer to pool
				memcpy(&pool[cursor], buf, buf_len);
				cursor += buf_len;
			}
		} else {
			// No target so we need to read it from buf

			// Unfinished length
			if (temp_header_offset) {
				auto toRead = sizeof(header_t) - temp_header_offset;
				// Still not enough to determine target value
				if (buf_len < toRead) {
					memcpy(&temp_header.bytes[temp_header_offset], buf, buf_len);
					temp_header_offset += buf_len;
				} else {
					// Read a few bytes and determine target value
					memcpy(&temp_header.bytes[temp_header_offset], buf, toRead);
					auto len = temp_header.getLength();
					if (len > maxRecv) return onBufferOverflow(len);
					header.value = temp_header.value;

					temp_header_offset = 0;

					recv(buf + toRead, buf_len - toRead);
				}
			} else {
				// Not enough to read target value still
				if (buf_len < sizeof(header_t)) {
					memcpy(temp_header.bytes, buf, buf_len);
					temp_header_offset = buf_len;
				} else {
					// Read target from first 8 bytes in buf
					auto temp = *((header_t*) buf);
					auto len = temp.getLength();
					if (len > maxRecv) return onBufferOverflow(len);
					header.value = temp.value;

					recv(buf + 8, buf_len - 8);
				}
			}
		}
	}

	virtual void onData(string_view buffer) = 0;
	virtual void disconnect() = 0;

	virtual void onBufferOverflow(uint64_t b) {
		printf("[error] buffer overflow: %lu\n", b);
		disconnect();
	};

	virtual void onDecompressionFailed() {
		printf("[error] decompression failed\n");
		disconnect();
	};
};