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
#include <map>
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

	// Shows the number of its last action, 0 after its setup.
	struct Ticker final : cam::Agent {
		explicit Ticker(const std::string& p_name) : Agent(p_name) {}
		void setup() override {
			(*m_observables)["Name"] = get_name();
			(*m_observables)["Tick"] = 0;
		}
		void default_action() override { (*m_observables)["Tick"] = ++m_ticks; }
	private:
		int m_ticks = 0;
	};

	// Records the tick it sees, at each perception.
	struct TickWatcher final : cam::Agent {
		explicit TickWatcher(const std::string& p_name) : Agent(p_name, true) {}
		[[nodiscard]] bool perception_filter(const cam::ObservablesPointer& p_observed) const override {
			return p_observed->contains("Tick");
		}
		void see(const std::vector<cam::ObservablesPointer>& p_observable_agents) override {
			for (const auto& l_observed: p_observable_agents) {
				g_recorder.record(get_name() + ":tick=" + l_observed->at("Tick").dump());
			}
		}
	};

	// Writes the same number under two keys, many times in each action.
	struct Pair final : cam::Agent {
		explicit Pair(const std::string& p_name) : Agent(p_name) {}
		void setup() override { write(0); }
		void default_action() override {
			for (int i = 0; i < 100; ++i) {
				write(++m_value);
			}
		}
	private:
		void write(const int p_value) {
			(*m_observables)["A"] = p_value;
			(*m_observables)["B"] = p_value;
		}
		int m_value = 0;
	};

	// Records whether the two keys of each pair it sees are equal.
	struct PairChecker final : cam::Agent {
		explicit PairChecker(const std::string& p_name) : Agent(p_name, true) {}
		[[nodiscard]] bool perception_filter(const cam::ObservablesPointer& p_observed) const override {
			return p_observed->contains("A");
		}
		void see(const std::vector<cam::ObservablesPointer>& p_observable_agents) override {
			for (const auto& l_observed: p_observable_agents) {
				g_recorder.record(l_observed->at("A") == l_observed->at("B") ? "pair:whole" : "pair:torn");
			}
		}
	};

	// Red from its constructor, before any setup.
	struct Painted final : cam::Agent {
		explicit Painted(const std::string& p_name) : Agent(p_name) {
			(*m_observables)["Name"] = p_name;
			(*m_observables)["Color"] = "red";
		}
	};

	// Adds a red agent at its first action.
	struct Maker final : cam::Agent {
		explicit Maker(const std::string& p_name) : Agent(p_name) {}
		void default_action() override {
			if (!m_done) {
				m_done = true;
				get_environment()->add<Painted>("newcomer");
			}
		}
	private:
		bool m_done = false;
	};

	// Records, at each perception, whether each copy it sees is the one it saw before.
	struct CopyWatcher final : cam::Agent {
		explicit CopyWatcher(const std::string& p_name) : Agent(p_name, true) {}
		[[nodiscard]] bool perception_filter(const cam::ObservablesPointer&) const override { return true; }
		void see(const std::vector<cam::ObservablesPointer>& p_observable_agents) override {
			for (const auto& l_observed: p_observable_agents) {
				const std::string l_name = l_observed->at("Name").get<std::string>();
				g_recorder.record(get_name() + ":" + l_name + (m_last[l_name] == l_observed ? ":same" : ":new"));
				m_last[l_name] = l_observed;
			}
		}
	private:
		std::map<std::string, cam::ObservablesPointer> m_last;
	};

	// The entries of the recorder that start with a prefix, in their order
	std::vector<std::string> entries_of(const std::string& p_prefix) {
		const auto l_entries = g_recorder.entries();
		std::vector<std::string> l_found;
		std::copy_if(l_entries.begin(), l_entries.end(), std::back_inserter(l_found),
					 [&p_prefix](const std::string& p_entry) { return p_entry.starts_with(p_prefix); });
		return l_found;
	}

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

	TEST_CASE("in a sequential turn, an agent sees what an agent that ran before it wrote in that turn, as the C#") {
		g_recorder.clear();
		cam::Environment l_env(4, Mode::Sequential);
		l_env.add<TickWatcher>("before");
		l_env.add<Ticker>("ticker");
		l_env.add<TickWatcher>("after");
		l_env.start();  // turn 0: the setups; turns 1 to 3: the ticker shows 1, 2, then 3
		const std::vector<std::string> l_expected = {"before:tick=0", "after:tick=1", "before:tick=1",
													 "after:tick=2", "before:tick=2", "after:tick=3"};
		CHECK(g_recorder.entries() == l_expected);
	}

	TEST_CASE("in a Parallel turn, an agent never sees a table half written") {
		g_recorder.clear();
		cam::Environment l_env(30, Mode::Parallel);
		for (int i = 0; i < 4; ++i) {
			l_env.add<Pair>("pair" + std::to_string(i));
			l_env.add<PairChecker>("checker" + std::to_string(i));
		}
		l_env.start();
		CHECK(g_recorder.count_of("pair:whole") == 4 * 4 * 29);  // each checker sees the 4 pairs at each turn after the setup
		CHECK(g_recorder.count_of("pair:torn") == 0);
	}

	TEST_CASE("an agent added during a turn is observable in that turn, as the C#") {
		g_recorder.clear();
		cam::Environment l_env(3, Mode::Sequential);
		l_env.add<Maker>("maker");
		l_env.add<Watcher>("watcher");  // runs after the maker in each turn
		l_env.start();  // turn 1: the maker adds the newcomer, which the watcher sees at once; turn 2: inserted, still seen
		CHECK(entries_of("watcher:saw:") == std::vector<std::string>{"watcher:saw:newcomer", "watcher:saw:newcomer"});
	}

	TEST_CASE("the others see a copy published again only when the observables change") {
		g_recorder.clear();
		cam::Environment l_env(4, Mode::Sequential);
		l_env.add<CopyWatcher>("copies");  // runs first in each turn
		l_env.add<Shown>("shown", "red");  // writes its observables at its setup only
		l_env.add<Ticker>("ticker");  // changes them at each action
		l_env.start();
		const std::vector<std::string> l_expected = {"copies:shown:new", "copies:ticker:new", "copies:shown:same",
													 "copies:ticker:new", "copies:shown:same", "copies:ticker:new"};
		CHECK(entries_of("copies:") == l_expected);
	}

	TEST_CASE("an agent reads its own observables as it writes them, as the C#") {
		g_recorder.clear();
		cam::Environment l_env(2, Mode::Sequential);
		const auto l_ticker = std::make_shared<Ticker>("ticker");
		l_env.add(l_ticker);
		l_env.start();  // turn 0: setup; turn 1: shows 1
		CHECK(l_ticker->get_observables()->at("Tick") == 1);
	}

}
