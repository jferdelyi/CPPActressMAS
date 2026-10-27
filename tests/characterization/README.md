# Characterization tests

The known defects of the library, and the tests of the fixed ones, kept as regression tests (label
`characterization`). Every defect found and fixed keeps its test here or next to the code it concerns.

**Value defects** (`known_defects.cpp`, test `characterization.known_defects`): one doctest case per defect, which
asserts the behaviour of ActressMAS C# v3.1.
- An open defect is marked `expected_failures(n)`, the number of its assertions that the defect fails: the case passes
  only with exactly that many failures. Another failure (a crash, an exception, one more wrong value) fails it, and so
  does the fix; the marker is then removed and the case is renamed `fixed: ...`. The assertions never change.
- `fixed: ...` cases are regression tests; `witness: ...` cases show behaviour that already matches the C#.

**Crash and race probes** (`probes.cpp`, tests `characterization.ok.<probe>` and `characterization.<sanitizer>.<probe>`):
one process per probe (`cam_probes <probe>`; `cam_probes` alone lists them with their status).
- A probe of an open defect passes when its sanitizer reports the defect (`PASS_REGULAR_EXPRESSION`, never
  `WILL_FAIL`), so a probe that stops crashing fails and signals the fix. It is registered in the build of its
  sanitizer only (`asan` or `tsan` preset).
- A probe of a fixed defect must run to completion without any sanitizer report (`characterization.ok.<probe>`), in
  every build.
- The probes run without the TSan suppressions of `support/`: they must see the races they document.

``` bash
ctest --preset debug-characterization
tests/run.sh tsan characterization
```
