# C# reference harness

Builds and runs the C# reference, ActressMAS v3.1 (commit `eb0bb73`), in throw-away containers of a Podman machine, to
produce what the C++ port is compared with: random generator vectors, turn order traces, benchmarks, and the **oracles**
of the ported examples. Nothing is installed on the host and the original C# sources are compiled unchanged: the
projects only reference them.

The C++ tests never need .NET: the oracles are produced here once and versioned in `tests/iso/oracles/`.

## Usage

```
tools/csharp-ref/csharp-ref.sh WORKDIR ACTION [net10|net8]
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
| `e00` | the seven E00 examples |
| `shimcheck` | `RngVectors` compiled through the seed shim: must equal `ref/expected/rng-vectors.txt` |
| `oracles` | runs every project of `ref/Oracles/` into `WORKDIR/logs/oracles/<Name>.txt` |
| `e20build`, `capture`, `e20`, `e20mono` | the ContainerServer harness of E20 (.NET 8 and Mono) |

## Oracles

Each project of `ref/Oracles/` compiles the original sources of one example, the original framework sources and the
seed shim into one assembly (`ref/Oracles/Oracle.props`). The output is versioned raw, without carriage returns, after
a `#cam` header (C# commit, runtime, command, seed); `tests/iso/iso_check` extracts the facts at test time.

To add an example: create `ref/Oracles/<Name>/<Name>.csproj` with its `OracleSources` folder, run `build` then
`oracles`, and copy `WORKDIR/logs/oracles/<Name>.txt` to `tests/iso/oracles/`.

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
