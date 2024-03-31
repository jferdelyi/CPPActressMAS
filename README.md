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

This project is developed with CLion (CMake). Compilation in pure CMake, the library and all tests will be compiled:
``` bash
git clone https://github.com/jferdelyi/CPPActressMAS.git
mkdir build
cmake . -B build
cmake --build build
```

## Exemples

Simples examples, general use
Observables examples, how to use observables
Benchmarks examples, to benchmark the library

## API
**Environment**
``` cpp
/**
 * Initializes a new instance of the EnvironmentMas class.
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
explicit EnvironmentMas(int p_no_turns = 0, const EnvironmentMasMode& p_mode = EnvironmentMasMode::Parallel, int p_delay_after_turn = 0, unsigned int p_seed = std::time(nullptr));

/**
 * Nothing to delete.
 **/
virtual ~EnvironmentMas() = default;

/**
 * Adds an agent to the environment.
 * @param p_args Argument to create a new agent
 * @return Id of the agent
 **/
template <ConceptAgent T>
const std::string& add(auto&&... p_args);

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
 *
 * @param p_agent_id The id of the agent to be removed
 **/
void remove(const std::string& p_agent_id);

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
void send(const std::string& p_sender_id, const std::string& p_receiver_id, const uint8_t* p_message, const size_t& p_length, const MessageBinaryFormat& p_binary_format = MessageBinaryFormat::RAW) const;

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
void send_by_name(const std::string& p_sender_id, const std::string& p_receiver_name, const uint8_t* p_message, const size_t& p_length, bool p_is_fragment, bool p_first_only, const MessageBinaryFormat& p_binary_format = MessageBinaryFormat::RAW) const;

/**
 * Send a new message to all agents.
 * @param p_sender_id From
 * @param p_message The message
 * @param p_length The message length
 * @param p_binary_format The message binary format
 **/
void broadcast(const std::string& p_sender_id, const uint8_t* p_message, const size_t& p_length, const MessageBinaryFormat& p_binary_format = MessageBinaryFormat::RAW) const;

/**
 * Starts the simulation.
 **/
 void start();

/**
 * The number of agents in the environment
 **/
[[nodiscard]] size_t agents_count() const;

/**
 * Get all agents by name
 * @param p_name the name of agent
 * @param p_first_only if true the first found
 * @return All agents by name
 **/
[[nodiscard]] std::vector<std::string> get_agents_by_name(const std::string& p_name, bool p_first_only = false) const;

/**
 * Get first agent by name
 * @param p_name the name of agent
 * @return All agents by name
 **/
[[nodiscard]] std::optional<std::string> get_first_agent_by_name(const std::string& p_name) const;

/**
 * Get agent name by id
 * @param p_id the name of agent
 * @return Agents name
 **/
[[nodiscard]] std::optional<std::string> get_agent_name(const std::string& p_id) const;

/**
 * Get all agents by fragment name
 * @param p_fragment_name the fragment name of agent
 * @param p_first_only if true the first found
 * @return All agents by fragment name
 **/
[[nodiscard]] std::vector<std::string> get_filtered_agents(const std::string& p_fragment_name, bool p_first_only = false) const;

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
std::vector<const ObservablesPointer> get_list_of_observable_agents(const Agent *p_perceiving_agent) const;

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
 * @return Id of the agent
 **/
[[nodiscard]] const std::string& get_id() const;

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
void send(const std::string& p_receiver_id, const uint8_t* p_message = nullptr, const size_t& p_length = 0) const;
void send(const std::string& p_receiver_id, const json& p_message) const;

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
virtual void see(const std::vector<const ObservablesPointer>& p_observable_agents);

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
[[nodiscard]] const std::string& get_sender() const;

/**
 * Get receiver.
 * @return Receiver.
 **/
[[nodiscard]] const std::string& get_receiver() const;

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
- [ ] Network system (using zeroMQ for exemple)
- [x] On macOS
- [ ] On Linux
- [ ] On Linux ARM
- [ ] On Windows
- [ ] On Android ?
- [ ] On iOS ?
- [x] Static library 
- [ ] Dynamic library

Extented backlog
- [ ] Godot plugin (for graphical protocols and algorithms)

Multiagent protocols and algorithms implemented with CPPActressMAS
- [x] Simple examples of using the ActressMas framework
- [x] Benchmarks: PingPong, Skynet
- [ ] Reactive architecture for agents
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
- [ ] Mobile agents
