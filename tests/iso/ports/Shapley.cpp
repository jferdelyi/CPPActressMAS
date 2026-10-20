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

// Harness program of the oracle tests (tests/iso): the C# original as it is, its bugs included, compared with its
// oracle by iso.Shapley.
// Port of the C# example E04 Shapley Value (ActressMAS v3.1): for each of 100 tasks, a manager picks three workers at
// random among 27, asks their time estimates, and a calculator shares the price of the task by the Shapley value of the
// best coalition. Parallel, 1 000 turns. Draws from the seeds of the C# oracle: the agents drawn by RandomAgent and the
// difficulties of the tasks. As in the C#, the manager matches the i-th time estimate it receives with the i-th worker
// it picked, whatever the worker that sent it.

#include <algorithm>
#include <cmath>
#include <exception>
#include <optional>
#include <string>
#include <vector>

#include <DotNet.hpp>
#include <Environment.h>
#include <util/DotnetRandom.h>

class Task {
	public:
		std::vector<int> m_difficulty_level;
		int m_price = 0;

		Task(const int p_no_attributes, const int p_max_level) :
				m_rand(cam::dotnet::next_site_seed("Task.cs:26")),
				m_no_attributes(p_no_attributes),
				m_max_level(p_max_level) {
			m_difficulty_level.resize(static_cast<std::size_t>(m_no_attributes));
			m_price = 0;

			for (int i = 0; i < m_no_attributes; i++) {
				m_difficulty_level[static_cast<std::size_t>(i)] = 6 * (m_rand.next(m_max_level) + 1);
				m_price += m_difficulty_level[static_cast<std::size_t>(i)];
			}
		}

		[[nodiscard]] std::string to_string() const {
			std::string l_s;
			for (int i = 0; i < m_no_attributes; i++) {
				l_s += std::to_string(m_difficulty_level[static_cast<std::size_t>(i)]) + " ";
			}
			return cam::dotnet::trim(l_s);
		}

	private:
		cam::DotnetRandom m_rand;
		int m_no_attributes;
		int m_max_level;
};

class CalculatorAgent final : public cam::Agent {
	private:
		static double max_gen(const std::vector<double>& p_x) {
			double l_max = p_x[0];
			for (std::size_t i = 1; i < p_x.size(); i++) {
				l_max = std::max(l_max, p_x[i]);
			}
			return l_max;
		}

		static void normalize(double& p_a, double& p_b) {
			const double l_sum = p_a + p_b;
			p_a /= l_sum;
			p_b /= l_sum;
		}

		static void normalize(double& p_a, double& p_b, double& p_c) {
			const double l_sum = p_a + p_b + p_c;
			p_a /= l_sum;
			p_b /= l_sum;
			p_c /= l_sum;
		}

		static bool are_equal(const double p_x, const double p_y) {
			return std::abs(p_x - p_y) < 1e-10;
		}

	public:
		explicit CalculatorAgent(const std::string& p_name) : Agent(p_name) {}

