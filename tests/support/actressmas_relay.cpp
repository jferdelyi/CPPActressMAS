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

// The relay of the tests as a program: it listens on a port of the system, runs a command in which "{port}" stands for
// that port, and exits with the exit code of the command. The output of the command is the output of this program: the
// iso test of RemoteMessages runs the port of the C# example over it. With --swap, the first of each two messages of a
// container goes out that long after the second, as the thread pool of the C# server may do.
// Usage: cam_actressmas_relay [--period <ms>] [--swap <ms>] -- <command> [arguments...]

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

#include "ActressMasRelay.hpp"

namespace {

	// "{port}" replaced by the port, the whole between double quotes
	std::string argument(std::string p_argument, const int p_port) {
		static const std::string s_placeholder = "{port}";
		for (std::size_t l_at = p_argument.find(s_placeholder); l_at != std::string::npos; l_at = p_argument.find(s_placeholder)) {
			p_argument.replace(l_at, s_placeholder.size(), std::to_string(p_port));
		}
		return "\"" + p_argument + "\"";
	}

} // namespace

int main(int argc, char** argv) {
	std::chrono::milliseconds l_period(50);
	std::chrono::milliseconds l_swap(0);
	int l_index = 1;
	for (; l_index + 1 < argc && (std::string(argv[l_index]) == "--period" || std::string(argv[l_index]) == "--swap"); l_index += 2) {
		(std::string(argv[l_index]) == "--period" ? l_period : l_swap) = std::chrono::milliseconds(std::atoi(argv[l_index + 1]));
	}
	if (l_index >= argc || std::string(argv[l_index]) != "--" || l_index + 1 >= argc) {
		std::cerr << "usage: cam_actressmas_relay [--period <ms>] [--swap <ms>] -- <command> [arguments...]\n";
		return 2;
	}
	l_index++;

	cam::test::ActressMasRelay l_relay(l_period, l_swap);
	if (!l_relay.start()) {
		std::cerr << "the relay cannot listen on 127.0.0.1\n";
		return 1;
	}
	std::string l_command = argument(argv[l_index++], l_relay.port());
	for (; l_index < argc; l_index++) {
		l_command += " " + argument(argv[l_index], l_relay.port());
	}
#if defined(_WIN32)
	// cmd /c drops the first and the last quote of a line with several quoted pieces: one more pair keeps them
	l_command = "\"" + l_command + "\"";
#endif
	std::fflush(stdout);
	const int l_status = std::system(l_command.c_str());
	l_relay.stop();
#if defined(_WIN32)
	return l_status;
#else
	return WIFEXITED(l_status) ? WEXITSTATUS(l_status) : 1;
#endif
}
