# C# reference harness

Builds and runs the C# reference, ActressMAS v3.1 (commit `eb0bb73`), in throw-away containers of a Podman machine, to
produce what the C++ port is compared with: random generator vectors, turn order traces, benchmarks, and the **oracles**
of the ported examples. Nothing is installed on the host and the original C# sources are compiled unchanged: the
projects only reference them.

The C++ tests never need .NET: the oracles are produced here once and versioned in `tests/iso/oracles/`.

## Usage

```
tests/tools/csharp-ref/csharp-ref.sh WORKDIR ACTION [net10|net8]
```

- `WORKDIR`: a work folder outside the repository (under `$HOME`). `ref/` is copied into it at each run and every build
  stays there; the reference repository is cloned into `WORKDIR/ActressMas/` if missing, then only read.
- The Podman machine is `cam` unless `CAM_PODMAN_CONNECTION` names another one. Images: `mcr.microsoft.com/dotnet/sdk:10.0`
  (default) or `:8.0`.

| Action | What it does |
|---|---|
| `build` | builds every project of `ref/` but `BfProbe` and `E20` |
| `rng`, `trace` | `RngVectors` and `TraceOrder`, to compare with `ref/expected/` from line 2 |
| `bench` | the E30 benchmarks |
| `benchref` | runs the E30 benchmarks, the sequential PingPong (`ref/PingPong.Seq`) and the PingPong by pairs (`ref/PingPong.Pairs`, parallel and sequential) `CAM_BENCH_RUNS` times (10 by default) and writes their medians into `WORKDIR/logs/bench-reference-<flavor>.txt`, the C# reference of the `perf.vs_csharp` tests on this machine (`tests/bench/README.md`) |
| `e00` | the seven E00 examples |
| `shimcheck` | `RngVectors` compiled through the seed shim: must equal `ref/expected/rng-vectors.txt` |
| `oracles` | runs every project of `ref/Oracles/`, from its output folder, into `WORKDIR/logs/oracles/<Name>.txt` |
| `distrib` | runs the projects of `CAM_DISTRIB` (PredatorPrey Reactive ContractNet Shapley by default) at the seeds 1 to `CAM_DISTRIB_SEEDS` (30) into `WORKDIR/logs/distrib/<Name>/seed<n>.txt`, the C# samples of the distribution tests (`tests/iso/iso_stats.cpp --extract`) |
| `e20build`, `capture`, `e20`, `e20mono` | the ContainerServer harness of E20 (.NET 8 and Mono) |
| `e20gui` | MasMobileGui without WinForms, server and three containers (.NET 8: the agent cannot move, `Agent.Move` serializes a `System.Type`) |
| `e20oracles` | `logs/oracles/RemoteMessages.txt` and `MasMobileConsole.txt` from the captures of `e20` and `e20mono`, the console of each container in turn |

`CAM_DOTNET=native` runs the `dotnet` of the host instead of a container, from `WORKDIR/ref` as well: the CI uses it
for `build` and `benchref`, on a runner where the job installs .NET.

## Oracles

Each project of `ref/Oracles/` compiles the original sources of one example, the original framework sources and the
seed shim into one assembly (`ref/Oracles/Oracle.props`). The output is versioned raw, without carriage returns, after
a `#cam` header (C# commit, runtime, command, seed); `tests/iso/iso_check` extracts the facts at test time.

To add an example: create `ref/Oracles/<Name>/<Name>.csproj` with its `OracleSources` folder, run `build` then
`oracles`, and copy `WORKDIR/logs/oracles/<Name>.txt` to `tests/iso/oracles/`.

An example with a WinForms form, or one that never stops, declares what replaces original code; the `#cam` header lists
it (`#cam stub`, `#cam patch`):

- `OracleExcludes`: original files not compiled, the form and its `.Designer.cs`. The stubs of `OracleStubs` (the
  project's `Stubs/` by default, or another project's) are compiled in their place: a stub keeps the form's members
  that the agents call and prints the state the form would draw, `ShowDialog` returns at once.
- `<File>.cs.sed`: rewrites one line of the original `<File>.cs` into `WORKDIR/ref/Oracles/<Name>/Patched/<File>.cs`,
  compiled instead of the original: `noTurns` of an example that never stops (`Program.cs.sed`), or a scenario that
  reaches a bug of the C# (`BDIAdjacent/TerrainAgent.cs.sed`). The script stops if a rewrite does not change
  exactly one line.
- `OracleData`: data files copied next to the program, which reads them from its working directory (`patterns.txt` of
  E13).

**Variants.** A variant of an example is a project of its own, `ref/Oracles/<Name><Variant>/`, with the same
`OracleSources` and a `Program.cs.sed` of one line: a variant commented in the C# example (`Agents5Seq`,
`PrisonerDilemmaConfess`, `VickreySeqRandom`, `SkynetNumericParallel`...), or the sequential version of a parallel one
(`ShapleySeq`, `ContractNetSeq`, `ReactiveSeq`...), where the C# fixes every line. The C++ port takes the matching
argument (`tests/iso/DotNet.hpp`: `scenario_mode`, `has_scenario`).

## Seed shim

`ref/Oracles/Shim/GlobalUsings.cs` maps the name `Random` to `CamOracle.SeededRandom`, so every `new Random()` of the
example and of the framework is seeded from its call site. The k-th instance created at file basename `B`, line `L`
gets:

```
h    = FNV-1a 64 of the UTF-8 text "B:L"
x    = SplitMix64(SplitMix64(CAM_SEED ^ h) + k)
seed = (int)(x & 0x7FFFFFFF)
```

`CAM_SEED` comes from the environment (0 when absent) and each seed is logged on stderr as `#camrng B:L k seed`. The
`oracles` action refuses an example that uses `System.Random` or `Random.Shared` directly, which the alias cannot reach.
`shimcheck` proves that the shim draws exactly as the seeded `System.Random`.
