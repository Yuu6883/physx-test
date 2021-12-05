#include "renderer.hpp"
#include <GL/freeglut.h>

#include <iomanip>
#include <sstream>
#include <chrono>
#include <thread>

using namespace std::chrono;

static float cylinderData[] = {
	1.0f,0.0f,1.0f,1.0f,0.0f,1.0f,1.0f,0.0f,0.0f,1.0f,0.0f,0.0f,
	0.866025f,0.500000f,1.0f,0.866025f,0.500000f,1.0f,0.866025f,0.500000f,0.0f,0.866025f,0.500000f,0.0f,
	0.500000f,0.866025f,1.0f,0.500000f,0.866025f,1.0f,0.500000f,0.866025f,0.0f,0.500000f,0.866025f,0.0f,
	-0.0f,1.0f,1.0f,-0.0f,1.0f,1.0f,-0.0f,1.0f,0.0f,-0.0f,1.0f,0.0f,
	-0.500000f,0.866025f,1.0f,-0.500000f,0.866025f,1.0f,-0.500000f,0.866025f,0.0f,-0.500000f,0.866025f,0.0f,
	-0.866025f,0.500000f,1.0f,-0.866025f,0.500000f,1.0f,-0.866025f,0.500000f,0.0f,-0.866025f,0.500000f,0.0f,
	-1.0f,-0.0f,1.0f,-1.0f,-0.0f,1.0f,-1.0f,-0.0f,0.0f,-1.0f,-0.0f,0.0f,
	-0.866025f,-0.500000f,1.0f,-0.866025f,-0.500000f,1.0f,-0.866025f,-0.500000f,0.0f,-0.866025f,-0.500000f,0.0f,
	-0.500000f,-0.866025f,1.0f,-0.500000f,-0.866025f,1.0f,-0.500000f,-0.866025f,0.0f,-0.500000f,-0.866025f,0.0f,
	0.0f,-1.0f,1.0f,0.0f,-1.0f,1.0f,0.0f,-1.0f,0.0f,0.0f,-1.0f,0.0f,
	0.500000f,-0.866025f,1.0f,0.500000f,-0.866025f,1.0f,0.500000f,-0.866025f,0.0f,0.500000f,-0.866025f,0.0f,
	0.866026f,-0.500000f,1.0f,0.866026f,-0.500000f,1.0f,0.866026f,-0.500000f,0.0f,0.866026f,-0.500000f,0.0f,
	1.0f,0.0f,1.0f,1.0f,0.0f,1.0f,1.0f,0.0f,0.0f,1.0f,0.0f,0.0f
};

#define MAX_NUM_MESH_VEC3S 1024
static PxVec3 vertexBuffer[MAX_NUM_MESH_VEC3S];

Renderer::Renderer(World* world) : currentWorld(world) {

	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(1280, 720);

    windowW = 1280;
    windowH = 720;
    windowHandle = glutCreateWindow("PhysX Debug Renderer");

    glutDisplayFuncUcall([](void* self) {
        static_cast<Renderer*>(self)->onRender();
    }, this);

    glutReshapeFuncUcall([](int w, int h, void* self) {
        static_cast<Renderer*>(self)->onResize(w, h);
    }, this);

    glutMouseFuncUcall([] (int button, int mode, int x, int y, void* self) {
        static_cast<Renderer*>(self)->onMouseButton(button, mode, x, y);
    }, this);

    glutMotionFuncUcall([] (int x, int y, void* self) {
        static_cast<Renderer*>(self)->onMouseMove(x, y);
    }, this);

	setupRenderState();
}

void Renderer::timer(int value, void* self) {
	if (glutGetWindow()) {
		glutPostRedisplay();
		glutTimerFuncUcall(1000.f / 60, timer, 0, self);
	}
}

void Renderer::setupRenderState() {
    // Setup default render states
	glClearColor(0.3f, 0.4f, 0.5f, 1.0);
	glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_COLOR_MATERIAL);

	// Setup lighting
	glEnable(GL_LIGHTING);
	PxReal ambientColor[] = { 0.0f, 0.1f, 0.2f, 0.0f };
	PxReal diffuseColor[] = { 1.0f, 1.0f, 1.0f, 0.0f };
	PxReal specularColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	PxReal position[] = { 100.0f, 100.0f, 400.0f, 1.0f };
	glLightfv(GL_LIGHT0, GL_AMBIENT, ambientColor);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseColor);
	glLightfv(GL_LIGHT0, GL_SPECULAR, specularColor);
	glLightfv(GL_LIGHT0, GL_POSITION, position);
	glEnable(GL_LIGHT0);
}

