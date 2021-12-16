#include "renderer.hpp"
#include "../../world/world.hpp"

#include <iomanip>
#include <sstream>
#include <chrono>
#include <thread>

using namespace std::chrono;

#define MAX_NUM_MESH_VEC3S 1024
static PxVec3 vertexBuffer[MAX_NUM_MESH_VEC3S];

ServerDebugRenderer::ServerDebugRenderer(World* world) : 
	BaseRenderer("PhysX Debug Renderer"), currentWorld(world) {}

void ServerDebugRenderer::renderGeometry(const PxGeometryHolder& obj) {
	switch (obj.getType()) {
		case PxGeometryType::eBOX:
			cube(obj.box().halfExtents);
			break;
		case PxGeometryType::eSPHERE:
			sphere(obj.sphere().radius);
			break;
		case PxGeometryType::eCAPSULE:
			capsule(obj.capsule().radius, obj.capsule().halfHeight);
			break;
		case PxGeometryType::eCONVEXMESH:
		case PxGeometryType::eTRIANGLEMESH:
		case PxGeometryType::eINVALID:
		case PxGeometryType::eHEIGHTFIELD:
		case PxGeometryType::eGEOMETRY_COUNT:
		case PxGeometryType::ePLANE:
			break;
	}
}

void ServerDebugRenderer::renderActors(const vector<PxRigidActor*>& actors, bool shadow, const PxVec3& color) {
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

void ServerDebugRenderer::render() {
	if (!currentWorld) return;
	auto scene = currentWorld->getScene();
	PxSceneReadLock lock(*scene);

	auto QFlags = PxActorTypeFlag::eRIGID_DYNAMIC | PxActorTypeFlag::eRIGID_STATIC;
	auto nbActors = scene->getNbActors(QFlags);
	if (nbActors) {
		std::vector<PxRigidActor*> actors(nbActors);
		scene->getActors(QFlags, reinterpret_cast<PxActor**>(actors.data()), nbActors);
		renderActors(actors);
	}
}
