#include "base.hpp"

#include <GL/freeglut.h>

#include <iomanip>
#include <sstream>
#include <chrono>
#include <thread>

using namespace std::chrono;

static float CylinderData[] = {
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

BaseRenderer::BaseRenderer(string name, BaseCamera* cam) : name(name), cam(cam) {
	if (!cam) this->cam = new BaseCamera(PxVec3(25.f), PxVec3(-1.f));

	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowSize(1280, 720);

	windowW = 1280;
	windowH = 720;
	windowHandle = glutCreateWindow("PhysX Debug Renderer");

	glutDisplayFuncUcall([](void* self) {
		static_cast<BaseRenderer*>(self)->onRender();
	}, this);

	glutReshapeFuncUcall([](int w, int h, void* self) {
		static_cast<BaseRenderer*>(self)->onResize(w, h);
	}, this);

	glutMouseFuncUcall([](int button, int mode, int x, int y, void* self) {
		static_cast<BaseRenderer*>(self)->onMouseButton(button, mode, x, y);
	}, this);

	glutMotionFuncUcall([](int x, int y, void* self) {
		static_cast<BaseRenderer*>(self)->onMouseMove(x, y);
	}, this);

	glutKeyboardFuncUcall([](unsigned char key, int, int, void* self) {
		static_cast<BaseRenderer*>(self)->onKeyDown(key);
	}, this);

	glutKeyboardUpFuncUcall([](unsigned char key, int, int, void* self) {
		static_cast<BaseRenderer*>(self)->onKeyUp(key);
	}, this);

	setupState();
}

void BaseRenderer::timer(int value, void* self) {
	if (glutGetWindow()) {
		glutPostRedisplay();
		glutTimerFuncUcall(1000.f / 60, timer, 0, self);
	}
}

void BaseRenderer::setupState() {
	// Setup default render states
	glClearColor(0.3f, 0.4f, 0.5f, 1.0);
	glDepthFunc(GL_LESS);
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_COLOR_MATERIAL);

	// Setup lighting
	glEnable(GL_LIGHTING);
	GLfloat ambientColor[] = { 0.0f, 0.1f, 0.2f, 0.0f };
	GLfloat diffuseColor[] = { 1.0f, 1.0f, 1.0f, 0.0f };
	GLfloat specularColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	GLfloat position[] = { 100.0f, 100.0f, 400.0f, 1.0f };
	glLightfv(GL_LIGHT0, GL_AMBIENT, ambientColor);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseColor);
	glLightfv(GL_LIGHT0, GL_SPECULAR, specularColor);
	glLightfv(GL_LIGHT0, GL_POSITION, position);
	glEnable(GL_LIGHT0);
}

void BaseRenderer::onMouseButton(int button, int mode, int x, int y) {
	cam->mouseMode = mode;
	cam->mouseX = x;
	cam->mouseY = y;
}

void BaseRenderer::onMouseMove(int x, int y) {
	cam->onMouseMove(x, y);
}

void BaseRenderer::onKeyDown(unsigned char key) {
	cam->downKeys.insert(key);
}

void BaseRenderer::onKeyUp(unsigned char key) {
	cam->downKeys.erase(key);
}

void BaseRenderer::onResize(int w, int h) {
	windowW = w;
	windowH = h;

	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(75, (GLfloat)w / (GLfloat)h, 0.1f, 250.0f);
	glMatrixMode(GL_MODELVIEW);
}

void BaseRenderer::onRender() {
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

	for (auto& k : cam->downKeys) cam->onKey(k);

	// Setup GL
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Setup camera
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60.0, GLdouble(glutGet(GLUT_WINDOW_WIDTH)) / GLdouble(glutGet(GLUT_WINDOW_HEIGHT)), 1.f, 1000.f);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	auto& eye = cam->eye;
	auto& dir = cam->dir;
	gluLookAt(GLdouble(eye.x), GLdouble(eye.y), GLdouble(eye.z), 
		GLdouble(eye.x + dir.x), GLdouble(eye.y + dir.y), GLdouble(eye.z + dir.z), 
		0.0, 1.0, 0.0);

	// Render grid and axes
	renderAxes();
	renderGrid(250);
	// Render PhysX objects
	glEnable(GL_LIGHTING);
	render();
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

void BaseRenderer::ortho(int w, int h) {
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0, w, 0, h);
	glScalef(1, -1, 1);
	glTranslatef(0, GLfloat(-h), 0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void BaseRenderer::perspective() {
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}

void BaseRenderer::renderString(int x, int y, int space, void* font, std::string text) {
	for (int i = 0; i < text.length(); i++) {
		glRasterPos2i(x, y);
		glutBitmapCharacter(font, text.at(i));
		x += glutBitmapWidth(font, text.at(i)) + space;
	}
}

void BaseRenderer::renderAxes() {
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

void BaseRenderer::renderGrid(float size) {
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

void BaseRenderer::loop() {
	running = true;
	timer(0, this);
	glutMainLoop();
	running = false;
}

BaseRenderer::~BaseRenderer() {
	if (cam) delete cam;
}

void BaseRenderer::cube(PxVec3 halfExtents) {
	glScalef(halfExtents.x, halfExtents.y, halfExtents.z);
	glutSolidCube(2.0);
}

void BaseRenderer::sphere(float radius) {
	glutSolidSphere(GLdouble(radius), 100, 100);
}

void BaseRenderer::capsule(float radius, float halfHeight) {
	//Sphere
	glPushMatrix();
	glTranslatef(halfHeight, 0.0f, 0.0f);
	glScalef(radius, radius, radius);
	glutSolidSphere(1, 10, 10);
	glPopMatrix();

	//Sphere
	glPushMatrix();
	glTranslatef(-halfHeight, 0.0f, 0.0f);
	glScalef(radius, radius, radius);
	glutSolidSphere(1, 10, 10);
	glPopMatrix();

	//Cylinder
	glPushMatrix();
	glTranslatef(-halfHeight, 0.0f, 0.0f);
	glScalef(2.0f * halfHeight, radius, radius);
	glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	glVertexPointer(3, GL_FLOAT, 2 * 3 * sizeof(float), CylinderData);
	glNormalPointer(GL_FLOAT, 2 * 3 * sizeof(float), CylinderData + 3);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 13 * 2);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glPopMatrix();
}