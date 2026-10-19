# Tests of CPPActressMAS

## Windows

Windows has not been tested by hand yet. Prerequisites:
- Visual Studio 2022 (MSVC 19.3x, C++20) and CMake 3.22 or later.
- For the MQTT transport: vcpkg, with `openssl` and `pthreads` (libmosquitto needs pthreads4w for its threads), and
  access to GitHub at the first configuration, which downloads libmosquitto (about 2 MB;
  `-DFETCHCONTENT_SOURCE_DIR_LIBMOSQUITTO=<folder>` points to a local copy).
- For the Zenoh transport: the zenoh-c and zenoh-cpp 1.7.1 release archives
  (`zenoh-c-1.7.1-x86_64-pc-windows-msvc-standalone.zip`, `zenoh-cpp-1.7.1-standalone.zip`), unpacked into one prefix.

The third-party libraries are git submodules (`ThirdParty/`). If a clone fails on a long path, run
`git config --global core.longpaths true` and clone again.

```powershell
git submodule update --init --depth 1

# Core only, without any dependency (start here)
cmake -S . -B build\msvc-core -DBUILD_EXAMPLES=ON
cmake --build build\msvc-core --config Debug
ctest --test-dir build\msvc-core -C Debug --output-on-failure

# With the MQTT transport
vcpkg install openssl pthreads --triplet x64-windows
cmake -S . -B build\msvc -DBUILD_EXAMPLES=ON -DCAM_TRANSPORT_MQTT=ON ^
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build\msvc --config Debug
ctest --test-dir build\msvc -C Debug --output-on-failure

# With the Zenoh transport, add to the configuration:
#   -DCAM_TRANSPORT_ZENOH=ON -Dzenohc_DIR=<prefix>\lib\cmake\zenohc -Dzenohcxx_DIR=<prefix>\lib\cmake\zenohcxx
```

Notes:
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
