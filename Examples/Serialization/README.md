# Examples — agent state

No C# counterpart: this example shows the C++ API used by migration.

| File | Executable | What it shows |
|---|---|---|
| `Serialization.cpp` | `Serialization` | An agent saves its state (`save_state`), sends it in a message, and the receiver builds a copy from that state (`load_state`). The copy is a new agent: it gets a new id and runs `setup` again. |

Its output is checked by the approval test `approval.Serialization`.
