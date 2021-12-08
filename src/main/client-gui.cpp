#include "../misc/repl.hpp"
#include "../client/gui.hpp"

int main(int argc, char** argv) {
	auto error = QuicClient::init(true);
	if (error) return error;

	glutInit(&argc, argv);

	auto client = new GUIClient();
	client->connect("localhost", 6969);
	client->loop();

	delete client;
	QuicClient::cleanup();
}