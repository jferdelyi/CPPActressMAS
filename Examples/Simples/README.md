# Examples: first steps

Ports of the ActressMAS v3.1 `E00` examples.

| File | Executable | C# example | What it shows | Port |
|---|---|---|---|---|
| `Exemple1.cpp` | `Exemple1` | E00/Agents1 | Two agents send ten messages each to a monitor during their setup, with random pauses. | complete, same behaviour as the C#: test `iso.Agents1` (parallel: invariants) |
| `Exemple2.cpp` | `Exemple2` | E00/Agents2 | Two agents exchanging messages between `setup` and `action`, in a random order at each turn. | complete, same behaviour as the C#: test `iso.Agents2` (exact trace) |
| `Exemple3.cpp` | `Exemple3` | E00/Agents3 | A monitor that runs three start/done rounds with four agents; the environment reports each turn. | complete, same behaviour as the C#: test `iso.Agents3` (parallel: invariants); the monitor addresses the agents in the order of insertion, the C# in the order of its dictionary |
| `Exemple4.cpp` | `Exemple4` | E00/Agents4 | Five agents write three messages per turn to a writer, then stop. | complete, same behaviour as the C# in `legacy`: tests `iso.Agents4`, `iso.Agents4.fixed`, `example.Exemple4.fixed` |
| `Exemple5.cpp` | `Exemple5` | E00/Agents5 | Long setups, then actions; an agent stops as soon as its mailbox is empty. | complete, same behaviour as the C#: test `iso.Agents5` (parallel: invariants) |
| `MultipleMessages.cpp` | `MultipleMessages` | E00/MultipleMessages | A manager and five workers with crossed requests to workers drawn at random. | complete, same behaviour as the C# in `legacy`: tests `iso.MultipleMessages`, `example.MultipleMessages.fixed` |
| `SendingObjects.cpp` | `SendingObjects` | E00/SendingObjects | Objects carried by messages: sent as JSON objects, read back with `as_json`. | complete, same behaviour as the C#: test `iso.SendingObjects` (parallel: invariants) |

The ports print the texts of the C# (`Agent::format` for the messages) and use the names of its agents. The parallel
examples write each line in one piece (`cam::dotnet::write_line`, `Examples/Common/DotNet.hpp`), as `Console.WriteLine`
does, so that the lines of two agents never mix. Random draws (pauses of `Exemple1`, order of `Exemple2`) come from the
seeds of the C# oracles (`CAM_SEED` in the environment, 0 by default).

**WARNING: two of these examples keep a bug of the C# by default.** Their argument chooses: `legacy` (the default)
behaves as the C#, `fixed` corrects the bug.

| Example | Bug of the C# | `legacy` | `fixed` |
|---|---|---|---|
| `Exemple4` | an agent stops at its fourth action, then still sends three messages | the late messages reach the writer if it has not acted yet in the last turn (45 to 60 lines) | the agent stops without sending (45 lines) |
| `MultipleMessages` | the reply is handled under the label `request_reply`, which no message carries | a single exchange: the replies are never handled | the worker tells the manager ("replying") and sends a new request, until the last turn |
