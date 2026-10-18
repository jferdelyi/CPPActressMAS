# ISO comparison with the C# examples

Each ported example is compared with its C# original by facts, never character by character (D74, D82).

| Path | Role |
|---|---|
| `iso_check.cpp` | The comparison tool: runs the C++ example, extracts the facts of both outputs and compares them (grammar of the rules at the top of the file). |
| `oracles/<Example>.txt` | Raw output of the C# original, with a `#cam` header (C# commit, runtime, command, seed), produced by `tools/csharp-ref` in a container. The C++ tests never run .NET. |
| `rules/<Example>.rules` | The facts of the example and how they are compared: multiset, order within a group, invariants. |
| `fixtures/` | Outputs that the tool must accept or refuse (`unit.iso_check.*`). |

An example is registered with `cam_add_iso(<Example>)` in `tests/CMakeLists.txt`: the test `iso.<Example>` (label `iso`)
exists when the examples are built (`-DBUILD_EXAMPLES=ON`).
