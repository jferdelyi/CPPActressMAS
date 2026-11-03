# Unit tests

The pieces of the library alone, one doctest program per piece (label `unit`, tests `unit.<name>`). Agents never
assert: in Parallel mode an exception thrown by an agent is swallowed, so they record what happens
(`support/Recorder.hpp`) and the test checks after `start()`.

| Test | File | What it checks |
|---|---|---|
| `unit.smoke` | `smoke.cpp` | The public API in one environment: configuration and modes, delivery by id, name, fragment and broadcast, typed contents, shared texts, lookups (from the environment and from an agent), global data, messages built by hand, stopping and removal, empty environments, Parallel turns |
| `unit.transport` | `envelope_codec.cpp`, `endpoint.cpp`, `actressmas_codec.cpp` | The transport layer without an environment: envelope codecs (round trips, binary vectors, malformed input), `Endpoint` (sequences, deduplication, reordering, overflows, counters), the ActressMAS wire codec against bytes captured from the C# code |
| `unit.connect` | `connect.cpp` | `Environment::connect(url)`: the URL kept by the environment, the schemes compiled in, malformed URLs, a custom factory, the shared loopback of a namespace and its codec |
| `unit.mpsc_queue` | `mpsc_queue.cpp` | The queue of the agent messages: arrival order, several producers at once, nodes recycled per thread |
| `unit.recycling_allocator` | `recycling_allocator.cpp` | Blocks recycled per thread: reuse, release from another thread, threads that end, arrays |
| `unit.message_pointer` | `message_pointer.cpp` | The pointer to a message with its counter inside: copies, assignments, moves, kept messages, release across threads |
| `unit.agent_id` | `agent_id.cpp` | The identifier of an agent: size, capacity, literals, comparisons, hash, stream, JSON |
| `unit.identifier` | `identifier.cpp` | UUIDs and short agent identifiers, unique from several threads |
| `unit.worker` | `worker.cpp` | The worker of the network transports: the jobs posted before its start run in order, a stop drops the pending ones |
| `unit.removal` | `removal.cpp` | Removal of agents: deferred to the turn boundary during a turn, at once outside a turn |
| `unit.observables` | `observables.cpp` | What an agent sees of the others before each action, as the C#: its filter, never itself, nor empty observables, nor a stopped agent; the default filter |
| `unit.dotnet_random` | `dotnet_random.cpp` | The generator of the seeded `System.Random` of .NET, against the series of the C# |
| `unit.random_order` | `random_order.cpp` | The random order of the sequential modes: the turn orders of the C#, portable Mt19937, seeds |
| `unit.message_parse` | `message_parse.cpp` | The text of a message as the C# reads it: `Parse`, `Parse1P`, `Format` with names |
| `unit.agent_types` | `agent_types.cpp` | The catalogue of the agent types of the process: automatic names, `CAM_AGENT_TYPE`, conflicts, migration |

**Data** (`vectors/`):
- `envelope/<codec>/v1/`: the binary vectors of each envelope codec (`.bin`, with the envelope in `.json`).
  `CAM_WRITE_VECTORS=1` in the environment writes them again; a change of format is a new version folder, never a
  rewrite of `v1`.
- `rng/`: the series of the seeded `System.Random` of .NET and the turn orders of the C#, produced by
  `tools/csharp-ref` (`rng`, `trace`).

``` bash
ctest --preset debug-unit
tests/run.sh asan unit
```
