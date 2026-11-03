#!/usr/bin/env bash
# Coverage of the library by the tests: merges the profiles that the coverage preset writes (one per process, test
# preset coverage), then reports the lines, functions, regions and branches of CPPActressMAS/ that the tests ran.
#
# Usage: tests/tools/coverage.sh [--functions] [build folder]
#   build folder  build/$CAM_BUILD_PREFIX coverage by default (cmake --workflow --preset coverage fills it)
#   --functions   lists the functions of the library and their coverage, instead of one line per file
# Clang only (source-based coverage): llvm-profdata and llvm-cov through xcrun on macOS, from the PATH elsewhere
# (LLVM_PROFDATA and LLVM_COV override them). The network tests skip without their broker: CAM_MQTT_HOST and
# CAM_MQTT_PORT in the environment of ctest cover the MQTT transport too.
set -euo pipefail

readonly s_root="$(cd "$(dirname "$0")/../.." && pwd)"
l_functions=0
l_build="${s_root}/build/${CAM_BUILD_PREFIX:-}coverage"
while [ $# -gt 0 ]; do
	case "$1" in
		--functions) l_functions=1 ;;
		-h|--help) sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
		*) l_build="$(cd "$1" && pwd)" ;;
	esac
	shift
done

if [ "$(uname)" = Darwin ]; then
	l_profdata="${LLVM_PROFDATA:-xcrun llvm-profdata}"
	l_cov="${LLVM_COV:-xcrun llvm-cov}"
else
	l_profdata="${LLVM_PROFDATA:-llvm-profdata}"
	l_cov="${LLVM_COV:-llvm-cov}"
fi

readonly l_profiles="${l_build}/profiles"
if ! compgen -G "${l_profiles}/*.profraw" > /dev/null; then
	echo "no profile in ${l_profiles}: run cmake --workflow --preset coverage first" >&2
	exit 1
fi
${l_profdata} merge -sparse "${l_profiles}"/*.profraw -o "${l_build}/coverage.profdata"

# Every program and library of the build but the dependencies: each holds the coverage mapping of the code it links
l_objects=()
while IFS= read -r l_object; do
	l_objects+=("${l_object}")
done < <(find "${l_build}" -type f ! -path '*/CMakeFiles/*' ! -path '*/_deps/*' \
	\( \( -perm -u+x ! -name '*.*' \) -o -name '*.dylib' -o -name '*.so' -o -name '*.so.*' \) | sort)
l_arguments=("${l_objects[0]}")
for l_object in "${l_objects[@]:1}"; do
	l_arguments+=("-object=${l_object}")
done

# The library only: neither the tests, the examples, the dependencies nor the system
l_sources=()
while IFS= read -r l_source; do
	l_sources+=("${s_root}/${l_source}")
done < <(cd "${s_root}" && git ls-files 'CPPActressMAS/*.cpp' 'CPPActressMAS/*.h' 'CPPActressMAS/*.hpp')
if [ "${l_functions}" = 1 ]; then
	${l_cov} report "${l_arguments[@]}" -instr-profile="${l_build}/coverage.profdata" -show-functions "${l_sources[@]}"
else
	${l_cov} report "${l_arguments[@]}" -instr-profile="${l_build}/coverage.profdata" "${l_sources[@]}"
fi
