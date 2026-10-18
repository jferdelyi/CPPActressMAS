# CPPActressMAS: ActressMAS wire format

Talking to the ActressMAS v3.1 `ContainerServer` (C#), which is kept as it is.

| File | Role |
|---|---|
| `ActressMasCodec.{h,cpp}` | Wire format of the `ContainerServer`: 38-byte frame, BinaryFormatter (MS-NRBF) subset, base64 (`util/Base64`), UTF-16LE. |

The client transport of the C# server will join this folder. The vectors captured from the C# are under
`tests/vectors/envelope/actressmas/v1/`.
