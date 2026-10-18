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

// Characterization crash probes.
//
// Each probe reproduces a known defect of the core (IDs from docs/port/00-analyse-initiale.md) whose
// symptom is a crash, undefined behaviour or a data race, so it never runs inside the test process:
// CTest launches one process per probe (cam_probes <name>) and the expected failure is the sanitizer
// signature (tests/CMakeLists.txt, PASS_REGULAR_EXPRESSION). A probe that runs to completion prints
// "OK no crash". Memory probes have a verdict under ASan + UBSan only, race probes under TSan only.
// Defects whose symptom is a wrong value are doctest cases in known_defects.cpp.
//
// Usage: cam_probes            lists the probes (exit 0)
//        cam_probes <name>     runs one probe; unknown name: exit 2

#include <Environment.h>


#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

	using Mode = cam::EnvironmentMasMode;

	const std::uint8_t* raw(const char* p_text) {
		return reinterpret_cast<const std::uint8_t*>(p_text);
	}

	// ---------------------------------------------------------------- shared agents
	struct Plain final : cam::Agent {  // does nothing
		explicit Plain(const std::string& p_name) : Agent(p_name) {}
	};

	struct Quitter final : cam::Agent {  // stops at its first action (turn 1; turn 0 is the setup)
		explicit Quitter(const std::string& p_name) : Agent(p_name) {}
		void default_action() override { stop(); }
	};

	// ---------------------------------------------------------------- C1 (fixed in 1A) and witness
	// random_permutation(0) computed p_number - 1 in size_t and read out of bounds; the probes stay as
	// regression tests (an empty environment runs one empty turn and finishes, as the C# does).
	void empty_start_parallel() {
		cam::Environment l_env(3, Mode::Parallel);
		l_env.start();
		std::cout << "OK no crash\n";
	}

	void empty_start_sequential() {  // witness: the Sequential branch never calls random_permutation
		cam::Environment l_env(3, Mode::Sequential);
		l_env.start();
		std::cout << "OK no crash\n";
	}

	void empty_start_sequential_random() {
		cam::Environment l_env(3, Mode::SequentialRandom);
		l_env.start();
		std::cout << "OK no crash\n";
	}

	void continue_all_dead_parallel() {
		cam::Environment l_env(2, Mode::Parallel);
		l_env.add<Quitter>("q");
		l_env.start();  // turn 1: stop(); start() leaves before process_buffers (C24)
		std::cout << "count after start=" << l_env.agents_count() << " (dead agent still counted)\n";
		l_env.continue_simulation(2);  // get_ids(alive) is empty -> random_permutation(0)
		std::cout << "OK no crash\n";
	}

	// ---------------------------------------------------------------- C5, C6
	void remove_unknown() {
		cam::Environment l_env(1, Mode::Sequential);
		l_env.remove("does-not-exist");  // get() -> nullptr, dereferenced (Environment.cpp:72)
		std::cout << "OK no crash\n";
	}

	void remove_buffered() {
		cam::Environment l_env(1, Mode::Sequential);
		const std::string l_id = l_env.add<Plain>("x");  // added, not inserted yet
		l_env.remove(l_id);
		std::cout << "OK no crash\n";
	}

	void remove_then_send_by_name() {  // C6, fixed in 1A: kept as a regression test
		cam::Environment l_env(2, Mode::Sequential);
		const std::string l_id = l_env.add<Plain>("target");
		l_env.start();       // agent inserted
		l_env.remove(l_id);  // used to erase m_agents only, not m_agents_by_name
		l_env.send_by_name("ext", "target", raw("x"), 1, false, false);  // used to dereference a null pointer
		std::cout << "OK no crash\n";
	}

	// ---------------------------------------------------------------- C4
	struct PickerEnvironment final : cam::Environment {  // random_agent() is protected
		using cam::Environment::Environment;
		[[nodiscard]] std::string pick() const { return random_agent()->get_name(); }
	};

	void random_agent_bound() {
		PickerEnvironment l_env(1, Mode::Sequential, 0, 1);
		l_env.add<Plain>("only");
		l_env.start();
		for (int i = 0; i < 20; ++i) {
			std::cout << l_env.pick() << " ";  // inclusive bound: end() once every two draws on average
		}
		std::cout << "\nOK no crash (20 draws)\n";
	}

	// ---------------------------------------------------------------- T1, T2, C10, C12
	struct Watcher final : cam::Agent {
		explicit Watcher(const std::string& p_name) : Agent(p_name, true) {}
		int m_key = 0;
		void setup() override { (*m_observables)["v"] = 0; }
		[[nodiscard]] bool perception_filter(const cam::ObservablesPointer&) const override { return true; }
		void see(const std::vector<cam::ObservablesPointer>& p_list) override {
			for (const auto& l_observables: p_list) {
				volatile std::size_t l_count = l_observables->count("v");
				(void) l_count;
				if (const auto l_it = l_observables->find("v"); l_it != l_observables->end()) {
					volatile int l_value = l_it->second.get<int>();
					(void) l_value;
				}
			}
		}
		void default_action() override {
			for (int i = 0; i < 50; i++) {
				(*m_observables)["v"] = i;
			}
			(*m_observables)["k" + std::to_string(m_key++ % 5)] = 1;  // insertions: concurrent rehash
		}
	};

	void observables_parallel() {  // live observables written by their owner while the others read them in see()
		cam::Environment l_env(50, Mode::Parallel);
		for (int i = 0; i < 8; i++) {
			l_env.add<Watcher>("o" + std::to_string(i));
		}
		l_env.start();
		std::cout << "done (verdict: TSan reports)\n";
	}

	struct Victim final : cam::Agent {  // writes m_is_dead (Agent.cpp:95) while handling a message
		Victim(const std::string& p_name, const int p_patience) : Agent(p_name), m_patience(p_patience) {}
		int m_patience;  // messages accepted before stop(): the victims die at successive turns
		int m_received = 0;
		void action(const cam::MessagePointer&) override {
			if (++m_received > m_patience) {
				stop();
			}
		}
	};

	struct Pinger final : cam::Agent {  // reads is_dead() of every victim (Environment.cpp:134) each turn
		explicit Pinger(const std::string& p_name) : Agent(p_name) {}
		void default_action() override { send_by_name("victim", json("ping"), false); }
	};

	void stop_and_send_parallel() {
		cam::Environment l_env(10, Mode::Parallel);
		for (int i = 0; i < 4; i++) {
			l_env.add<Victim>("victim", 5 * i);
		}
		for (int i = 0; i < 5; i++) {
			l_env.add<Pinger>("pinger");
		}
		l_env.start();
		std::cout << "done (verdict: TSan reports)\n";
	}

	struct Remover final : cam::Agent {
		explicit Remover(const std::string& p_name) : Agent(p_name) {}
		void default_action() override {
			const auto l_victims = get_agents_by_name("victim");  // everybody reads m_agents (Environment.cpp:201)
			if (get_name() == "killer" && !l_victims.empty()) {
				get_environment()->remove(l_victims[l_victims.size() / 2]);  // immediate erase (AgentCollection.cpp:60)
			}
		}
	};

	void remove_during_parallel_turn() {
		cam::Environment l_env(20, Mode::Parallel);
		l_env.add<Remover>("killer");
		for (int i = 0; i < 200; i++) {
			l_env.add<Remover>("victim");
		}
		l_env.start();
		std::cout << "done\n";
	}

	struct SequentialKiller final : cam::Agent {
		explicit SequentialKiller(const std::string& p_name) : Agent(p_name) {}
		void default_action() override {
			if (get_name() == "killer") {
				const auto l_victims = get_agents_by_name("victim");
				if (!l_victims.empty()) {
					get_environment()->remove(l_victims[0]);  // during the range-for of run_turn()
				}
			}
		}
	};

	void remove_during_sequential_turn() {
		cam::Environment l_env(2, Mode::Sequential);
		l_env.add<SequentialKiller>("killer");
		for (int i = 0; i < 6; i++) {
			l_env.add<SequentialKiller>("victim");
		}
		l_env.start();
		std::cout << "OK no crash\n";
	}

	// ---------------------------------------------------------------- T7
	void rng_two_threads() {  // two environments in two threads: same static mt19937, no lock
		const auto l_run = [] {
			cam::Environment l_env(200, Mode::SequentialRandom, 0, 1);
			for (const char* l_name: {"A", "B", "C"}) {
				l_env.add<Plain>(l_name);
			}
			l_env.start();
		};
		std::thread l_first(l_run);
		std::thread l_second(l_run);
		l_first.join();
		l_second.join();
		std::cout << "done (verdict: TSan reports)\n";
	}

	// ---------------------------------------------------------------- C16
	void dangling_add_ref() {
		cam::Environment l_env(3, Mode::Sequential);
		const std::string& l_id = l_env.add<Quitter>("q1");  // reference to Agent::m_id
		l_env.add<Quitter>("q2");  // q2 no longer stays alive in the sentinel node of m_new_agents (C17, fixed in 3B)
		l_env.start();             // q1 and q2 stop and are removed: q1 is destroyed
		std::cout << "id still readable? size=" << l_id.size() << " value=" << l_id << "\n";
		std::cout << "OK no crash\n";
	}

	// ---------------------------------------------------------------- registry
	struct Probe {
		const char* m_name;
		const char* m_ids;
		void (*m_run)();
	};

	const Probe g_probes[] = {
			{"empty_start_parallel", "C1 (fixed in 1A)", empty_start_parallel},
			{"empty_start_sequential", "witness C1", empty_start_sequential},
			{"empty_start_sequential_random", "C1 (fixed in 1A)", empty_start_sequential_random},
			{"continue_all_dead_parallel", "C1 (fixed in 1A) C24", continue_all_dead_parallel},
			{"remove_unknown", "C5", remove_unknown},
			{"remove_buffered", "C5", remove_buffered},
			{"remove_then_send_by_name", "C6 (fixed in 1A)", remove_then_send_by_name},
			{"random_agent_bound", "C4", random_agent_bound},
			{"observables_parallel", "T1 (TSan)", observables_parallel},
			{"stop_and_send_parallel", "T2 (TSan)", stop_and_send_parallel},
			{"remove_during_parallel_turn", "C10 (TSan, ASan)", remove_during_parallel_turn},
			{"remove_during_sequential_turn", "C12", remove_during_sequential_turn},
			{"rng_two_threads", "T7 (TSan)", rng_two_threads},
			{"dangling_add_ref", "C16", dangling_add_ref},
	};

	void list_probes(std::ostream& p_out) {
		p_out << "usage: cam_probes <probe>\n";
		for (const auto& l_probe: g_probes) {
			p_out << "  " << std::left << std::setw(32) << l_probe.m_name << l_probe.m_ids << "\n";
		}
	}

} // namespace

int main(int argc, char** argv) {
	std::cout << std::unitbuf;  // every line is flushed before a possible sanitizer abort
	if (argc < 2) {
		list_probes(std::cout);
		return 0;
	}
	const std::string l_name = argv[1];
	for (const auto& l_probe: g_probes) {
		if (l_name == l_probe.m_name) {
			l_probe.m_run();
			return 0;
		}
	}
	std::cerr << "unknown probe: " << l_name << "\n";
	list_probes(std::cerr);
	return 2;
}