		void action(const cam::MessagePointer& p_message) override {
			try {
				cam::dotnet::write_line("\t" + format(p_message));
				const auto [l_action, l_parameters] = p_message->parse();

				// utility = 1 / time

				const double l_u1 = 1.0 / cam::dotnet::to_double(l_parameters.at(0));
				const double l_u2 = 1.0 / cam::dotnet::to_double(l_parameters.at(1));
				const double l_u3 = 1.0 / cam::dotnet::to_double(l_parameters.at(2));
				const double l_u12 = 1.0 / cam::dotnet::to_double(l_parameters.at(3));
				const double l_u13 = 1.0 / cam::dotnet::to_double(l_parameters.at(4));
				const double l_u23 = 1.0 / cam::dotnet::to_double(l_parameters.at(5));
				const double l_u123 = 1.0 / cam::dotnet::to_double(l_parameters.at(6));

				const double l_max = max_gen({l_u1, l_u2, l_u3, l_u12, l_u13, l_u23, l_u123});

				if (are_equal(l_max, l_u1)) {
					send_by_name("manager", "results 1 0 0");
				} else if (are_equal(l_max, l_u2)) {
					send_by_name("manager", "results 0 1 0");
				} else if (are_equal(l_max, l_u3)) {
					send_by_name("manager", "results 0 0 1");
				} else if (are_equal(l_max, l_u12)) {
					// computing the Shapley value with 2 agents
					double l_value_added1 = (l_u1 + (l_u12 - l_u2)) / 2.0;  // AB, BA
					double l_value_added2 = (l_u2 + (l_u12 - l_u1)) / 2.0;  // BA, AB
					normalize(l_value_added1, l_value_added2);
					send_by_name("manager", "results " + cam::dotnet::to_string(l_value_added1) + " " + cam::dotnet::to_string(l_value_added2) + " 0");
				} else if (are_equal(l_max, l_u13)) {
					// computing the Shapley value with 2 agents
					double l_value_added1 = (l_u1 + (l_u13 - l_u3)) / 2.0;  // AC, CA
					double l_value_added3 = (l_u3 + (l_u13 - l_u1)) / 2.0;  // CA, AC
					normalize(l_value_added1, l_value_added3);
					send_by_name("manager", "results " + cam::dotnet::to_string(l_value_added1) + " 0 " + cam::dotnet::to_string(l_value_added3));
				} else if (are_equal(l_max, l_u23)) {
					// computing the Shapley value with 2 agents
					double l_value_added2 = (l_u2 + (l_u23 - l_u3)) / 2.0;  // BC, CB
					double l_value_added3 = (l_u3 + (l_u23 - l_u2)) / 2.0;  // CB, BC
					normalize(l_value_added2, l_value_added3);
					send_by_name("manager", "results 0 " + cam::dotnet::to_string(l_value_added2) + " " + cam::dotnet::to_string(l_value_added3));
				} else if (are_equal(l_max, l_u123)) {
					// computing the Shapley value with 3 agents
					double l_value_added1 = (l_u1 + l_u1 + (l_u12 - l_u2) + (l_u123 - l_u23) + (l_u13 - l_u3) + (l_u123 - l_u23)) / 6.0;  // ABC, ACB, BAC, BCA, CAB, CBA
					double l_value_added2 = (l_u2 + l_u2 + (l_u12 - l_u1) + (l_u123 - l_u13) + (l_u123 - l_u13) + (l_u23 - l_u3)) / 6.0;  // BAC, BCA, ABC, ACB, CAB, CBA
					double l_value_added3 = (l_u3 + l_u3 + (l_u123 - l_u12) + (l_u13 - l_u1) + (l_u123 - l_u12) + (l_u23 - l_u2)) / 6.0;  // CAB, CBA, ABC, ACB, BAC, BCA
					normalize(l_value_added1, l_value_added2, l_value_added3);
					send_by_name("manager", "results " + cam::dotnet::to_string(l_value_added1) + " " + cam::dotnet::to_string(l_value_added2) + " " +
													cam::dotnet::to_string(l_value_added3));
				}
			} catch (const std::exception& p_exception) {
				cam::dotnet::write_line(p_exception.what());
			}
		}
};

class ManagerAgent final : public cam::Agent {
	private:
		std::vector<std::string> m_agent_bidders;
		std::vector<std::string> m_bids;
		std::optional<Task> m_current_task;
		int m_task_count = 0;
		int m_no_bids = 0;
		int m_no_tasks = 0;
		int m_no_attributes = 0;
		int m_max_level = 0;

		[[nodiscard]] std::string random_agent_name() const {
			const auto l_id = get_environment()->random_agent();
			return l_id ? get_agent_name(*l_id).value_or("") : "";
		}

		static bool starts_with_worker(const std::string& p_name) {
			return p_name.starts_with("worker");
		}

		void generate_new_task() {
			m_current_task.emplace(m_no_attributes, m_max_level);
			m_task_count++;

			select_agents();
			cam::dotnet::write_line("Selected: " + m_agent_bidders[0] + " " + m_agent_bidders[1] + " " + m_agent_bidders[2]);

			for (int i = 0; i < m_no_bids; i++) {
				send_by_name(m_agent_bidders[static_cast<std::size_t>(i)], "task " + m_current_task->to_string());
			}

			m_bids.clear();
		}

