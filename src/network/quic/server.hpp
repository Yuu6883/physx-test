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
public:
	struct Connection {
		QuicServer* server; // terrible design but no other way around ):
		HQUIC conn;
		HQUIC stream; // can be list of streams

		virtual ~Connection() {};

		virtual void onConnect() {};
		virtual void onData(string_view buffer) {};
		virtual void onDisconnect() {};
		void send(string_view buffer, bool freeAfterSend);
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
			if (freeAfterSend) {
				for (uint32_t i = 0; i < len; i++) free(buffers[i].Buffer);
			}
			delete[] buffers;
		}
	};

	QuicServer() : listener(nullptr) {};
	~QuicServer() { stop(); };

	template<typename SyncCallback>
	inline void sync(const SyncCallback& cb) {
		m.lock();
		cb(connections);
		m.unlock();
	}

	size_t count() {
		m.lock();
		auto size = connections.size();
		m.unlock();
		return size;
	}

	bool listen(uint16_t port);
	bool stop();

	void broadcast(string_view buffer, bool freeAfterSend);
	virtual Connection* client() { return new Connection(); };

	static int init();
	static void cleanup();

private:
	HQUIC listener;
	mutex m; // Guards connections
	list<Connection*> connections;
};