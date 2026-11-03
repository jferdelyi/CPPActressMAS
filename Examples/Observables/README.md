# Examples: observables

Ports of the ActressMAS v3.1 `E12` examples: agents publish observable properties, and each agent sees the ones that
pass its perception filter before acting (`see`, `perception_filter`).

| File | Executable | C# example | What it shows | Port |
|---|---|---|---|---|
| `ColorGame.cpp` | `ColorGame` | E12/ColorGame | Each agent sees the agents of the color it looks for, says what it saw, then draws a new color. | complete, same behaviour as the C#: test `iso.ColorGame` |
| `NumberGame.cpp` | `NumberGame` | E12/NumberGame | Numeric observables, written with two decimals: an agent sees the neighbours whose value is within 10 of its own. | complete, same behaviour as the C#: test `iso.NumberGame` |

Both run 10 sequential turns in a fixed order and draw their colors or numbers from the seeds of their C# oracles
(`cam::dotnet::next_site_seed`, `tests/iso/DotNet.hpp`): `CAM_SEED` in the environment changes them, as for the
oracle. The texts are those of the C#, typos included ("I did't see"), so that the outputs can be compared.
