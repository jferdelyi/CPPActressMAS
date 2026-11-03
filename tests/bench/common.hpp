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

#pragma once

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include <Environment.h>

namespace bench {

	/**
	 * Command line options shared by the benchmarks.
	 **/
	struct Options {
		/** Deterministic variant: Sequential mode, fixed order, no thread pool. **/
		bool m_sequential = false;
		/** Parallel mode (SkynetNumeric runs SequentialRandom by default, as the C# does). **/
		bool m_parallel = false;
		/** Number of turns (PingPong). **/
		int m_turns = 10000;
		/** Number of trials (PingPong). **/
		int m_trials = 5;
		/** Number of agents (PingPong) or maximum number of agents (Skynet). **/
		int m_agents = 0;
		/** Number of pairs of agents (PingPongPairs). **/
		int m_pairs = 500;
		/** Number of messages per pair, even (PingPongPairs). **/
		int m_messages = 100;
		/** Seed of the random generators (Wsg, Flockers). **/
		int m_seed = 1;
		/** Print one CSV line per measurement instead of the C# text. **/
		bool m_csv = false;
	};

	/**
	 * Parse the command line: --sequential, --parallel, --turns N, --trials N, --agents N, --pairs N, --messages N,
	 * --seed N, --csv.
	 * @param p_argc Argument count
	 * @param p_argv Arguments
	 * @param p_default_agents Default agent count of the benchmark
	 * @return The options
	 **/
	inline Options parse(const int p_argc, char** p_argv, const int p_default_agents) {
		Options l_options;
		l_options.m_agents = p_default_agents;
		for (int i = 1; i < p_argc; i++) {
			const std::string l_argument = p_argv[i];
			const auto l_value = [&]() { return i + 1 < p_argc ? std::atoi(p_argv[++i]) : 0; };
			if (l_argument == "--sequential") {
				l_options.m_sequential = true;
			} else if (l_argument == "--parallel") {
				l_options.m_parallel = true;
			} else if (l_argument == "--turns") {
				l_options.m_turns = l_value();
			} else if (l_argument == "--trials") {
				l_options.m_trials = l_value();
			} else if (l_argument == "--agents") {
				l_options.m_agents = l_value();
			} else if (l_argument == "--pairs") {
				l_options.m_pairs = l_value();
			} else if (l_argument == "--messages") {
				l_options.m_messages = l_value();
			} else if (l_argument == "--seed") {
				l_options.m_seed = l_value();
			} else if (l_argument == "--csv") {
				l_options.m_csv = true;
			} else {
				std::cerr << "usage: " << p_argv[0] << " [--sequential|--parallel] [--turns N] [--trials N] [--agents N] [--pairs N]"
						  << " [--messages N] [--seed N] [--csv]\n";
				std::exit(2);
			}
		}
		return l_options;
	}

	/**
	 * Monotonic stopwatch in microseconds.
	 **/
	class Stopwatch final {
	private:
		std::chrono::steady_clock::time_point m_start;

	public:
		Stopwatch() : m_start(std::chrono::steady_clock::now()) {}

		/**
		 * Restart the stopwatch.
		 **/
		void restart() { m_start = std::chrono::steady_clock::now(); }

		/**
		 * Elapsed time since the start.
		 * @return Microseconds
		 **/
		[[nodiscard]] long long elapsed_us() const {
			return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - m_start).count();
		}
	};

	/**
	 * Pointer to the bytes of a text, for raw (unencoded) message contents.
	 * @param p_text The text
	 * @return The bytes
	 **/
	inline const std::uint8_t* raw(const std::string& p_text) {
		return reinterpret_cast<const std::uint8_t*>(p_text.data());
	}

} // namespace bench
