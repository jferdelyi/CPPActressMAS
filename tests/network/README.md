# Network tests

The network transports, label `network` plus the transport (`mqtt` or `zenoh`). Each program skips with exit code 77
(reported as skipped by CTest) when its transport is not compiled in or its server is missing; `-LE network` keeps a
run off the network.

| Test | File | Needs |
|---|---|---|
| `network.mqtt` | `network_tests.cpp` | The MQTT transport (`CAM_TRANSPORT_MQTT`) and a broker: `CAM_MQTT_HOST` and `CAM_MQTT_PORT`. The reconnection case also needs `CAM_MQTT_BROKER_CONTROL`, a command that takes `stop` or `start` and stops or starts the broker; without it, that case is skipped. Namespaces get a random suffix per process, so two machines can share a broker. |
| `network.zenoh` | `zenoh_tests.cpp` | The Zenoh transport (`CAM_TRANSPORT_ZENOH`). Two peers of the process on 127.0.0.1, explicit endpoints (ports 17447 to 17454), no multicast, no router. |

``` bash
CAM_MQTT_HOST=127.0.0.1 CAM_MQTT_PORT=1883 ctest --preset debug-network
ctest --preset debug -LE network
```
