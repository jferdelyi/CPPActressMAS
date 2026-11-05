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


// The turn loop, as the C# (RunTurn): an exception thrown by an agent leaves start() and continue_simulation() at once
// in the Sequential modes; in Parallel mode every agent of the turn runs to its end, then the exceptions of the turn
// leave together, in the order of the agents, in an AggregateException (Parallel.Invoke); turn_finished() and
// simulation_finished() are not called then. The setup of an agent runs alone at its first turn, or, with
// m_run_setup_separately set to false, once in that turn before its first see and action. Agents record into a Recorder
// and never assert; the tests check after start().

#include <doctest/doctest.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

#include <Environment.h>

#include "Recorder.hpp"

namespace {

	using Mode = cam::EnvironmentMasMode;

	cam::test::Recorder g_recorder;

	// Acts at every turn.
	struct Worker final : cam::Agent {
		explicit Worker(const std::string& p_name) : Agent(p_name) {}
		void default_action() override { g_recorder.record(get_name() + ":acted"); }
	};

	// Throws at each of its actions, or at its first one only.
	struct Thrower final : cam::Agent {
		Thrower(const std::string& p_name, const bool p_once) : Agent(p_name), m_once(p_once) {}
		void default_action() override {
			g_recorder.record(get_name() + ":acted");
			if (!m_once || !m_thrown) {
				m_thrown = true;
				throw std::runtime_error("boom from " + get_name());
			}
		}
	private:
		bool m_once;
		bool m_thrown = false;
	};

	// Uses observables, and records its setup, its perceptions and its actions.
	struct Phases final : cam::Agent {
		explicit Phases(const std::string& p_name) : Agent(p_name, true) {}
		void setup() override {
			(*m_observables)["Name"] = get_name();
			g_recorder.record(get_name() + ":setup");
		}
		[[nodiscard]] bool perception_filter(const cam::ObservablesPointer&) const override { return true; }
		void see(const std::vector<cam::ObservablesPointer>& p_observable_agents) override {
			g_recorder.record(get_name() + ":see:" + std::to_string(p_observable_agents.size()));
		}
		void default_action() override { g_recorder.record(get_name() + ":act"); }
	};

	// Records the end of each turn and the end of the simulation.
	struct Watched final : cam::Environment {
		Watched(const int p_turns, const Mode p_mode) : Environment(p_turns, p_mode) {}
		void turn_finished(const int p_turn) override { g_recorder.record("turn:" + std::to_string(p_turn)); }
		void simulation_finished() override { g_recorder.record("finished"); }
	};

	// The number of entries of the recorder that start with a prefix
	std::size_t count_prefix(const std::string& p_prefix) {
		const auto l_entries = g_recorder.entries();
		return static_cast<std::size_t>(std::count_if(l_entries.begin(), l_entries.end(),
													  [&p_prefix](const std::string& p_entry) { return p_entry.starts_with(p_prefix); }));
	}

	// The messages of the exceptions of an aggregate, in their order
	std::vector<std::string> messages_of(const cam::AggregateException& p_aggregate) {
		std::vector<std::string> l_messages;
		for (const auto& l_exception: p_aggregate.exceptions()) {
			try {
				std::rethrow_exception(l_exception);
			} catch (const std::exception& p_exception) {
				l_messages.emplace_back(p_exception.what());
			}
		}
		return l_messages;
	}

} // namespace

