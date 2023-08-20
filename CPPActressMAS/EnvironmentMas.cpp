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

#include <thread>
#include <chrono>

#include "Agent.h"

cam::EnvironmentMas::EnvironmentMas(int p_no_turns, const EnvironmentMasMode& p_mode, int p_delay_after_turn, const unsigned int p_seed) : 
	m_no_turns(p_no_turns),
	m_delay_after_turn(p_delay_after_turn),
	m_random_order(p_mode == EnvironmentMasMode::SequentialRandom || p_mode == EnvironmentMasMode::Parallel),
	m_parallel(p_mode == EnvironmentMasMode::Parallel),
	m_seed(p_seed),
	m_agents() {
	srand(m_seed);
}

const std::string cam::EnvironmentMas::add(cam::AgentPointer&& p_agent) {
	p_agent->set_environment(this);
	m_agents.add(p_agent);
	return p_agent->get_id();
}

void cam::EnvironmentMas::continue_simulation(int p_no_turns) {
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

const std::vector<cam::AgentPointer> cam::EnvironmentMas::filtered_agents(const std::string& p_name_fragment) {
	return m_agents.filtered_agents(p_name_fragment);
}

const cam::AgentPointer& cam::EnvironmentMas::random_agent() {
	return m_agents.random_agent();
}

void cam::EnvironmentMas::remove(const AgentPointer& p_agent) {
	m_agents.remove(p_agent->get_name());
}

void cam::EnvironmentMas::remove(const std::string& p_agent_name) {
	remove(m_agents.get(p_agent_name));
}

void cam::EnvironmentMas::send(const cam::MessagePointer& p_message) {
	const std::string& l_receiver_name = p_message->get_receiver();
	AgentPointer l_agent = m_agents.get(l_receiver_name);
	if (l_agent) {
		l_agent->post(p_message);
	}
}

void cam::EnvironmentMas::broadcast(const std::string& p_sender, const json& p_message) {
	for (auto& p_agent : m_agents.get_agents()) {
		p_agent.second->post(MessagePointer(new cam::Message(p_sender, p_agent.first, p_message)));
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

int cam::EnvironmentMas::agents_count() const {
	return m_agents.count();
}

void cam::EnvironmentMas::simulation_finished() {  }

void cam::EnvironmentMas::turn_finished(int) {  }

std::vector<cam::ObservableAgentPointer> cam::EnvironmentMas::get_list_of_observable_agents(const cam::Agent* p_perceiving_agent) {
	std::vector<cam::ObservableAgentPointer> l_observable_agent_list;

	// Map name:agent
	for (const auto& l_agent_it : m_agents.get_agents()) {
		const AgentPointer& l_agent = l_agent_it.second;

		if (l_agent_it.first == p_perceiving_agent->get_name() || !l_agent->is_using_observables()) {
			continue;
		}

		if (p_perceiving_agent->perception_filter(l_agent->get_observables())) {
			l_observable_agent_list.push_back(ObservableAgentPointer(new ObservableAgent(l_agent->get_observables())));
		}
	}
	return l_observable_agent_list;
}

std::future<void> cam::EnvironmentMas::execute_setup(AgentPointer& p_agent) {
	return std::async(std::launch::async, [p_agent]() {
		p_agent->internal_setup();
	});
}

std::future<void> cam::EnvironmentMas::execute_see_action(AgentPointer& p_agent) {
	return std::async(std::launch::async, [p_agent]() {
		if (p_agent->is_using_observables()) {
			p_agent->internal_see();
		}
		p_agent->internal_action();
	});
}

void cam::EnvironmentMas::run_turn(int p_turn) {
	const std::vector<int> l_agent_order = m_random_order ? random_permutation(agents_count()) : sorted_permutation(agents_count());

	std::vector<std::string> l_agents_left;
	const std::vector<std::string> l_agent_names = m_agents.get_names();
	for (int i = 0; i < agents_count(); i++) {
		l_agents_left.push_back(l_agent_names[l_agent_order.at(i)]);
	}

	std::vector<std::future<void>> l_asyncs;
	while (l_agents_left.size() > 0) {
		std::string l_agent_name = l_agents_left.at(0);
		l_agents_left.erase(l_agents_left.begin());

		AgentPointer l_agent = m_agents.get(l_agent_name);
		if (l_agent) { 
			if (!l_agent->is_setup()) {
				if (!m_parallel) {
					execute_setup(l_agent).wait();
				} else {
					l_asyncs.push_back(execute_setup(l_agent));
				}
			} else {
				if (!m_parallel)  {
					execute_see_action(l_agent).wait();
				} else {
					l_asyncs.push_back(execute_see_action(l_agent));
				} 
			}
		}
	}
	for(std::future<void>& l_async : l_asyncs) {
		l_async.wait();
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(m_delay_after_turn));
	turn_finished(p_turn);
}

const std::vector<int> cam::EnvironmentMas::random_permutation(int p_number) {
	std::vector<int> l_numbers(p_number);
	for (int l_index = 0; l_index < p_number; l_index++) {
		l_numbers[l_index] = l_index;
	}

	while (p_number > 1) {
		int l_k = rand() % p_number--;
		int l_temp = l_numbers[p_number]; 
		l_numbers[p_number] = l_numbers[l_k]; 
		l_numbers[l_k] = l_temp;
	}

	return l_numbers;
}

const std::vector<int> cam::EnvironmentMas::sorted_permutation(int p_number) {
	std::vector<int> l_numbers(p_number);
	for (int l_index = 0; l_index < p_number; l_index++) {
		l_numbers[l_index] = l_index;
	}
	return l_numbers;
}
