# Tests of CPPActressMAS

One folder per type of test, one CTest label per type; every test is named `<type>.<name>`.

| Folder | Label | What it checks |
|---|---|---|
| [`unit/`](unit/README.md) | `unit` | The pieces of the library alone |
| [`integration/`](integration/README.md) | `integration` | Several environments of one process, through the loopback transport: messages, directory, migration |
| [`network/`](network/README.md) | `network`, plus `mqtt` or `zenoh` | The network transports; skipped without them or without their broker |
| [`characterization/`](characterization/README.md) | `characterization` | The known defects, and the tests of the fixed ones, kept as regression tests |
| [`iso/`](iso/README.md) | `iso` | The behaviour of ActressMAS v3.1 (C#): its examples ported as they are, compared with the output of the C# originals |
| [`examples/`](examples/README.md) | `examples` | The programs of `Examples/`, when they are built |
| [`bench/`](bench/README.md) | `perf` | The benchmarks, run once (`bench` preset) |

[`tools/`](tools/README.md) produces what the tests compare with (the C# oracles and vectors); [`support/`](support/README.md)
holds what the types share.

**Rules of the suite.**
- A behaviour comes with its test, in the same change.
- Every problem found and fixed keeps a test that reproduces it, as a regression test.
- No assertion inside an agent: agents record, the test checks after `start()`.
- A build has no warning (`-Wall -Wextra -Wpedantic`), and no test leaves a sanitizer report.

## Running the tests by hand

The presets need CMake 3.25 or later.

| Command | What it does |
|---|---|
| `cmake --workflow --preset debug` | Configures, builds and runs every test of a preset: `debug`, `release`, `core` (no network transport), `asan` (AddressSanitizer and UBSan), `tsan` (ThreadSanitizer), `coverage`, `bench`, `msvc` (Windows) |
| `cmake --workflow --preset iso` | The same for one type, on `debug`: `unit`, `integration`, `network`, `characterization`, `iso`, `examples` |
| `ctest --preset asan-iso` | One type on a preset already built: `<preset>-<type>` for `debug`, `release`, `core`, `asan`, `tsan`, `msvc` (no `examples` for `msvc`) |
| `tests/run.sh [preset] [type...]` | macOS and Linux: configures, builds and runs the types given (all of them by default) |
| `tests/run.sh --linux [--compiler gcc\|clang] [preset] [type...]` | The same in a Linux container of the Podman machine, in `build/linux-<compiler>-<preset>` |
| `tests\run.ps1 [-Configuration Debug] [type...]` | Windows (MSVC, `msvc` preset) |

The build folders are `build/<preset>`; `CAM_BUILD_PREFIX` in the environment puts a prefix before the preset name
(`tests/run.sh --linux` uses it).

Environment variables read by the tests:
- `CAM_MQTT_HOST`, `CAM_MQTT_PORT`, `CAM_MQTT_BROKER_CONTROL`: the MQTT broker of `network.mqtt`.
- `CAM_SEED`: the base of the seeds of the oracle tests (0 by default, as their oracles; see `iso/README.md`).
- `CAM_WRITE_VECTORS=1`: writes the binary vectors of the envelope codecs again (`unit/vectors/`).

## Platforms

- **macOS** (Apple clang): every preset.
- **Linux** (gcc and clang): every preset; `tests/run.sh --linux` runs them in a container.
- **Windows** (MSVC): the `msvc` preset, core only (no network transport).

## Continuous integration

`.github/workflows/tests.yml` runs at every push and pull request, without the network transports (the runners have
neither a broker nor Zenoh, so the network tests skip):

| Job | Configuration | Tests |
|---|---|---|
| macOS | Apple clang, `core` preset (Debug) | every type |
| Linux | gcc and clang, Debug and Release, warnings as errors | every type |
| Linux sanitizers | gcc, `asan` and `tsan` presets | every type |
| Windows | MSVC, `msvc` preset (Debug) | `unit` and `iso` |

## Windows

Windows has not been tested by hand yet. Prerequisites:
- Visual Studio 2022 (MSVC 19.3x, C++20) and CMake 3.22 or later (3.25 for the presets and `tests\run.ps1`).
- For the MQTT transport: vcpkg, with `openssl` and `pthreads` (libmosquitto needs pthreads4w for its threads), and
  access to GitHub at the first configuration, which downloads libmosquitto (about 2 MB;
  `-DFETCHCONTENT_SOURCE_DIR_LIBMOSQUITTO=<folder>` points to a local copy).
- For the Zenoh transport: the zenoh-c and zenoh-cpp 1.7.1 release archives
  (`zenoh-c-1.7.1-x86_64-pc-windows-msvc-standalone.zip`, `zenoh-cpp-1.7.1-standalone.zip`), unpacked into one prefix.

The third-party libraries are git submodules (`ThirdParty/`). If a clone fails on a long path, run
`git config --global core.longpaths true` and clone again.

```powershell
git submodule update --init --depth 1

# Static library and tests, core only, Debug (the msvc preset; start here)
tests\run.ps1

# With the MQTT transport
vcpkg install openssl pthreads --triplet x64-windows
cmake -S . -B build\msvc -DBUILD_SHARED=OFF -DCAM_TRANSPORT_MQTT=ON ^
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build\msvc --config Debug
ctest --test-dir build\msvc -C Debug --output-on-failure

# With the Zenoh transport, add to the configuration:
#   -DCAM_TRANSPORT_ZENOH=ON -Dzenohc_DIR=<prefix>\lib\cmake\zenohc -Dzenohcxx_DIR=<prefix>\lib\cmake\zenohcxx
```

Notes:
- The shared library exports no symbol yet, and the static one has the same file name as its import library: with
  MSVC, build the static library only (`-DBUILD_SHARED=OFF`, as the `msvc` preset does). The examples link to the
  shared library, so they are left out (`BUILD_EXAMPLES=OFF`).
- The Visual Studio generator is multi-configuration: `CMAKE_BUILD_TYPE` is empty, so give `--config` to the build
  and `-C` to CTest. The `asan`, `tsan`, `coverage` and `bench` presets pass GCC and Clang options: they do not apply
  to MSVC.
- The crash probes of `characterization/` have a verdict under a sanitizer only; without one, only the probes that
  must run to completion and the value cases of `known_defects.cpp` run.
- `network.zenoh` opens TCP sockets on 127.0.0.1 (ports 17447 to 17454), no multicast: the Windows firewall may ask
  for permission at the first run; `-LE network` keeps a run off the network. Zenoh is linked dynamically on Windows:
  `zenohc.dll` must be next to the executable or in the `PATH`.
- `network.mqtt` skips (exit code 77) without `CAM_MQTT_HOST` and `CAM_MQTT_PORT`. Its reconnection case calls the
  script named by `CAM_MQTT_BROKER_CONTROL` with `stop` and `start`: give a `.cmd` that does so, or leave the variable
  empty to skip that case.