TEST_SUITE("turns") {

	TEST_CASE("in Parallel mode, the exceptions of a turn leave start() together once every agent has run, in their order") {
		g_recorder.clear();
		Watched l_env(3, Mode::Parallel);
		l_env.add<Worker>("w1");
		l_env.add<Thrower>("t1", false);
		l_env.add<Worker>("w2");
		l_env.add<Thrower>("t2", false);
		l_env.add<Worker>("w3");
		std::vector<std::string> l_messages;
		std::string l_what;
		try {
			l_env.start();  // turn 0: the setups; turn 1: both throwers throw
		} catch (const cam::AggregateException& p_aggregate) {
			l_messages = messages_of(p_aggregate);
			l_what = p_aggregate.what();
		}
		CHECK(l_messages == std::vector<std::string>{"boom from t1", "boom from t2"});  // the order of the agents
		CHECK(l_what == "One or more errors occurred in the agents of a Parallel turn (2): boom from t1");
		for (const auto* l_name: {"w1", "t1", "w2", "t2", "w3"}) {
			CHECK(g_recorder.count_of(std::string(l_name) + ":acted") == 1);  // every agent of the turn has run
		}
		CHECK(g_recorder.contains("turn:0"));
		CHECK_FALSE(g_recorder.contains("turn:1"));  // as the C#: no TurnFinished
		CHECK_FALSE(g_recorder.contains("finished"));  // nor SimulationFinished
	}

	TEST_CASE("after the exceptions of a Parallel turn, the simulation continues") {
		g_recorder.clear();
		Watched l_env(3, Mode::Parallel);
		l_env.add<Worker>("w");
		l_env.add<Thrower>("t", true);
		CHECK_THROWS_AS(l_env.start(), cam::AggregateException);
		l_env.continue_simulation(2);  // the thrower throws no more
		CHECK(l_env.agents_count() == 2);
		CHECK(g_recorder.count_of("w:acted") == 3);
		CHECK(g_recorder.count_of("t:acted") == 3);
		CHECK(g_recorder.count_of("finished") == 1);
	}

	TEST_CASE("in the Sequential modes, an exception leaves start() at once, as it was thrown, as the C#") {
		g_recorder.clear();
		Watched l_env(3, Mode::Sequential);
		l_env.add<Worker>("w1");
		l_env.add<Thrower>("t", false);
		l_env.add<Worker>("w2");
		bool l_aggregate = false;
		std::string l_what;
		try {
			l_env.start();
		} catch (const cam::AggregateException&) {
			l_aggregate = true;
		} catch (const std::runtime_error& p_error) {
			l_what = p_error.what();
		}
		CHECK_FALSE(l_aggregate);
		CHECK(l_what == "boom from t");
		CHECK(g_recorder.count_of("w1:acted") == 1);
		CHECK(g_recorder.count_of("w2:acted") == 0);  // the turn stops at the thrower
		CHECK_FALSE(g_recorder.contains("turn:1"));
		CHECK_FALSE(g_recorder.contains("finished"));
	}


	TEST_CASE("by default, the setup of an agent runs alone at its first turn, in every mode, as the C#") {
		for (const auto l_mode: {Mode::Sequential, Mode::SequentialRandom, Mode::Parallel}) {
			g_recorder.clear();
			cam::Environment l_env(3, l_mode);
			l_env.add<Phases>("p1");
			l_env.add<Phases>("p2");
			l_env.start();
			for (const std::string l_name: {"p1", "p2"}) {
				CHECK(g_recorder.count_of(l_name + ":setup") == 1);
				CHECK(count_prefix(l_name + ":see:") == 2);
				CHECK(g_recorder.count_of(l_name + ":act") == 2);
			}
		}
	}

	TEST_CASE("with the setup in the same turn, it runs once, then see and the action, from the first turn, in every mode") {
		for (const auto l_mode: {Mode::Sequential, Mode::SequentialRandom, Mode::Parallel}) {
			g_recorder.clear();
			cam::Environment l_env({.m_turns = 3, .m_mode = l_mode, .m_run_setup_separately = false});
			l_env.add<Phases>("p1");
			l_env.add<Phases>("p2");
			l_env.start();
			for (const std::string l_name: {"p1", "p2"}) {
				CHECK(g_recorder.count_of(l_name + ":setup") == 1);
				CHECK(count_prefix(l_name + ":see:") == 3);
				CHECK(g_recorder.count_of(l_name + ":act") == 3);
			}
		}
	}

	TEST_CASE("with the setup in the same turn, an agent sees in its first turn what the others showed at their setup") {
		g_recorder.clear();
		cam::Environment l_env({.m_turns = 1, .m_mode = Mode::Sequential, .m_run_setup_separately = false});
		l_env.add<Phases>("p1");
		l_env.add<Phases>("p2");
		l_env.start();  // p1: setup, sees nothing yet; p2: setup, sees p1
		CHECK(g_recorder.entries() == std::vector<std::string>{"p1:setup", "p1:see:0", "p1:act", "p2:setup", "p2:see:1", "p2:act"});
	}

}
