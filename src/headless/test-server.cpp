

#include <thread>
#include <chrono>
#include "../world/world.hpp"
#include "../network/server.hpp"

int main() {
    auto error = World::init();
    if (error) return error;
    error = QuicServer::init();
    if (error) return error;

    auto server = new QuicServer();

    uint16_t port = 6969;
    if (!server->listen(port)) return 1;

    auto world = new World();
    world->initScene();

    int i = 0;
    while (true) {
        world->step(0);
        std::this_thread::sleep_for(std::chrono::milliseconds{ 1000 });
        constexpr char msg[] = "Hello world";
        auto ptr = (char*) malloc(sizeof(msg));
        strcpy(ptr, msg);
        server->broadcast(string_view(msg, sizeof(msg)), true);
    }

    delete world;
    delete server;

    World::cleanup();
    QuicServer::cleanup();
	return 0;
}