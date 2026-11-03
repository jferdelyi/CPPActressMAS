# Examples: Game of Life

Port of the ActressMAS v3.1 example `E13 Game of Life`.

| File | Executable | C# example | What it shows | Port |
|---|---|---|---|---|
| `GameOfLife.cpp` | `GameOfLife` | E13 Game of Life | One agent per cell of a 25 x 25 grid sees the observable state of its 8 neighbours through its perception filter; a grid agent broadcasts "evolve" then "update", turn after turn, and shows the grid. 626 agents in a fixed sequential order, observables, `perception_filter`, `see`, `broadcast`, the size in the global data of the environment. | complete, same behaviour as the C#: test `iso.GameOfLife` |
| `patterns.txt` | | E13 Game of Life | The initial pattern (`x` living, `.` dead, separated by tabs), read from the working directory; CMake copies it next to the executable. | copy of the original |

The C# draws the grid in a form. This port prints instead, each time the form would be refreshed, `evolve` or `update`
followed by the grid the form would show: one line per row, `x` for a living cell, `.` for a dead one. As in the C#,
the grid copies the states of the cells during the turn where they update, before they do, so what it shows lags one
generation behind.

Arguments: `GameOfLife [turns] [delay]`, the number of turns (0, the default, for no end, as the C#) and the delay
after each turn in milliseconds (10 by default, as the C#). Without `patterns.txt` in the working directory, or with a
line of less than 25 cells, the program prints an error and exits with code 1, where the C# stops on an unhandled
exception.

`patterns.txt` comes unchanged from ActressMAS v3.1 (`Examples/E13 Game of Life/bin/Release/patterns.txt`),
https://github.com/florinleon/ActressMas, by Florin Leon, under the Apache License 2.0.
