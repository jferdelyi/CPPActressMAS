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
// oracle by iso.ContractNet.
// Port of the C# example E08 Contract Net Protocol (ActressMAS v3.1): three postmen share 20 letters; in a cycle of 7
// turns, a postman offers its worst letter (call for proposals), the others propose a price, and the initiator awards
// the letter to the best proposal; each tour is solved by nearest neighbour (the TSP of the example). Parallel, 100
// turns, then a "finish" message to each postman. The locations and the first assignment come from the seed of the C#
// oracle.
// As in the C#, one CNPMessage object is shared by every receiver of a broadcast and changed in place by each of them:
// the messages carry the index of the object in a table of the program, which keeps the references of the C#. The
// postmen act under one lock, so that these changes are a sequence the C# may also produce, without data race.

#include <cmath>
#include <deque>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <DotNet.hpp>
#include <Environment.h>
#include <util/DotnetRandom.h>

// ---------------------------------------------------------------- the TSP of the example

struct Location {
	std::string m_name;
	double m_latitude;
	double m_longitude;
};

// A path of the TSP: its locations (indexes into the problem) and its length in metres
struct IntegerPath {
	std::vector<int> m_locations;
	int m_length = std::numeric_limits<int>::min();

	[[nodiscard]] bool contains(const int p_location) const {
		for (const int l_location: m_locations) {
			if (l_location == p_location) {
				return true;
			}
		}
		return false;
	}

	[[nodiscard]] double get_path_length() const {
		return static_cast<double>(m_length) / 1000.0;
	}
};

struct TSP {
	static double to_radians(const double p_angle) {
		return std::numbers::pi / 180.0 * p_angle;
	}

	static double dist_from(const double p_lat1, const double p_lng1, const double p_lat2, const double p_lng2) {
		const double l_earth_radius = 6371000;  // meters
		const double l_d_lat = to_radians(p_lat2 - p_lat1);
		const double l_d_lng = to_radians(p_lng2 - p_lng1);
		const double l_a = std::sin(l_d_lat / 2) * std::sin(l_d_lat / 2) +
						   std::cos(to_radians(p_lat1)) * std::cos(to_radians(p_lat2)) * std::sin(l_d_lng / 2) * std::sin(l_d_lng / 2);
		const double l_c = 2 * std::atan2(std::sqrt(l_a), std::sqrt(1 - l_a));
		const double l_dist = static_cast<float>(l_earth_radius * l_c);
		return l_dist;
	}

	// ProblemBuilder.BuildIntegerArray with the first location fixed, then NearestNeighborSolver.Solve
	static IntegerPath solve(const std::vector<Location>& p_locations) {
		const int l_count = static_cast<int>(p_locations.size());
		const int l_fixed_first_location = 0;
		std::vector<int> l_distances(static_cast<std::size_t>(l_count) * static_cast<std::size_t>(l_count));
		for (int i = 0; i < l_count; i++) {
			for (int j = 0; j < l_count; j++) {
				const Location& l_from = p_locations[static_cast<std::size_t>(i)];
				const Location& l_to = p_locations[static_cast<std::size_t>(j)];
				// MapToInt: (int)(value * intMultiplicator), the multiplicator being 1
				l_distances[static_cast<std::size_t>(i * l_count + j)] =
						static_cast<int>(dist_from(l_from.m_latitude, l_from.m_longitude, l_to.m_latitude, l_to.m_longitude) * 1);
			}
		}

		IntegerPath l_start;
		for (int j = 0; j < l_count; j++) {
			std::vector<IntegerPath> l_neighbors;
			for (int i = 0; i < l_count; i++) {
				if (!l_start.contains(i)) {
					IntegerPath l_neighbor = l_start;
					l_neighbor.m_locations.push_back(i);
					l_neighbors.push_back(std::move(l_neighbor));
				}
			}
			// CalculateLengths
			for (auto& l_path: l_neighbors) {
				if (l_fixed_first_location >= 0 && !l_path.m_locations.empty() && l_path.m_locations[0] != l_fixed_first_location) {
					l_path.m_length = std::numeric_limits<int>::max();
					continue;
				}
				int l_result = 0;
				for (std::size_t i = 1; i < l_path.m_locations.size(); i++) {
					l_result += l_distances[static_cast<std::size_t>(l_path.m_locations[i - 1] * l_count + l_path.m_locations[i])];
				}
				l_path.m_length = l_result;
			}
			// neighbors.Sort(): CompareTo compares the lengths as doubles
			cam::dotnet::list_sort(l_neighbors, [](const IntegerPath& p_a, const IntegerPath& p_b) {
				const auto l_a = static_cast<double>(p_a.m_length);
				const auto l_b = static_cast<double>(p_b.m_length);
				return l_a < l_b ? -1 : (l_a > l_b ? 1 : 0);
			});
			l_start = l_neighbors[0];
		}
		return l_start;
	}
};

