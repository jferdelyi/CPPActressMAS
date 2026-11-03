# Examples: original benchmarks

The two benchmarks of the C++ port as they were written first, close to the ActressMAS v3.1 `E30` examples.

| File | Executable | C# example | What it shows |
|---|---|---|---|
| `PingPong.cpp` | `PingPong` | E30/PingPong | Message throughput between ten agents, five trials. |
| `Skynet.cpp` | `Skynet` | E30/Skynet | A tree of 10 000 agents that sum their identifiers up to the root (expected sum 49 995 000). |

For measurements, use the programs of [`tests/bench/`](../../tests/bench/): they follow the C# programs exactly (a new
environment per trial, same counting, atomic counter), which these two do not.
