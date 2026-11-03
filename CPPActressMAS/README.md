# CPPActressMAS: core

The multi-agent core: environments, agents, messages and the turn loop, ported from ActressMAS (C#). This folder has
no network dependency; the communication between environments lives in [`transport/`](transport/README.md) and the
helpers that are not objects of the library live in [`util/`](util/README.md).

| File | Role |
|---|---|
| `Environment.{h,cpp}` | The environment: adds and removes agents, runs the turns (`start`), delivers messages locally (by id, by name or fragment, broadcast), connects to a transport from a URL (`connect`) and handles agent migration. |
| `AgentCollection.{h,cpp}` | The agents of one environment, in insertion order, with the name index and the agents added during a turn; runs one turn in the chosen mode (`Sequential`, `SequentialRandom`, `Parallel`) and removes the dead agents at the turn boundary: during a turn its collections never change, a stopped or removed agent is only marked, no longer counted. |
| `Agent.{h,cpp}` | Base class of the agents: `setup`, `see`, `action`, `default_action`; `send`, `send_by_name`, `send_by_fragment_name`, `broadcast`, `move`; the mailbox of incoming messages. |
| `AgentTypes.{h,cpp}` | Catalogue of the agent types of the process, for migration: the wire name of each type and a factory that builds an arriving agent. `add<T>()` names a type automatically (its qualified C++ name), `CAM_AGENT_TYPE(T)` names it for a program that only receives it, `Environment::register_agent_type<T>(name)` gives an explicit name; each environment may restrict the accepted types (`EnvironmentConfig::m_accepted_agent_types`). |
| `AgentId.{h,cpp}` | Identifier of an agent: at most 15 characters in 16 bytes, trivially copyable, hashed for the standard containers, written and read as a string (stream, JSON). |
| `Message.{h,cpp}` | A message: sender, receiver and a typed content (text, JSON value or bytes), read with `kind`, `as_text`, `as_json`, `as_bytes`; `MessagePointer`, the pointer that shares it, its counter inside the message; `Text`, an optional immutable text shared by the messages that carry it (read as text). |

## Notes
- **Identifiers**: an agent is named by a `cam::AgentId` (`AgentId.{h,cpp}`): at most 15 characters held in 16 bytes, copied, compared and hashed as two machine words; generated ones have 15 random base62 characters (about 89 bits), and a string literal such as `"ext"` converts when compiling. Environments, nodes and transfers use version 4 UUIDs (`util/Identifier.h`).
- **Turns**: a message posted during a turn may be handled in the same turn if its receiver has not run yet (as in ActressMAS). The `Parallel` mode runs every agent of a turn on the persistent threads of `util/TurnPool.h`; the other modes use the calling thread only.
- **Offline by default**: an environment talks to no other until `connect(url)` is called (`loopback://` in the same process, `zenoh://` or `mqtt://` across processes).
