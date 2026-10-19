# Oracle tests: ISO comparison with the C# originals

The harness checks that the library behaves as ActressMAS v3.1 (C#): each C# example is ported as it is, its bugs
included, and its output is compared with the output of the C# original by facts, never character by character.
These programs are not the examples of the library (`Examples/`): an example shows how to use the
library at its best; a harness program reproduces the C#, line for line.

| Path | Role |
|---|---|
| `ports/<Project>.cpp` | One program per C# project, named after it (`Agents1`, `BDI`, `PingPong`, ...): the C# original on the mechanics of the library, same agents, names, texts (typos included) and logic, its bugs kept. Built as `iso_<Project>` with the tests, examples or not. |
| `oracles/<Oracle>.txt` | Raw output of the C# original, with a `#cam` header (C# commit, runtime, command, seed, and what replaces original code: form stubs, bounded number of turns, a scenario patch), produced by `tests/tools/csharp-ref` in a container. The C++ tests never run .NET. |
| `rules/<Oracle>.rules` | The facts of the output and how they are compared: exact sequence, multiset, order within a group, invariants (grammar at the top of `iso_check.cpp`). |
| `iso_check.cpp` | The comparison tool: runs a program, extracts the facts of both outputs and compares them. |
| `DotNet.hpp` | .NET behaviour the ports need: number formatting and parsing (`format_fixed` as `ToString("F2")`, `to_string` as `Double.ToString()`, `to_int32` as `Convert.ToInt32`, invariant culture), `write` and `write_line` (one piece per line, as `Console.WriteLine`), and the seeds of the oracles (`next_site_seed`). Checked by `iso.dotnet_format` and `iso.dotnet_site_seed` (`dotnet_format.cpp`, `dotnet_seed.cpp`) against vectors of the C# (`vectors/format.txt`). |
| `fixtures/` | Outputs that the tool must accept or refuse (`iso.check.*`). |

A program is registered with `cam_add_iso(<Project> [NAME <test>] [ORACLE <oracle>] [arguments])` in
`tests/iso/CMakeLists.txt`: the test `iso.<oracle>` (label `iso`) runs `iso_<Project>` from its folder, where its data files
are copied, with the arguments that match the oracle (the number of turns of an example that never stops, a scenario).

**Comparison levels.**
- **Exact trace**, for a sequential example: every line in order (`Agents2`, `ColorGame`, `NumberGame`, `MechanismDesign`, `PrisonerDilemma`, `BDI`, `GameOfLife`).
- **Invariants**, for a parallel one: what does not depend on the interleaving, such as the lines of each agent in order, multisets, counts, sums and turn barriers (`Agents1`, `Agents3`, `Agents4`, `Agents5`, `MultipleMessages`, `SendingObjects`, `PingPong`, `Skynet`).

**Random draws.** A C# example creates its `System.Random` without a seed; the oracle seeds each one from its call site
(`tests/tools/csharp-ref/README.md`), and the port draws from the same seed (`cam::dotnet::next_site_seed`). `CAM_SEED` in
the environment changes them on both sides; with it set, a port reports its seeds on the error output as the oracle
does (`#camrng`).

**Arithmetic.** The ports are compiled with `-ffp-contract=off` (GCC and Clang): no fused multiply-add, so that their
arithmetic rounds as the C# does.
