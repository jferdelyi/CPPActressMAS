# Support of the tests

What the types of tests share; no test here.

| File | Role |
|---|---|
| `doctest_main.cpp` | The `main` of every doctest program (`cam_doctest_main`) |
| `Recorder.hpp` | A thread-safe recorder: agents never assert (the checks of doctest are not made for the threads of a Parallel turn, and an exception thrown by an agent stops the simulation), they record what happens and the test checks after `start()` |
| `wait_until.hpp` | Bounded wait for a condition, for the tests that cross threads or the network |
| `tcp_probe.hpp` | Bounded TCP connection attempt: the network tests skip when their broker does not answer |
| `ActressMasRelay.hpp` | A relay with the rules of the ContainerServer of ActressMAS v3.1 (C#), on 127.0.0.1: the ActressMAS transport is tested against it in the process of the test, without .NET. It keeps the order of the messages of a container, unless asked to swap them as the thread pool of the C# server may do |
| `actressmas_relay.cpp` | That relay as a program (`cam_actressmas_relay [--swap <ms>] -- <command>`, `{port}` replaced by its port): the iso tests of RemoteMessages run the port over it, in order and swapped |
| `run_checked.cmake` | Runs a command (`cmake -P run_checked.cmake -- <command>`, `CAM_RUN_CHECKED`) and prints `cam: exit code <code>` when it exits with an error: CTest ignores the exit code of a test that passes on a regular expression, and such a test fails on that line |
| `tsan_suppressions.txt` | ThreadSanitizer suppressions of the races known inside the dependencies (zenoh-c, zenoh-cpp, libmosquitto), applied to the ordinary tests of a `tsan` build, never to the probes |
