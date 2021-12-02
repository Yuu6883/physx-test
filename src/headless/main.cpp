#ifndef NDEBUG
#define NDEBUG
#endif
#include "../world/world.hpp"

int main() {
    auto error = World::init();
    if (error) return error;

    auto world = new World();
    world->initScene();

    for (int i = 0; i < 100; i++) {
        world->step(0);
    }

    delete world;

    World::cleanup();
	return 0;
}