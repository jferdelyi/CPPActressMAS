# Examples — observables

Ports of the ActressMAS v3.1 `E12` examples: agents publish observable properties, and each agent sees the ones that
pass its perception filter before acting (`see`, `perception_filter`).

| File | Executable | C# example | What it shows | Port |
|---|---|---|---|---|
| `ColorGame.cpp` | `ColorGame` | E12/ColorGame | Each agent sees a color around it, then changes its own. | complete |
| `NumberGame.cpp` | `NumberGame` | E12/NumberGame | Numeric observables: an agent sees the neighbours whose value is within 10 of its own. | complete |
