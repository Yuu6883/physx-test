#pragma once
#include <GL/freeglut.h>

#include <string>

using std::string;

class BaseRenderer {
    string name;

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

    void setupState();

    void ortho(int w, int h);
    void perspective();

    void renderString(int x, int y, int space, void* font, string text);
    void renderAxes();
    void renderGrid(float size);

    static void timer(int value, void* self);
public:
    BaseRenderer(string name = "Base Renderer");
    virtual ~BaseRenderer();

    virtual void loop();
    virtual void render() {};
};