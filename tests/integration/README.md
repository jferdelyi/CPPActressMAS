# Integration tests

Several environments of one process, connected through the loopback transport, in memory: no network, no broker,
deterministic (one turn per `start()`). Label `integration`, tests `integration.<name>`.

| Test | File | What it checks |
|---|---|---|
| `integration.loopback` | `loopback.cpp` | Environments connected through the loopback transport, in memory and serializing with every codec: messages by id, name, fragment and broadcast across environments, scopes, the directory of the agents, unknown destinations, refused identifiers, duplicates and losses injected by the transport, three environments on one transport, `can_move` |
| `integration.migration` | `migration.cpp` | The migration of an agent between environments: round trip with its id, state and observables, refusals, lost or late acceptance, lost confirmation, messages that follow the agent, texts carried by the offer, Parallel mode, disconnection during a move, the observables an agent brings seen at its arrival, the setup in the same turn as the first action at the destination |
| `integration.transparent_loopback` | `transparent_loopback.cpp` | Environments of one process without configuration: started, they reach each other through the loopback of the process (unicast, network scope, migration, in every mode), hidden from `is_connected()` and `statistics()`; alone there, offline answers; a namespace of its own (`loopback://name`) isolates an environment; an explicit connection or transport replaces the implicit loopback; `disconnect()` leaves it for good; two environments running at the same time |

``` bash
ctest --preset debug-integration
```
