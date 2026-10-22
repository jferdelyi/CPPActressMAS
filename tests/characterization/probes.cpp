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
// Each probe reproduces a known defect of the core whose symptom is a crash, undefined behaviour or a data race, so it never runs inside the test process:
// CTest launches one process per probe (cam_probes <name>) and the expected failure is the sanitizer
// signature (tests/characterization/CMakeLists.txt, PASS_REGULAR_EXPRESSION). A probe that runs to completion prints
// "OK no crash". Memory probes have a verdict under ASan + UBSan only, race probes under TSan only. A fixed defect
// keeps its probe as a regression test, which must run to completion without any sanitizer report.
// Defects whose symptom is a wrong value are doctest cases in known_defects.cpp.
//
// Usage: cam_probes            lists the probes (exit 0)
//        cam_probes <name>     runs one probe; unknown name: exit 2

#include <Environment.h>


#include <atomic>
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

	// ---------------------------------------------------------------- empty turn (fixed)
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
		l_env.start();  // turn 1: stop(); start() leaves before process_buffers
		std::cout << "count after start=" << l_env.agents_count() << " (dead agent still counted)\n";
		l_env.continue_simulation(2);  // get_ids(alive) is empty -> random_permutation(0)
		std::cout << "OK no crash\n";
	}

	// ---------------------------------------------------------------- removal (fixed)
	void remove_unknown() {
		cam::Environment l_env(1, Mode::Sequential);
		l_env.remove("does-not-exist");  // used to dereference the null pointer of get(); now ignored
		std::cout << "OK no crash\n";
	}

	void remove_buffered() {
		cam::Environment l_env(1, Mode::Sequential);
		const cam::AgentId l_id = l_env.add<Plain>("x");  // added, not inserted yet: ignored
		l_env.remove(l_id);
		std::cout << "OK no crash\n";
	}

	void remove_then_send_by_name() {
		cam::Environment l_env(2, Mode::Sequential);
		const cam::AgentId l_id = l_env.add<Plain>("target");
		l_env.start();       // agent inserted
		l_env.remove(l_id);  // used to erase m_agents only, not m_agents_by_name
		l_env.send_by_name("ext", "target", raw("x"), 1, false, false);  // used to dereference a null pointer
		std::cout << "OK no crash\n";
	}

	// ---------------------------------------------------------------- random agent (fixed)
	struct PickerEnvironment final : cam::Environment {  // random_agent() was protected and returned the agent
		using cam::Environment::Environment;
		[[nodiscard]] std::string pick() const { return get_agent_name(*random_agent()).value_or("?"); }
	};

	void random_agent_bound() {  // one bounded draw, an id returned
		PickerEnvironment l_env(1, Mode::Sequential, 0, 1);
		l_env.add<Plain>("only");
		l_env.start();
		for (int i = 0; i < 20; ++i) {
			std::cout << l_env.pick() << " ";  // the inclusive bound used to reach end() once every two draws
		}
		std::cout << "\nOK no crash (20 draws)\n";
	}

	// ---------------------------------------------------------------- Parallel turns
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

	struct Victim final : cam::Agent {  // writes its dead flag while handling a message
		Victim(const std::string& p_name, const int p_patience) : Agent(p_name), m_patience(p_patience) {}
		int m_patience;  // messages accepted before stop(): the victims die at successive turns
		int m_received = 0;
		void action(const cam::MessagePointer&) override {
			if (++m_received > m_patience) {
				stop();
			}
		}
	};

	struct Pinger final : cam::Agent {  // reads is_dead() of every victim each turn
		explicit Pinger(const std::string& p_name) : Agent(p_name) {}
		void default_action() override { send_by_name("victim", json("ping"), false); }
	};

	void stop_and_send_parallel() {  // fixed: atomic dead flag
		cam::Environment l_env(10, Mode::Parallel);
		for (int i = 0; i < 4; i++) {
			l_env.add<Victim>("victim", 5 * i);
		}
		for (int i = 0; i < 5; i++) {
			l_env.add<Pinger>("pinger");
		}
		l_env.start();
		std::cout << "OK no crash\n";
	}

	struct Remover final : cam::Agent {
		explicit Remover(const std::string& p_name) : Agent(p_name) {}
		void default_action() override {
			const auto l_victims = get_agents_by_name("victim");  // everybody reads the collection
			if (get_name() == "killer" && !l_victims.empty()) {
				get_environment()->remove(l_victims[l_victims.size() / 2]);  // used to erase at once; now marked
			}
		}
	};

	void remove_during_parallel_turn() {  // fixed
		cam::Environment l_env(20, Mode::Parallel);
		l_env.add<Remover>("killer");
		for (int i = 0; i < 200; i++) {
			l_env.add<Remover>("victim");
		}
		l_env.start();
		std::cout << "OK no crash\n";
	}

	struct SequentialKiller final : cam::Agent {
		explicit SequentialKiller(const std::string& p_name) : Agent(p_name) {}
		void default_action() override {
			if (get_name() == "killer") {
				const auto l_victims = get_agents_by_name("victim");
				if (!l_victims.empty()) {
					get_environment()->remove(l_victims[0]);  // during the range-for of run_turn(): now marked
				}
			}
		}
	};

	void remove_during_sequential_turn() {  // fixed
		cam::Environment l_env(2, Mode::Sequential);
		l_env.add<SequentialKiller>("killer");
		for (int i = 0; i < 6; i++) {
			l_env.add<SequentialKiller>("victim");
		}
		l_env.start();
		std::cout << "OK no crash\n";
	}

	// ---------------------------------------------------------------- outside the turns (known)
	std::atomic<bool> g_sink_ready{false};
	std::atomic<bool> g_outside_done{false};

	struct Sink final : cam::Agent {  // takes the messages from outside; stops once the outside thread is done
		explicit Sink(const std::string& p_name) : Agent(p_name) {}
		void setup() override { g_sink_ready = true; }
		void action(const cam::MessagePointer&) override {}
		void default_action() override {
			if (g_outside_done) {
				stop();
			}
		}
	};

	// send_content() from another thread looks the receiver up in the collection that the turn boundary changes when it
	// inserts the agents added from outside: some messages are lost (the C# keeps its agents in a ConcurrentDictionary
	// in Parallel mode). The end does not depend on a message, which could be lost.
	void outside_add_and_send_parallel() {
		cam::Environment l_env({.m_turns = 0, .m_mode = Mode::Parallel});
		const cam::AgentId l_sink = l_env.add<Sink>("sink");
		std::thread l_runner([&l_env] { l_env.start(); });
		while (!g_sink_ready) {
			std::this_thread::yield();
		}
		for (int i = 0; i < 100; i++) {
			l_env.add<Quitter>("quitter");
			l_env.send_content("outside", l_sink, cam::MessageContent(std::string("m")));
		}
		g_outside_done = true;
		l_runner.join();
		std::cout << "done (verdict: TSan reports)\n";
	}

	// remove() from another thread looks the agent up and marks it without synchronisation, then the turn boundary erases
	// it while a send_content() from that thread looks its receiver up in the same collection.
	void outside_remove_and_send_parallel() {
		cam::Environment l_env({.m_turns = 0, .m_mode = Mode::Parallel});
		const cam::AgentId l_sink = l_env.add<Sink>("sink");
		std::vector<cam::AgentId> l_plain;
		for (int i = 0; i < 100; i++) {
			l_plain.push_back(l_env.add<Plain>("plain"));
		}
		std::thread l_runner([&l_env] { l_env.start(); });
		while (!g_sink_ready) {
			std::this_thread::yield();
		}
		for (const auto& l_id: l_plain) {
			l_env.remove(l_id);
			l_env.send_content("outside", l_sink, cam::MessageContent(std::string("m")));
		}
		g_outside_done = true;
		l_runner.join();
		std::cout << "done (verdict: TSan reports)\n";
	}

	// ---------------------------------------------------------------- random generator per environment (fixed)
	void rng_two_threads() {
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
		std::cout << "OK no crash\n";
	}

	// ---------------------------------------------------------------- identifier returned by add (fixed)
	void dangling_add_ref() {
		cam::Environment l_env(3, Mode::Sequential);
		const cam::AgentId& l_id = l_env.add<Quitter>("q1");  // a copy now (it referred to Agent::m_id)
		l_env.add<Quitter>("q2");  // q2 no longer stays alive in the sentinel node of m_new_agents
		l_env.start();             // q1 and q2 stop and are removed: q1 is destroyed
		std::cout << "id still readable? size=" << l_id.size() << " value=" << l_id << "\n";
		std::cout << "OK no crash\n";
	}

	// ---------------------------------------------------------------- registry
	struct Probe {
		const char* m_name;
		const char* m_status;
		void (*m_run)();
	};

	const Probe g_probes[] = {
			{"empty_start_parallel", "fixed", empty_start_parallel},
			{"empty_start_sequential", "witness", empty_start_sequential},
			{"empty_start_sequential_random", "fixed", empty_start_sequential_random},
			{"continue_all_dead_parallel", "fixed", continue_all_dead_parallel},
			{"remove_unknown", "fixed", remove_unknown},
			{"remove_buffered", "fixed", remove_buffered},
			{"remove_then_send_by_name", "fixed", remove_then_send_by_name},
			{"random_agent_bound", "fixed", random_agent_bound},
			{"observables_parallel", "known (TSan)", observables_parallel},
			{"stop_and_send_parallel", "fixed (TSan)", stop_and_send_parallel},
			{"remove_during_parallel_turn", "fixed (TSan, ASan)", remove_during_parallel_turn},
			{"remove_during_sequential_turn", "fixed", remove_during_sequential_turn},
			{"outside_add_and_send_parallel", "known (TSan)", outside_add_and_send_parallel},
			{"outside_remove_and_send_parallel", "known (TSan)", outside_remove_and_send_parallel},
			{"rng_two_threads", "fixed (TSan)", rng_two_threads},
			{"dangling_add_ref", "fixed", dangling_add_ref},
	};

	void list_probes(std::ostream& p_out) {
		p_out << "usage: cam_probes <probe>\n";
		for (const auto& l_probe: g_probes) {
			p_out << "  " << std::left << std::setw(32) << l_probe.m_name << l_probe.m_status << "\n";
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
