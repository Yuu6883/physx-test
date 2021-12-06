

#include <thread>
#include <chrono>

#include "../misc/repl.hpp"
#include "../world/world.hpp"
#include "../server/game.hpp"
#include "../server/debug/renderer.hpp"

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