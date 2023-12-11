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

#include "EnvironmentMas.h"

#include <chrono>
#include <thread>

#include "Agent.h"

cam::EnvironmentMas::EnvironmentMas(const int p_no_turns, const EnvironmentMasMode& p_mode, const int p_delay_after_turn, const unsigned int p_seed) :
	m_no_turns(p_no_turns),
	m_delay_after_turn(p_delay_after_turn),
	m_random_order(p_mode == EnvironmentMasMode::SequentialRandom || p_mode == EnvironmentMasMode::Parallel),
	m_parallel(p_mod == EnvironmentMasMode::Parallel),
	m_random_generator(p_seed) {}

std::string cam::EnvironmentMas::add(AgentPointer&& p_agent) {
	p_agent->set_environment(this);
	m_agents.add(p_agent);
	return p_agent->get_id();
}

cam::AgentPointer cam::EnvironmentMas::get(const std::string& p_id) {
	return m_agents.get(p_id);
}

void cam::EnvironmentMas::continue_simulation(const int p_no_turns) {
	int l_turn = 0;
	while (true) {
		run_turn(l_turn++);

		if (p_no_turns != 0 && l_turn >= p_no_turns) {
			break;
		}
		if (m_agents.count() == 0) {
			break;
		}
	}
	simulation_finished();
}

const cam::AgentPointer& cam::EnvironmentMas::random_agent() {
	return m_agents.random_agent();
}

void cam::EnvironmentMas::remove(const AgentPointer& p_agent) {
	m_agents.remove(p_agent->get_id());
}

void cam::EnvironmentMas::remove(const std::string& p_agent_id) {
	remove(m_agents.get(p_agent_id));
}

void cam::EnvironmentMas::send(const MessagePointer& p_message) {
	const std::string& l_receiver_id = p_message->get_receiver();
	if (const AgentPointer l_agent = m_agents.get(l_receiver_id); l_agent) {
		l_agent->post(p_message);
	}
}

void cam::EnvironmentMas::broadcast(const std::string& p_sender, const json& p_message) const {
	for (auto& [l_id, l_agent] : m_agents.get_agents()) {
		if (l_id != p_sender) {
			l_agent->post(std::make_shared<Message>(p_sender, l_id, p_message));
		}
	}
}

void cam::EnvironmentMas::start() {
	int l_turn = 0;

	while (true) {
		run_turn(l_turn++);
		if (m_no_turns != 0 && l_turn >= m_no_turns) {
			break;
		}
		if (m_agents.count() == 0) {
			break;
		}
	}

	simulation_finished();
}

size_t cam::EnvironmentMas::agents_count() const {
	return m_agents.count();
}

void cam::EnvironmentMas::simulation_finished() {}

void cam::EnvironmentMas::turn_finished(int) {}

std::vector<const cam::ObservablesPointer>
cam::EnvironmentMas::get_list_of_observable_agents(
	const Agent *p_perceiving_agent) const {
	std::vector<const ObservablesPointer> l_observable_agent_list;

	// Map id:agent
	for (auto& [l_id, l_agent] : m_agents.get_agents()) {
		if (l_id == p_perceiving_agent->get_id() || !l_agent->is_using_observables()) {
			continue;
		}

		if (p_perceiving_agent->perception_filter(l_agent->get_observables())) {
			l_observable_agent_list.push_back(l_agent->get_observables());
		}
	}
	return l_observable_agent_list;
}

std::future<void> cam::EnvironmentMas::execute_setup(AgentPointer& p_agent) {
	return std::async(std::launch::async, [p_agent] {
		p_agent->internal_setup();
	});
}

std::future<void>
cam::EnvironmentMas::execute_see_action(AgentPointer& p_agent) {
	return std::async(std::launch::async, [p_agent] {
		if (p_agent->is_using_observables()) {
			p_agent->internal_see();
		}
		p_agent->internal_action();
	});
}

void cam::EnvironmentMas::run_turn(const int p_turn) {
	// const std::vector<int> l_agent_order = m_random_order ?
	// random_permutation(agents_count()) : sorted_permutation(agents_count());
	std::vector<std::string> l_agents_left = m_agents.get_ids();

	std::vector<std::future<void>> l_asyncs;
	while (!l_agents_left.empty()) {
		std::string l_agent_id = l_agents_left.at(0);
		l_agents_left.erase(l_agents_left.begin());

		if (AgentPointer l_agent = m_agents.get(l_agent_id); l_agent) {
			if (!l_agent->is_setup()) {
				if (!m_parallel) {
					execute_setup(l_agent).wait();
				} else {
					l_asyncs.push_back(execute_setup(l_agent));
				}
			} else {
				if (!m_parallel) {
					execute_see_action(l_agent).wait();
				} else {
					l_asyncs.push_back(execute_see_action(l_agent));
				}
			}
		}
	}
	for (std::future<void>& l_async : l_asyncs) {
		l_async.wait();
	}
	if (m_delay_after_turn) {
		std::this_thread::sleep_for(std::chrono::milliseconds(m_delay_after_turn));
	}
	turn_finished(p_turn);
}

std::vector<int> cam::EnvironmentMas::random_permutation(int p_number) {
	std::vector<int> l_numbers(p_number);
	for (int l_index = 0; l_index < p_number; l_index++) {
		l_numbers[l_index] = l_index;
	}

	std::random_device l_rd;
	while (p_number > 1) {
		std::uniform_int_distribution l_dist(0, p_number--);

		const int l_k = l_dist(l_rd);
		const int l_temp = l_numbers[p_number];
		l_numbers[p_number] = l_numbers[l_k];
		l_numbers[l_k] = l_temp;
	}

	return l_numbers;
}

std::vector<int> cam::EnvironmentMas::sorted_permutation(const int p_number) {
	std::vector<int> l_numbers(p_number);
	for (int l_index = 0; l_index < p_number; l_index++) {
		l_numbers[l_index] = l_index;
	}
	return l_numbers;
}