		void select_agents() {
			[[maybe_unused]] const auto l_no_agents = get_environment()->agents_count();

			m_agent_bidders[0] = "";
			while ((m_agent_bidders[0].empty()) || !starts_with_worker(m_agent_bidders[0])) {
				m_agent_bidders[0] = random_agent_name();
			}

			m_agent_bidders[1] = "";
			while (((m_agent_bidders[1].empty()) || (m_agent_bidders[1] == m_agent_bidders[0])) || !starts_with_worker(m_agent_bidders[1])) {
				m_agent_bidders[1] = random_agent_name();
			}

			m_agent_bidders[2] = "";
			while ((((m_agent_bidders[2].empty()) || (m_agent_bidders[2] == m_agent_bidders[0])) || (m_agent_bidders[2] == m_agent_bidders[1])) ||
				   !starts_with_worker(m_agent_bidders[2])) {
				m_agent_bidders[2] = random_agent_name();
			}
		}

		void handle_time(const std::string& p_time_estimate) {
			m_bids.push_back(p_time_estimate);
			if (static_cast<int>(m_bids.size()) == m_no_bids) {
				analyze_coalitions();
			}
		}

		void handle_results(const std::string& p_results) {
			const auto l_toks = cam::dotnet::split(p_results);
			for (int i = 0; i < m_no_bids; i++) {
				const double l_r = m_current_task->m_price * cam::dotnet::to_double(l_toks.at(static_cast<std::size_t>(i)));
				send_by_name(m_agent_bidders[static_cast<std::size_t>(i)], "reward " + cam::dotnet::to_string(l_r));
			}
			if (m_task_count >= m_no_tasks) {
				cam::dotnet::write_line("Manager has finished");

				for (const auto& l_worker: get_environment()->get_filtered_agents("worker")) {
					send(l_worker, "report");
				}
			} else {
				generate_new_task();
			}
		}

		void analyze_coalitions() {
			const auto l_bids = static_cast<std::size_t>(m_no_bids);
			const auto l_attributes = static_cast<std::size_t>(m_no_attributes);
			std::vector<std::vector<int>> l_time_to_solve(l_bids, std::vector<int>(l_attributes));

			for (std::size_t i = 0; i < l_bids; i++) {
				const auto l_toks = cam::dotnet::split(m_bids[i]);
				for (std::size_t j = 0; j < l_attributes; j++) {
					l_time_to_solve[i][j] = cam::dotnet::to_int32(l_toks.at(j));
				}
			}

			const int l_v1 = (l_time_to_solve[0][0] + l_time_to_solve[0][1]) + l_time_to_solve[0][2];
			const int l_v2 = (l_time_to_solve[1][0] + l_time_to_solve[1][1]) + l_time_to_solve[1][2];
			const int l_v3 = (l_time_to_solve[2][0] + l_time_to_solve[2][1]) + l_time_to_solve[2][2];
			const int l_v12 = (std::min(l_time_to_solve[0][0], l_time_to_solve[1][0]) + std::min(l_time_to_solve[0][1], l_time_to_solve[1][1])) + std::min(l_time_to_solve[0][2], l_time_to_solve[1][2]);
			const int l_v13 = (std::min(l_time_to_solve[0][0], l_time_to_solve[2][0]) + std::min(l_time_to_solve[0][1], l_time_to_solve[2][1])) + std::min(l_time_to_solve[0][2], l_time_to_solve[2][2]);
			const int l_v23 = (std::min(l_time_to_solve[1][0], l_time_to_solve[2][0]) + std::min(l_time_to_solve[1][1], l_time_to_solve[2][1])) + std::min(l_time_to_solve[1][2], l_time_to_solve[2][2]);
			const int l_v123 = (min3(l_time_to_solve[0][0], l_time_to_solve[1][0], l_time_to_solve[2][0]) + min3(l_time_to_solve[0][1], l_time_to_solve[1][1], l_time_to_solve[2][1])) + min3(l_time_to_solve[0][2], l_time_to_solve[1][2], l_time_to_solve[2][2]);

			send_by_name("shapley", "calculate " + std::to_string(l_v1) + " " + std::to_string(l_v2) + " " + std::to_string(l_v3) + " " +
											 std::to_string(l_v12) + " " + std::to_string(l_v13) + " " + std::to_string(l_v23) + " " +
											 std::to_string(l_v123));
		}

