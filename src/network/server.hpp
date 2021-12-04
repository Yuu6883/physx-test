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
#include <list>
#include <mutex>
#include <atomic>

using std::string;
using std::list;
using std::string_view;
using std::mutex;
using std::atomic;

class QuicServer {
	HQUIC listener;

public:
	struct Connection {
		QuicServer* server; // terrible design but no other way around ):
		HQUIC conn;
		HQUIC stream; // can be list of streams
	};

	struct RefCounter {
		atomic<uint32_t> ref;
		virtual ~RefCounter() {};
	};

	struct BroadcastReq : RefCounter {
		uint32_t len;
		QUIC_BUFFER* buffers;
		bool freeAfterSend;
		~BroadcastReq() {
			if (freeAfterSend) delete[] buffers;
			printf("Delete broadcast req\n");
		}
	};

	mutex m; // Guards connections
	list<Connection*> connections;

	QuicServer() : listener(nullptr) {};
	~QuicServer() { stop(); };

	bool listen(uint16_t port);
	bool stop();

	void broadcast(string_view buffer, bool freeAfterSend);

	static int init();
	static void cleanup();
};