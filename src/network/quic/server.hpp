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

	class Connection : public MessageProtocol {
	public:
		QuicServer* server = nullptr; // terrible design but no other way around ):
		HQUIC conn = nullptr;
		HQUIC stream = nullptr; // can be list of streams

		Connection() : MessageProtocol(1024) {};

		virtual ~Connection() {};

		virtual void onError() {};
		virtual void onConnect() {};
		virtual void onData(string_view buffer) {};
		virtual void onDisconnect() {};

		void send(string_view buffer, bool freeAfterSend, uint8_t compressionMethod = COMP_NONE);
		void disconnect();
	};

	struct RefCounter {
		atomic<uint32_t> ref;
		RefCounter(uint32_t ref) : ref(ref) {};
		virtual ~RefCounter() {};
	};

	struct SendReq : RefCounter {
		bool freeAfterSend;
		QUIC_BUFFER buffers[2];
		uint64_t header;

		SendReq(string_view buf, uint32_t ref, bool freeAfterSend, uint8_t compress) : 
			RefCounter(ref), freeAfterSend(freeAfterSend) {

			header = buf.size() | (uint64_t(compress) << (64 - COMP_PROFILE_BITS));
			buffers[0].Buffer = (uint8_t*) &header;
			buffers[0].Length = sizeof(uint64_t);
			buffers[1].Buffer = (uint8_t*) buf.data();
			buffers[1].Length = buf.size();
		}

		~SendReq() {
			if (freeAfterSend) free(buffers[1].Buffer);
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

	template<typename T, typename SyncCallback>
	inline void syncPerConn(const SyncCallback& cb) {
		m.lock();
		for (auto& conn : connections) cb((T*&) conn);
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

	void broadcast(string_view buffer, bool freeAfterSend, bool compress);
	virtual Connection* client() { return new Connection(); };

	static int init();
	static void cleanup();

	list<Connection*> connections;

private:
	HQUIC listener;
	mutex m; // Guards connections
};