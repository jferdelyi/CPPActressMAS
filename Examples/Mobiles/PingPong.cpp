
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

#include <EnvironmentMas.h>

class PingAgent : public cam::Agent {
public:
	explicit PingAgent(const std::string& p_name) : Agent(p_name) {}

	void setup() override {
		std::cout << "[" << get_name() << "] Sending request" << std::endl;
		send_by_name("PongAgent",{{"data", "request-info"}, {"from", get_name()}});
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

int main() {
	cam::EnvironmentMas l_environment_b(3, cam::EnvironmentMasMode::Parallel, 10);
	l_environment_b.initialise_remote_connection();
	l_environment_b.add<PongAgent>("PongAgent");
	std::thread l_environment_1_thread(&cam::EnvironmentMas::start, &l_environment_b);

	cam::EnvironmentMas l_environment_a(3, cam::EnvironmentMasMode::Parallel, 10);
	l_environment_a.initialise_remote_connection();
	l_environment_a.add<PingAgent>("PingAgent");
	std::thread l_environment_2_thread(&cam::EnvironmentMas::start, &l_environment_a);

	l_environment_1_thread.join();
	l_environment_2_thread.join();
}
