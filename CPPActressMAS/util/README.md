# CPPActressMAS: utilities

Helpers the library uses but which are not objects of the multi-agent model. Namespace `cam`, no dependency.

| File | Role |
|---|---|
| `Identifier.{h,cpp}` | `generate_uuid` (version 4, RFC 9562) for environments, nodes and transfers; `generate_agent_id` (15 base62 characters) for agents. One generator per thread, seeded once. |
| `Base64.{h,cpp}` | Base64 (RFC 4648) for the json and ActressMAS codecs. |
| `DotnetRandom.{h,cpp}` | Generator drawing exactly as the seeded `System.Random` of .NET: an environment can use it so that its random order matches the C# draw for draw. |
