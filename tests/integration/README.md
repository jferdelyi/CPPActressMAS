# Integration tests

Several environments of one process, connected through the loopback transport, in memory: no network, no broker,
deterministic (one turn per `start()`). Label `integration`, tests `integration.<name>`.

| Test | File | What it checks |
|---|---|---|
| `integration.loopback` | `loopback.cpp` | Environments connected through the loopback transport, in memory and serializing with every codec: messages by id, name, fragment and broadcast across environments, scopes, the directory of the agents, unknown destinations, refused identifiers, duplicates and losses injected by the transport, three environments on one transport, `can_move` |
| `integration.migration` | `migration.cpp` | The migration of an agent between environments: round trip with its id, state and observables, refusals, lost or late acceptance, lost confirmation, messages that follow the agent, texts carried by the offer, Parallel mode, disconnection during a move, the observables an agent brings seen at its arrival, the setup in the same turn as the first action at the destination |

``` bash
ctest --preset debug-integration
```
