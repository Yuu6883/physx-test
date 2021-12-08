#include <thread>

#include "../misc/repl.hpp"
#include "../server/game.hpp"
#include "../server/debug/renderer.hpp"
#include "../network/util/bitmagic.hpp"

using std::thread;

int main(int argc, char** argv) {
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

    auto t = new thread([&] {
        glutInit(&argc, argv);
        auto renderer = new ServerDebugRenderer(server->world);
        renderer->loop();
        std::raise(SIGINT);
    });

    server->run(tick, 100);

    delete server;
    delete t;

    World::cleanup();
    QuicServer::cleanup();
    return 0;
}