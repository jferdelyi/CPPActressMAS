# Tools of the tests

Not a type of test: the tools that produce what the tests compare with.

| Folder | Role |
|---|---|
| [`csharp-ref/`](csharp-ref/README.md) | Builds and runs ActressMAS v3.1 (C#) in a .NET container: the oracles of `iso/`, the vectors of `unit/vectors/rng` and `iso/vectors`, and the C# side of the benchmarks. The C++ tests never run .NET: what it produces is versioned. |
