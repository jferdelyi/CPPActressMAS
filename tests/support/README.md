# Support of the tests

What the types of tests share; no test here.

| File | Role |
|---|---|
| `doctest_main.cpp` | The `main` of every doctest program (`cam_doctest_main`) |
| `Recorder.hpp` | A thread-safe recorder: agents never assert (an exception thrown by an agent is swallowed in Parallel mode), they record what happens and the test checks after `start()` |
| `wait_until.hpp` | Bounded wait for a condition, for the tests that cross threads or the network |
| `tcp_probe.hpp` | Bounded TCP connection attempt: the network tests skip when their broker does not answer |
| `tsan_suppressions.txt` | ThreadSanitizer suppressions of the races known inside the dependencies (zenoh-c, zenoh-cpp, libmosquitto), applied to the ordinary tests of a `tsan` build, never to the probes |
