#pragma once

#ifdef _WIN32
// The conformant preprocessor along with the newest SDK throws this warning for
// a macro in C mode. As users might run into this exact bug, exclude this
// warning here. This is not an MsQuic bug but a Windows SDK bug.
#pragma warning(disable:5105)
#endif

#include <msquic.h>

#include <string>
#include <string_view>
#include <stdlib.h>

using std::string;
using std::string_view;

class QuicClient {
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

	QuicClient() : conn(nullptr), stream(nullptr) {};
	~QuicClient() { disconnect(); };

	bool connect(string host, uint16_t port);
	bool disconnect();

	bool send(string_view buffer, bool freeAfterSend);
	virtual void onData(string_view buffer) {};
};