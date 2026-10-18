# CPPActressMAS — helpers of the examples

Header-only helpers shared by the examples (included through `Examples/CMakeLists.txt`), not part of the library.

| File | Role |
|---|---|
| `DotNet.hpp` | `cam::dotnet::format_fixed` (as `ToString("F2")`), `cam::dotnet::to_string` (as `Double.ToString()`) and `cam::dotnet::to_int32` (as `Convert.ToInt32`), with the invariant culture: a port of a C# example prints and reads numbers exactly as the original. Checked against `tests/vectors/dotnet/format.txt`, produced by `tools/csharp-ref` (`FormatVectors`). |

The examples are compiled with `-ffp-contract=off` (GCC and Clang): no fused multiply-add, so that their arithmetic
rounds as the C# does.
