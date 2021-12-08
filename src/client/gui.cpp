#include <uv.h>
#include "gui.hpp"

void GUIClient::Cube::render() {
	glScalef(halfExtents.x, halfExtents.y, halfExtents.z);
	glutSolidCube(2.0);
}

void GUIClient::Sphere::render() {
	glutSolidSphere(GLdouble(radius), 100, 100);
}

void GUIClient::Plane::render() {
}

constexpr float MS_TO_NANO = 1000000.f;

void GUIClient::render() {
	const PxVec3& color = PxVec3(0.75f, 0.75f, 0.75f);
	bool shadow = false;

	float lerp = 0.f;
	uint64_t lastPacket = lastPacketTime();

	if (lastPacket) lerp = (uv_hrtime() - lastPacket) / MS_TO_NANO / 200.f;
	lerp = min(lerp, 1.f);

	sync([&] (auto ptr) {
		auto obj = static_cast<RenderableObject*>(ptr);
		obj->update(1.f / 60, lerp);

		const PxMat44 model(PxTransform(obj->currPos, obj->currQuat));

		glPushMatrix();
		glMultMatrixf(reinterpret_cast<const float*>(&model));
		if (obj->isSleeping()) {
			PxVec3 darkColor = color * 0.5f;
			glColor4f(darkColor.x, darkColor.y, darkColor.z, 1.0f);
		} else {
			glColor4f(color.x, color.y, color.z, 1.0f);
		}
		obj->render();
		glPopMatrix();

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		if (shadow) {
			const PxVec3 shadowDir(0.0f, -0.7071067f, -0.7071067f);
			const PxReal shadowMat[] = { 1, 0, 0, 0, -shadowDir.x / shadowDir.y, 0, -shadowDir.z / shadowDir.y, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
			glPushMatrix();
			glMultMatrixf(shadowMat);
			glMultMatrixf(reinterpret_cast<const float*>(&model));
			glDisable(GL_LIGHTING);
			glColor4f(0.1f, 0.2f, 0.3f, 1.0f);
			obj->render();
			glEnable(GL_LIGHTING);
			glPopMatrix();
		}
	});
}

GUIClient::NetworkedObject* GUIClient::addObj(uint16_t type, uint16_t state, uint16_t flags, Reader& r) {
	if (type == BOX_T) {
		return new Cube(r.read<PxVec3>());
	} else if (type == SPH_T) {
		return new Sphere(r.read<float>());
	} else if (type == PLN_T) {
		return new Plane();
	} else {
		printf("Unknown renderable object type: %u\n", type);
	}

	return new RenderableObject();
};