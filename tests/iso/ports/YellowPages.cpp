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
// oracle by iso.YellowPages.
// Port of the C# example E06 Yellow Pages (ActressMAS v3.1): providers register their service (Add or Sub) with a
// broker, clients search for one, pick a provider at random and send it a request. Between the runs, provider1 is told
// to unregister and new providers and clients are added. Parallel, 100 turns per run. The clients draw from one
// generator, as from the static Random of the C#, with the seed of its oracle, but under a lock: the C# draws without
// one, which C++ cannot do without undefined behaviour.

#include <algorithm>
#include <cstdint>
#include <exception>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <DotNet.hpp>
#include <Environment.h>
#include <util/DotnetRandom.h>

enum class ServiceType { Add, Sub };

std::string to_string(const ServiceType p_type) {
	return p_type == ServiceType::Add ? "Add" : "Sub";
}

class BrokerAgent final : public cam::Agent {
	private:
		std::map<std::string, std::vector<std::string>> m_service_providers;

		void handle_register(const std::string& p_provider, const std::string& p_service) {
			if (const auto l_it = m_service_providers.find(p_service); l_it != m_service_providers.end()) {
				std::vector<std::string>& l_sp = l_it->second;
				if (std::find(l_sp.begin(), l_sp.end(), p_provider) == l_sp.end()) {
					l_sp.push_back(p_provider);
				}
			} else {
				m_service_providers.emplace(p_service, std::vector<std::string>{p_provider});
			}
		}

		void handle_unregister(const std::string& p_provider, const std::string& p_service) {
			if (const auto l_it = m_service_providers.find(p_service); l_it != m_service_providers.end()) {
				std::vector<std::string>& l_sp = l_it->second;
				if (const auto l_position = std::find(l_sp.begin(), l_sp.end(), p_provider); l_position != l_sp.end()) {
					l_sp.erase(l_position);
				}
			}
		}

		void handle_search(const std::string& p_client, const std::string& p_service) const {
			if (const auto l_it = m_service_providers.find(p_service); l_it != m_service_providers.end()) {
				std::string l_res;
				for (const auto& l_provider: l_it->second) {
					l_res += l_provider + " ";
				}
				send_by_name(p_client, "providers " + cam::dotnet::trim(l_res));
			}
		}

	public:
		explicit BrokerAgent(const std::string& p_name) : Agent(p_name) {}

		void action(const cam::MessagePointer& p_message) override {
			try {
				cam::dotnet::write_line("\t" + format(p_message));
				const auto [l_action, l_parameters] = p_message->parse();
				const std::string l_sender = get_agent_name(p_message->get_sender()).value_or("");

				if (l_action == "register") {
					handle_register(l_sender, l_parameters.at(0));
				} else if (l_action == "unregister") {
					handle_unregister(l_sender, l_parameters.at(0));
				} else if (l_action == "search") {
					handle_search(l_sender, l_parameters.at(0));
				}
			} catch (const std::exception& p_exception) {
				cam::dotnet::write_line(p_exception.what());
			}
		}
};

class ClientAgent final : public cam::Agent {
	private:
		ServiceType m_type;
		int m_operation_parameter1 = 0;
		int m_operation_parameter2 = 0;

		// Shared by the clients, as the static Random of the C#
		static std::int32_t next(const std::int32_t p_max) {
			static std::mutex s_mutex;
			static cam::DotnetRandom s_rand(cam::dotnet::next_site_seed("ClientAgent.cs:26"));
			const std::scoped_lock l_lock(s_mutex);
			return s_rand.next(p_max);
		}

		void handle_providers(const std::vector<std::string>& p_providers) {
			const std::string l_selected = p_providers.at(static_cast<std::size_t>(next(static_cast<std::int32_t>(p_providers.size()))));
			m_operation_parameter1 = next(100);
			m_operation_parameter2 = next(100);
			send_by_name(l_selected, "request " + std::to_string(m_operation_parameter1) + " " + std::to_string(m_operation_parameter2));
		}

