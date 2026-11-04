# CPPActressMAS: transport layer

How environments talk to each other: an environment connected to a URL owns an `Endpoint` attached to a `Transport`.
The layer is part of the core library; the network transports join it behind their CMake options
(`CAM_TRANSPORT_MQTT`, `CAM_TRANSPORT_ZENOH`), with their own dependencies.

| File | Role |
|---|---|
| `Transport.h` | The `Transport` interface (`open`, `close`, `attach`, `detach`, `publish`, `capabilities`, `codec`) and the `EndpointRegistry` shared by every transport, which delivers in memory what stays in the process. |
| `Endpoint.{h,cpp}` | One environment on a transport: outbox flushed at the turn boundary, inbox, deduplication and reordering by sequence, directory of the agents of the other environments, presences, counters. |
| `Envelope.h` | What travels: the `Envelope` and its payloads (`AgentTransfer`, `AgentControl`, `Presence`, `DirectoryEvent`). |
| `Types.h` | Addresses and keys, send options, capabilities, configurations, statistics, events, `Error` and `Result`. |
| `Facade.{h,cpp}` | `open(url)`, `parse_url`, `schemes`: the schemes compiled in (`loopback`, `mqtt`, `zenoh`); loopback transports shared per namespace in the process. |

| Folder | Role |
|---|---|
| [`codec/`](codec/README.md) | Envelope codecs (`compact`, `protobuf` by default, `json`), the envelope schema, and the encoding of a message content at the network boundary. |
| [`loopback/`](loopback/README.md) | In-memory transport between the environments of one process, always compiled in. |
| [`network/`](network/README.md) | Parts shared by the network transports: presence cache, ordered worker thread. |
| [`mqtt/`](mqtt/README.md) | MQTT transport (`CAM_TRANSPORT_MQTT`, libmosquitto). |
| [`zenoh/`](zenoh/README.md) | Zenoh transport (`CAM_TRANSPORT_ZENOH`, zenoh-c and zenoh-cpp). |
| [`actressmas/`](actressmas/README.md) | Wire format of the ActressMAS v3.1 `ContainerServer` (C#), and the transport that is its client (`CAM_TRANSPORT_ACTRESSMAS`). |

## Notes
- **Content**: a message keeps its typed content locally; it is encoded only when it leaves the process.
- **Threads**: transports call the registry from their network threads; the environment drains its endpoint at the turn boundary, never during a turn.
- **Adding a transport**: see "Adding a transport" in the main [README](../../README.md).
