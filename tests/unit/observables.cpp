/**************************************************************************
 *                                                                        *
 *  Description: CPPActressMas multi-agent framework                      *
 *  Website:     https://github.com/jferdelyi/CPPActressMAS               *
 *  Copyright:   (c) 2023-Today, Jean-François Erdelyi                    *
 *                                                                        *
 *  CPP version of ActressMAS by Florin Leon                              *
 *  https://github.com/florinleon/ActressMas                              *
 *                                                                        *
 *  This program and the accompanying materials are made available under  *
 *  the terms of the Eclipse Public License 2.0, which is available at    *
 *  https://www.eclipse.org/legal/epl-2.0/ and in the LICENSE file.       *
 *                                                                        *
 *  SPDX-License-Identifier: EPL-2.0                                      *
 *                                                                        *
 **************************************************************************/

// Observables, as the C# (GetListOfObservableAgents): an agent that uses them sees, before each action, the observables
// of the other agents that pass its perception filter; never its own, nor empty ones, nor those of an agent that has
// stopped; an agent that does not use them is seen, but never sees; the default filter lets nothing through.
// Agents record into a Recorder and never assert; the tests check after start().

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

#include <Environment.h>

#include "Recorder.hpp"

namespace {

	using Mode = cam::EnvironmentMasMode;

	cam::test::Recorder g_recorder;

	// The names it sees, sorted, recorded at each perception
	std::string seen_names(const std::vector<cam::ObservablesPointer>& p_observable_agents) {
		std::vector<std::string> l_names;
		for (const auto& l_observed: p_observable_agents) {
			l_names.push_back(l_observed->at("Name").get<std::string>());
		}
		std::sort(l_names.begin(), l_names.end());
		std::string l_text;
		for (const auto& l_name: l_names) {
			l_text += (l_text.empty() ? "" : ",") + l_name;
		}
		return l_text;
	}

	// Red, uses observables and looks for red agents: itself red, it must never see itself.
	struct Watcher final : cam::Agent {
		explicit Watcher(const std::string& p_name) : Agent(p_name, true) {}
		void setup() override {
			(*m_observables)["Name"] = get_name();
			(*m_observables)["Color"] = "red";
		}
		[[nodiscard]] bool perception_filter(const cam::ObservablesPointer& p_observed) const override {
			return p_observed->at("Color") == "red";
		}
		void see(const std::vector<cam::ObservablesPointer>& p_observable_agents) override {
			g_recorder.record("watcher:saw:" + seen_names(p_observable_agents));
		}
	};

	// Uses observables without its own see: the default one does nothing, and the agent acts at every turn.
	struct Unseeing final : cam::Agent {
		explicit Unseeing(const std::string& p_name) : Agent(p_name, true) {}
		void default_action() override { g_recorder.record("unseeing:acts"); }
	};

	// Shows a color without using observables: it is seen, but never sees.
	struct Shown final : cam::Agent {
		Shown(const std::string& p_name, std::string p_color) : Agent(p_name), m_color(std::move(p_color)) {}
		void setup() override {
			(*m_observables)["Name"] = get_name();
			(*m_observables)["Color"] = m_color;
		}
		void see(const std::vector<cam::ObservablesPointer>&) override { g_recorder.record("shown:saw"); }
	private:
		std::string m_color;
	};

	// Uses observables but shows none, and keeps the default filter: it is never seen and sees nothing.
	struct Blind final : cam::Agent {
		explicit Blind(const std::string& p_name) : Agent(p_name, true) {}
		void see(const std::vector<cam::ObservablesPointer>& p_observable_agents) override {
			g_recorder.record("blind:saw:" + seen_names(p_observable_agents));
		}
	};

	// Red, stops at its second action: seen until it stops, never after.
	struct Quitter final : cam::Agent {
		explicit Quitter(const std::string& p_name) : Agent(p_name) {}
		void setup() override {
			(*m_observables)["Name"] = get_name();
			(*m_observables)["Color"] = "red";
		}
		void default_action() override {
			if (++m_actions == 2) {
				stop();
			}
		}
	private:
		int m_actions = 0;
	};

} // namespace

TEST_SUITE("observables") {

	TEST_CASE("an agent sees the others that pass its filter, never itself, nor empty observables, nor a stopped agent") {
		for (const auto l_mode: {Mode::Sequential, Mode::SequentialRandom, Mode::Parallel}) {
			g_recorder.clear();
			cam::Environment l_env(6, l_mode);
			l_env.add<Watcher>("watcher");
			l_env.add<Shown>("red", "red");
			l_env.add<Shown>("green", "green");
			l_env.add<Blind>("blind");
			l_env.add<Quitter>("quitter");
			l_env.start();

			const auto l_entries = g_recorder.entries();
			std::vector<std::string> l_watcher;
			std::copy_if(l_entries.begin(), l_entries.end(), std::back_inserter(l_watcher),
						 [](const std::string& p_entry) { return p_entry.starts_with("watcher:saw:"); });
			REQUIRE(l_watcher.size() == 5);  // once per turn after the setup turn
			for (const auto& l_seen: l_watcher) {
				// the red agents only: never itself, nor the green one, nor the blind one (no observables)
				CHECK((l_seen == "watcher:saw:quitter,red" || l_seen == "watcher:saw:red"));
			}
			CHECK(l_watcher.front() == "watcher:saw:quitter,red");  // turn 1: the quitter has not stopped yet
			CHECK(l_watcher.back() == "watcher:saw:red");  // once stopped, the quitter is no longer seen
			CHECK(g_recorder.count_of("shown:saw") == 0);  // without observables, never perceives
			CHECK(g_recorder.count_of("blind:saw:") == 5);  // the default filter lets nothing through
		}
	}

	TEST_CASE("an agent that uses observables without its own see acts at every turn, in every mode") {
		for (const auto l_mode: {Mode::Sequential, Mode::SequentialRandom, Mode::Parallel}) {
			g_recorder.clear();
			cam::Environment l_env(4, l_mode);
			l_env.add<Unseeing>("unseeing");
			l_env.add<Watcher>("red");
			l_env.start();
			CHECK(g_recorder.count_of("unseeing:acts") == 3);  // once per turn after the setup turn
		}
	}

}
