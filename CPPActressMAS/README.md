# CPPActressMAS — core

The multi-agent core: environments, agents, messages and the turn loop, ported from ActressMAS (C#). This folder has
no network dependency; the communication between environments lives in [`transport/`](transport/README.md) and the
helpers that are not objects of the library live in [`util/`](util/README.md).

| File | Role |
|---|---|
| `Environment.{h,cpp}` | The environment: adds and removes agents, runs the turns (`start`), delivers messages locally (by id, by name or fragment, broadcast), connects to a transport from a URL (`connect`) and handles agent migration. |
| `AgentCollection.{h,cpp}` | The agents of one environment, in insertion order, with the name index and the agents added during a turn; runs one turn in the chosen mode (`Sequential`, `SequentialRandom`, `Parallel`) and removes the dead agents at the turn boundary. |
| `Agent.{h,cpp}` | Base class of the agents: `setup`, `see`, `action`, `default_action`; `send`, `send_by_name`, `send_by_fragment_name`, `broadcast`, `move`; the mailbox of incoming messages. |
| `Message.{h,cpp}` | A message: sender, receiver and a typed content (text, JSON value or bytes), read with `kind`, `as_text`, `as_json`, `as_bytes`. |

## Notes
- **Identifiers**: an agent has 15 random base62 characters, which copy without allocation; environments, nodes and transfers use version 4 UUIDs (`util/Identifier.h`).
- **Turns**: a message posted during a turn may be handled in the same turn if its receiver has not run yet (as in ActressMAS). The `Parallel` mode runs every agent of a turn on the persistent threads of `util/TurnPool.h`; the other modes use the calling thread only.
- **Offline by default**: an environment talks to no other until `connect(url)` is called (`loopback://` in the same process, `zenoh://` or `mqtt://` across processes).
