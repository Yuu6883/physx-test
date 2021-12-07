#pragma once

#ifdef _WIN32
// The conformant preprocessor along with the newest SDK throws this warning for
// a macro in C mode. As users might run into this exact bug, exclude this
// warning here. This is not an MsQuic bug but a Windows SDK bug.
#pragma warning(disable:5105)
#endif

#include <msquic.h>

#include <string>
#include <stdlib.h>

#include "message.hpp"

using std::string;

class QuicClient : public LengthPrefix {
	HQUIC conn;
public:
	struct SendReq {
		uint32_t len;
		QUIC_BUFFER* buffers;
		bool freeAfterSend;
		~SendReq() {
			if (freeAfterSend) {
				for (uint32_t i = 0; i < len; i++) free(buffers[i].Buffer);
			}
			delete[] buffers;
		}
	};

	HQUIC stream;
	static int init(bool insecure);
	static void cleanup();

	QuicClient() : LengthPrefix(1024 * 1024), conn(nullptr), stream(nullptr) {};

	~QuicClient() { disconnect(); };

	bool connect(string host, uint16_t port);
	void disconnect();

	bool send(string_view buffer, bool freeAfterSend);

	virtual void onError() {};
	virtual void onData(string_view buffer) {};
};