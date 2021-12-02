#include <GL/freeglut.h>
#include "viz/renderer.hpp"

int main(int argc, char** argv) {

    glutInit(&argc, argv);

    auto error = World::init();
    if (error) return error;

    auto world = new World();
    world->initScene();

    auto renderer = new Renderer(world);

    renderer->loop();

    delete renderer;
    delete world;

    World::cleanup();

    return EXIT_SUCCESS;
}