		void handle_response(const std::string& p_result) const {
			cam::dotnet::write_line("[" + get_name() + "]: " + to_string(m_type) + "(" + std::to_string(m_operation_parameter1) + ", " +
									std::to_string(m_operation_parameter2) + ") = " + p_result);
		}

	public:
		ClientAgent(const std::string& p_name, const ServiceType p_service_type) : Agent(p_name), m_type(p_service_type) {}

		void setup() override {
			send_by_name("broker", "search " + to_string(m_type));
		}

		void action(const cam::MessagePointer& p_message) override {
			try {
				cam::dotnet::write_line("\t" + format(p_message));
				const auto [l_action, l_parameters] = p_message->parse();

				if (l_action == "providers") {
					handle_providers(l_parameters);
				} else if (l_action == "response") {
					handle_response(l_parameters.at(0));
				}
			} catch (const std::exception& p_exception) {
				cam::dotnet::write_line(p_exception.what());
			}
		}
};

class ProviderAgent final : public cam::Agent {
	private:
		ServiceType m_type;

		void handle_force_unregister() const {
			send_by_name("broker", "unregister " + to_string(m_type));
		}

		void handle_request(const cam::MessagePointer& p_message, const std::vector<std::string>& p_parameters) const {
			const int l_p1 = cam::dotnet::to_int32(p_parameters.at(0));
			const int l_p2 = cam::dotnet::to_int32(p_parameters.at(1));
			const int l_result = (m_type == ServiceType::Add) ? (l_p1 + l_p2) : (l_p1 - l_p2);
			send_by_name(get_agent_name(p_message->get_sender()).value_or(""), "response " + std::to_string(l_result));
		}

	public:
		ProviderAgent(const std::string& p_name, const ServiceType p_service_type) : Agent(p_name), m_type(p_service_type) {}

		void setup() override {
			send_by_name("broker", "register " + to_string(m_type));
		}

		void action(const cam::MessagePointer& p_message) override {
			try {
				cam::dotnet::write_line("\t" + format(p_message));
				const auto [l_action, l_parameters] = p_message->parse();

				if (l_action == "force-unregister") {
					handle_force_unregister();
				} else if (l_action == "request") {
					handle_request(p_message, l_parameters);
				}
			} catch (const std::exception& p_exception) {
				cam::dotnet::write_line(p_exception.what());
			}
		}
};

int main() {
	cam::Environment l_environment({.m_turns = 100,
									.m_mode = cam::EnvironmentMasMode::Parallel,
									.m_seed = static_cast<unsigned int>(cam::dotnet::next_site_seed("EnvironmentMas.cs:61")),
									.m_random = cam::RandomEngine::DotNet});

	l_environment.add<BrokerAgent>("broker");

	const cam::AgentId l_pa1 = l_environment.add<ProviderAgent>("provider1", ServiceType::Add);
	l_environment.add<ProviderAgent>("provider2", ServiceType::Add);
	l_environment.add<ProviderAgent>("provider3", ServiceType::Sub);
	l_environment.add<ProviderAgent>("provider4", ServiceType::Sub);

	l_environment.add<ClientAgent>("client1", ServiceType::Add);
	l_environment.add<ClientAgent>("client2", ServiceType::Add);
	l_environment.add<ClientAgent>("client3", ServiceType::Sub);
	l_environment.add<ClientAgent>("client4", ServiceType::Sub);

	l_environment.start();

	// pa1.Send("provider1", "force-unregister"): provider1 writes to itself, from outside any turn
	l_environment.send_content_by_name(l_pa1, "provider1", cam::MessageContent(std::string("force-unregister")), false, true);

	l_environment.add<ProviderAgent>("provider5", ServiceType::Sub);

	l_environment.continue_simulation(100);

	l_environment.add<ClientAgent>("client5", ServiceType::Add);
	l_environment.add<ClientAgent>("client6", ServiceType::Sub);

	l_environment.continue_simulation(100);
	return 0;
}
