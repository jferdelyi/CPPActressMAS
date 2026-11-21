<p align="center"><img src="https://github.com/jferdelyi/CPPActressMAS/assets/4105962/d6b7bd65-27ba-45fa-8e35-317113c2e05b" width="256"></p>

# <p align="center">CPPActressMAS</p>

<p align="center"><a href="https://github.com/jferdelyi/CPPActressMAS/actions/workflows/tests.yml"><img src="https://github.com/jferdelyi/CPPActressMAS/actions/workflows/tests.yml/badge.svg" alt="tests"></a></p>

A C++20 multi-agent framework based on the actor model, a port of [ActressMAS](https://github.com/florinleon/ActressMas) v3.1 (.NET). The aim of this project is to propose an easy-to-use multi-agent system in modern C++, with the behaviour of ActressMAS, its examples, and a network that is simple to set up.

**This project is currently under development and may contain bugs**.

## Platforms

- **macOS** (Apple Silicon, Apple clang): built and tested at every change.
- **Linux arm64** (Ubuntu 24.04 in a container, gcc 13 and clang 18): built and tested at every change, under AddressSanitizer and ThreadSanitizer too; also built and tested by the continuous integration on an arm64 runner (not tried on a Raspberry Pi itself).
- **Linux x86-64** (gcc and clang) and **Windows** (MSVC, static library, unit and oracle tests): built and tested by the continuous integration (`.github/workflows/tests.yml`).
- **iOS** (arm64, iOS 16.3 or later) and **Android** (arm64-v8a, API 24 or later): the library is built by the continuous integration, core only, without network transport; nothing runs on them yet (`ios` and `android` presets).

## Quick start

``` bash
git clone https://github.com/jferdelyi/CPPActressMAS.git
cd CPPActressMAS
git submodule update --init --depth 1
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

`git submodule update --init --depth 1` fetches only the pinned commit of each library (about 20 MB). `git clone --recursive` works too, but downloads every branch of nlohmann/json (about 180 MB). The configuration stops with that hint while the submodules are missing; once they are checked out, the build works offline.

Two agents that exchange three pings and pongs, in a sequential environment of at most ten turns:
``` cpp
#include <iostream>
#include <string>

#include <Environment.h>

// Answers every message with "pong", to its sender
class Pong final : public cam::Agent {
public:
	explicit Pong(const std::string& p_name) : Agent(p_name) {}

	void action(const cam::MessagePointer& p_message) override {
		std::cout << format(p_message) << '\n';
		send(p_message->get_sender(), "pong");
	}
};

// Sends "ping" to the agent named "pong" at its setup, then after each answer; stops after the third one
class Ping final : public cam::Agent {
private:
	int m_answers = 0;

public:
	explicit Ping(const std::string& p_name) : Agent(p_name) {}

	void setup() override {
		send_by_name("pong", "ping");
	}

	void action(const cam::MessagePointer& p_message) override {
		std::cout << format(p_message) << '\n';
		if (++m_answers == 3) {
			stop();
		} else {
			send(p_message->get_sender(), "ping");
		}
	}
};

int main() {
	cam::Environment l_environment({.m_turns = 10, .m_mode = cam::EnvironmentMasMode::Sequential});
	l_environment.add<Ping>("ping");
	l_environment.add<Pong>("pong");
	l_environment.start();
	return 0;
}
```
It prints `[ping -> pong]: ping` and `[pong -> ping]: pong` three times each.

## Dependencies and licences

| Dependency | Version | How it is obtained | Used for | Licence |
|---|---|---|---|---|
| [nlohmann/json](https://github.com/nlohmann/json) | 3.12.0 | git submodule (`ThirdParty/json`) | the library: JSON contents, agent state, JSON envelope codec | MIT |
| [Tessil/ordered-map](https://github.com/Tessil/ordered-map) | 1.2.0 | git submodule (`ThirdParty/ordered-map`) | the library: agents in their order of insertion | MIT |
| [doctest](https://github.com/doctest/doctest) | 2.5.3 | git submodule (`ThirdParty/doctest`) | the tests only | MIT |
| libmosquitto ([Eclipse Mosquitto](https://github.com/eclipse-mosquitto/mosquitto) client library) | 2.0.22 | fetched and built by CMake from a pinned archive (SHA-256), with `CAM_TRANSPORT_MQTT` | the MQTT transport | EPL-2.0 or EDL-1.0 |
| OpenSSL | 3 | the system (development headers), with `CAM_TRANSPORT_MQTT` | TLS of the MQTT transport (`mqtts://`) | Apache-2.0 |
| [zenoh-c](https://github.com/eclipse-zenoh/zenoh-c) and [zenoh-cpp](https://github.com/eclipse-zenoh/zenoh-cpp) | 1.7.1 exactly | the system, `find_package(zenohc)` and `find_package(zenohcxx)`, with `CAM_TRANSPORT_ZENOH` | the Zenoh transport | EPL-2.0 or Apache-2.0 |

The core needs CMake 3.22 or later (3.25 for the presets) and a C++20 compiler, nothing else. The tests also use tools that are never linked: a Mosquitto 2 broker for the MQTT tests, and the .NET SDK, in containers, to produce the outputs of the C# originals the oracle tests compare with (`tests/tools/csharp-ref/`).

**Build options**: `BUILD_EXAMPLES` (ON in the presets), `CAM_BUILD_TESTS` (ON when the project is the top-level project), `BUILD_BENCH` (ON in the `bench` preset only), `BUILD_STATIC` and `BUILD_SHARED` (ON), `CAM_TRANSPORT_MQTT` and `CAM_TRANSPORT_ZENOH` (OFF by default; ON in the `debug`, `release`, `asan` and `tsan` presets, OFF in `core` and `bench`). The presets of `CMakePresets.json` are `debug`, `release`, `core`, `asan`, `tsan`, `coverage` and `bench`, built in `build/<preset>`. Every target of the project is built with `-Wall -Wextra -Wpedantic` under GCC and Clang. The library (`CPPActressMAS`, `CPPActressMASStatic`) depends on a network library only when its transport option is ON: the transports compiled in join the library itself, and `Environment::connect()` selects them by the protocol of the network of the environment (the former `cam_transport` target remains as an alias).

## Using the library

The full documentation of each class is in its header: [`Environment.h`](CPPActressMAS/Environment.h), [`Agent.h`](CPPActressMAS/Agent.h), [`Message.h`](CPPActressMAS/Message.h).

**The environment** (`cam::Environment`) runs the agents turn by turn. It is built from a configuration written with designated initializers: `m_turns` (0 for no limit), `m_mode`, `m_delay_after_turn` (ms), `m_seed`, `m_random`, `m_run_setup_separately` (`false` runs the setup of an agent in the same turn as its first see and action), and, for the network, `m_network` and `m_endpoint`. The modes are those of ActressMAS: `Parallel` (the agents of a turn run on several threads), `Sequential` (in their order of insertion) and `SequentialRandom` (in an order drawn at each turn). `start()` runs the simulation until the last turn or until no agent is left; `continue_simulation(n)` runs it further. An exception thrown by an agent leaves `start()` as in ActressMAS: at once in the sequential modes, and in `Parallel` mode once every agent of the turn has run, together with those of the others, in a `cam::AggregateException`. An environment of your own may override `turn_finished(turn)` and `simulation_finished()`.

| Environment | |
|---|---|
| `add<T>(args...)` | creates an agent of type `T` with these arguments, returns its id (`cam::AgentId`); it is reached at once by every send and search and runs from the next turn |
| `remove(id)` | removes an agent (at the next turn boundary while a simulation runs) |
| `agents_count()`, `all_agents()`, `get_agents_by_name(name)`, `get_first_agent_by_name(name)`, `get_agent_name(id)`, `random_agent()` | finds the agents; the first agent of a name is the one a send by name reaches |
| `set_global_data(key, value)`, `get_global_data()`, `get_global_data(key)` | data shared with the agents, from any thread |
| `send(...)`, `send_by_name(...)`, `broadcast(...)` | sends from outside the agents |

**An agent** (`cam::Agent`) is a class of your own that overrides some of its phases: `setup()` at its first turn, then at each turn `see(observables)` when it uses observables, `action(message)` for each message received, or `default_action()` when none came. It sends with `send(id, content)`, `send_to_many(ids, content)`, `send_by_name(name, content)`, `send_by_fragment_name(fragment, content)` and `broadcast(content)` (`{.m_include_sender = true}` in its options to receive its own copy), lists the agents with `all_agents()`, checks a destination with `can_move(environment id)` before `move()`, and leaves the simulation with `stop()`. `get_id()`, `get_name()` and `get_environment()` give its identity and its environment.

**A message** (`cam::Message`, received as a `cam::MessagePointer`) carries a text, a JSON value or bytes: `kind()`, `as_text()`, `as_json()`, `as_bytes()` (`content()` gives a JSON view of any of them). `get_sender()` and `get_sender_name()` give its sender; `parse()`, `parse_rest()` and `parse_one()` split a text content into an action and its parameters, as the `Parse` and `Parse1P` of ActressMAS do.

**Observables**: an agent created with `Agent(name, true)` publishes its observable properties in `(*m_observables)["key"] = value` and receives, in `see()`, those of the other agents that its `perception_filter()` keeps. The others see a copy published at the end of each of its turns: in a sequential turn, what the agents that ran before wrote, as in ActressMAS; in a `Parallel` turn, never a table being written.

**Randomness**: the random order and `random_agent()` draw from the seed of the environment (`seed()`), with `RandomEngine::Mt19937` (the same numbers with every standard library) or `RandomEngine::DotNet` (the numbers of the seeded `System.Random` of .NET, so that a port follows the order of its C# original).

## Network

An environment opens no network until it is given one. Without any configuration, the environments of one process reach each other through the in-process loopback once started (namespace `default`; `is_connected()` stays false); alone there, an environment answers as offline. Its network is a `cam::NetworkConfig` ([`NetworkConfig.h`](CPPActressMAS/NetworkConfig.h)), in `EnvironmentConfig::m_network` or through `set_network()`: a protocol (`None` by default, `Loopback`, `Zenoh`, `Mqtt`, `Mqtts`, `ActressMas`), the host (`127.0.0.1` by default) and the port (0: that of the protocol, 7447 for Zenoh, 1883 for MQTT, 8883 for MQTTS, 5000 for ActressMAS) of the meeting point, the namespace (`default`), the codec, and the options of a protocol (MQTT version, credentials, TLS files, ActressMAS container). The namespace serves the loopback and the transport alike: environments of another namespace never meet, and an invalid one (`[a-z0-9-]{1,32}`) is refused at construction (`std::invalid_argument`). With a protocol, the environment connects at its first start(), or earlier by `connect()`; without one, `connect()` refuses. A protocol is available when its transport is compiled in the library (`Loopback` always, `Zenoh` with `CAM_TRANSPORT_ZENOH`, `Mqtt` and `Mqtts` with `CAM_TRANSPORT_MQTT`, `ActressMas` with `CAM_TRANSPORT_ACTRESSMAS`). With Zenoh, MQTT or MQTTS, the environments of one process in the same namespace still reach each other through the loopback, and the network serves the other processes; a transport given by hand (`connect(transport)`) or by a factory (`set_transport_factory`) is used alone, which keeps two environments of one process on the network (tests, benchmarks). Zenoh is the recommended network transport: peers talk to each other without a broker; the first one listens on the meeting point and the others, which cannot listen there, connect to it:
``` cpp
CAM_AGENT_TYPE(MyMobileAgent);  // after the class, at global scope: its wire name, for programs that only receive it

// every peer of the simulation, in every program: the first one listens on 127.0.0.1:7447, the others connect to it
cam::Environment l_environment({.m_network = {.m_protocol = cam::Protocol::Zenoh, .m_namespace = "my-simulation"}});
l_environment.add<MyMobileAgent>("traveller");  // add<T>() also names the type automatically
l_environment.start();                           // connects first; throws cam::transport::TransportError if it cannot
// on several machines: .m_host = the address of the machine of the first peer
// MQTT broker instead: {.m_protocol = cam::Protocol::Mqtt, .m_host = "broker.local", .m_namespace = "my-simulation"}
// several environments of one process: nothing to configure; {.m_namespace = "my-simulation"} isolates them
// to connect earlier and get a Result instead: if (const auto l_result = l_environment.connect(); !l_result) { ... }
```
Migration needs the wire name of the agent type in both programs: `add<T>()` gives a type of a named namespace its qualified C++ name, `CAM_AGENT_TYPE(T)` fixes it for a program that only receives `T`, and `EnvironmentConfig::m_accepted_agent_types` restricts what an environment accepts.
Zenoh: no multicast scouting, so an environment only talks to its meeting point; presence relies on Zenoh liveliness, and the death of a process takes its environments offline for everyone. A transport opened by hand with `cam::transport::open(url)` takes the options of its URL (`zenoh://[host:port][/namespace][?options]`: `mode=peer|client`, `listen=tcp/host:port`, several comma separated, `connect=tcp/host:port`, `scouting=multicast`; see [`Facade.h`](CPPActressMAS/transport/Facade.h)), then `environment.connect(transport)`.

**Envelope codecs.** Every envelope on the wire starts with a 4-byte header (magic, codec id, version) and is encoded by the codec of the transport, chosen by `NetworkConfig::m_codec` (`codec=` in a URL of `transport::open`) and the same for every environment of a namespace: `protobuf` (default) is the proto3 wire format of [`CPPActressMAS/transport/codec/envelope.proto`](CPPActressMAS/transport/codec/envelope.proto), written and read by the project itself without libprotobuf, so any proto3 implementation given that file can talk to it; `compact` is a fixed little-endian layout; `json` is readable UTF-8 JSON (a text content travels in clear), for debugging and the simplest clients. The binary vectors of each codec are under `tests/unit/vectors/envelope/<codec>/v1/`; the protobuf ones were checked against `protoc`.

**Message content.** Locally nothing is encoded: a JSON value is shared by the receivers of a broadcast. On the wire a text travels as UTF-8, bytes as they are, and a JSON value in the binary encoding of the send options (`cam::ContentEncoding`: MessagePack by default, or CBOR, BSON, UBJSON, BJData, JSON text). `send(receiver, "hello")` sends a text; `send(receiver, json{{"k", 1}})` a JSON value; `send(receiver, bytes, size)` raw bytes.

**ActressMAS.** `{.m_protocol = cam::Protocol::ActressMas, .m_host = ..., .m_container = "<container>"}` (port 5000 by default) connects an environment to the `ContainerServer` of ActressMAS v3.1, the server of the C#, as a container of that name: C++ and C# agents talk to each other. It is the basic protocol of the C#, with its qualities and its defects: one environment per container, the list of the server as presence (every 3 s with the C# server; `connect()` waits for the first one), no acknowledgement, no order between two messages of a container (the C# server relays each one on a thread of its pool), no reconnection. An agent of another container is `name@container`: `send_by_name("pong@Container2", ...)` with the network scope goes to that container, a name without `@` goes to every other container (one without that agent drops it, as in C#), a reply by id goes back to its sender, and a message from another container is signed `name@container`, as in C#. Texts travel as they are, a JSON value as its text, bytes as `base64:` and their base64. Migration, broadcast and fragments of names are not part of the protocol: they are refused (`NotSupported`) and counted.

The examples `Mobile` and `MobilePingPong` run on the loopback of the process without argument (available in every build) and take a network in argument, `[protocol [host [port]]]` (for example `Mobile zenoh 127.0.0.1 17447`); between the environments of one program, the traffic stays on the loopback.
Messages by id reach a remote agent through the directory of the connected environments; `send_by_name`, `send_by_fragment_name` and `broadcast` stay local unless the last parameter asks for the network scope (a `cam::transport::SendOptions` with `m_scope = cam::transport::Scope::Network`). Everything received from the network is applied at the turn boundary. An agent moves to another environment with `move(environment_id)`: its state (`save_state()`, `load_state()`), its observables and its pending messages travel with it, and it runs `setup()` again on arrival. Several environments of one process can share one transport (`cam::transport::open(url)`, then `environment.connect(transport)`): they then form one node with one connection. Statistics of the transport side: `environment.statistics()`; the other environments: `environment.environments()`.

### Adding a transport

A transport implements `cam::transport::Transport` (`CPPActressMAS/transport/Transport.h`): `open`, `close`, `attach`, `detach`, `publish`, `capabilities`, `config`, `node_id`, `codec`, `name`. The core asks little of it:
- `publish(sender, address, envelope)` is called at the turn boundary for every envelope leaving an environment; `EndpointRegistry::deliver_locally` serves the environments of the same transport directly, the rest is encoded with `codec()` and sent to the address (`env/<id>/in`, `env/<id>/agent`, `env/<id>/presence`, `all/in`, see `Address::to_key`).
- Whatever arrives is handed to the endpoint of the target environment with `Endpoint::deliver(address, bytes)` (or an already decoded `Envelope`), from any thread: the core applies it at its next turn boundary. Presences (`Presence` envelopes) make the other environments known; connection changes go through `Endpoint::push_event`.
- `capabilities()` says honestly what the transport guarantees; the core only needs `pubsub` or a way to reach every known environment.

Two ways to plug it in:
- **In your program**, without touching the library: hand the environment the transport directly, in the namespace of its network; or give it a factory, which receives the URL the environment builds from its network and may replace the transport of a protocol, falling back on the built-in ones for the others.
``` cpp
auto l_transport = std::make_shared<MyTransport>(cam::transport::TransportConfig{.m_namespace = "default"});
if (l_transport->open()) {
	l_environment.connect(l_transport);
}
// or: l_environment.set_transport_factory([](const std::string& p_url, cam::transport::TransportConfig p_config)
//		-> cam::transport::Result<std::shared_ptr<cam::transport::Transport>> {
//	if (p_url.rfind("mqtt://", 0) == 0) { ... return my MQTT transport ... }
//	return cam::transport::open(p_url, std::move(p_config));   // loopback, zenoh...
// });
```
- **In the library**: a subdirectory `CPPActressMAS/transport/<name>/` whose `CMakeLists.txt` sets `CAM_<NAME>_SOURCES`, `CAM_<NAME>_INCLUDES`, `CAM_<NAME>_LIBRARIES` and `CAM_<NAME>_DEFINITIONS` for its parent; an option `CAM_TRANSPORT_<NAME>` and a `cam_add_transport(<NAME> CAM_TRANSPORT_<NAME>)` call in the root `CMakeLists.txt`; the scheme in `schemes()`, `parse_url` and `open` of `CPPActressMAS/transport/Facade.cpp` behind `#if CAM_TRANSPORT_<NAME>`; its value of `cam::Protocol` (`NetworkConfig.h`) and its URL in `Environment::connect_network()`; a network test with its CTest label that exits with code 77 when its server is missing. The transports of the library (`mqtt/`, `zenoh/`) are complete examples.

## Tests

One folder of `tests/` per type of test, one CTest label per type: `unit`, `integration`, `network` (plus `mqtt` or `zenoh`), `characterization`, `iso` (the C# examples ported as they are, compared with the output of the C# originals), `examples` and `perf`. How to run them, by hand and on each platform: [tests/README.md](tests/README.md).
``` bash
cmake --workflow --preset debug        # every test
cmake --workflow --preset iso          # one type
tests/run.sh asan unit iso             # types under a sanitizer preset
```

## Examples

`Examples/` holds C++ versions of the examples of ActressMAS v3.1, built with `-DBUILD_EXAMPLES=ON` ([Examples/README.md](Examples/README.md)): first steps, observables, reactive agents, mobile agents, BDI, the game of life, mechanism design, the prisoner's dilemma, serialization, benchmarks. They run offline; `Mobile` and `MobilePingPong` also go through the network.

Every example project of ActressMAS v3.1 that runs agents (33 of them; the two servers of its remote example have no environment) is also ported line by line, its bugs included, in `tests/iso/ports/`: the `iso` tests run the ports and compare their output with the output of the C# originals ([tests/iso/README.md](tests/iso/README.md)).

## Benchmarks and performance

`tests/bench/` holds the measurement copies of the C# E30 benchmarks (PingPong, Skynet, SkynetNumeric), two models of Antelmi et al. (Wolf, Sheep and Grass; Flockers), the end-to-end transport benchmark and the envelope codec micro-benchmark ([tests/bench/README.md](tests/bench/README.md)):
``` bash
cmake --workflow --preset bench
build/bench/tests/bench/bench_PingPong --sequential --trials 3
```

Against ActressMAS v3.1 measured on the same machine, the C++ is faster on every benchmark: ×4.8 on Skynet and SkynetNumeric, ×2.3 on PingPong, from ×4.1 (1 000 agents) to ×1.6 (128 000 agents) on Wolf, Sheep and Grass. Method, conditions and every number: [docs/PERFORMANCE.md](docs/PERFORMANCE.md).

## License

CPPActressMAS is distributed under the Eclipse Public License 2.0 ([LICENSE](LICENSE)).

It is a C++ port of [ActressMAS](https://github.com/florinleon/ActressMas) by Florin Leon, distributed under the Apache License 2.0: the framework follows its behaviour, and the ports of `tests/iso/ports/` follow the code of its examples line by line.

Its licence is in [LICENSE-ActressMAS](LICENSE-ActressMAS).

The lock-free queue of `CPPActressMAS/detail/MPSCQueue.hpp` comes from [CppBenchmark](https://github.com/chronoxor/CppBenchmark) by Ivan Shynkarenka, where this file is in the public domain (Unlicense).
