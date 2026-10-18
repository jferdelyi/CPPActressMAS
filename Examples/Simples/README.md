# Examples — first steps

Ports of the ActressMAS v3.1 `E00` examples.

| File | Executable | C# example | What it shows | Port |
|---|---|---|---|---|
| `Exemple1.cpp` | `Exemple1` | E00/Agents1 | Two agents send ten messages each to a monitor during their setup, with random pauses. | complete |
| `Exemple2.cpp` | `Exemple2` | E00/Agents2 | Two agents exchanging messages between `setup` and `action`. | complete |
| `Exemple3.cpp` | `Exemple3` | E00/Agents3 | A monitor that runs three start/done rounds with four agents. | complete |
| `Exemple4.cpp` | `Exemple4` | E00/Agents4 | Five agents write three messages per turn to a writer, then stop. | partial: an agent stops at its fourth action without sending, where the C# still sends three messages after `Stop()` |
| `Exemple5.cpp` | `Exemple5` | E00/Agents5 | Long setups, then actions; an agent stops as soon as its mailbox is empty. | complete |
| `MultipleMessages.cpp` | `MultipleMessages` | E00/MultipleMessages | A manager and five workers with crossed requests. | partial: a chain of replies was added |
| `SendingObjects.cpp` | `SendingObjects` | E00/SendingObjects | Objects carried by messages: serialized and sent as bytes, read back with `as_bytes`. | complete |
