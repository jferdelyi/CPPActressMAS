# Data files of the C# examples

Files that a ported example reads from its working directory, copied unchanged from the original ActressMAS
repository (https://github.com/florinleon/ActressMas, commit `eb0bb73`, by Florin Leon). The harness copies them next
to the port at build time (`tests/iso/CMakeLists.txt`).

| File | Origin | Read by |
|---|---|---|
| `Pendulum.grx` | `ActressMAS v3.1/Source Code and Examples/Examples/E03 LRTA Search/bin/Release/Pendulum.grx` | `iso_LrtaStar`: the graph of the map |
| `Pendulum.grh` | `ActressMAS v3.1/Source Code and Examples/Examples/E03 LRTA Search/bin/Release/Pendulum.grh` | `iso_LrtaStar`: the heuristic of each node |

The ActressMAS repository is distributed under the Apache License 2.0 (its `LICENSE` file); the map files carry no
notice of their own. The C# sources of the example carry a GNU General Public License header, "Copyright (c) 2018,
Florin Leon".
