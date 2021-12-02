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

using std::string;

class QuicServer {
	HQUIC listener;
public:
	static int init();
	static void cleanup();

	QuicServer() : listener(nullptr) {};
	~QuicServer() { stop(); };

	bool listen(uint16_t port);
	bool stop();
};