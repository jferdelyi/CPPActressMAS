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

#include <EnvironmentMas.h>
#include <Agent.h>

static constexpr int CHILD_COUNT = 10;
static constexpr int AGENT_COUNT = 10000;

class MyAgent : public cam::Agent {
	protected:
		std::string m_parent_id;
		int m_sum;
		int m_sum_id;
		int m_message_left;

	public:
		explicit MyAgent(const std::string& p_name, const int p_sum_id, std::string p_parent_id) :
			Agent(p_name),
			m_parent_id(std::move(p_parent_id)),
			m_sum(p_sum_id),
			m_sum_id(p_sum_id),
			m_message_left(0) {}

		void setup() override {
			for (int i = 1; i <= CHILD_COUNT; i++) {
				if (const int l_new_id = m_sum_id * 10 + i; l_new_id < AGENT_COUNT) {
					get_environment()->add<MyAgent>("a" + std::to_string(l_new_id), l_new_id, get_id());
					m_message_left++;
				}
			}

			if (m_message_left == 0) {
				send(m_parent_id, {{"sum", m_sum_id}}); // send id to parent
				stop();
			}
		}

		void action(const cam::MessagePointer& p_message) override {
			m_sum += static_cast<int>(p_message->content()["sum"]);
			m_message_left--;

			if (m_message_left == 0) {
				if (get_name() == "a0") {
					std::cout << "Sum " << m_sum << std::endl;
				} else {
					send(m_parent_id, {{"sum", m_sum}}); // send id to parent
				}
				stop();
			}
		}
};

int main() {
	const auto& l_start_time = std::chrono::high_resolution_clock::now();

	cam::EnvironmentMas l_environment(0, cam::EnvironmentMasMode::Parallel);
	l_environment.add<MyAgent>("a0", 0, "");
	l_environment.start();

	const auto& l_end_time = std::chrono::high_resolution_clock::now();
	const auto& l_elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(l_end_time - l_start_time).count();
	std::cout << l_elapsed_time << "ms" << std::endl;
}
