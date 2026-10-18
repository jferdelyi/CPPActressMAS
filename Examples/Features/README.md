# Examples — features of the C++ port

Examples without a C# original: what the C++ port adds, measured where it is a matter of speed.

| File | Executable | What it shows |
|---|---|---|
| `TransportRoundTrip.cpp` | `TransportRoundTrip` | Round trips by id between two environments through a transport chosen by its URL (`loopback://` by default, Zenoh or MQTT as arguments), with the round-trip time and the message rate. Environments configured with `EnvironmentConfig` (`m_url`, `m_endpoint`) and connected before their threads start. |
| `SharedText.cpp` | `SharedText` | A long text broadcast to many receivers, copied for each one as a `std::string`, shared by all of them as a `cam::Text`, with the time of each. |

For the measured benchmarks of the library (protocol, series against the C#), see `bench/`.