void Renderer::onMouseButton(int button, int mode, int x, int y) {
    if (mode == GLUT_DOWN) {
        lastMouseX = x;
        lastMouseY = y;
    }

    if (button == GLUT_MIDDLE_BUTTON) {
        state = 0;
    } else {
        state = 1;
    }
}

void Renderer::onMouseMove(int x, int y) {
    if (state == 0) {
        dist *= (1 + (y - lastMouseY) / 60.f);
    } else {
        rotationY += (x - lastMouseX) / 5.f;
        rotationX += (y - lastMouseY) / 5.f;
    }

    lastMouseX = x;
    lastMouseY = y;
}

void Renderer::onResize(int w, int h) {
    windowW = w;
    windowH = h;

    glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(75, (GLfloat) w / (GLfloat) h, 0.1f, 250.0f);
	glMatrixMode(GL_MODELVIEW);
}

void Renderer::onRender() {
    totalFrames++;
    int current = glutGet(GLUT_ELAPSED_TIME);
	float elapsedTime = float(current - startTime);
	static float lastfpsTime = 0.0f;
	if ((current - lastfpsTime) > 1000) {
		fps = ((totalFrames * 1000) / ((current - lastfpsTime)));
		totalFrames = 0;
		lastfpsTime = current;
	}
	startTime = current;

    std::stringstream stream;
    stream << "FPS: " << std::fixed << std::setprecision(2) << fps;
    auto fpsStr = stream.str();

	// Setup GL
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();
	glTranslatef(0, 0, dist);
	glRotatef(rotationX, 1, 0, 0);
	glRotatef(rotationY, 0, 1, 0);

	// Render grid and axes
	renderAxes();
	renderGrid(250);
	// Render PhysX objects
	glEnable(GL_LIGHTING);
	renderWorld();
	glDisable(GL_LIGHTING);

	// Show the fps
	ortho(windowW, windowH);
	glColor3f(1, 1, 1);
	renderString(10, 20, 0, GLUT_BITMAP_HELVETICA_12, fpsStr);
	// renderString(10, 40, 0, GLUT_BITMAP_HELVETICA_12, tickStr);

	perspective();

	// Finish render
	glutSwapBuffers();
}

