# CPPActressMAS: third-party libraries

Git submodules, each pinned on a release tag of its upstream repository and cloned shallow (`shallow = true` in `.gitmodules`). Clone the project with `git clone --recursive`, or run `git submodule update --init` in an existing clone; the CMake configuration stops with that hint while one is missing.

| Folder | Library | Version | License | Used by |
|---|---|---|---|---|
| `json/` | [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 | MIT (`json/LICENSE.MIT`) | the library: JSON message contents, agent state, JSON envelope codec (`single_include/nlohmann/json.hpp`) |
| `ordered-map/` | [Tessil/ordered-map](https://github.com/Tessil/ordered-map) | 1.2.0 | MIT (`ordered-map/LICENSE`) | the library: agents of an environment in insertion order (`include/tsl/ordered_map.h`) |
| `doctest/` | [doctest/doctest](https://github.com/doctest/doctest) | 2.4.12 | MIT (`doctest/LICENSE.txt`) | the tests only (`doctest/doctest.h`) |

To move a library to another release: `git fetch --depth 1 origin tag <tag>` and `git checkout <tag>` inside its folder, then commit the new submodule commit in the project.
