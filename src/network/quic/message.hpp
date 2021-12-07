#pragma once

#include <cinttypes>
#include <memory>

#include <thread>
#include <string_view>
using std::string_view;

class LengthPrefix {
	uint64_t cursor;
	uint64_t target;
	uint64_t maxRecv;
	
	char* pool;

	union {
		uint64_t value;
		uint8_t bytes[8];
	} len_header;
	uint64_t len_header_offset;

public:

	LengthPrefix(uint64_t maxRecv) : maxRecv(maxRecv) {
		cursor = 0;
		target = 0;
		len_header_offset = 0;
		len_header.value = 0;
		pool = (char*) malloc(maxRecv);
	}

	virtual ~LengthPrefix() {
		free(pool);
	}

	void recv(uint8_t* buf, uint64_t buf_len) {
		if (!buf_len) return;

		// printf("[recv thread=%u] buf_len = %lu, target: [%lu/%lu]\n", std::this_thread::get_id(), buf_len, cursor, target);

		if (target) {
			uint64_t required = target - cursor;

			// Received more than enough for current call back
			if (buf_len >= required) {
				// printf("[recv] onData\b");

				// Nothing in pool to concat so there's no need to memcpy
				if (!cursor) {
					onData(string_view((char*) buf, target));
				} else {
					// Concat with buffer from the pool
					memcpy(&pool[cursor], buf, required);
					// Now we have a complete message
					onData(string_view(pool, target));
				}

				// Reset target and cursor
				cursor = 0;
				target = 0;
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
			if (len_header_offset) {
				// printf("[recv] read partial length header [1]\n");

				auto toRead = 8 - len_header_offset;
				// Still not enough to determine target value
				if (buf_len < toRead) {
					memcpy(&len_header.bytes[len_header_offset], buf, buf_len);
					len_header_offset += buf_len;
				} else {
					// Read a few bytes and determine target value
					memcpy(&len_header.bytes[len_header_offset], buf, toRead);
					if (len_header.value > maxRecv) return onError(len_header.value);
					target = len_header.value;
					// printf("[0] target = %lu\n", target);
					len_header_offset = 0;

					recv(buf + toRead, buf_len - toRead);
				}
			} else {
				// Not enough to read target value still
				if (buf_len < 8) {
					// printf("[recv] read partial length header [0]\n");

					memcpy(len_header.bytes, buf, buf_len);
					len_header_offset = buf_len;
				} else {
					// Read target from first 8 bytes in buf
					auto value = *((uint64_t*) buf);
					if (value > maxRecv) return onError(value);
					target = value;
					// printf("[1] target = %lu\n", target);

					recv(buf + 8, buf_len - 8);
				}
			}
		}
	}

	virtual void onData(string_view buffer) = 0;
	virtual void disconnect() = 0;

	void onError(uint64_t b) {
		printf("[error] buffer overflow: %lu\n", b);
		disconnect();
	};
};