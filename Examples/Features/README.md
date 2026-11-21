# Examples: features of the C++ port

Examples without a C# original: what the C++ port adds, measured where it is a matter of speed.

| File | Executable | What it shows |
|---|---|---|
| `TransportRoundTrip.cpp` | `TransportRoundTrip` | Round trips by id between two environments of one program, with the round-trip time and the message rate: `TransportRoundTrip [round trips [protocol [host [port]]]]`, the loopback of the process without configuration by default. Environments configured with `EnvironmentConfig` (`m_network`, `m_endpoint`); with a protocol, connected before their threads start, and their traffic stays on the loopback. |
| `SharedText.cpp` | `SharedText` | A long text broadcast to many receivers, copied for each one as a `std::string`, shared by all of them as a `cam::Text`, with the time of each. |

For the measured benchmarks of the library (protocol, series against the C#), see `tests/bench/`.
