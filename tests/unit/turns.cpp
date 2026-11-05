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
// simulation_finished() are not called then. Agents record into a Recorder and never assert; the tests check after
// start().

#include <doctest/doctest.h>

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

	// Records the end of each turn and the end of the simulation.
	struct Watched final : cam::Environment {
		Watched(const int p_turns, const Mode p_mode) : Environment(p_turns, p_mode) {}
		void turn_finished(const int p_turn) override { g_recorder.record("turn:" + std::to_string(p_turn)); }
		void simulation_finished() override { g_recorder.record("finished"); }
	};

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

}
