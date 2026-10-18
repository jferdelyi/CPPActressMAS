# CPPActressMAS — examples

C++ ports of the examples shipped with ActressMAS v3.1 (C#), built with `-DBUILD_EXAMPLES=ON` (one executable each,
in `build/<preset>/Examples/`). Each folder has its own README with the C# example it ports and how far the port goes.

| Folder | Examples | Topic |
|---|---|---|
| [`Simples/`](Simples/README.md) | `Exemple1` to `Exemple5`, `MultipleMessages`, `SendingObjects` | First steps: setup, actions, messages by name and id, stopping, objects in messages |
| [`Observables/`](Observables/README.md) | `ColorGame`, `NumberGame` | Agents that see the observable properties of the others |
| [`MAS/`](MAS/README.md) | `Reactive` | Reactive explorers bringing samples back to a base |
| [`Mobiles/`](Mobiles/README.md) | `Mobile`, `MobilePingPong` | Several environments: agent migration and remote messages |
| [`Serialization/`](Serialization/README.md) | `Serialization` | Saving and restoring the state of an agent |
| [`Benchmarks/`](Benchmarks/README.md) | `PingPong`, `Skynet` | The two original C++ benchmarks |

The examples run offline. `Mobile` and `MobilePingPong` connect their environments to `loopback://` by default
(in memory, same process); a Zenoh or MQTT URL can be given instead, see their sources. The inventory of the
C# examples and of what remains to port is kept outside the code, in the porting notes.
