# Examples: reactive agents

Port of the ActressMAS v3.1 `E01 Reactive` example.

| File | Executable | C# example | What it shows | Port |
|---|---|---|---|---|
| `Reactive.cpp` | `Reactive` | E01 Reactive | Reactive explorers move on a grid, pick up samples and bring them back to a base. | partial: 100 turns without delay instead of an unbounded run with a delay, no map display, an unloaded sample is removed (in C# it stays at the base and can be picked up again), different output |
