#include "../client/base.hpp"

int main() {
	auto error = QuicClient::init(true);
	if (error) return error;

	auto client = new BaseClient();

	client->connect("localhost", 6969);

	getchar();
	delete client;

	QuicClient::cleanup();
}