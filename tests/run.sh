#!/usr/bin/env bash
# Runs the tests of CPPActressMAS by hand, on macOS or Linux: configures and builds a CMake preset, then runs the tests
# of the types given, or all of them.
#
# Usage: tests/run.sh [--linux [--compiler gcc|clang]] [preset] [type...]
#   preset    debug (default), release, core, asan, tsan, coverage or bench (CMakePresets.json)
#   type      unit, integration, network, characterization, iso, examples or perf (the CTest labels, tests/README.md)
#   --linux   runs the same in a Linux container of the Podman machine: connection CAM_PODMAN_CONNECTION (cam by
#             default), image CAM_LINUX_IMAGE (localhost/cam-linux-dev:24.04-zenoh by default), build folder
#             build/linux-<compiler>-<preset>
# The network tests skip without their broker: CAM_MQTT_HOST and CAM_MQTT_PORT give it.
#
# Examples: tests/run.sh                  every test of the debug preset
#           tests/run.sh asan unit iso    the unit and oracle tests under AddressSanitizer
#           tests/run.sh --linux tsan     every test under ThreadSanitizer, in the Linux container
set -euo pipefail

readonly s_root="$(cd "$(dirname "$0")/.." && pwd)"
readonly s_types="unit integration network characterization iso examples perf"

l_linux=0
l_compiler=gcc
l_arguments=()
while [ $# -gt 0 ]; do
	case "$1" in
		--linux) l_linux=1 ;;
		--compiler) l_compiler="${2:?--compiler needs gcc or clang}"; shift ;;
		-h|--help) sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
		-*) echo "unknown option: $1" >&2; exit 2 ;;
		*) l_arguments+=("$1") ;;
	esac
	shift
done

l_preset=debug
l_selected=()
if [ ${#l_arguments[@]} -gt 0 ]; then
	l_preset="${l_arguments[0]}"
	l_selected=("${l_arguments[@]:1}")
fi
for l_type in ${l_selected[@]+"${l_selected[@]}"}; do
	case " $s_types " in
		*" $l_type "*) ;;
		*) echo "unknown type: $l_type (types: $s_types)" >&2; exit 2 ;;
	esac
done

if [ "$l_linux" = 1 ]; then
	case "$l_compiler" in
		gcc) l_cc=gcc; l_cxx=g++ ;;
		clang) l_cc=clang; l_cxx=clang++ ;;
		*) echo "unknown compiler: $l_compiler (gcc or clang)" >&2; exit 2 ;;
	esac
	exec podman --connection "${CAM_PODMAN_CONNECTION:-cam}" run --rm --pull=never --network=host \
		-e CC="$l_cc" -e CXX="$l_cxx" -e CAM_BUILD_PREFIX="linux-$l_compiler-" -e CAM_MQTT_HOST -e CAM_MQTT_PORT \
		-v "$s_root:/src" -w /src "${CAM_LINUX_IMAGE:-localhost/cam-linux-dev:24.04-zenoh}" \
		bash tests/run.sh "$l_preset" ${l_selected[@]+"${l_selected[@]}"}
fi

cd "$s_root"
cmake --preset "$l_preset"
cmake --build --preset "$l_preset"
if [ ${#l_selected[@]} -eq 0 ]; then
	ctest --preset "$l_preset"
else
	l_labels="$(IFS='|'; echo "${l_selected[*]}")"
	ctest --preset "$l_preset" -L "^($l_labels)\$"
fi
