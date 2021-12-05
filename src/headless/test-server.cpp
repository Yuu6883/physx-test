

#include <thread>
#include <chrono>
#include "../world/world.hpp"
#include "../network/game-server.hpp"
#include "../misc/repl.hpp"

int main() {
    auto error = World::init();
    if (error) return error;
    error = QuicServer::init();
    if (error) return error;

    auto server = new PhysXServer();

    uint16_t port = 6969;
    if (!server->listen(port)) return 1;
    server->world->initScene();

    repl::run();
#ifdef WIN32
    uint64_t tick = 15;
#else
    uint64_t tick = 20;
#endif
    server->run(tick, 100);

    delete server;

    World::cleanup();
    QuicServer::cleanup();
	return 0;
}