# Tests of CPPActressMAS.
#
# CTest labels: unit, characterization, iso (reserved), integration, mqtt, zenoh (reserved), perf (bench/).
# ThreadSanitizer suppressions (tests/tsan/suppressions.txt) apply to the ordinary tests only.

set(CAM_SANITIZER "" CACHE STRING "Sanitizer this build is instrumented with: asan, tsan or empty (selects the crash probes)")

set(CAM_TEST_LIBRARY CPPActressMAS)
if (NOT TARGET CPPActressMAS)
	set(CAM_TEST_LIBRARY CPPActressMASStatic)
endif()
set(CAM_DOCTEST_HEADER ${CMAKE_SOURCE_DIR}/Externals/doctest/doctest.h)
set(CAM_TSAN_SUPPRESSIONS ${CMAKE_CURRENT_LIST_DIR}/tsan/suppressions.txt)

# ---------------------------------------------------------------- support
add_library(cam_test_support INTERFACE)
target_include_directories(cam_test_support INTERFACE ${CMAKE_CURRENT_LIST_DIR}/support ${CMAKE_SOURCE_DIR}/Externals)

# Ordinary tests run with the TSan suppressions of the known races (each entry carries its ID).
function(cam_apply_tsan_suppressions p_test)
	if (CAM_SANITIZER STREQUAL "tsan")
		set_property(TEST ${p_test} APPEND PROPERTY ENVIRONMENT "TSAN_OPTIONS=suppressions=${CAM_TSAN_SUPPRESSIONS}")
	endif()
endfunction()

# ---------------------------------------------------------------- approval of deterministic examples
add_executable(cam_approve approval/approve.cpp)
target_link_libraries(cam_approve cam_test_support)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/approval)

# Runs an example, normalises its output (UUIDs, durations, optional line sort) and compares it with
# tests/approval/expected/<example>.txt. The expected files pin the current C++ behaviour, not the C# oracle.
function(cam_add_approval p_example p_sort)
	if (TARGET ${p_example})
		set(l_args)
		if (p_sort)
			list(APPEND l_args --sort)
		endif()
		add_test(NAME approval.${p_example}
			COMMAND cam_approve ${l_args}
				${CMAKE_CURRENT_LIST_DIR}/approval/expected/${p_example}.txt
				${CMAKE_CURRENT_BINARY_DIR}/approval/${p_example}.actual.txt
				$<TARGET_FILE:${p_example}>)
		set_tests_properties(approval.${p_example} PROPERTIES LABELS integration TIMEOUT 120)
		cam_apply_tsan_suppressions(approval.${p_example})
	endif()
endfunction()

# Only Serialization is deterministic today. The other sequential examples seed their generator with
# the clock and the parallel ones (Exemple1, Exemple3, Exemple5, SendingObjects) produce a different
# set of lines from one run to the next, even sorted (checked 5 times in 1A): their approval waits
# for deterministic examples (mission 3, docs/port/BACKLOG.md).
cam_add_approval(Serialization OFF)
if (TARGET Skynet)
	add_test(NAME approval.Skynet.sum COMMAND Skynet)
	set_tests_properties(approval.Skynet.sum PROPERTIES LABELS integration PASS_REGULAR_EXPRESSION "Sum 49995000" TIMEOUT 300)
	cam_apply_tsan_suppressions(approval.Skynet.sum)
endif()

# ---------------------------------------------------------------- doctest-based tests
if (EXISTS ${CAM_DOCTEST_HEADER})
	add_library(cam_doctest_main STATIC support/doctest_main.cpp)
	target_link_libraries(cam_doctest_main PUBLIC cam_test_support)

	add_executable(cam_tests unit/smoke.cpp)
	target_link_libraries(cam_tests cam_doctest_main ${CAM_TEST_LIBRARY})
	add_test(NAME unit.smoke COMMAND cam_tests)
	set_tests_properties(unit.smoke PROPERTIES LABELS unit TIMEOUT 300)
	cam_apply_tsan_suppressions(unit.smoke)
else()
	message(WARNING "${CAM_DOCTEST_HEADER} is missing: the unit tests are not built")
endif()
