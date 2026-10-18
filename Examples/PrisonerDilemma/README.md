# Examples — iterated prisoner's dilemma

Port of the ActressMAS v3.1 example `E10 Iterated Prisoner Dilemma`.

| File | Executable | C# example | What it shows | Port |
|---|---|---|---|---|
| `PrisonerDilemma.cpp` | `PrisonerDilemma` | E10 Iterated Prisoner Dilemma | Two tit-for-tat prisoners play ten rounds against the police; an environment that overrides `turn_finished` announces the rounds and sends the end of the game from outside (`send_content("env", ...)`). The other strategies of the C# (always confess, random) are ported and commented out in `main()`, as in the original. | complete, same behaviour as the C#: test `iso.PrisonerDilemma` |
