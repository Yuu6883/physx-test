#include <sstream>
#include <uv.h>

#include "gui.hpp"
#include "../network/util/writer.hpp"

void GUIClient::Cube::render() {
	cube(halfExtents);
}

void GUIClient::Sphere::render() {
	sphere(radius);
}

void GUIClient::Plane::render() {
}

void GUIClient::Capsule::render() {
	capsule(r, hh);
}

constexpr float MS_TO_NANO = 1000000.f;

void GUIClient::render() {
	if (!roam) {
		auto dim = getDimension();
		glutWarpPointer(int(dim.x / 2), int(dim.y / 2));
	}

	float lerp = 0.f;
	uint64_t lastPacket = lastPacketTime();

	if (lastPacket) lerp = (uv_hrtime() - lastPacket) / MS_TO_NANO / 200.f;
	lerp = min(lerp, 1.f);

	syncObj([&] (auto ptr) {
		auto obj = static_cast<RenderableObject*>(ptr);
		obj->update(1.f / 60, lerp);
		render(obj);		
	});

	syncPlayer([&] (auto ptr) {
		auto p = static_cast<RenderablePlayer*>(ptr);
		p->update(1.f / 60, lerp);
		render(p);
	});

	auto dt = duration<float, std::milli>(system_clock::now() - lastBandwidthUpdate).count();
	if (dt > 1000.f) {
		lastBandwidthUpdate += milliseconds{ 1000 };

		bandwidth = received_bytes.load() - lastTotalBytes;
		lastTotalBytes = received_bytes.load();
	}
}

void GUIClient::render(RenderableObject* obj) {

	const PxVec3& color = PxVec3(0.75f, 0.75f, 0.75f);
	bool shadow = false;

	const PxMat44 model(PxTransform(obj->currPos, obj->currQuat));

	glPushMatrix();
	glMultMatrixf(reinterpret_cast<const float*>(&model));
	if (obj->isSleeping()) {
		PxVec3 darkColor = color * 0.5f;
		glColor4f(darkColor.x, darkColor.y, darkColor.z, 1.0f);
	}
	else {
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
}

void GUIClient::postRender() {
	BaseRenderer::postRender();

	std::stringstream stream;
	stream << "Bandwidth: ";
	if (bandwidth < 1024) stream << bandwidth << "B/S";
	else {
		stream.precision(3);
		if (bandwidth < 2 * 1024 * 1024) stream << (bandwidth / 1024.f) << "KB/s";
		else stream << (bandwidth / 1024.f / 1024.f) << "MB/s";
	}
	renderString(10, 40, 0, stream.str());

	if (!roam) {
		auto p = me();
		if (p) {
			cam()->eye = p->position();

			std::stringstream stream2;
			stream2 << "PID: " << p->pid;
			renderString(10, 60, 0, stream2.str());
		}
		input.dir = PxVec2(cam()->dir.x, cam()->dir.z);
	}

	Writer w;
	w.write<PlayerInput>(input);
	send(w.finalize(), true);
}

GUIClient::NetworkedObject* GUIClient::addObj(uint16_t type, uint16_t state, uint16_t flags, Reader& r) {
	if (type == BOX_T) {
		return new Cube(r.read<PxVec3>());
	} else if (type == SPH_T) {
		return new Sphere(r.read<float>());
	} else if (type == PLN_T) {
		return new Plane();
	} else if (type == CPS_T) {
		return new Capsule(r.read<float>(), r.read<float>());
	} else {
		printf("Unknown renderable object type: %u\n", type);
	}

	return new RenderableObject();
};

void GUIClient::sendInput() {
	Writer w;
	w.write<PlayerInput>(input);
	send(w.finalize(), true);
}

void GUIClient::onKeyUp(unsigned char k) {

	switch (std::toupper(k)) {
		case 27:
			roam = !roam;
			glutSetCursor(roam ? GLUT_CURSOR_INHERIT : GLUT_CURSOR_NONE);
			break;
		case ' ':
			if (!roam) input.jump = false;
			break;
		case 'W':
			if (!roam) input.movF = false;
			break;
		case 'S':
			if (!roam) input.movB = false;
			break;
		case 'A':
			if (!roam) input.movL = false;
			break;
		case 'D':
			if (!roam) input.movR = false;
			break;
	}
}

void GUIClient::onKeyDown(unsigned char k) {
	switch (std::toupper(k)) {
		case ' ':
			if (!roam) input.jump = true;
			break;
		case 'W':
			if (!roam) input.movF = true;
			break;
		case 'S':
			if (!roam) input.movB = true;
			break;
		case 'A':
			if (!roam) input.movL = true;
			break;
		case 'D':
			if (!roam) input.movR = true;
			break;
	}

	sendInput();
}

void GUIClient::onSpecialKeyUp(int k) {
}

void GUIClient::onSpecialKeyDown(int k) {
	switch (k) {
		case GLUT_KEY_UP:
			input.movF = true;
			break;
		case GLUT_KEY_DOWN:
			input.movB = true;
			break;
		case GLUT_KEY_LEFT:
			input.movL = true;
			break;
		case GLUT_KEY_RIGHT:
			input.movR = true;
			break;
	}

	sendInput();
}

void GUIClient::onMouseButton(int button, int mode, int x, int y) {
	if (roam) {
		BaseRenderer::onMouseButton(button, mode, x, y);
	} else {
		auto dim = getDimension();
		BaseRenderer::onMouseButton(button, mode, dim.x / 2, dim.y / 2);
	}
}

void GUIClient::onMouseMove(int x, int y) {
	if (roam) {
		BaseRenderer::onMouseMove(x, y);
	} else {
		auto dim = getDimension();
		cam()->mouseX = dim.x / 2;
		cam()->mouseY = dim.y / 2;
		cam()->onMouseMove(x, y);
	}
}

void GUIClient::onKeyHold(unsigned char k) {
	if (roam) BaseRenderer::onKeyHold(k);
}