void Renderer::ortho(int w, int h) {
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0, w, 0, h);
	glScalef(1, -1, 1);
	glTranslatef(0, GLfloat(-h), 0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void Renderer::perspective() {
    glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}

void Renderer::renderString(int x, int y, int space, void* font, std::string text) {
	for (int i = 0; i < text.length(); i++) {
		glRasterPos2i(x, y);
		glutBitmapCharacter(font, text.at(i));
		x += glutBitmapWidth(font, text.at(i)) + space;
	}
}

void Renderer::renderAxes() {
    // To prevent the view from disturbed on repaint
	// this push matrix call stores the current matrix state
	// and restores it once we are done with the arrow rendering
	glPushMatrix();
	glColor3f(0, 0, 1);
	glPushMatrix();
	glTranslatef(0, 0, 0.8f);
	glutSolidCone(0.0325, 0.2, 4, 1);

	// Draw label			
	glTranslatef(0, 0.0625, 0.225f);
	renderString(0, 0, 0, GLUT_BITMAP_HELVETICA_10, "Z");
	glPopMatrix();
	glutSolidCone(0.0225, 1, 4, 1);

	glColor3f(1, 0, 0);
	glRotatef(90, 0, 1, 0);
	glPushMatrix();
	glTranslatef(0, 0, 0.8f);
	glutSolidCone(0.0325, 0.2, 4, 1);

	// Draw label
	glTranslatef(0, 0.0625, 0.225f);
	renderString(0, 0, 0, GLUT_BITMAP_HELVETICA_10, "X");
	glPopMatrix();
	glutSolidCone(0.0225, 1, 4, 1);

	glColor3f(0, 1, 0);
	glRotatef(90, -1, 0, 0);
	glPushMatrix();
	glTranslatef(0, 0, 0.8f);
	glutSolidCone(0.0325, 0.2, 4, 1);

	// Draw label
	glTranslatef(0, 0.0625, 0.225f);
	renderString(0, 0, 0, GLUT_BITMAP_HELVETICA_10, "Y");
	glPopMatrix();
	glutSolidCone(0.0225, 1, 4, 1);
	glPopMatrix();
}

void Renderer::renderGrid(float size) {
    glBegin(GL_LINES);
	glColor4f(0.75f, 0.75f, 0.75f, 0.5f);
	for (float i = -size; i <= size; i++)
	{
		glVertex3f(i, 0, -size);
		glVertex3f(i, 0, size);

		glVertex3f(-size, 0, i);
		glVertex3f(size, 0, i);
	}
	glEnd();
}

void Renderer::renderGeometry(const PxGeometryHolder& obj) {
	switch (obj.getType()) {
		case PxGeometryType::eBOX:
			glScalef(obj.box().halfExtents.x, obj.box().halfExtents.y, obj.box().halfExtents.z);
			glutSolidCube(2.0);
			break;
		case PxGeometryType::eSPHERE:
			glutSolidSphere(GLdouble(obj.sphere().radius), 100, 100);
			break;
		case PxGeometryType::eCAPSULE:
		case PxGeometryType::eCONVEXMESH:
		case PxGeometryType::eTRIANGLEMESH:
		case PxGeometryType::eINVALID:
		case PxGeometryType::eHEIGHTFIELD:
		case PxGeometryType::eGEOMETRY_COUNT:
		case PxGeometryType::ePLANE:
			break;
	}
}

void Renderer::renderActors(const vector<PxRigidActor*>& actors, bool shadow, const PxVec3& color) {
	vector<PxShape*> shapes;

	for (auto& actor : actors) {
		const PxU32 nbShapes = actor->getNbShapes();
		shapes.reserve(nbShapes);
		shapes.resize(nbShapes);

		actor->getShapes(shapes.data(), nbShapes);
		bool sleeping = actor->is<PxRigidDynamic>() ? actor->is<PxRigidDynamic>()->isSleeping() : false;

		for (auto& shape : shapes) {

			const PxMat44 shapePose(PxShapeExt::getGlobalPose(*shape, *actor));
			PxGeometryHolder h = shape->getGeometry();

			if (shape->getFlags() & PxShapeFlag::eTRIGGER_SHAPE) {
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			}

			if (h.getType() == PxGeometryType::eSPHERE) {

				auto t = actor->is<PxRigidDynamic>()->getGlobalPose();

				// Smallest three ???

				uint8_t index = 0;
				float ab[4] = { fabsf(t.q.x), fabsf(t.q.y), fabsf(t.q.z), fabsf(t.q.w) };

				float max = ab[0];
				for (uint8_t i = 1; i < 4; i++) {
					if (ab[i] > max) {
						max = ab[i];
						index = i;
					}
				}

				// printf("%.5f, %.5f, %.5f, %.5f, max_index = %u\n", t.q.x, t.q.y, t.q.z, t.q.w, index);
			}

			// Render object
			glPushMatrix();
			glMultMatrixf(reinterpret_cast<const float*>(&shapePose));
			if (sleeping) {
				PxVec3 darkColor = color * 0.5f;
				glColor4f(darkColor.x, darkColor.y, darkColor.z, 1.0f);
			} else {
				glColor4f(color.x, color.y, color.z, 1.0f);
			}

			renderGeometry(h);
			glPopMatrix();

			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

			if (shadow) {
				const PxVec3 shadowDir(0.0f, -0.7071067f, -0.7071067f);
				const PxReal shadowMat[] = { 1, 0, 0, 0, -shadowDir.x / shadowDir.y, 0, -shadowDir.z / shadowDir.y, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
				glPushMatrix();
				glMultMatrixf(shadowMat);
				glMultMatrixf(reinterpret_cast<const float*>(&shapePose));
				glDisable(GL_LIGHTING);
				glColor4f(0.1f, 0.2f, 0.3f, 1.0f);
				renderGeometry(h);
				glEnable(GL_LIGHTING);
				glPopMatrix();
			}
		}
	}
}

void Renderer::renderWorld() {
	if (!currentWorld) return;
	auto scene = currentWorld->getScene();

	auto nbActors = scene->getNbActors(PxActorTypeFlag::eRIGID_DYNAMIC | PxActorTypeFlag::eRIGID_STATIC);
	if (nbActors) {
		std::vector<PxRigidActor*> actors(nbActors);
		scene->getActors(PxActorTypeFlag::eRIGID_DYNAMIC | PxActorTypeFlag::eRIGID_STATIC, reinterpret_cast<PxActor**>(actors.data()), nbActors);
		renderActors(actors);
	}
}

void Renderer::loop() {
	running = true;
	timer(0, this);
    glutMainLoop();
	running = false;
}

Renderer::~Renderer() {

}