		static int min3(const int p_x1, const int p_x2, const int p_x3) {
			return std::min(p_x1, std::min(p_x2, p_x3));
		}

	public:
		explicit ManagerAgent(const std::string& p_name) : Agent(p_name) {}

		void setup() override {
			const json l_memory = get_environment()->get_global_data();
			m_no_bids = l_memory.at("NoBids").get<int>();
			m_no_tasks = l_memory.at("NoTasks").get<int>();
			m_no_attributes = l_memory.at("NoAttributes").get<int>();
			m_max_level = l_memory.at("MaxLevel").get<int>();

			m_agent_bidders.assign(static_cast<std::size_t>(m_no_bids), "");
			generate_new_task();
		}

		void action(const cam::MessagePointer& p_message) override {
			try {
				cam::dotnet::write_line("\t" + format(p_message));
				const auto [l_action, l_parameters] = p_message->parse_rest();

				if (l_action == "time") {
					handle_time(l_parameters);
				} else if (l_action == "results") {
					handle_results(l_parameters);
				}
			} catch (const std::exception& p_exception) {
				cam::dotnet::write_line(p_exception.what());
			}
		}
};

class WorkerAgent final : public cam::Agent {
	private:
		std::vector<int> m_skill_levels;
		double m_total_reward = 0;

		void handle_task(const std::vector<std::string>& p_task_parameters) const {
			std::string l_estimates;
			const int l_no_attributes = get_environment()->get_global_data().at("NoAttributes").get<int>();
			for (int i = 0; i < l_no_attributes; i++) {
				const int l_time = cam::dotnet::to_int32(p_task_parameters.at(static_cast<std::size_t>(i))) / m_skill_levels.at(static_cast<std::size_t>(i));
				l_estimates += std::to_string(l_time) + " ";
			}
			send_by_name("manager", "time " + cam::dotnet::trim(l_estimates));
		}

		void handle_reward(const double p_reward) {
			m_total_reward += p_reward;
		}

		void handle_report() const {
			cam::dotnet::write_line("Agent " + get_name() + " has " + cam::dotnet::format_fixed(m_total_reward, 3));
		}

	public:
		WorkerAgent(const std::string& p_name, const int p_s1, const int p_s2, const int p_s3) :
				Agent(p_name),
				m_skill_levels{p_s1, p_s2, p_s3} {}  // in the rest of the program, NoAttributes and MaxLevel are used

		void action(const cam::MessagePointer& p_message) override {
			try {
				const auto [l_action, l_parameters] = p_message->parse();

				if (l_action == "task") {
					handle_task(l_parameters);
				} else if (l_action == "reward") {
					handle_reward(cam::dotnet::to_double(l_parameters.at(0)));
				} else if (l_action == "report") {
					handle_report();
				}
			} catch (const std::exception& p_exception) {
				cam::dotnet::write_line(p_exception.what());
			}
		}
};

int main() {
	cam::Environment l_environment({.m_turns = 1000,
									.m_mode = cam::EnvironmentMasMode::Parallel,
									.m_seed = static_cast<unsigned int>(cam::dotnet::next_site_seed("EnvironmentMas.cs:61")),
									.m_random = cam::RandomEngine::DotNet});

	for (int i = 1; i <= 3; i++) {  // MaxLevel
		for (int j = 1; j <= 3; j++) {
			for (int k = 1; k <= 3; k++) {
				l_environment.add<WorkerAgent>("worker" + std::to_string(i) + std::to_string(j) + std::to_string(k), i, j, k);
			}
		}
	}

	l_environment.add<CalculatorAgent>("shapley");

	l_environment.add<ManagerAgent>("manager");

	l_environment.set_global_data("NoAttributes", 3);
	l_environment.set_global_data("MaxLevel", 3);
	l_environment.set_global_data("NoTasks", 100);
	l_environment.set_global_data("NoBids", 3);

	l_environment.start();
	return 0;
}
