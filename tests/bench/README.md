# Benchmarks

The measurement programs of the library (label `perf` for their smoke tests), built with `-DBUILD_BENCH=ON`, normally
through the `bench` preset: `-O3 -DNDEBUG`, without `-march=native` nor `-flto`, core only, linked to the static
library. They keep the workloads and the measured scope of the C# v3.1 E30 benchmarks, count with atomic counters and
time with `steady_clock` in microseconds; `--csv` prints one line per trial.

| Program | What it measures |
|---|---|
| `bench_PingPong` | 10 agents send each other messages by name and reply by id, 10 000 turns, 10 million messages at most, 5 trials with a new environment each. `--sequential` (fixed order, no thread pool) is the deterministic variant; `--turns`, `--trials`, `--agents` |
| `bench_PingPongPairs` | PingPong by pairs, the variant that the ActressMAS article cites without its code ("pairs of agents, 100 messages"), with the definition of its C# counterpart (`tools/csharp-ref/ref/PingPong.Pairs`): 500 pairs, the first agent of a pair starts, each agent sends 50 messages and stops after its 50th sent and 50th received, so a trial ends after exactly 50 000 messages; no turn limit, 5 trials, times to the microsecond. `--sequential`, `--pairs`, `--messages` (per pair, even), `--trials` |
| `bench_Skynet` | A tree of 10 000 agents created during the simulation; the leaves send their identifier, each node sums and reports (`Sum = 49995000`). `--sequential`, `--agents` |
| `bench_SkynetNumeric` | Skynet with 20 000 agents, sequential random order, numeric contents (`Long Sum = 199990000`). `--parallel`, `--sequential`, `--agents` |
| `bench_Wsg` | Wolf, Sheep and Grass of Antelmi et al. (*Applied Sciences* 13(1), 13, 2023; `tools/csharp-ref/ref/Abm`): wolves and sheep with energy on a field whose grass grows back, 200 turns in a random sequential order; `--agents N` (1 000 by default, 60 % sheep) on a field of side `floor(sqrt(10 N))`, the density of the article; `--seed`. Prints the populations of each turn and the time in ms |
| `bench_Flockers` | Flockers of Antelmi et al.: birds that move from their 8 neighbouring cells, 200 turns in a random sequential order, never lost (`Turn 200: 1000 ants`); `--agents N`, `--seed` |
| `bench_Churn` | Agents added and removed at every turn: N spawners, each removing the child of its previous action (inserted since) and adding a new one, Sequential; `--agents N` (2 000 by default), `--turns T` (20 by default). Prints the number of agents at the end and the time in ms |
| `bench_Transport` | Round trips between two environments through a transport compiled in: `--transport loopback|mqtt|zenoh`, `--url-a`, `--url-b`, `--pings`, `--trials`; exit code 77 when the transport or its broker is missing |
| `bench_Envelope` | Encoding and decoding of one envelope with a content of 0, 64 and 1 024 bytes, for every codec: `--count`, `--codec` |

The smoke tests (`perf.smoke.<program>`) run each program once and check its invariant: they are not a measurement.

**Growth with the number of agents.** `perf.scaling.Churn` runs `bench_Churn` with 2 000 then 8 000 spawners, 5 times
each (`scaling.cpp`, `cam_bench_scaling`), and fails when the median time grows more than 8 times: a linear cost gives
about 4, removals that search every agent added in the turn about 16. A ratio does not depend on the machine, so the
test holds anywhere; it is left out under a sanitizer, which changes the costs. `measure.hpp` holds what this tool and
`compare.cpp` share: running a program, reading its times, the median.
The faithful ports of the C# benchmarks, compared with their C# output, are in `iso/ports/`.

``` bash
cmake --workflow --preset bench
build/bench/tests/bench/bench_Skynet
build/bench/tests/bench/bench_PingPong --sequential --trials 3
```

## Faster than the C#

`perf.vs_csharp.<benchmark>` compares a benchmark with its C# original measured **on the same machine**
(`compare.cpp`, `cam_bench_compare`): the C++ program runs 10 times (`CAM_BENCH_RUNS`), and the median of its runs must
be faster than the C# median of the reference: a speedup above `CAM_BENCH_MIN_SPEEDUP` (1 when not set). The
measure is meant for a Release build (the `bench` preset), the C# reference and the C++ runs on the same machine.

| Test | C++ | C# (ActressMAS v3.1, E30) | Metric |
|---|---|---|---|
| `perf.vs_csharp.Skynet` | `bench_Skynet` | `Skynet` | time, ms (lower is better) |
| `perf.vs_csharp.SkynetNumeric` | `bench_SkynetNumeric` | `SkynetNumeric` | time, ms |
| `perf.vs_csharp.PingPong` | `bench_PingPong` | `PingPong` | msg/ms (higher is better) |
| `perf.vs_csharp.PingPongSequential` | `bench_PingPong --sequential` | `PingPong` in a fixed sequential order (`tools/csharp-ref/ref/PingPong.Seq`) | msg/ms |
| `perf.vs_csharp.PingPongPairs` | `bench_PingPongPairs` | PingPong by pairs on the v3.1 framework (`tools/csharp-ref/ref/PingPong.Pairs`), not an E30 program | msg/ms |
| `perf.vs_csharp.PingPongPairsSequential` | `bench_PingPongPairs --sequential` | the same, `--sequential` | msg/ms |
| `perf.vs_csharp.Wsg` | `bench_Wsg` | Wolf, Sheep and Grass of Antelmi et al. on the v3.1 framework (`tools/csharp-ref/ref/Abm/Wsg`), 1 000 agents | time, ms |
| `perf.vs_csharp.Flockers` | `bench_Flockers` | Flockers of Antelmi et al. (`tools/csharp-ref/ref/Abm/Flockers`), 1 000 agents | time, ms |

On both sides, the first run of a time and the first trial of each PingPong run warm up and are dropped; a PingPong run
gives the mean msg/ms of its other trials. The parallel PingPong compares throughputs only: the C# stops at its 10 000
turns, with fewer messages than the C++.

The reference is a text file, one line per benchmark (`<benchmark> ms|msg_per_ms <median>`), named by
`CAM_BENCH_REFERENCE`; without it the tests are skipped. It belongs to one machine, so it is not versioned:
- **Podman machine**: `tools/csharp-ref/csharp-ref.sh <workdir> benchref` measures the C# in a .NET container of the
  machine and writes `<workdir>/logs/bench-reference-net10.txt`; then
  `CAM_BENCH_REFERENCE=<that file> tests/run.sh --linux bench perf` runs the C++ in a Linux container of the same
  machine.
- **CI**: the `bench` workflow (`.github/workflows/bench.yml`, started by hand) installs .NET on the runner, measures
  the C# there (`CAM_DOTNET=native`), then builds and runs the C++ against it.

The reference is measured before the C++, not interleaved with it: on a shared or busy machine, measure both again
before reading a failure as a regression. `perf.compare.*` check the tool itself on fixture references (`fixtures/`).
