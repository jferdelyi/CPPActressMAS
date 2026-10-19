# Tests of the examples (label examples): the programs of Examples/, when they are built (BUILD_EXAMPLES). They check
# that an example runs and shows what it is meant to show; the behaviour of the C# is checked by iso/.
set(CAM_TEST_TYPE examples)

# ---------------------------------------------------------------- approval
add_executable(cam_approve approve.cpp)
target_link_libraries(cam_approve cam_test_support)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/approval)

# Runs an example, normalises its output (UUIDs, durations, optional line sort) and compares it with
# expected/<example>.txt. The expected files pin the current C++ behaviour, not the C# oracle.
function(cam_add_approval p_example p_sort)
	if (TARGET ${p_example})
		set(l_args)
		if (p_sort)
			list(APPEND l_args --sort)
		endif()
		add_test(NAME examples.${p_example}.approval
			COMMAND cam_approve ${l_args}
				${CMAKE_CURRENT_LIST_DIR}/expected/${p_example}.txt
				${CMAKE_CURRENT_BINARY_DIR}/approval/${p_example}.actual.txt
				$<TARGET_FILE:${p_example}>)
		set_tests_properties(examples.${p_example}.approval PROPERTIES LABELS examples TIMEOUT 120)
		cam_apply_tsan_suppressions(examples.${p_example}.approval)
	endif()
endfunction()

# Only Serialization is deterministic: the other examples draw at random or run in parallel, and their lines change
# from one run to the next.
cam_add_approval(Serialization OFF)
if (TARGET Skynet)
	add_test(NAME examples.Skynet.sum COMMAND Skynet)
	set_tests_properties(examples.Skynet.sum PROPERTIES LABELS examples PASS_REGULAR_EXPRESSION "Sum 49995000" TIMEOUT 300)
	cam_apply_tsan_suppressions(examples.Skynet.sum)
endif()

# ---------------------------------------------------------------- corrected behaviour
# The "fixed" mode of an example corrects the bugs of the C#: on the scenarios of the C# it matches the same oracles
# (iso/); where a bug is reached, it is checked alone. The patterns stay linear: CTest matches them with backtracking,
# and "(.|\n)*" repeated blows up on the whole output.
function(cam_add_example_oracle p_example)
	cmake_parse_arguments(PARSE_ARGV 1 l "" "NAME;ORACLE" "")
	if (TARGET ${p_example})
		add_test(NAME examples.${l_NAME}
			COMMAND cam_iso_check ${CMAKE_SOURCE_DIR}/tests/iso/rules/${l_ORACLE}.rules ${CMAKE_SOURCE_DIR}/tests/iso/oracles/${l_ORACLE}.txt
				-- $<TARGET_FILE:${p_example}> ${l_UNPARSED_ARGUMENTS}
			WORKING_DIRECTORY $<TARGET_FILE_DIR:${p_example}>)
		set_tests_properties(examples.${l_NAME} PROPERTIES LABELS examples TIMEOUT 300)
		cam_apply_tsan_suppressions(examples.${l_NAME})
	endif()
endfunction()
cam_add_example_oracle(Exemple4 NAME Exemple4.fixed.oracle ORACLE Agents4 fixed)
cam_add_example_oracle(BDI NAME BDI.fixed.oracle ORACLE BDI 120 0 fixed)
if (TARGET Exemple4)
	add_test(NAME examples.Exemple4.fixed COMMAND Exemple4 fixed)
	set_tests_properties(examples.Exemple4.fixed PROPERTIES LABELS examples TIMEOUT 120
		PASS_REGULAR_EXPRESSION "Agent a5 turn 3 index 3" FAIL_REGULAR_EXPRESSION "turn 4")
endif()
if (TARGET MultipleMessages)
	add_test(NAME examples.MultipleMessages.fixed COMMAND MultipleMessages fixed)
	set_tests_properties(examples.MultipleMessages.fixed PROPERTIES LABELS examples TIMEOUT 120
		PASS_REGULAR_EXPRESSION "\\[Manager\\]: Worker[0-9] replying" FAIL_REGULAR_EXPRESSION "out of range|Exception")
endif()
if (TARGET BDI)
	add_test(NAME examples.BDI.fixed_adjacent COMMAND BDI 120 0 fixed-adjacent)
	set_tests_properties(examples.BDI.fixed_adjacent PROPERTIES LABELS examples TIMEOUT 120
		PASS_REGULAR_EXPRESSION "fire-out\n\t\\[terrain -> patrol\\]: percepts 3 0 0 0" FAIL_REGULAR_EXPRESSION "out of range")
endif()

# ---------------------------------------------------------------- features of the C++ port
# The examples without a C# original (Examples/Features) run and report what they measure.
if (TARGET TransportRoundTrip)
	add_test(NAME examples.TransportRoundTrip COMMAND TransportRoundTrip loopback://smoke-roundtrip loopback://smoke-roundtrip 50)
	set_tests_properties(examples.TransportRoundTrip PROPERTIES LABELS examples PASS_REGULAR_EXPRESSION "50 round trips in" TIMEOUT 120)
	cam_apply_tsan_suppressions(examples.TransportRoundTrip)
endif()
if (TARGET SharedText)
	add_test(NAME examples.SharedText COMMAND SharedText 10 4)
	set_tests_properties(examples.SharedText PROPERTIES LABELS examples PASS_REGULAR_EXPRESSION "cam::Text [^\n]* 40960 characters received"
		FAIL_REGULAR_EXPRESSION "MISSING" TIMEOUT 120)
	cam_apply_tsan_suppressions(examples.SharedText)
endif()
