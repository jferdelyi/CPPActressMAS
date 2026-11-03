# CPPActressMAS: loopback transport

The transport of the environments of one process, always compiled in, with no dependency.

| File | Role |
|---|---|
| `LoopbackTransport.{h,cpp}` | In-memory transport between the environments of one process, optionally serializing, with fault injection for the tests. `Facade` shares one instance per namespace (`loopback://<namespace>`). |
