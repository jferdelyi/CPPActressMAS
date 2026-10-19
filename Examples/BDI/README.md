# Examples: BDI architecture

Port of the ActressMAS v3.1 example `E02 BDI Architecture`.

| File | Executable | C# example | What it shows | Port |
|---|---|---|---|---|
| `BDI.cpp` | `BDI` | E02 BDI Architecture | A patrol walks along a terrain of 15 cells and puts out the fires that start on it, with water taken from the left end. The patrol is a BDI agent: beliefs revised from its percepts, desires, one intention, and a plan sent to the terrain one action at a time. Two agents in a fixed sequential order, the size of the terrain in the global data of the environment. | complete, same behaviour as the C# (`legacy`): tests `iso.BDI` and `iso.BDIAdjacent`; `fixed`: `iso.BDI.fixed`, `example.BDI.fixed_adjacent` |

The C# draws the terrain in a form. This port prints instead, each time the form would be refreshed, the line
`GUI pos=<patrol position> <cells>`: `.` normal, `F` fire, `W` water, `w` water being drawn.

Arguments: `BDI [turns] [delay] [legacy|fixed] [shipped|adjacent]`:

- `turns`: the number of turns, 0 (the default) for no end, as the C#;
- `delay`: the delay after each turn in milliseconds, 250 by default, as the C#;
- `legacy` (the default) or `fixed`: see below;
- `shipped` (the default), the fires of the C#, or `adjacent`, where the first fire starts on two cells side by side
  (3 and 4 instead of 4).

**WARNING: the C# has three design bugs, kept by default.** `legacy` reproduces them, as the C# behaves; `fixed`
corrects them. On the shipped fires both modes print the same output, the one of the C#: none of the bugs is reached.

| Bug of the C# | `legacy` (as the C#) | `fixed` |
|---|---|---|
| A plan ends while a fire is still in sight: the patrol keeps its intention, does not plan again, and reads its empty plan | the exception is printed and the patrol stops for good: nothing moves any more (reached with `adjacent`) | the patrol drops its intention and deliberates again: it fetches water and puts out the other fire |
| `drop-water` on a cell without fire | the terrain answers nothing, the patrol waits forever | the terrain answers with the percepts, as for `get-water` |
| A patrol plan is one move too long | the patrol would leave the terrain, and the next access throws | a move into an end of the terrain leaves the patrol where it is |

The last two cannot be reached by this patrol, whatever the fires: they only protect the terrain from another sender.
In `legacy`, the program also prints a warning on the error output.