// ---------------------------------------------------------------- the protocol

enum class Performatives { CallForProposals, Propose, Refuse, AcceptProposal, RejectProposal, InformDone, InformResult, Failure };

std::string to_string(const Performatives p_performative) {
	switch (p_performative) {
		case Performatives::CallForProposals:
			return "CallForProposals";
		case Performatives::Propose:
			return "Propose";
		case Performatives::Refuse:
			return "Refuse";
		case Performatives::AcceptProposal:
			return "AcceptProposal";
		case Performatives::RejectProposal:
			return "RejectProposal";
		case Performatives::InformDone:
			return "InformDone";
		case Performatives::InformResult:
			return "InformResult";
		case Performatives::Failure:
			return "Failure";
	}
	return "";
}

struct CNPMessage {
	int m_index = -1;  // its place in the table of the objects, carried by the messages
	std::string m_sender;
	Performatives m_performative;
	Location m_task;
	double m_initiator_payoff = 0;
	double m_participant_payoff = 0;
	double m_contract_price = 0;
};

// The CNPMessage objects the messages refer to, as the references of the C#
class CNPMessages {
	private:
		static std::mutex& mutex() {
			static std::mutex s_mutex;
			return s_mutex;
		}

		static std::deque<std::shared_ptr<CNPMessage>>& table() {
			static std::deque<std::shared_ptr<CNPMessage>> s_table;
			return s_table;
		}

	public:
		static std::shared_ptr<CNPMessage> create(const std::string& p_sender, const Performatives p_performative, const Location& p_task,
												  const double p_initiator_payoff) {
			const std::scoped_lock l_lock(mutex());
			auto l_message = std::make_shared<CNPMessage>(
					CNPMessage{static_cast<int>(table().size()), p_sender, p_performative, p_task, p_initiator_payoff, 0, 0});
			table().push_back(l_message);
			return l_message;
		}

		static std::shared_ptr<CNPMessage> get(const int p_index) {
			const std::scoped_lock l_lock(mutex());
			return table().at(static_cast<std::size_t>(p_index));
		}
};

class TaskCollection {
	private:
		std::vector<Location> m_assignment;  // letters currently assigned
		double m_cost_per_km;
		double m_length = 0;
		std::vector<int> m_path;
		double m_payment_per_letter;

		[[nodiscard]] double compute_payoff(const std::vector<Location>& p_list, const double p_path_length) const {
			const double l_income = static_cast<double>(p_list.size()) * m_payment_per_letter;
			const double l_cost = p_path_length * m_cost_per_km;
			const double l_payoff = l_income - l_cost;
			return l_payoff;
		}

		void solve() {
			if (m_assignment.empty()) {
				m_path.clear();
				m_length = 0.0;
				return;
			}

			const IntegerPath l_path = TSP::solve(m_assignment);
			m_path = l_path.m_locations;  // 0 to n-1
			m_length = l_path.get_path_length();
		}

		[[nodiscard]] std::vector<Location> virtual_add_task(const Location& p_location) const {
			std::vector<Location> l_new_list = m_assignment;
			l_new_list.push_back(p_location);
			return l_new_list;
		}

		[[nodiscard]] std::vector<Location> virtual_remove_task(const std::string& p_task_name) const {
			std::vector<Location> l_list;
			for (const auto& l_task: m_assignment) {
				if (l_task.m_name != p_task_name) {
					l_list.push_back(l_task);
				}
			}
			return l_list;
		}

	public:
		explicit TaskCollection(std::vector<Location> p_assignment) :
				m_assignment(std::move(p_assignment)),
				m_cost_per_km(1),
				m_payment_per_letter(5) {
			solve();
		}

