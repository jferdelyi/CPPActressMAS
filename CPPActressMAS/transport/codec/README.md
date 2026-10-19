# CPPActressMAS: codecs

How an envelope, and the content of a message, become bytes on the wire. Every envelope codec writes the same 4-byte
header (magic, codec id, version); a transport uses one codec, chosen by the URL option `codec=` (`protobuf` by default).

| File | Role |
|---|---|
| `EnvelopeCodec.{h,cpp}` | The codec interface, the 4-byte envelope header, envelope validation and `make_codec`. |
| `CompactCodec.{h,cpp}` | Codec 1: fixed-width little-endian fields, no dependency. |
| `ProtobufCodec.{h,cpp}` | Codec 2, the default: the proto3 wire format of `envelope.proto`, written by hand (no libprotobuf). |
| `JsonCodec.{h,cpp}` | Codec 3: readable JSON, bytes in base64 (`util/Base64`). |
| `envelope.proto` | Schema of the envelope (reference for the protobuf codec; not compiled by the project). |
| `ContentCodec.{h,cpp}` | Encoding of a message content at the network boundary: once when it leaves the node, once when it arrives. |

The binary vectors of each envelope codec are under `tests/unit/vectors/envelope/<codec>/v1/`.
