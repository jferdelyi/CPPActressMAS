# Benchmarks

The measurement programs of the library (label `perf` for their smoke tests), built with `-DBUILD_BENCH=ON`, normally
through the `bench` preset: `-O3 -DNDEBUG`, without `-march=native` nor `-flto`, core only, linked to the static
library. They keep the workloads and the measured scope of the C# v3.1 E30 benchmarks, count with atomic counters and
time with `steady_clock` in microseconds; `--csv` prints one line per trial.

| Program | What it measures |
|---|---|
| `bench_PingPong` | 10 agents send each other messages by name and reply by id, 10 000 turns, 10 million messages at most, 5 trials with a new environment each. `--sequential` (fixed order, no thread pool) is the deterministic variant; `--turns`, `--trials`, `--agents` |
| `bench_Skynet` | A tree of 10 000 agents created during the simulation; the leaves send their identifier, each node sums and reports (`Sum = 49995000`). `--sequential`, `--agents` |
| `bench_SkynetNumeric` | Skynet with 20 000 agents, sequential random order, numeric contents (`Long Sum = 199990000`). `--parallel`, `--sequential`, `--agents` |
| `bench_Transport` | Round trips between two environments through a transport compiled in: `--transport loopback|mqtt|zenoh`, `--url-a`, `--url-b`, `--pings`, `--trials`; exit code 77 when the transport or its broker is missing |
| `bench_Envelope` | Encoding and decoding of one envelope with a content of 0, 64 and 1 024 bytes, for every codec: `--count`, `--codec` |

The smoke tests (`perf.smoke.<program>`) run each program once and check its invariant: they are not a measurement.
The faithful ports of the C# benchmarks, compared with their C# output, are in `iso/ports/`.

``` bash
cmake --workflow --preset bench
build/bench/tests/bench/bench_Skynet
build/bench/tests/bench/bench_PingPong --sequential --trials 3
```
