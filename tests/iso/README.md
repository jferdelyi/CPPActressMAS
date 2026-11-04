# Oracle tests: ISO comparison with the C# originals

The harness checks that the library behaves as ActressMAS v3.1 (C#): each C# example is ported as it is, its bugs
included, and its output is compared with the output of the C# original by facts, never character by character.
These programs are not the examples of the library (`Examples/`): an example shows how to use the
library at its best; a harness program reproduces the C#, line for line.

| Path | Role |
|---|---|
| `ports/<Project>.cpp` | One program per C# project, named after it (`Agents1`, `BDI`, `PingPong`, ...): the C# original on the mechanics of the library, same agents, names, texts (typos included) and logic, its bugs kept. Built as `iso_<Project>` with the tests, examples or not. |
| `oracles/<Oracle>.txt` | Raw output of the C# original, with a `#cam` header (C# commit, runtime, command, seed, and what replaces original code: form stubs, bounded number of turns, a scenario patch), produced by `tests/tools/csharp-ref` in a container. The C++ tests never run .NET. |
| `rules/<Oracle>.rules` | The facts of the output and how they are compared: exact sequence, multiset, order within a group, an aggregate of the C# against the C++, published values (`expect`), invariants and predicates on every tuple (grammar at the top of `iso_check.cpp`). |
| `oracles/seed42/`, `oracles/seed7/` | The oracles of the same examples at two other seeds (`CAM_SEED`), for the examples whose output depends on the draws: tests `iso.<Oracle>.seed42` and `.seed7`. A program fitted to one seed fails them. |
| `data/` | Data files of the C# examples that the ports read from their folder: the map of E03 (`Pendulum.grx`, `Pendulum.grh`, ActressMAS, Apache-2.0); `iso.LrtaStar` is registered only when they are there. |
| `iso_stats.cpp`, `distributions/` | `iso.<Oracle>.distribution.<generator>`: where the C# itself is not deterministic, statistics of 30 runs of the port (`CAM_SEED` 1 to 30) against those of the C# (`<Oracle>.txt`, extracted from `csharp-ref distrib` with `<Oracle>.spec`), by a two-sample Kolmogorov-Smirnov test, with the generator of the oracles and with the default one of the library (`CAM_RANDOM_ENGINE=mt19937`). Label `stat`, not under the sanitizers. |
| `gardner.cpp` | `iso.gardner`: the game of life of the port of E13 against the patterns of Gardner (1970), generation by generation, with the facts of the article. |
| `iso_check.cpp` | The comparison tool: runs a program, extracts the facts of both outputs and compares them. |
| `DotNet.hpp` | .NET behaviour the ports need: number formatting and parsing (`format_fixed` as `ToString("F2")`, `to_string` as `Double.ToString()`, `to_int32` as `Convert.ToInt32`, invariant culture), `write` and `write_line` (one piece per line, as `Console.WriteLine`), and the seeds of the oracles (`next_site_seed`), and the sort of `List<T>.Sort()` (`list_sort`, not stable). Checked by `iso.dotnet_format` and `iso.dotnet_site_seed` (`dotnet_format.cpp`, `dotnet_seed.cpp`) against vectors of the C# (`vectors/format.txt`). |
| `fixtures/` | Outputs that the tool must accept or refuse (`iso.check.*`). |

A program is registered with `cam_add_iso(<Project> [NAME <test>] [ORACLE <oracle>] [SEED <seed>] [arguments])` in
`tests/iso/CMakeLists.txt`: the test `iso.<oracle>` (label `iso`) runs `iso_<Project>` from its folder, where its data files
are copied, with the arguments that match the oracle (the number of turns of an example that never stops, a scenario).

**Comparison levels.**
- **Exact trace**, for a sequential example: every line in order (`Agents2`, `ColorGame`, `NumberGame`, `MechanismDesign`, `PrisonerDilemma`, `BDI`, `GameOfLife`, `Zeuthen`, `Voting`, `EnglishWithBroadcast`, `LrtaStar`).
- **Invariants**, for a parallel one: what does not depend on the interleaving, such as the lines of each agent in order, multisets, counts, sums and turn barriers (`Agents1`, `Agents3`, `Agents4`, `Agents5`, `MultipleMessages`, `SendingObjects`, `PingPong`, `Skynet`, `FictitiousPlay`, `ReplicatorDynamics`, `EnglishWithoutBroadcast`, `Vickrey`, `YellowPages`, `Shapley`, `ContractNet`, `Reactive`, and per container `RemoteMessages`, `MasMobileConsole`), or an example that prints only a result (`SkynetNumeric`, its sum).
- **Exact beginning, then invariants**, for a sequential example in a random order whose C# order changes once agents leave (the C# `Dictionary` reuses their places): the first turns line by line, then counts and bounds (`PredatorPrey`, `PredatorPreyGui`; `same ... first N` compares the first tuples only).

**Variants.** The oracles of a variant (`<Oracle><Variant>.txt`) come from the original sources but one line
(`tests/tools/csharp-ref/README.md`): the variants commented in the C# examples, and the sequential version of the
parallel ones, compared line by line where the parallel original only allows invariants (`iso.ShapleySeq`,
`iso.ContractNetSeq`, `iso.ReactiveSeq`...).

**Published values and scenarios.** Where an example has published values, the rules check them on both outputs
(`expect`: the Clarke taxes of `MechanismDesign`, the Nash agreement of `Zeuthen`). A scenario that the C# does not run
is an argument of the port (`adjacent` for BDI, `calculate` for Shapley: `iso.shapley.two` and `.three` check the
calculator alone on the examples of the Shapley value); the example itself runs unchanged without it.

**Over the ActressMAS transport.** `iso.RemoteMessages.actressmas` runs the port of E20 with each container connected
under its name to the relay of the tests (`tests/support/ActressMasRelay.hpp`), which follows the rules of the C#
server; its rules compare the remote sender as `name@container`, as the C# writes it. The C# server relays each message
on a thread of its pool, so the two answers of the pong agent may come in either order:
`iso.RemoteMessages.actressmas.unordered` has the relay swap them (`--swap`) and compares the messages received as a
multiset. `tests/tools/csharp-ref/csharp-ref.sh e20cpp` runs the same port against the C# server itself.

**A known divergence is paired, never tolerated wholesale.** `same NAME bag alias C <regex>` pairs a C++ tuple whose
column C matches the regex with a C# tuple equal on every other column; any other difference still fails the test. No
rule needs it today: it served while a message showed the identifier of a sender already removed.

**Durations are never compared.** A run of the C# and a run of the C++ take different times: a rule checks only that a
duration is printed, and in what form (for instance `fact duration ^\d+\.000 ms$` and `same duration count`), never its
value.

**Random draws.** A C# example creates its `System.Random` without a seed; the oracle seeds each one from its call site
(`tests/tools/csharp-ref/README.md`), and the port draws from the same seed (`cam::dotnet::next_site_seed`). `CAM_SEED` in
the environment changes them on both sides; with it set, a port reports its seeds on the error output as the oracle
does (`#camrng`).

**Arithmetic.** The ports are compiled with `-ffp-contract=off` (GCC and Clang): no fused multiply-add, so that their
arithmetic rounds as the C# does.

**Strength of the rules.** `CAM_ISO_SAVE=<folder> ctest --preset debug-iso` keeps the output of every port;
`python3 -I tests/tools/iso-mutate.py build/debug/tests/iso/cam_iso_check tests/iso <folder>` then alters each output line by
line and reports the alterations that still pass. A new or changed rule is measured this way before it is kept.
