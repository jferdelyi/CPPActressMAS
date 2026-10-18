# CPPActressMAS: third-party libraries

Git submodules, each pinned on a release tag of its upstream repository. After cloning the project, run `git submodule update --init --depth 1`: it fetches only the pinned commits (about 20 MB). `git clone --recursive` also works but downloads every branch of nlohmann/json (about 180 MB), even with `shallow = true` in `.gitmodules`. The CMake configuration stops with that hint while a submodule is missing.

| Folder | Library | Version | License | Used by |
|---|---|---|---|---|
| `json/` | [nlohmann/json](https://github.com/nlohmann/json) | 3.12.0 | MIT (`json/LICENSE.MIT`) | the library: JSON message contents, agent state, JSON envelope codec (`single_include/nlohmann/json.hpp`) |
| `ordered-map/` | [Tessil/ordered-map](https://github.com/Tessil/ordered-map) | 1.2.0 | MIT (`ordered-map/LICENSE`) | the library: agents of an environment in insertion order (`include/tsl/ordered_map.h`) |
| `doctest/` | [doctest/doctest](https://github.com/doctest/doctest) | 2.5.3 | MIT (`doctest/LICENSE.txt`) | the tests only (`doctest/doctest.h`) |

To move a library to another release: `git fetch --depth 1 origin tag <tag>` and `git checkout <tag>` inside its folder, then commit the new submodule commit in the project.
