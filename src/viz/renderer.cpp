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

	stream.str(std::string());
	if (currentWorld) {
		auto start = high_resolution_clock::now();
		currentWorld->step(elapsedTime * 0.001f);
		auto end = high_resolution_clock::now();

		stream << "Tick: " << std::fixed << std::setprecision(2) << duration_cast<microseconds>(end - start).count() / 1000.f << " ms";
	} else {
		stream << "No world attached to renderer";
	}

	auto tickStr = stream.str();

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
	renderString(10, 40, 0, GLUT_BITMAP_HELVETICA_12, tickStr);

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
		case PxGeometryType::eBOX: {
			glScalef(obj.box().halfExtents.x, obj.box().halfExtents.y, obj.box().halfExtents.z);
			glutSolidCube(2.0);
			break;
		}
		case PxGeometryType::eSPHERE: {
			glutSolidSphere(GLdouble(obj.sphere().radius), 100, 100);
			break;
		}
		case PxGeometryType::eCAPSULE: {

			const PxF32 radius = obj.capsule().radius;
			const PxF32 halfHeight = obj.capsule().halfHeight;

			//Sphere
			glPushMatrix();
			glTranslatef(halfHeight, 0.0f, 0.0f);
			glScalef(radius, radius, radius);
			glutSolidSphere(1, 10, 10);
			glPopMatrix();

			// Sphere
			glPushMatrix();
			glTranslatef(-halfHeight, 0.0f, 0.0f);
			glScalef(radius, radius, radius);
			glutSolidSphere(1, 10, 10);
			glPopMatrix();

			// Cylinder
			glPushMatrix();
			glTranslatef(-halfHeight, 0.0f, 0.0f);
			glScalef(2.0f * halfHeight, radius, radius);
			glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
			glEnableClientState(GL_VERTEX_ARRAY);
			glEnableClientState(GL_NORMAL_ARRAY);
			glVertexPointer(3, GL_FLOAT, 2 * 3 * sizeof(float), cylinderData);
			glNormalPointer(GL_FLOAT, 2 * 3 * sizeof(float), cylinderData + 3);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 13 * 2);
			glDisableClientState(GL_VERTEX_ARRAY);
			glDisableClientState(GL_NORMAL_ARRAY);
			glPopMatrix();
			break;
		}
		case PxGeometryType::eCONVEXMESH: {

			// Compute triangles for each polygon.
			const PxVec3 scale = obj.convexMesh().scale.scale;
			PxConvexMesh* mesh = obj.convexMesh().convexMesh;
			const PxU32 nbPolys = mesh->getNbPolygons();
			const PxU8* polygons = mesh->getIndexBuffer();
			const PxVec3* verts = mesh->getVertices();
			PxU32 nbVerts = mesh->getNbVertices();
			PX_UNUSED(nbVerts);

			PxU32 numTotalTriangles = 0;
			for (PxU32 i = 0; i < nbPolys; i++) {
				PxHullPolygon data;
				mesh->getPolygonData(i, data);

				const PxU32 nbTris = PxU32(data.mNbVerts - 2);
				const PxU8 vref0 = polygons[data.mIndexBase + 0];
				PX_ASSERT(vref0 < nbVerts);
				for (PxU32 j = 0; j < nbTris; j++) {
					const PxU32 vref1 = polygons[data.mIndexBase + 0 + j + 1];
					const PxU32 vref2 = polygons[data.mIndexBase + 0 + j + 2];

					//generate face normal:
					PxVec3 e0 = verts[vref1] - verts[vref0];
					PxVec3 e1 = verts[vref2] - verts[vref0];

					PX_ASSERT(vref1 < nbVerts);
					PX_ASSERT(vref2 < nbVerts);

					PxVec3 fnormal = e0.cross(e1);
					fnormal.normalize();

					if (numTotalTriangles * 6 < MAX_NUM_MESH_VEC3S)
					{
						vertexBuffer[numTotalTriangles * 6 + 0] = fnormal;
						vertexBuffer[numTotalTriangles * 6 + 1] = verts[vref0];
						vertexBuffer[numTotalTriangles * 6 + 2] = fnormal;
						vertexBuffer[numTotalTriangles * 6 + 3] = verts[vref1];
						vertexBuffer[numTotalTriangles * 6 + 4] = fnormal;
						vertexBuffer[numTotalTriangles * 6 + 5] = verts[vref2];
						numTotalTriangles++;
					}
				}
			}
			glPushMatrix();
			glScalef(scale.x, scale.y, scale.z);
			glEnableClientState(GL_NORMAL_ARRAY);
			glEnableClientState(GL_VERTEX_ARRAY);
			glNormalPointer(GL_FLOAT, 2 * 3 * sizeof(float), vertexBuffer);
			glVertexPointer(3, GL_FLOAT, 2 * 3 * sizeof(float), vertexBuffer + 1);
			glDrawArrays(GL_TRIANGLES, 0, int(numTotalTriangles * 3));
			glPopMatrix();
			break;
		}
		case PxGeometryType::eTRIANGLEMESH: {
			const PxTriangleMeshGeometry& triGeom = obj.triangleMesh();
			const PxTriangleMesh& mesh = *triGeom.triangleMesh;
			const PxVec3 scale = triGeom.scale.scale;

			const PxU32 triangleCount = mesh.getNbTriangles();
			const PxU32 has16BitIndices = mesh.getTriangleMeshFlags() & PxTriangleMeshFlag::e16_BIT_INDICES;
			const void* indexBuffer = mesh.getTriangles();

			const PxVec3* verts = mesh.getVertices();

			const PxU32* intIndices = reinterpret_cast<const PxU32*>(indexBuffer);
			const PxU16* shortIndices = reinterpret_cast<const PxU16*>(indexBuffer);
			PxU32 numTotalTriangles = 0;
			for (PxU32 i = 0; i < triangleCount; ++i) {
				PxVec3 triVert[3];

				if (has16BitIndices) {
					triVert[0] = verts[*shortIndices++];
					triVert[1] = verts[*shortIndices++];
					triVert[2] = verts[*shortIndices++];
				} else {
					triVert[0] = verts[*intIndices++];
					triVert[1] = verts[*intIndices++];
					triVert[2] = verts[*intIndices++];
				}

				PxVec3 fnormal = (triVert[1] - triVert[0]).cross(triVert[2] - triVert[0]);
				fnormal.normalize();

				if (numTotalTriangles * 6 < MAX_NUM_MESH_VEC3S) {
					vertexBuffer[numTotalTriangles * 6 + 0] = fnormal;
					vertexBuffer[numTotalTriangles * 6 + 1] = triVert[0];
					vertexBuffer[numTotalTriangles * 6 + 2] = fnormal;
					vertexBuffer[numTotalTriangles * 6 + 3] = triVert[1];
					vertexBuffer[numTotalTriangles * 6 + 4] = fnormal;
					vertexBuffer[numTotalTriangles * 6 + 5] = triVert[2];
					numTotalTriangles++;
				}
			}
			glPushMatrix();
			glScalef(scale.x, scale.y, scale.z);
			glEnableClientState(GL_NORMAL_ARRAY);
			glEnableClientState(GL_VERTEX_ARRAY);
			glNormalPointer(GL_FLOAT, 2 * 3 * sizeof(float), vertexBuffer);
			glVertexPointer(3, GL_FLOAT, 2 * 3 * sizeof(float), vertexBuffer + 1);
			glDrawArrays(GL_TRIANGLES, 0, int(numTotalTriangles * 3));
			glDisableClientState(GL_VERTEX_ARRAY);
			glDisableClientState(GL_NORMAL_ARRAY);
			glPopMatrix();
			break;
		}
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
	std::this_thread::sleep_for(seconds{ 3 });
	timer(0, this);
    glutMainLoop();
	running = false;
}

Renderer::~Renderer() {

}