# Models of Antelmi et al.

The two ActressMAS models of Antelmi et al., "Experimenting with Agent-Based Model Simulation Tools", *Applied Sciences*
13(1), 13, 2023, that run one agent per animal and so measure the framework: Wolf, Sheep and Grass (`Wsg`) and
`Flockers`. The article measures them from 1 000 to 128 000 agents at a constant density of about 10 % (its Table 8:
fields of 100 x 100 to 1131 x 1131, 200 steps), on ActressMAS v3.0 and .NET Framework 4.7.2. These projects run them on
ActressMAS v3.1 (`ActressMas.Ref`), against their C++ ports `tests/bench/Wsg.cpp` and `tests/bench/Flockers.cpp`.

The sources come from https://github.com/isislab-unisa/ABM_Comparison (folder `ActressMAS/`, commit `eaf5dcd`), under
the Apache License 2.0 (its `LICENSE` file); the files of `PredatorPreyConsole` keep the GNU General Public License
header of the ActressMAS example they were adapted from ("Copyright (c) 2018, Florin Leon"). `csharp-ref.sh` clones the
repository into `WORKDIR/ABM_Comparison` and only reads it: nothing of it is copied into this repository, the projects
compile the original files (`Abm.props`).

## Changes

Only what keeps the published code from compiling or running on ActressMAS v3.1 and .NET 10, or from taking its size
on the command line:

| Project | Change | Why |
|---|---|---|
| both | `Program.cs` replaced by `Bench.cs`, the same `Main` with the size from `--agents N` (field side `floor(sqrt(10 N))`, Table 8) and the elapsed time printed in ms | `Settings.cs` holds one size: 8 000 agents on 282 x 282 for `Wsg`, 10 million on 10 000 x 10 000 for `Flockers` |
| `Wsg` | the 200 turns run through `Start()`, and the grass grows in `TurnFinished`, after the populations are written (`BenchEnvironment`) | the original calls `RunTurn(turn)` then `UpdateGrass()` from `Main`; `RunTurn` is private in v3.1. `Start()` also ends when no agent is left, where the original would run its remaining turns empty |
| `Flockers` | `AntAgent`, in `Bench.cs`, is the `FlockAgent` of `FlockAgent.cs` under the name that `Program.cs` creates and `World.cs` recognizes | the published agent class was renamed without its users; the original project lists `FlocksAgent.cs`, which does not exist |
| `Flockers` | `GlobalUsings.cs` imports the namespaces `Flockers` and `PredatorPrey` | the agent classes and the world are in two namespaces that do not import each other |
| `Flockers` | `Stubs/WinForms.cs`: empty `System.Windows.Forms` types | `InsectAgent.cs` imports two of them with `using static` and uses none; Windows Forms does not exist on Linux |
| `Flockers` | `InsectAgent.cs.sed`: the neighbourhood is bounded by `Settings.GridSize` instead of 1131 | written by hand for the largest field ("works for any matrix as long as you change the lenght"): an index off any smaller field |

Kept as published, here and in the C++ ports: in `Flockers`, the consistency divided twice by the count and the
avoidance divided by an integer; in `Wsg`, a sheep that dies still eats and breeds in the same action. The generators
are not seeded (`new Random()`), so two runs differ: only the C++ ports take a seed (`--seed`).
