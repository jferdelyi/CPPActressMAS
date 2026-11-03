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

Minor versions of the envelope (`version_minor`, the same major version for every codec):
- 1.0: the first version.
- 1.1: the name of the sender agent of a message (`sender_name`), so that a receiver names it once the sender is gone
  or when it lives in another environment. Compact: a text right after `sender_agent`, present from 1.1 only; protobuf:
  field 24, absent when empty; JSON: key `sender_name`, absent when empty. A 1.1 decoder reads a 1.0 envelope (no
  name). A compact 1.0 decoder refuses a 1.1 envelope (trailing bytes); protobuf and JSON 1.0 decoders ignore the name.