		// this[taskName]: the first letter of that name, or none (null in the C#)
		[[nodiscard]] std::optional<Location> operator[](const std::string& p_task_name) const {
			for (const auto& l_task: m_assignment) {
				if (l_task.m_name == p_task_name) {
					return l_task;
				}
			}
			return std::nullopt;
		}

		[[nodiscard]] double additional_payoff_by_accepting(const CNPMessage& p_message) const {
			double l_payoff = compute_payoff(m_assignment, m_length);
			const double l_p0 = l_payoff;
			const auto l_ss = virtual_add_task(p_message.m_task);
			const IntegerPath l_path = TSP::solve(l_ss);
			const double l_len = l_path.get_path_length();
			l_payoff = compute_payoff(l_ss, l_len);
			const double l_p1 = l_payoff;
			return l_p1 - l_p0;
		}

		void add_task(const Location& p_location) {
			m_assignment.push_back(p_location);
			solve();
		}

		[[nodiscard]] double payoff() const {
			return compute_payoff(m_assignment, m_length);
		}

		[[nodiscard]] std::string solution() const {
			std::string l_sb;
			for (std::size_t i = 0; i < m_assignment.size(); i++) {
				const Location& l_location = m_assignment[static_cast<std::size_t>(m_path[i])];
				l_sb += std::to_string(i + 1) + "." + l_location.m_name + " ";
			}
			l_sb += "-> ";
			l_sb += cam::dotnet::to_string(m_length) + " km";
			return l_sb;
		}

		void remove_task(const std::string& p_task_name) {
			m_assignment = virtual_remove_task(p_task_name);
			solve();
		}

		bool worst_task(std::string& p_task_name, double& p_difference) const {
			const double l_p0 = compute_payoff(m_assignment, m_length);
			double l_max_payoff = std::numeric_limits<double>::lowest();
			int l_selected = -1;

			for (std::size_t i = 0; i < m_assignment.size(); i++) {
				const auto l_ss = virtual_remove_task(m_assignment[i].m_name);
				double l_payoff;

				if (l_ss.empty()) {
					l_payoff = 0.0;  // no tasks left: payoff is zero (no income, no travel cost)
				} else {
					const IntegerPath l_path = TSP::solve(l_ss);
					const double l_len = l_path.get_path_length();
					l_payoff = compute_payoff(l_ss, l_len);
				}

				if (l_payoff > l_max_payoff) {
					l_max_payoff = l_payoff;
					l_selected = static_cast<int>(i);
				}
			}

			if (l_selected != -1) {
				const double l_p1 = l_max_payoff;
				p_difference = l_p1 - l_p0;
				p_task_name = m_assignment[static_cast<std::size_t>(l_selected)].m_name;

				if (p_difference > 0) {
					return true;
				}
			}

			p_task_name = "";
			p_difference = 0;
			return false;
		}
};

class PostmanAgent final : public cam::Agent {
	private:
		TaskCollection m_tasks;
		std::vector<std::shared_ptr<CNPMessage>> m_cfps;
		std::vector<std::shared_ptr<CNPMessage>> m_proposals;
		double m_money = 0;  // payments from subcontracting

		// The postmen change the shared CNPMessage objects: one at a time
		static std::mutex& lock() {
			static std::mutex s_mutex;
			return s_mutex;
		}

		void display_status() const {
			cam::dotnet::write_line("\n" + get_name() + "\nTour: " + m_tasks.solution() + "\nPayoff: " + cam::dotnet::format_fixed(m_tasks.payoff() + m_money, 2) +
									" (" + cam::dotnet::format_fixed(m_tasks.payoff(), 2) + " + " + cam::dotnet::format_fixed(m_money, 2) + ")");
		}

		void send_calls_for_proposals() const {
			// the agent has the role of initiator

			std::string l_task_name;
			double l_dif = 0;
			const bool l_can_initiate = m_tasks.worst_task(l_task_name, l_dif);

			if (l_can_initiate) {  // difference in payoff > 0
				const auto l_task = m_tasks[l_task_name];
				if (!l_task) {
					throw std::invalid_argument("Value cannot be null. (Parameter 'task')");
				}
				const auto l_message = CNPMessages::create(get_name(), Performatives::CallForProposals, *l_task, l_dif);
				broadcast(json(l_message->m_index));
			}
		}

