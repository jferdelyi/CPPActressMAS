# CPPActressMAS: ActressMAS transport

Talking to the ActressMAS v3.1 `ContainerServer` (C#), which is kept as it is: C++ environments are containers of a C#
simulation, `actressmas://host:port/<container>`.

| File | Role |
|---|---|
| `ActressMasCodec.{h,cpp}` | Wire format of the `ContainerServer`: 38-byte frame, BinaryFormatter (MS-NRBF) subset, base64 (`util/Base64`), UTF-16LE. Part of the core whatever the option. |
| `ActressMasTransport.{h,cpp}` | The client (`CAM_TRANSPORT_ACTRESSMAS`): one TCP connection per container, the list of the server as presence, messages between agents as `name@container`, identifiers derived from the names for the containers and agents of the C#. |
| `Socket.{h,cpp}` | Blocking TCP connection on POSIX sockets or Winsock, for one reading thread. |
| `CMakeLists.txt` | Sources of the transport for the core library, and `ws2_32` on Windows. |

The vectors captured from the C# are under `tests/unit/vectors/envelope/actressmas/v1/`. The tests run against a relay
that follows the rules of the C# server (`tests/support/ActressMasRelay.hpp`); `tests/tools/csharp-ref/csharp-ref.sh
e20cpp` runs the port of the E20 example against the C# server itself.
