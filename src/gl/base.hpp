#pragma once
#include <GL/freeglut.h>

#include <PxPhysics.h>
#include <PxPhysicsAPI.h>

#include <string>
#include <unordered_set>

using std::string;
using std::unordered_set;

using namespace physx;

static inline PxVec3 UP_DIR = PxVec3(0.f, 1.f, 0.f);

struct BaseCamera {
    PxVec3 eye, dir;
    int mouseMode = 0;
    int mouseX = 0;
    int mouseY = 0;
    float speed = 1;

    unordered_set<unsigned char> downKeys;

    BaseCamera(PxVec3 eye, PxVec3 dir) :  eye(eye), dir(dir) {};

    void onMouseMove(int x, int y) {
        if (mouseMode != GLUT_DOWN) return;

        int dx = (mouseX - x) * 0.5f;
        int dy = (mouseY - y) * 0.5f;

        PxVec3 viewY = dir.cross(UP_DIR).getNormalized();

        PxQuat qx(PxPi * dx / 180.0f, UP_DIR);
        dir = qx.rotate(dir);
        PxQuat qy(PxPi * dy / 180.0f, viewY);
        dir = qy.rotate(dir);

        dir.normalize();

        mouseX = x;
        mouseY = y;
    }

    void onKey(unsigned char k) {
        PxVec3 viewY = dir.cross(UP_DIR).getNormalized();
        switch (toupper(k)) {
            case 'W':	eye += dir * 2.0f * speed;	  break;
            case 'S':	eye -= dir * 2.0f * speed;	  break;
            case 'A':	eye -= viewY * 2.0f * speed;  break;
            case 'D':	eye += viewY * 2.0f * speed;  break;
        }
    }

    void analogMove(float x, float y) {
        PxVec3 viewY = dir.cross(UP_DIR).getNormalized();
        eye += dir * y;
        eye += viewY * x;
    }

    PxTransform transform() {
        PxVec3 viewY = dir.cross(UP_DIR);

        if (viewY.normalize() < 1e-6f)
            return PxTransform(eye);

        PxMat33 m(dir.cross(viewY), viewY, -dir);
        return PxTransform(eye, PxQuat(m));
    }
};

class BaseRenderer {
    string name;

    BaseCamera cam;

    float fps = 0;

    int startTime = 0;
    int totalFrames = 0;

    int windowW;
    int windowH;
    int windowHandle;

    bool running = false;

    void onMouseButton(int button, int mode, int x, int y);
    void onMouseMove(int x, int y);
    void onKeyDown(unsigned char key);
    void onKeyUp(unsigned char up);
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
    virtual void render() = 0;
};