		void evaluate_calls_for_proposals() {
			// the agent has the role of participant
			// it evaluates the best call for proposals from initiators

			if (m_cfps.empty()) {
				return;
			}

			double l_best_price = std::numeric_limits<double>::lowest();
			std::shared_ptr<CNPMessage> l_best_proposal;

			for (const auto& l_p: m_cfps) {
				if (l_p->m_participant_payoff + l_p->m_contract_price > l_best_price) {
					l_best_price = l_p->m_participant_payoff + l_p->m_contract_price;
					l_best_proposal = l_p;
				}
			}

			if (l_best_proposal) {
				const std::string l_receiver = l_best_proposal->m_sender;
				l_best_proposal->m_sender = get_name();
				l_best_proposal->m_performative = Performatives::Propose;
				send_by_name(l_receiver, json(l_best_proposal->m_index));
			}

			// here the agent can send a Performatives.Refuse message to all the other agents
			// but in this version of the protocol these messages will be ignored anyway

			m_cfps.clear();
		}

		void evaluate_proposals() {
			// the agent has the role of initiator
			// it evaluates the best proposals from participants

			if (m_proposals.empty()) {
				return;
			}

			double l_best_price = std::numeric_limits<double>::lowest();
			std::shared_ptr<CNPMessage> l_best_proposal;

			for (const auto& l_p: m_proposals) {
				if (l_p->m_initiator_payoff - l_p->m_contract_price > l_best_price) {
					l_best_price = l_p->m_initiator_payoff - l_p->m_contract_price;
					l_best_proposal = l_p;
				}
			}

			if (l_best_proposal) {
				// the initiator awards the contract for the task and pays the participant
				const std::string l_receiver = l_best_proposal->m_sender;
				l_best_proposal->m_sender = get_name();
				l_best_proposal->m_performative = Performatives::AcceptProposal;
				send_by_name(l_receiver, json(l_best_proposal->m_index));
				m_tasks.remove_task(l_best_proposal->m_task.m_name);
				m_money -= l_best_proposal->m_contract_price;
			}

			// here the agent can send a Performatives.RejectProposal message to all the other agents
			// but in this version of the protocol these messages will be ignored anyway

			m_proposals.clear();
		}

	public:
		PostmanAgent(const std::string& p_name, std::vector<Location> p_init_assign) : Agent(p_name), m_tasks(std::move(p_init_assign)) {}

		void setup() override {
			const std::scoped_lock l_lock(lock());
			display_status();
			send_calls_for_proposals();
		}

		void action(const cam::MessagePointer& p_message) override {
			const std::scoped_lock l_lock(lock());
			try {
				if (p_message->to_string() == "finish") {  // protocol finished
					display_status();
					stop();
					return;
				}

				const auto l_msg = CNPMessages::get(p_message->content().get<int>());
				cam::dotnet::write_line("\n" + get_agent_name(p_message->get_sender()).value_or("") + " -> " + get_name() + ": " + to_string(l_msg->m_performative) +
										" " + l_msg->m_task.m_name + " " + cam::dotnet::format_fixed(l_msg->m_initiator_payoff, 2) + " " +
										cam::dotnet::format_fixed(l_msg->m_participant_payoff, 2) + " " + cam::dotnet::format_fixed(l_msg->m_contract_price, 2));

				switch (l_msg->m_performative) {
					case Performatives::CallForProposals: {
						// the agent has the role of participant

						const double l_payoff = m_tasks.additional_payoff_by_accepting(*l_msg);
						if (l_payoff > 0) {
							l_msg->m_participant_payoff = l_payoff;

							// the final payoffs of the initiator and the participant should be equal
							const double l_average_price = (l_msg->m_initiator_payoff + l_msg->m_participant_payoff) / 2.0;

							// intiator pays ContractPrice to the participant
							l_msg->m_contract_price = l_average_price - l_payoff;

							m_cfps.push_back(l_msg);
							cam::dotnet::write_line(get_name() + " receives cfp " + l_msg->m_task.m_name + " with " + cam::dotnet::format_fixed(l_msg->m_initiator_payoff, 2) +
													"/" + cam::dotnet::format_fixed(l_msg->m_participant_payoff, 2) + "/" +
													cam::dotnet::format_fixed(l_msg->m_contract_price, 2));
						}
						break;
					}

					case Performatives::Propose:
						// the agent has the role of intiator

						m_proposals.push_back(l_msg);
						cam::dotnet::write_line(get_name() + " receives proposal " + l_msg->m_task.m_name + " with " + cam::dotnet::format_fixed(l_msg->m_initiator_payoff, 2) +
												"/" + cam::dotnet::format_fixed(l_msg->m_participant_payoff, 2) + "/" + cam::dotnet::format_fixed(l_msg->m_contract_price, 2));
						break;

					case Performatives::AcceptProposal:
						// the agent has the role of participant

						// the participant receives the task and the payment
						cam::dotnet::write_line(get_name() + " is given " + l_msg->m_task.m_name + " with " + cam::dotnet::format_fixed(l_msg->m_initiator_payoff, 2) + "/" +
												cam::dotnet::format_fixed(l_msg->m_participant_payoff, 2) + "/" + cam::dotnet::format_fixed(l_msg->m_contract_price, 2));
						m_tasks.add_task(l_msg->m_task);
						m_money += l_msg->m_contract_price;
						break;

					// the refused cfps and proposals are ignored; no confirmation, result or failure in this application
					case Performatives::Refuse:
					case Performatives::RejectProposal:
					case Performatives::InformDone:
					case Performatives::InformResult:
					case Performatives::Failure:
						break;
				}
			} catch (const std::exception& p_exception) {
				cam::dotnet::write_line(p_exception.what());
			}
		}

