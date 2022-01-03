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
#include <mutex>
#include <condition_variable>

#include "message.hpp"

using std::mutex;
using std::string;
using std::condition_variable;

class QuicClient : public MessageProtocol {
	HQUIC conn;
public:
	condition_variable cv;

	struct SendReq {
		bool freeAfterSend;
		QUIC_BUFFER buffers[2];
		uint64_t header;

		SendReq(string_view buf, bool freeAfterSend, uint8_t compress) : freeAfterSend(freeAfterSend) {
			header = buf.size() | (uint64_t(compress) << (64 - COMP_PROFILE_BITS));
			buffers[0].Buffer = (uint8_t*)&header;
			buffers[0].Length = sizeof(uint64_t);
			buffers[1].Buffer = (uint8_t*)buf.data();
			buffers[1].Length = buf.size();
		}

		~SendReq() {
			if (freeAfterSend) free(buffers[1].Buffer);
		}
	};
	
	// TODO: make stream wrapper
	HQUIC stream;
	static int init(bool insecure);
	static void cleanup();

	QuicClient() : MessageProtocol(2 * 1024 * 1024, 5 * 1024 * 1024), conn(nullptr), stream(nullptr) {};
	~QuicClient() { disconnect(); };

	bool connect(string host, uint16_t port);
	void disconnect();

	bool send(string_view buffer, bool freeAfterSend, uint8_t compression = COMP_NONE);

	virtual void onConnect() {};
	virtual void onDisconnect() { conn = nullptr; };
	virtual void onError() {};
	virtual void onData(string_view buffer) {};

	bool isConnected() { return !!conn; };
};