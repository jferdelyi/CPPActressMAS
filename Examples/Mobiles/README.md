# Examples — several environments

Ports of the ActressMAS v3.1 `E20` examples. In C# each container is a separate program connected to a central
server; here the environments of one program are connected by a transport URL, `loopback://` by default.

| File | Executable | C# example | What it shows | Port |
|---|---|---|---|---|
| `Mobile.cpp` | `Mobile` | E20/MasMobileConsole (and MasMobileGui) | A mobile agent visits every other environment, asks the static agents there for information and reports once home. | partial: three environments in one process instead of three programs and a server |
| `PingPong.cpp` | `MobilePingPong` | E20/RemoteMessages | A ping agent asks a pong agent of another environment for information, by name; the pong agent answers twice. | partial: transparent addressing instead of `agent@container` |

Run across processes or machines with Zenoh or MQTT URLs, as shown in the comments of each `main`
(for example `Mobile "mqtt://127.0.0.1:1883/mobile"`).
