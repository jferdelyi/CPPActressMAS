/**************************************************************************
 *                                                                        *
 *  Description: CPPActressMas multi-agent framework                      *
 *  Website:     https://github.com/jferdelyi/CPPActressMAS               *
 *  Copyright:   (c) 2023-Today, Jean-François Erdelyi                    *
 *                                                                        *
 *  CPP version of ActressMAS by Florin Leon                              *
 *  https://github.com/florinleon/ActressMas                              *
 *                                                                        *
 *  This program is free software; you can redistribute it and/or modify  *
 *  it under the terms of the GNU General License as published by         *
 *  the Free Software Foundation. This program is distributed in the      *
 *  hope that it will be useful, but WITHOUT ANY WARRANTY; without even   *
 *  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR   *
 *  PURPOSE. See the GNU General License for more details.                *
 *                                                                        *
 **************************************************************************/

#include <iostream>
#include <thread>

#include <Environment.h>
#include <transport/Facade.h>

// Port of ActressMAS E20 RemoteMessages: a ping agent asks a pong agent of another environment for
// information, by name over the network; the pong agent answers twice by id. Two environments of this
// process, each its own node: MobilePingPong [url-pong [url-ping]]; by default Zenoh peers on 127.0.0.1
// (pong listens on port 17448, ping connects), no router nor broker. An MQTT URL works too.

class PingAgent : public cam::Agent {
public:
	explicit PingAgent(const std::string& p_name) : Agent(p_name) {}

	void setup() override {
		std::cout << "[" << get_name() << "] Sending request" << std::endl;
		// The pong agent lives in another environment: network scope, asked explicitly.
		cam::transport::SendOptions l_options;
		l_options.m_scope = cam::transport::Scope::Network;
		send_by_name("PongAgent", {{"data", "request-info"}, {"from", get_name()}}, true, l_options);
	}

	void action(const cam::MessagePointer& p_message) override {
		std::cout << p_message->content() << std::endl;
	}
};

class PongAgent : public cam::Agent {
public:
	explicit PongAgent(const std::string& p_name) : Agent(p_name) {}

	void setup() override {
		std::cout << "[" << get_name() << "] Waiting for requests..." << std::endl;
	}

	void action(const cam::MessagePointer& p_message) override {
		std::cout << p_message->content() << std::endl;
		if (p_message->content()["data"] == "request-info") {
			std::string l_info = "Info 1 from agent " + get_name() + " in container " + get_environment()->get_id();
			send(p_message->get_sender(), l_info);

			l_info = "Info 2 from agent " + get_name() + " in container " + get_environment()->get_id();
			send(p_message->get_sender(), l_info);
		}
	}
};

int main(int argc, char** argv) {
	// Without argument the two environments share the in-process loopback, available in every build.
	// With a URL they go through the network, for example:
	//   MobilePingPong "zenoh://?listen=tcp/127.0.0.1:17448&ns=pingpong" "zenoh://127.0.0.1:17448/pingpong"
	//   MobilePingPong "mqtt://127.0.0.1:1883/pingpong"
	const std::string l_url_pong = argc > 1 ? argv[1] : "loopback://pingpong";
	const std::string l_url_ping = argc > 2 ? argv[2] : l_url_pong;

	// A remote message reaches its agent at the turn after its reception: a few turns are needed.
	cam::Environment l_environment_b(10, cam::EnvironmentMasMode::Parallel, 100);
	cam::Environment l_environment_a(10, cam::EnvironmentMasMode::Parallel, 100);
	for (const auto& [l_environment, l_url] : {std::pair{&l_environment_b, l_url_pong}, std::pair{&l_environment_a, l_url_ping}}) {
		if (const auto l_result = cam::transport::connect(*l_environment, l_url); !l_result) {
			std::cerr << "Cannot connect to " << l_url << ": " << l_result.error().m_message << std::endl;
			return 1;
		}
	}

	l_environment_b.add<PongAgent>("PongAgent");
	std::thread l_environment_1_thread(&cam::Environment::start, &l_environment_b);

	l_environment_a.add<PingAgent>("PingAgent");
	std::thread l_environment_2_thread(&cam::Environment::start, &l_environment_a);

	l_environment_1_thread.join();
	l_environment_2_thread.join();
}
