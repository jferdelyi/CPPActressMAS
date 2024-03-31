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

#include <chrono>
#include <iostream>

#include <EnvironmentMas.h>

static constexpr int MESSAGE_COUNT_MAX = 100 * 1000 * 10000;
static constexpr int AGENT_COUNT = 10;

static size_t s_message_count = 0;

class MyAgent : public cam::Agent {
	public:
		explicit MyAgent(const std::string& p_name) :
			Agent(p_name) {
		}

		void setup() override {
			broadcast();
			s_message_count += AGENT_COUNT;
		}

		void action(const cam::MessagePointer& p_message) override {
			if (s_message_count < MESSAGE_COUNT_MAX) {
				send(p_message->get_sender());
				s_message_count++;
			}
	}
};

int main() {
	constexpr int l_trial_max = 5;
	double l_sum = 0;

	cam::EnvironmentMas l_environment(10000, cam::EnvironmentMasMode::Parallel);
	for (int i = 0; i < AGENT_COUNT; i++) {
		l_environment.add<MyAgent>("agent " + std::to_string(i));
	}

	for (int l_trial = 1; l_trial <= l_trial_max; l_trial++) {
		std::cout << "Trial " << l_trial << std::endl;
		s_message_count = 0;

		const auto& l_start_time = std::chrono::high_resolution_clock::now();
		l_environment.start();
		const auto& l_elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - l_start_time).count();

		std::cout << s_message_count << "msg" << std::endl;
		std::cout << l_elapsed_time << "ms" << std::endl;
		l_sum += static_cast<double>(s_message_count) / static_cast<double>(l_elapsed_time);
	}
	std::cout << l_sum / l_trial_max << "msg/ms" << std::endl;
}
