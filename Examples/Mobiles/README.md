# Examples: several environments

Ports of the ActressMAS v3.1 `E20` examples. In C# each container is a separate program connected to a central
server; here the environments of one program meet on the loopback of the process by default, and a network protocol
can be given in argument.

| File | Executable | C# example | What it shows | Port |
|---|---|---|---|---|
| `Mobile.cpp` | `Mobile` | E20/MasMobileConsole (and MasMobileGui) | A mobile agent visits every other environment, asks the static agents there for information and reports once home. | partial: three environments in one process instead of three programs and a server |
| `PingPong.cpp` | `MobilePingPong` | E20/RemoteMessages | A ping agent asks a pong agent of another environment for information, by name; the pong agent answers twice. | partial: transparent addressing instead of `agent@container` |

Both take `[protocol [host [port]]]`: `loopback`, `zenoh`, `mqtt` or `mqtts`, then the meeting point, by default
`127.0.0.1` and the port of the protocol (for example `Mobile zenoh 127.0.0.1 17447` or `Mobile mqtt`). With Zenoh, the
first environment listens on the meeting point and the others connect to it. Between the environments of one program the
traffic stays on the loopback: the network serves other programs, as shown in the comments of each `main`.
