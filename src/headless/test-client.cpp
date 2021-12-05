#include "../network/quic/client.hpp"
#include <thread>
#include <chrono>

using namespace std::chrono;

int main() {
	auto error = QuicClient::init(true);
	if (error) return error;

	auto client = new QuicClient();

	client->connect("127.0.0.1", 6969);

	getchar();
	delete client;

	QuicClient::cleanup();
}