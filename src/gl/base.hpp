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

struct AbstractCamera {
    PxVec3 eye, dir;
    int mouseX = 0;
    int mouseY = 0;
    float speed = 1;

    AbstractCamera(PxVec3 eye, PxVec3 dir) : eye(eye), dir(dir) {};

    virtual ~AbstractCamera() {};

    virtual void onMouseButton(int button, int mode, int x, int y) {};
    
    virtual void onMouseMove(int x, int y) {
#define RAD(n) PxPi * n / 180.0f
        float dx = RAD((mouseX - x) * 0.5f);
        float dy = RAD((mouseY - y) * 0.5f);
#undef RAD

        PxVec3 viewY = dir.cross(UP_DIR).getNormalized();

        PxQuat qx(dx, UP_DIR);
        dir = qx.rotate(dir);

        PxQuat qy(dy, viewY);
        dir = qy.rotate(dir);

        dir.normalize();

        if (fabsf(dir.y) > 0.75f) {
            dir.y = dir.y > 0 ? +0.75f : -0.75f;
            dir.normalize();
        }

        mouseX = x;
        mouseY = y;
    };

    virtual void onKey(unsigned char k) {};

    virtual const PxVec3 getEye() { return eye; };
    virtual const PxVec3 getTarget() { return eye + dir; };
};

struct FreeRoamCamera : AbstractCamera {
    int mouseMode = -1;

    FreeRoamCamera(PxVec3 eye, PxVec3 dir) : AbstractCamera(eye, dir) {};

    void onMouseMove(int x, int y) {
        if (mouseMode != GLUT_DOWN) return;
        AbstractCamera::onMouseMove(x, y);
    }

    void onMouseButton(int button, int mode, int x, int y) {
        mouseMode = mode;
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
};

static inline FreeRoamCamera DefaultCamera(PxVec3(25.f), PxVec3(-1.f));

class BaseRenderer {
    string name;

    float fps = 0;

    int startTime = 0;
    int totalFrames = 0;

    int windowW;
    int windowH;
    int windowHandle;

    bool running = false;

    unordered_set<unsigned char> downKeys;
    unordered_set<int> downSpecialKeys;

    void onResize(int w, int h);
    void onRender();

    void setupState();

    void ortho(int w, int h);
    void perspective();

    void renderAxes();
    void renderGrid(float size);

    static void timer(int value, void* self);

protected:

    virtual AbstractCamera* cam() { return &DefaultCamera; };

    void renderString(int x, int y, int space, string text, void* font = GLUT_BITMAP_HELVETICA_12);
public:
    BaseRenderer(string name = "Base Renderer");
    virtual ~BaseRenderer();

    PxVec2 getDimension() { return PxVec2(windowW, windowH); };

    virtual void loop();
    virtual void render() = 0;
    virtual void postRender();

    virtual void onMouseButton(int button, int mode, int x, int y);
    virtual void onMouseMove(int x, int y);

    virtual void onKeyUp(unsigned char k) {};
    virtual void onKeyDown(unsigned char k) {};
    virtual void onKeyHold(unsigned char k) { cam()->onKey(k); };

    virtual void onSpecialKeyUp(int k) {};
    virtual void onSpecialKeyDown(int k) {};
    virtual void onSpecialKeyHold(int k) {};

    static void cube(PxVec3 halfExtends);
    static void sphere(float radius);
    static void capsule(float radius, float halfHeight);
};