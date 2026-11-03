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

// C++ feature example (no C# original): a long text broadcast to many receivers, copied for each receiver when it is a
// std::string, shared by all of them when it is a cam::Text (one immutable buffer, its characters never copied).
// Arguments: number of receivers (100 by default), size of the text in kilobytes (256 by default).

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

#include <Environment.h>

namespace {

	std::size_t g_received_characters = 0;  // Sequential environments: one thread

	// Counts the characters of every text it receives.
	class Reader final : public cam::Agent {
		public:
			explicit Reader(const std::string& p_name) : Agent(p_name) {}

			void action(const cam::MessagePointer& p_message) override {
				g_received_characters += p_message->as_text().size();
			}
	};

	// Broadcasts its text once, at its setup.
	template<typename T>
	class Broadcaster final : public cam::Agent {
		private:
			T m_text;

		public:
			Broadcaster(const std::string& p_name, T p_text) : Agent(p_name), m_text(std::move(p_text)) {}

			void setup() override {
				broadcast(m_text);
			}
	};

	// One environment, one broadcast to every reader: the time of the run and the characters received
	template<typename T>
	void run(const char* p_label, const T& p_text, const int p_receivers, const std::size_t p_size) {
		g_received_characters = 0;
		const auto l_start = std::chrono::steady_clock::now();
		{
			cam::Environment l_environment({.m_turns = 2, .m_mode = cam::EnvironmentMasMode::Sequential});
			for (int i = 0; i < p_receivers; i++) {
				l_environment.add<Reader>("reader");
			}
			l_environment.add<Broadcaster<T>>("broadcaster", p_text);
			l_environment.start();
		}
		const double l_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - l_start).count();
		std::cout << p_label << ": " << l_ms << " ms, " << g_received_characters << " characters received"
				  << (g_received_characters == p_size * static_cast<std::size_t>(p_receivers) ? "" : " (MISSING)") << std::endl;
	}

} // namespace

int main(int argc, char** argv) {
	const int l_receivers = argc > 1 ? std::atoi(argv[1]) : 100;
	const std::size_t l_size = static_cast<std::size_t>(argc > 2 ? std::atol(argv[2]) : 256) * 1024;
	const std::string l_characters(l_size, 'x');

	std::cout << "A text of " << l_size << " characters broadcast to " << l_receivers << " receivers" << std::endl;
	run("std::string (copied for each receiver)", l_characters, l_receivers, l_size);
	run("cam::Text (shared by every receiver)", cam::Text(l_characters), l_receivers, l_size);
	return 0;
}
