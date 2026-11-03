# Tests of the examples

The programs of `Examples/`, when they are built (`BUILD_EXAMPLES=ON`; label `examples`, tests `examples.<name>`). They
check that an example runs and shows what it is meant to show; the behaviour of the C# is checked by `iso/`.

- **Approval** (`approve.cpp`, `expected/`): `cam_approve` runs a deterministic example, normalises its output (UUIDs
  and agent identifiers, durations, an optional sort of the lines) and compares it with `expected/<example>.txt`,
  which pins the current C++ behaviour. `--update` writes the expected file again.
- **Corrected behaviour**: an example whose C# original has a bug corrects it; on the scenarios of the C# it matches
  the same oracles as `iso/`, and where the bug is reached its output is checked alone.
- **Features of the C++ port** (`Examples/Features`): the examples without a C# original run and report what they
  measure.

``` bash
ctest --preset debug-examples
```
