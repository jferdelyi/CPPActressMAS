# CPPActressMAS: Zenoh transport

Built when `CAM_TRANSPORT_ZENOH=ON` (OFF by default). URL: `zenoh://host:port/namespace`, or
`zenoh://?listen=tcp/127.0.0.1:7447&ns=name` for a peer that listens.

| File | Role |
|---|---|
| `ZenohTransport.{h,cpp}` | Zenoh transport: one session per transport (node), peer or client mode, presence by liveliness tokens plus a request to the queryable of each environment, local echo filtered out (`Z_LOCALITY_REMOTE`). |
| `CMakeLists.txt` | Finds zenoh-c and zenoh-cpp 1.7.1 exactly (`find_package`), links the static zenoh-c on macOS and the shared one elsewhere, and adds the sources to the core library. |

## Notes
- **Install**: zenoh-c and zenoh-cpp 1.7.1 from their release archives (see `docs/WINDOWS.md` for Windows).
- **Tests**: `network.zenoh` opens TCP sockets on 127.0.0.1, no multicast.
- **Dependencies**: zenoh-c and zenoh-cpp (EPL-2.0 or Apache-2.0).
