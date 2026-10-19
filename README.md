<p align="center"><img src="https://github.com/jferdelyi/CPPActressMAS/assets/4105962/d6b7bd65-27ba-45fa-8e35-317113c2e05b" width="256"></p>

# <p align="center">CPPActressMAS</p>

A C++ multi-agent framework based on the actor model (based on .NET ActressMas v3.0 https://github.com/florinleon/ActressMas). The aim of this project is to propose an easy-to-use multi-agent system in modern C++ based on actors such as ActressMAS.

**This project is currently under development and may contain bugs**.

The project works on 
 - macOS : Tested
 - Linux : Not tested
 - Windows : Not tested

Dependencies:
 - cmake 3.22 (may work with lower versions)
 - C++20
 - nlohmann/json 3.12.0, tessil/ordered-map 1.2.0 and, for the tests, doctest 2.5.3: git submodules under `ThirdParty/` ([ThirdParty/README.md](ThirdParty/README.md))
 - OpenSSL (development headers) only for the MQTT transport (`CAM_TRANSPORT_MQTT`); libmosquitto 2.0.22 (the Eclipse Mosquitto client library) is fetched and built by the project
 - zenoh-c and zenoh-cpp 1.7.1 only for the Zenoh transport (`CAM_TRANSPORT_ZENOH`), found with `find_package(zenohc)` and `find_package(zenohcxx)`

This project is developed with CLion (CMake). Compilation in pure CMake, the library, the examples and the tests are compiled through the presets of `CMakePresets.json` (`debug`, `release`, `core`, `asan`, `tsan`, `coverage`, `bench`; build directories under `build/<preset>`):
``` bash
git clone https://github.com/jferdelyi/CPPActressMAS.git
cd CPPActressMAS
git submodule update --init --depth 1
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

`git submodule update --init --depth 1` fetches only the pinned commit of each library (about 20 MB). `git clone --recursive` works too, but downloads every branch of nlohmann/json (about 180 MB). The configuration stops with that hint while the submodules are missing; once they are checked out, the build works offline.

Options: `BUILD_EXAMPLES` (ON in the presets), `CAM_BUILD_TESTS` (ON when the project is the top-level project), `BUILD_BENCH` (ON in the `bench` preset only), `BUILD_STATIC` and `BUILD_SHARED` (ON), `CAM_TRANSPORT_MQTT` and `CAM_TRANSPORT_ZENOH` (OFF by default; ON in the `debug`, `release`, `asan` and `tsan` presets, OFF in `core` and `bench`). The library (`CPPActressMAS`, `CPPActressMASStatic`) depends on a network library only when its transport option is ON: the transports compiled in join the library itself, and `Environment::connect(url)` selects them by scheme (the former `cam_transport` target remains as an alias).

## Network

An environment stays offline until it is connected to a transport. It owns its transport URL: the scheme selects the transport among those compiled in the library (`loopback://` always, `zenoh://` with `CAM_TRANSPORT_ZENOH`, `mqtt://` and `mqtts://` with `CAM_TRANSPORT_MQTT`). Without a URL, `connect()` refuses. Zenoh is the recommended network transport: peers talk to each other without a broker, one of them listens and the others connect to it (or to a `zenohd` router):
``` cpp
CAM_AGENT_TYPE(MyMobileAgent);  // after the class, at global scope: its wire name, for programs that only receive it

// first peer of the simulation: listens on 127.0.0.1:7447, connected at its first start()
cam::Environment l_environment({.m_url = "zenoh://?listen=tcp/127.0.0.1:7447&ns=my-simulation"});
l_environment.add<MyMobileAgent>("traveller");  // add<T>() also names the type automatically
l_environment.start();                           // throws cam::transport::TransportError if the URL does not connect
// other peers: {.m_url = "zenoh://127.0.0.1:7447/my-simulation"}
// MQTT broker instead: {.m_url = "mqtt://127.0.0.1:1883/my-simulation"}
// several environments of one process: {.m_url = "loopback://my-simulation"} for each
// to connect earlier and get a Result instead: if (const auto l_result = l_environment.connect(); !l_result) { ... }
```
Migration needs the wire name of the agent type in both programs: `add<T>()` gives a type of a named namespace its qualified C++ name, `CAM_AGENT_TYPE(T)` fixes it for a program that only receives `T`, and `EnvironmentConfig::m_accepted_agent_types` restricts what an environment accepts.
Zenoh options: `mode=peer|client`, `listen=tcp/host:port` (several, comma separated), `connect=tcp/host:port`, `scouting=multicast` (discovery by UDP multicast; off by default, so an environment only talks to the endpoints it was given). Presence relies on Zenoh liveliness: the death of a process takes its environments offline for everyone.

**Envelope codecs.** Every envelope on the wire starts with a 4-byte header (magic, codec id, version) and is encoded by the codec of the transport, chosen by the URL option `codec=` and the same for every environment of a namespace: `protobuf` (default) is the proto3 wire format of [`CPPActressMAS/transport/codec/envelope.proto`](CPPActressMAS/transport/codec/envelope.proto), written and read by the project itself without libprotobuf, so any proto3 implementation given that file can talk to it; `compact` is a fixed little-endian layout, the floor of the micro-benchmark; `json` is readable UTF-8 JSON (a text content travels in clear), for debugging and the simplest clients. The binary vectors of each codec are under `tests/vectors/envelope/<codec>/v1/`; the protobuf ones were checked against `protoc`.

**Message content.** A message carries text, a JSON value or opaque bytes (`Message::kind()`, `as_text()`, `as_json()`, `as_bytes()`; `content()` still returns a JSON view). Locally nothing is encoded: a JSON value is shared by the receivers of a broadcast. On the wire a text travels as UTF-8, bytes as they are, and a JSON value in the binary encoding of the send options (MessagePack by default, or CBOR, BSON, UBJSON, BJData, JSON text). `send(receiver, "hello")` sends a text; `send(receiver, json{{"k", 1}})` a JSON value; `send(receiver, bytes, size)` raw bytes.

The examples `Mobile` and `MobilePingPong` run on the in-process `loopback://` transport without argument (available in every build) and take the URLs of their environments in argument to go through the network.
Messages by id reach a remote agent through the directory of the connected environments; `send_by_name`, `send_by_fragment_name` and `broadcast` stay local unless the last parameter asks for the network scope (a `cam::transport::SendOptions` with `m_scope = cam::transport::Scope::Network`). Everything received from the network is applied at the turn boundary. An agent moves to another environment with `move(environment_id)`: its state (`save_state()`, `load_state()`), its observables and its pending messages travel with it, and it runs `setup()` again on arrival. Several environments of one process can share one transport (`cam::transport::open(url)`, then `environment.connect(transport)`): they then form one node with one connection. Statistics of the transport side: `environment.statistics()`; the other environments: `environment.environments()`.

### Adding a transport

A transport implements `cam::transport::Transport` (`CPPActressMAS/transport/Transport.h`): `open`, `close`, `attach`, `detach`, `publish`, `capabilities`, `config`, `node_id`, `codec`, `name`. The core asks little of it:
- `publish(sender, address, envelope)` is called at the turn boundary for every envelope leaving an environment; `EndpointRegistry::deliver_locally` serves the environments of the same transport directly, the rest is encoded with `codec()` and sent to the address (`env/<id>/in`, `env/<id>/agent`, `env/<id>/presence`, `all/in`, see `Address::to_key`).
- Whatever arrives is handed to the endpoint of the target environment with `Endpoint::deliver(address, bytes)` (or an already decoded `Envelope`), from any thread: the core applies it at its next turn boundary. Presences (`Presence` envelopes) make the other environments known; connection changes go through `Endpoint::push_event`.
- `capabilities()` says honestly what the transport guarantees; the core only needs `pubsub` or a way to reach every known environment.

Two ways to plug it in:
- **In your program**, without touching the library: give the environment a factory that knows your scheme and falls back on the built-in ones, or hand it the transport directly.
``` cpp
l_environment.set_transport_factory([](const std::string& p_url, cam::transport::TransportConfig p_config)
		-> cam::transport::Result<std::shared_ptr<cam::transport::Transport>> {
	if (p_url.rfind("mine://", 0) == 0) {
		auto l_transport = std::make_shared<MyTransport>(std::move(p_config));
		if (const auto l_result = l_transport->open(); !l_result) return l_result.error();
		return std::shared_ptr<cam::transport::Transport>(l_transport);
	}
	return cam::transport::open(p_url, std::move(p_config));   // loopback, mqtt, zenoh...
});
l_environment.connect("mine://somewhere");
// or: l_environment.connect(std::make_shared<MyTransport>(...));
```
- **In the library**: a subdirectory `CPPActressMAS/transport/<name>/` whose `CMakeLists.txt` sets `CAM_<NAME>_SOURCES`, `CAM_<NAME>_INCLUDES`, `CAM_<NAME>_LIBRARIES` and `CAM_<NAME>_DEFINITIONS` for its parent; an option `CAM_TRANSPORT_<NAME>` and a `cam_add_transport(<NAME> CAM_TRANSPORT_<NAME>)` call in the root `CMakeLists.txt`; the scheme in `schemes()`, `parse_url` and `open` of `CPPActressMAS/transport/Facade.cpp` behind `#if CAM_TRANSPORT_<NAME>`; a network test with its CTest label that exits with code 77 when its server is missing. The transports of the library (`mqtt/`, `zenoh/`) are complete examples.

## Tests

CTest labels: `unit` (API smoke tests, envelope codec and endpoint), `characterization` (known defects pinned as expected failures: crash probes run in a separate process under the `asan` or `tsan` preset and are checked against the sanitizer signature; value defects are doctest cases marked `should_fail`), `integration` (approval tests of the deterministic examples, normalised output; environments connected through the loopback transport; agent migration), `mqtt` (environments through a broker; skipped with exit code 77 unless `CAM_MQTT_HOST` and `CAM_MQTT_PORT` point to a reachable broker and the build has the MQTT transport; the reconnection test also needs `CAM_MQTT_BROKER_CONTROL`, a script accepting `stop` and `start`), `zenoh` (two Zenoh peers of the process on 127.0.0.1, no router; skipped with exit code 77 without the Zenoh transport), `perf` (smoke run of the benchmarks of `bench/`, including the end-to-end transport benchmark `bench_Transport --transport loopback|mqtt|zenoh`).
``` bash
ctest --preset debug -L unit
CAM_MQTT_HOST=127.0.0.1 CAM_MQTT_PORT=1883 ctest --preset debug -L mqtt
cmake --preset asan && cmake --build --preset asan && ctest --preset asan -L characterization
```

## Benchmarks

`bench/` holds the measurement copies of the C# E30 benchmarks (PingPong, Skynet, SkynetNumeric), the end-to-end transport benchmark (`bench_Transport`) and the envelope codec micro-benchmark (`bench_Envelope`): same workloads and measured scope as the C# programs, atomic counters, `steady_clock`, a deterministic variant (`--sequential`), linked to the static library and built without `-march=native` nor `-flto`:
``` bash
cmake --preset bench && cmake --build --preset bench
build/bench/bench/bench_Skynet
build/bench/bench/bench_PingPong --sequential --trials 3
```

## Exemples

Simples examples, general use
Observables examples, how to use observables
Benchmarks examples, to benchmark the library

## API
**Environment**
``` cpp
/**
 * Initializes a new instance of the Environment class.
 *
 * @param p_no_turns The maximum number of turns of the simulation. Setup is
 * considered to be the first turn. The simulation may stop earlier if there
 * are no more agents in the environment. If the number of turns is 0, the
 * simulation runs indefinitely, or until there are no more agents in the
 * environment.
 * @param p_mode Whether agent behaviors are executed in parallel,
 * sequentially or sequentially with random order. The code of a single agent
 * in a turn is always executed sequentially.
 * @param p_delay_after_turn A delay (in miliseconds) after each turn.
 * @param p_seed A random number generator seed for non-deterministic but
 * repeatable experiments.
 **/
explicit Environment(int p_no_turns = 0, const EnvironmentMasMode& p_mode = EnvironmentMasMode::Parallel, int p_delay_after_turn = 0, unsigned int p_seed = std::random_device()());

/**
 * Initializes a new instance of the Environment class from its configuration (D82), for instance
 * cam::Environment l_environment({.m_turns = 10, .m_mode = cam::EnvironmentMasMode::Sequential}).
 * EnvironmentConfig: m_turns, m_mode, m_delay_after_turn, m_seed (drawn from std::random_device when absent),
 * m_random (RandomEngine::Mt19937, the same numbers with every standard library, or RandomEngine::DotNet, the draws of
 * the seeded System.Random so that a port follows the random order of the C#). seed() returns the seed in use.
 * m_url and m_endpoint: transport URL and endpoint configuration; the environment connects through them at its first
 * start(), which throws cam::transport::TransportError if the connection fails (a later start() retries; never after an
 * explicit disconnect()). connect() stays available to connect earlier, for instance to synchronize several
 * environments started in their own threads.
 * @param p_config The configuration
 **/
explicit Environment(const EnvironmentConfig& p_config);

/**
 * Disconnects from the transport, if connected.
 **/
virtual ~Environment();

/**
 * Adds an agent to the environment.
 * @param p_args Argument to create a new agent
 * @return Id of the agent
 **/
template <ConceptAgent T>
AgentId add(auto&&... p_args);

/**
 * Continues the simulation for an additional number of turns, after an
 * initial simulation has finished. The simulation may stop earlier if there
 * are no more agents in the environment. If the number of turns is 0, the
 * simulation runs indefinitely, or until there are no more agents in the
 * environment.
 *
 * @param p_no_turns The maximum number of turns of the continued simulation
 **/
void continue_simulation(int p_no_turns = 0);

/**
 * Stops the execution of the agent identified by id and removes it from the
 * environment. Use the Remove method instead of Agent.Stop when the decision
 * to stop an agent does not belong to the agent itself, but to some other
 * agent or to an external factor.
 * During a turn the agent is no longer counted, found by name nor sent to, and it is destroyed at the turn
 * boundary (in Parallel mode it still runs in this turn if it has not yet, as the C#); outside a turn it is
 * removed at once. An unknown id, or an agent added but not yet inserted, is ignored.
 *
 * @param p_agent_id The id of the agent to be removed
 **/
void remove(const AgentId& p_agent_id);

/**
 * Sends a message from the outside of the multiagent system. Whenever
 * possible, the agents should use the Send method of their own class, not
 * the Send method of the environment. This method can also be used to
 * simulate a forwarding behavior.
 * @param p_sender_id The sender ID
 * @param p_receiver_id The receiver name
 * @param p_message The message to be sent
 * @param p_length The message length
 * @param p_binary_format The message binary format
 **/
void send(const AgentId& p_sender_id, const AgentId& p_receiver_id, const uint8_t* p_message, const size_t& p_length, const MessageBinaryFormat& p_binary_format = MessageBinaryFormat::RAW) const;

/**
 * Sends a message by name.
 * @param p_sender_id The sender ID
 * @param p_receiver_name The receiver name
 * @param p_message The message to be sent
 * @param p_length The message length
 * @param p_is_fragment If true search all agent that the name contain "p_receiver_name"
 * @param p_first_only If true send to the first agent found
 * @param p_binary_format The message binary format
 **/
void send_by_name(const AgentId& p_sender_id, const std::string& p_receiver_name, const uint8_t* p_message, const size_t& p_length, bool p_is_fragment, bool p_first_only, const MessageBinaryFormat& p_binary_format = MessageBinaryFormat::RAW) const;

/**
 * Send a new message to all agents.
 * @param p_sender_id From
 * @param p_message The message
 * @param p_length The message length
 * @param p_binary_format The message binary format
 **/
void broadcast(const AgentId& p_sender_id, const uint8_t* p_message, const size_t& p_length, const MessageBinaryFormat& p_binary_format = MessageBinaryFormat::RAW) const;

/**
 * Starts the simulation.
 **/
 void start();

/**
 * The number of agents in the environment, the stopped or removed ones excluded
 **/
[[nodiscard]] size_t agents_count() const;

/**
 * Get all agents by name (the stopped or removed ones excluded)
 * @param p_name the name of agent
 * @param p_first_only if true the first found
 * @return All agents by name
 **/
[[nodiscard]] std::vector<AgentId> get_agents_by_name(const std::string& p_name, bool p_first_only = false) const;

/**
 * Get first agent by name
 * @param p_name the name of agent
 * @return All agents by name
 **/
[[nodiscard]] std::optional<AgentId> get_first_agent_by_name(const std::string& p_name) const;

/**
 * Get agent name by id
 * @param p_id the name of agent
 * @return Agents name
 **/
[[nodiscard]] std::optional<std::string> get_agent_name(const AgentId& p_id) const;

/**
 * Returns the id of a randomly selected agent from the environment: one draw of the generator of this
 * environment over the agents in their order, as RandomAgent of the C# (the stopped or removed ones excluded).
 * Any agent may come out, the one that calls included: a caller that wants another kind of agent draws again,
 * as the manager of the C# example E04 does.
 * @return The id, or nothing when no agent is left (the C# throws)
 **/
[[nodiscard]] std::optional<AgentId> random_agent() const;

/**
 * Get all agents by fragment name (the stopped or removed ones excluded)
 * @param p_fragment_name the fragment name of agent
 * @param p_first_only if true the first found
 * @return All agents by fragment name
 **/
[[nodiscard]] std::vector<AgentId> get_filtered_agents(const std::string& p_fragment_name, bool p_first_only = false) const;

/**
 * A method that may be optionally overriden to perform additional
 * processing after the simulation has finished.
 **/
virtual void simulation_finished();

/**
 * A method that may be optionally overriden to perform additional
 * processing after a turn of the simulation has finished.
 * @param p_turn The turn that has just finished
 **/
virtual void turn_finished(int p_turn);

/**
 * Get the list of observable agents for an agent and its perception filter.
 * @param p_perceiving_agent Peceiving agent
 **/
std::vector<ObservablesPointer> get_list_of_observable_agents(const Agent *p_perceiving_agent) const;

// Delete copy constructor
EnvironmentMas(const EnvironmentMas& ) = delete;
EnvironmentMas& operator=(EnvironmentMas& ) = delete;
```
**Agent**
``` cpp
/**
 * Create a new agent.
 * @param p_name Name of the new agent
 **/
explicit Agent(std::string p_name);

/**
 * Nothing to delete.
 **/
virtual ~Agent() = default;

/**
 * Return id.
 * @return Id of the agent: 15 random base62 characters (environments and nodes use version 4 UUIDs)
 **/
[[nodiscard]] const AgentId& get_id() const;

/**
 * Return name.
 * @return Name of the agent
 **/
[[nodiscard]] const std::string& get_name() const;

/**
 * True if using observables.
 * @return True if using observables
 **/
[[nodiscard]] bool is_using_observables() const;

/**
 * Get observables.
 * @return Observables
 **/
[[nodiscard]] ObservablesPointer get_observables() const;

/**
 * True is must run setup.
 * @return True is must run setup
 **/
[[nodiscard]] bool is_setup() const;

/**
 * True if is dead.
 * @return True if is dead
 **/
[[nodiscard]] bool is_dead() const;

/**
 * Set environment.
 * @param p_environment The environment
 **/
void set_environment(EnvironmentMas *p_environment);

/**
 * Internal setup called by the environment.
 **/
void internal_setup();

/**
 * Internal see called by the environment.
 **/
void internal_see();

/**
 * Internal action called by the environment.
 **/
void internal_action();

/**
 * Stops the execution of the agent and removes it from the environment.
 * Use the Stop method instead of Environment.
 * Remove when the decision to be stopped belongs to the agent itself.
 * The agent is no longer counted, found by name nor sent to; it finishes its current turn and is
 * destroyed at the turn boundary. Stopping twice does nothing.
 **/
void stop();

/**
 * Receive a new message.
 * @param p_message The new message
 **/
void post(const MessagePointer& p_message);

/**
 * Send a new message by ID.
 * @param p_receiver_id The id of the receiver
 * @param p_message The message
 * @param p_length The message size
 **/
void send(const AgentId& p_receiver_id, const uint8_t* p_message = nullptr, const size_t& p_length = 0) const;
void send(const AgentId& p_receiver_id, const json& p_message) const;

/**
 * Send a new message by name.
 * @param p_receiver_name The name of the receiver
 * @param p_message The message
 * @param p_length The message size
 * @param p_first_only If true, only the first agent found
 **/
void send_by_name(const std::string& p_receiver_name, const uint8_t* p_message = nullptr, const size_t& p_length = 0, bool p_first_only = true) const;
void send_by_name(const std::string& p_receiver_name, const json& p_message, bool p_first_only = true) const;

/**
 * Send a new message by fragment name.
 * @param p_fragment_name The fragment of the receivers
 * @param p_message The message
 * @param p_length The message size
 * @param p_first_only If true, only the first agent found
 **/
void send_by_fragment_name(const std::string& p_fragment_name, const uint8_t* p_message = nullptr, const size_t& p_length = 0, bool p_first_only = true) const;
void send_by_fragment_name(const std::string& p_fragment_name, const json& p_message, bool p_first_only = true) const;

/**
 * Send a new message to all agents.
 * @param p_message The message
 * @param p_length The message size
 **/
void broadcast(const uint8_t* p_message = nullptr, const size_t& p_length = 0) const;
void broadcast(const json& p_message) const;

/**
 * Perception filter.
 * @param p_observed Observed properties
 * @return True if the agent is observable
 **/
[[nodiscard]] virtual bool perception_filter(const ObservablesPointer& p_observed) const;

/**
 * Setup the agent.
 **/
virtual void setup();

/**
 * Compute see.
 * @param p_observable_agents The list of observable agents
 **/
virtual void see(const std::vector<ObservablesPointer>& p_observable_agents);

/**
 * Compute action.
 * @param p_message The message to compute
 **/
virtual void action(const MessagePointer& p_message);

/**
 * Compute action if there is no message.
 **/
virtual void default_action();

// Delete copy constructor
/**
 * As Format() of the C# message (D82): "[sender -> receiver]: content" with the names of the agents of this
 * environment, the raw id otherwise ("env", an agent of another environment or already erased).
 **/
[[nodiscard]] std::string format(const MessagePointer& p_message) const;

Agent(const Agent& ) = delete;
Agent& operator=(Agent& ) = delete;
```
**Message**
``` cpp
/**
 * Message.
 * @param p_sender Sender.
 * @param p_receiver Receiver.
 * @param p_message Message.
 * @param p_binary_format Binary format used.
 **/
Message(std::string p_sender, std::string p_receiver, const json& p_message, const MessageBinaryFormat& p_binary_format = MessageBinaryFormat::MessagePack);
Message(std::string p_sender, std::string p_receiver, const uint8_t* p_message, const size_t& p_length, const MessageBinaryFormat& p_binary_format = MessageBinaryFormat::RAW);

/**
 * Nothing to delete.
 **/
/*virtual*/ ~Message() = default;

/**
 * Get sender.
 * @return Sender.
 **/
[[nodiscard]] const AgentId& get_sender() const;

/**
 * Get receiver.
 * @return Receiver.
 **/
[[nodiscard]] const AgentId& get_receiver() const;

/**
 * Get binary message.
 * @return binary message JSON
 **/
[[nodiscard]] const std::vector<std::uint8_t>& get_binary_message() const;

/**
 * Get binary message.
 * @return binary message JSON
 **/
[[nodiscard]] const MessageBinaryFormat& get_binary_format() const;

/**
 * Get message.
 * @return message JSON
 **/
[[nodiscard]] json content() const;

/**
 * As Parse, Parse(out string) and Parse1P of the C# (D82): the content split at every white space as String.Split()
 * (Unicode spaces, empty words kept); the first word is the action. parse_one throws std::out_of_range on a single word.
 **/
[[nodiscard]] std::pair<std::string, std::vector<std::string>> parse() const;
[[nodiscard]] std::pair<std::string, std::string> parse_rest() const;
[[nodiscard]] std::pair<std::string, std::string> parse_one() const;

/**
 * Format message to string.
 * @return string message from JSON
 **/
[[nodiscard]] std::string to_string() const;

/**
 * Format message.
 * @return string formated message
 **/
[[nodiscard]] /*virtual*/ std::string format() const;

// Delete copy constructor
Message(const Message& ) = delete;
Message& operator=(Message& ) = delete;
```

## Backlogs

Core backlog
- [x] Offline core (parallel, sequential, random sequential)
- [x] Observables
- [x] Performance check (compared to .NET ActressMAS)
- [x] Network system (using MQTT)
- [x] On macOS
- [ ] On Linux
- [ ] On Linux ARM
- [ ] On Windows
- [ ] On Android ?
- [ ] On iOS ?
- [x] Static library 
- [x] Dynamic library

Extented backlog
- [ ] Godot plugin (for graphical protocols and algorithms)

Multiagent protocols and algorithms implemented with CPPActressMAS
- [x] Simple examples of using the ActressMas framework
- [x] Benchmarks: PingPong, Skynet
- [x] Reactive architecture for agents
- [ ] Belief-Desire-Intention (BDI) architecture for agents
- [ ] Learning Real-Time A* (LRTA*) search algorithm
- [ ] Computing the Shapley value
- [ ] Auctions: English, Vickrey
- [ ] Yellow Pages to register and find services
- [ ] Zeuthen Strategy for negotiation
- [ ] Contract Net protocol
- [ ] Mechanism design: Grove-Clarke taxes
- [ ] Iterated Prisoner's Dilemma
- [ ] Predator-Prey Simulation
- [ ] Game of Life
- [ ] Voting methods: Copeland, Borda count
- [x] Mobile agents
