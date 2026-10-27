# Tools of the tests

Not a type of test: the tools that produce what the tests compare with.

| Folder | Role |
|---|---|
| [`csharp-ref/`](csharp-ref/README.md) | Builds and runs ActressMAS v3.1 (C#) in a .NET container: the oracles of `iso/`, the vectors of `unit/vectors/rng` and `iso/vectors`, and the C# side of the benchmarks. The C++ tests never run .NET: what it produces is versioned. |
| `coverage.sh` | Merges the profiles of the `coverage` preset and reports the coverage of the library by the tests (`cmake --workflow --preset coverage`, then `tests/tools/coverage.sh`; `--functions` for one line per function). |
| `iso-mutate.py` | Measures how strong the rules of `iso/` are: the outputs of a test run with `CAM_ISO_SAVE=<folder>` are altered line by line, and each alteration that still passes is reported (a blind spot of a rule, or a legitimate variation of a parallel example). A few minutes for the whole collection: run by hand when a rule is written or changed (`../iso/README.md`). |
