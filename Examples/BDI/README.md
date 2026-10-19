# Examples: BDI architecture

Port of the ActressMAS v3.1 example `E02 BDI Architecture`.

| File | Executable | C# example | What it shows | Port |
|---|---|---|---|---|
| `BDI.cpp` | `BDI` | E02 BDI Architecture | A patrol walks along a terrain of 15 cells and puts out the fires that start on it, with water taken from the left end. The patrol is a BDI agent: beliefs revised from its percepts, desires, one intention, and a plan sent to the terrain one action at a time. Two agents in a fixed sequential order, the size of the terrain in the global data of the environment. | complete, same behaviour as the C#: test `iso.BDI` |

The C# draws the terrain in a form. This port prints instead, each time the form would be refreshed, the line
`GUI pos=<patrol position> <cells>`: `.` normal, `F` fire, `W` water, `w` water being drawn.

Arguments: `BDI [turns] [delay]`, the number of turns (0, the default, for no end, as the C#) and the delay after each
turn in milliseconds (250 by default, as the C#).

Three defects of the C# that its scenario never triggers are not reproduced, so the output stays the same:

- when a plan ends while a fire is still in sight, the C# reads its empty plan and the patrol stops for good on the
  exception; here the patrol drops its intention and deliberates again;
- the C# terrain answers nothing to `drop-water` on a cell without fire, and the patrol waits forever; here it answers
  with the percepts, as it does for `get-water`;
- a patrol plan is one move too long, which would take the patrol off the terrain; here a move into an end of the
  terrain leaves the patrol where it is.