		void default_action() override {
			const std::scoped_lock l_lock(lock());
			try {
				// we rely on turn numbers to handle deadlines
				const int l_turn = get_environment()->get_global_data().at("Turn").get<int>();

				if (l_turn % 7 == 2) {
					evaluate_calls_for_proposals();
				} else if (l_turn % 7 == 4) {
					evaluate_proposals();
				} else if (l_turn % 7 == 6) {
					send_calls_for_proposals();
				}
			} catch (const std::exception& p_exception) {
				cam::dotnet::write_line(p_exception.what());
			}
		}
};

class MyEnv final : public cam::Environment {
	public:
		explicit MyEnv(const int p_no_turns) :
				Environment({.m_turns = p_no_turns,
							 .m_mode = cam::EnvironmentMasMode::Parallel,
							 .m_seed = static_cast<unsigned int>(cam::dotnet::next_site_seed("EnvironmentMas.cs:61")),
							 .m_random = cam::RandomEngine::DotNet}) {
			set_global_data("Turn", 0);
		}

		void turn_finished(const int p_turn) override {
			set_global_data("Turn", p_turn + 1);  // turn is updated after TurnFinished
		}
};

int main() {
	MyEnv l_environment(100);

	constexpr int l_no_postmen = 3;
	constexpr int l_no_letters = 20;

	std::vector<Location> l_locations;
	std::vector<int> l_assignment(l_no_letters);
	cam::DotnetRandom l_rand(cam::dotnet::next_site_seed("Program.cs:34"));

	for (int i = 0; i < l_no_letters; i++) {
		// locations between 47.1 - 47.2 latitude, 27.5 - 27.6 longitude
		const double l_latitude = 47.1 + l_rand.next_double() / 10.0;
		const double l_longitude = 27.5 + l_rand.next_double() / 10.0;
		l_locations.push_back({(i + 1 < 10 ? "L0" : "L") + std::to_string(i + 1), l_latitude, l_longitude});
		l_assignment[static_cast<std::size_t>(i)] = l_rand.next(l_no_postmen);
	}

	for (int j = 0; j < l_no_postmen; j++) {
		std::vector<Location> l_init_assign;
		for (int i = 0; i < l_no_letters; i++) {
			if (l_assignment[static_cast<std::size_t>(i)] == j) {
				l_init_assign.push_back(l_locations[static_cast<std::size_t>(i)]);
			}
		}
		l_environment.add<PostmanAgent>("postman" + std::to_string(j + 1), l_init_assign);
	}

	l_environment.start();

	// end of exchanges

	for (int j = 0; j < l_no_postmen; j++) {
		l_environment.send_content_by_name(cam::AgentId("env"), "postman" + std::to_string(j + 1), cam::MessageContent(std::string("finish")), false, true);
	}

	l_environment.continue_simulation(1);
	return 0;
}
