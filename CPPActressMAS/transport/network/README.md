# CPPActressMAS: shared parts of the network transports

Helpers used by the network transports ([`mqtt/`](../mqtt/README.md), [`zenoh/`](../zenoh/README.md)), with no dependency.

| File | Role |
|---|---|
| `PresenceCache.{h,cpp}` | Last presence of each environment seen by a transport, replayed to late endpoints and served to peers. |
| `Worker.{h,cpp}` | One thread running jobs in order (Zenoh: calls into the client library asked from its own callbacks). |
