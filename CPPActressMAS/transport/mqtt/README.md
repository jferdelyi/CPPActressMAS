# CPPActressMAS: MQTT transport

Built when `CAM_TRANSPORT_MQTT=ON` (OFF by default). URL: `mqtt://host:port/namespace`.

| File | Role |
|---|---|
| `MqttTransport.{h,cpp}` | MQTT 5 transport on the libmosquitto C client: one connection per transport (node), its own loop thread, `TCP_NODELAY`, presence retained on the broker, waits bounded in time. |
| `CMakeLists.txt` | Fetches libmosquitto 2.0.22 (`FetchContent`, pinned by SHA-256) and builds its static client library with TLS; adds the sources and the dependency to the core library. |

## Notes
- **Broker**: any MQTT 5 broker; the tests use a local Mosquitto with `set_tcp_nodelay true` (`network.mqtt`, skipped without `CAM_MQTT_HOST`).
- **Dependencies**: libmosquitto (EPL-2.0 / EDL-1.0), OpenSSL for TLS.
