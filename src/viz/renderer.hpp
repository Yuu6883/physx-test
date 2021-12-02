#pragma once

#include "../world/world.hpp"
#include <string>
#include <vector>

using std::vector;

class Renderer {

    World* currentWorld;
    
    int lastMouseX = 0;
    int lastMouseY = 0;
    float rotationX = 30.f;
    float rotationY = 130.f;
    float fps = 0;
    int startTime = 0;
    int totalFrames = 0;
    int state = 1;
    float dist = -40;

    int windowW;
    int windowH;
    int windowHandle;

    bool running = false;

    void onMouseButton(int button, int mode, int x, int y);
    void onMouseMove(int x, int y);
    void onResize(int w, int h);
    void onRender();

    void setupRenderState();

    void ortho(int w, int h);
    void perspective();

    void renderString(int x, int y, int space, void* font, std::string&& text);
    void renderAxes();
    void renderGrid(float size);
    void renderGeometry(const PxGeometryHolder& obj);
    void renderActors(const vector<PxRigidActor*>& actors, bool shadow = false, const PxVec3& color = PxVec3(0.0f, 0.75f, 0.0f));
    void renderWorld();

    static void timer(int value, void* self);
public:
    Renderer(World* world);
    ~Renderer();

    void loop();
};