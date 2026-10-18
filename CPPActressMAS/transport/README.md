# CPPActressMAS — transport layer

How environments talk to each other: an environment connected to a URL owns an `Endpoint` attached to a `Transport`.
The layer is part of the core library; the network transports join it behind their CMake options
(`CAM_TRANSPORT_MQTT`, `CAM_TRANSPORT_ZENOH`), with their own dependencies, in [`mqtt/`](mqtt/README.md) and
[`zenoh/`](zenoh/README.md).

| File | Role |
|---|---|
| `Transport.h` | The `Transport` interface (`open`, `close`, `attach`, `detach`, `publish`, `capabilities`, `codec`) and the `EndpointRegistry` shared by every transport, which delivers in memory what stays in the process. |
| `Endpoint.{h,cpp}` | One environment on a transport: outbox flushed at the turn boundary, inbox, deduplication and reordering by sequence, directory of the agents of the other environments, presences, counters. |
| `Envelope.h` | What travels: the `Envelope` and its payloads (`AgentTransfer`, `AgentControl`, `Presence`, `DirectoryEvent`). |
| `Types.h` | Addresses and keys, send options, capabilities, configurations, statistics, events, `Error` and `Result`. |
| `EnvelopeCodec.{h,cpp}` | The codec interface, the 4-byte envelope header, envelope validation and `make_codec`. |
| `CompactCodec.{h,cpp}` | Codec 1: fixed-width little-endian fields, no dependency. |
| `ProtobufCodec.{h,cpp}` | Codec 2, the default: the proto3 wire format of `envelope.proto`, written by hand (no libprotobuf). |
| `JsonCodec.{h,cpp}` | Codec 3: readable JSON, bytes in base64. |
| `envelope.proto` | Schema of the envelope (reference for the protobuf codec; not compiled by the project). |
| `ContentCodec.{h,cpp}` | Encoding of a message content at the network boundary: once when it leaves the node, once when it arrives. |
| `ActressMasCodec.{h,cpp}` | Wire format of the ActressMAS v3.1 `ContainerServer` (C#): 38-byte frame, BinaryFormatter (MS-NRBF) subset, base64, UTF-16LE. |
| `Facade.{h,cpp}` | `open(url)`, `parse_url`, `schemes`: the schemes compiled in (`loopback`, `mqtt`, `zenoh`); loopback transports shared per namespace in the process. |
| `LoopbackTransport.{h,cpp}` | In-memory transport between the environments of one process, optionally serializing, with fault injection for the tests. |
| `PresenceCache.{h,cpp}` | Last presence of each environment seen by a transport, replayed to late endpoints and served to peers. |
| `Worker.{h,cpp}` | One thread running jobs in order (Zenoh: calls into the client library asked from its own callbacks). |

## Notes
- **Content**: a message keeps its typed content locally; it is encoded only when it leaves the process.
- **Threads**: transports call the registry from their network threads; the environment drains its endpoint at the turn boundary, never during a turn.
- **Adding a transport**: see "Adding a transport" in the main [README](../../README.md).
