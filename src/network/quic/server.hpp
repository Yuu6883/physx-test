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

#include "message.hpp"

using std::string;
using std::list;
using std::string_view;
using std::mutex;
using std::atomic;

class QuicServer {
public:

	class Connection : public LengthPrefix {
	public:
		QuicServer* server = nullptr; // terrible design but no other way around ):
		HQUIC conn = nullptr;
		HQUIC stream = nullptr; // can be list of streams

		Connection() : LengthPrefix(1024) {};

		virtual ~Connection() {};

		virtual void onError() {};
		virtual void onConnect() {};
		virtual void onData(string_view buffer) {};
		virtual void onDisconnect() {};

		void send(string_view buffer, bool freeAfterSend);
		void disconnect();
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
			// [0] is always length prefix buffer
			free(buffers[0].Buffer);

			if (freeAfterSend) {
				for (uint32_t i = 1; i < len; i++) free(buffers[i].Buffer);
			}
			delete[] buffers;
		}
	};

	QuicServer() : listener(nullptr) {};
	~QuicServer() { stop(); };

	template<typename SyncCallback>
	inline void sync(const SyncCallback& cb) {
		m.lock();
		cb();
		m.unlock();
	}

	template<typename SyncCallback>
	inline void syncPerConn(const SyncCallback& cb) {
		m.lock();
		for (auto& conn : connections) cb(conn);
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

	list<Connection*> connections;

private:
	HQUIC listener;
	mutex m; // Guards connections
};