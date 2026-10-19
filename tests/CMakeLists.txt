# Tests of CPPActressMAS: one folder per type of test, one CTest label per type.
#
#   unit/              unit              the pieces of the library alone (doctest)
#   integration/       integration       several environments of one process, through the loopback transport
#   network/           network           the network transports (plus mqtt or zenoh), skipped without them
#   characterization/  characterization  known defects, and the probes of the fixed ones kept as regression tests
#   iso/               iso               the behaviour of ActressMAS v3.1 (C#), compared with the output of its examples
#   examples/          examples          the programs of Examples/, when they are built
#   bench/             perf              the benchmarks, run once (BUILD_BENCH, bench preset)
#
# One type runs with ctest -L <label>. tools/ produces the C# references (oracles, vectors); support/ holds what the
# types share.

set(CAM_SANITIZER "" CACHE STRING "Sanitizer this build is instrumented with: asan, tsan or empty (selects the crash probes)")

set(CAM_TEST_LIBRARY CPPActressMAS)
if (NOT TARGET CPPActressMAS)
	set(CAM_TEST_LIBRARY CPPActressMASStatic)
endif()
set(CAM_DOCTEST_HEADER ${CMAKE_SOURCE_DIR}/ThirdParty/doctest/doctest/doctest.h)
if (NOT EXISTS ${CAM_DOCTEST_HEADER})
	message(FATAL_ERROR "ThirdParty/doctest is missing: run git submodule update --init --depth 1")
endif()

# ---------------------------------------------------------------- shared by the types
add_library(cam_test_support INTERFACE)
target_include_directories(cam_test_support INTERFACE ${CMAKE_CURRENT_LIST_DIR}/support ${CMAKE_SOURCE_DIR}/ThirdParty/doctest)

add_library(cam_doctest_main STATIC support/doctest_main.cpp)
target_link_libraries(cam_doctest_main PUBLIC cam_test_support)

# The races known inside the dependencies (support/tsan_suppressions.txt) are suppressed for the ordinary tests; the
# crash probes of characterization/ must see every race.
set(CAM_TSAN_SUPPRESSIONS ${CMAKE_CURRENT_LIST_DIR}/support/tsan_suppressions.txt)
function(cam_apply_tsan_suppressions p_test)
	if (CAM_SANITIZER STREQUAL "tsan")
		set_property(TEST ${p_test} APPEND PROPERTY ENVIRONMENT "TSAN_OPTIONS=suppressions=${CAM_TSAN_SUPPRESSIONS}")
	endif()
endfunction()

# A doctest program of the type of its folder (CAM_TEST_TYPE): target p_target built from the other arguments, test
# <type>.<p_name>, label <type>.
function(cam_add_doctest p_name p_target)
	add_executable(${p_target} ${ARGN})
	target_link_libraries(${p_target} cam_doctest_main ${CAM_TEST_LIBRARY})
	add_test(NAME ${CAM_TEST_TYPE}.${p_name} COMMAND ${p_target})
	set_tests_properties(${CAM_TEST_TYPE}.${p_name} PROPERTIES LABELS ${CAM_TEST_TYPE} TIMEOUT 300)
	cam_apply_tsan_suppressions(${CAM_TEST_TYPE}.${p_name})
endfunction()

# ---------------------------------------------------------------- one folder per type
add_subdirectory(unit)
add_subdirectory(integration)
add_subdirectory(network)
add_subdirectory(characterization)
add_subdirectory(iso)
add_subdirectory(examples)
