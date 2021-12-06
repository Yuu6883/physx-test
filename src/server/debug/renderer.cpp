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

void ServerDebugRenderer::render() {
	if (!currentWorld) return;
	auto scene = currentWorld->getScene();

	auto nbActors = scene->getNbActors(PxActorTypeFlag::eRIGID_DYNAMIC | PxActorTypeFlag::eRIGID_STATIC);
	if (nbActors) {
		std::vector<PxRigidActor*> actors(nbActors);
		scene->getActors(PxActorTypeFlag::eRIGID_DYNAMIC | PxActorTypeFlag::eRIGID_STATIC, reinterpret_cast<PxActor**>(actors.data()), nbActors);
		renderActors(actors);
	}
}
