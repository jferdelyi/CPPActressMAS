# Oracle tests (label iso): the behaviour of ActressMAS v3.1 (C#). Each C# example is ported as it is, its bugs included,
# in ports/<Project>.cpp, and its output is compared with the output of the C# original (oracles/<Oracle>.txt, produced
# by tools/csharp-ref) on the facts of rules/<Oracle>.rules (README.md).
set(CAM_TEST_TYPE iso)

# ---------------------------------------------------------------- the tools of the harness, and their own tests
add_executable(cam_iso_check iso_check.cpp)
set(CAM_ISO_FIXTURES ${CMAKE_CURRENT_LIST_DIR}/fixtures)

# The comparison tool on fixtures: the same behaviour, then each kind of difference it must report.
add_test(NAME iso.check.same COMMAND cam_iso_check ${CAM_ISO_FIXTURES}/basic.rules ${CAM_ISO_FIXTURES}/basic.oracle --actual ${CAM_ISO_FIXTURES}/same.txt)
set_tests_properties(iso.check.same PROPERTIES LABELS iso PASS_REGULAR_EXPRESSION "same behaviour" FAIL_REGULAR_EXPRESSION "FAIL")
foreach (l_case IN ITEMS "order|FAIL same msg seq" "uncovered|FAIL cover all on C\\+\\+" "missing|FAIL same total bag")
	string(REPLACE "|" ";" l_parts "${l_case}")
	list(GET l_parts 0 l_name)
	list(GET l_parts 1 l_expected)
	add_test(NAME iso.check.${l_name} COMMAND cam_iso_check ${CAM_ISO_FIXTURES}/basic.rules ${CAM_ISO_FIXTURES}/basic.oracle --actual ${CAM_ISO_FIXTURES}/${l_name}.txt)
	set_tests_properties(iso.check.${l_name} PROPERTIES LABELS iso PASS_REGULAR_EXPRESSION "${l_expected}")
endforeach()
add_test(NAME iso.check.typo COMMAND cam_iso_check ${CAM_ISO_FIXTURES}/typo.rules ${CAM_ISO_FIXTURES}/basic.oracle --actual ${CAM_ISO_FIXTURES}/same.txt)
set_tests_properties(iso.check.typo PROPERTIES LABELS iso PASS_REGULAR_EXPRESSION "FAIL fact msg: no tuple in the C# oracle")

# The .NET behaviour of the ports (DotNet.hpp): number formatting and parsing against the vectors of the C#
# (vectors/format.txt, produced by tools/csharp-ref), and the seeds of the oracles.
cam_add_doctest(dotnet_format cam_dotnet_format_tests dotnet_format.cpp)
target_include_directories(cam_dotnet_format_tests PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_compile_definitions(cam_dotnet_format_tests PRIVATE CAM_VECTORS_DIR="${CMAKE_CURRENT_LIST_DIR}/vectors")
cam_add_doctest(dotnet_site_seed cam_dotnet_seed_tests dotnet_seed.cpp)
target_include_directories(cam_dotnet_seed_tests PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# ---------------------------------------------------------------- the ports of the C# examples
# One program per C# project, named after it: the C# original as it is, its bugs included, on the mechanics of the
# library, with the .NET helpers of DotNet.hpp. Floating point as the C# (-ffp-contract=off: no fused multiply-add),
# and the warnings of the project in Release.
function(cam_add_iso_port p_project)
	add_executable(iso_${p_project} ports/${p_project}.cpp)
	target_link_libraries(iso_${p_project} ${CAM_TEST_LIBRARY})
	target_include_directories(iso_${p_project} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
	if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
		target_compile_options(iso_${p_project} PRIVATE -ffp-contract=off $<$<CONFIG:Release>:-Wall -Wextra -Wpedantic>)
	endif()
endfunction()

foreach (l_project IN ITEMS Agents1 Agents2 Agents3 Agents4 Agents5 MultipleMessages SendingObjects ColorGame NumberGame
		MechanismDesign PrisonerDilemma BDI GameOfLife PingPong Skynet SkynetNumeric Zeuthen Voting EnglishWithBroadcast
		FictitiousPlay ReplicatorDynamics LrtaStar EnglishWithoutBroadcast Vickrey)
	cam_add_iso_port(${l_project})
endforeach()
# GameOfLife reads its initial pattern from the working directory, as the C#
add_custom_command(TARGET iso_GameOfLife POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/Examples/GameOfLife/patterns.txt $<TARGET_FILE_DIR:iso_GameOfLife>)

# Runs the port of a C# project and compares its facts with the output of the C# original (oracles/<oracle>.txt) under
# rules/<oracle>.rules. The oracle is named after the project unless ORACLE names another one (a scenario of the same
# project), the test is iso.<oracle> unless NAME says otherwise; the other arguments go to the program, which runs from
# its own folder, where its data files are copied.
function(cam_add_iso p_project)
	cmake_parse_arguments(PARSE_ARGV 1 l "" "NAME;ORACLE" "")
	if (NOT l_ORACLE)
		set(l_ORACLE ${p_project})
	endif()
	if (NOT l_NAME)
		set(l_NAME ${l_ORACLE})
	endif()
	add_test(NAME iso.${l_NAME}
		COMMAND cam_iso_check ${CMAKE_CURRENT_LIST_DIR}/rules/${l_ORACLE}.rules ${CMAKE_CURRENT_LIST_DIR}/oracles/${l_ORACLE}.txt
			-- $<TARGET_FILE:iso_${p_project}> ${l_UNPARSED_ARGUMENTS}
		WORKING_DIRECTORY $<TARGET_FILE_DIR:iso_${p_project}>)
	set_tests_properties(iso.${l_NAME} PROPERTIES LABELS iso TIMEOUT 300)
	cam_apply_tsan_suppressions(iso.${l_NAME})
endfunction()

# Sequential examples are compared line by line (exact trace), the parallel ones on what does not depend on the
# interleaving; Agents2, ColorGame and NumberGame draw from the seeds of their oracles
cam_add_iso(Agents1)
cam_add_iso(Agents2)
cam_add_iso(Agents3)
cam_add_iso(Agents4)
cam_add_iso(Agents5)
cam_add_iso(MultipleMessages)
cam_add_iso(SendingObjects)
cam_add_iso(ColorGame)
cam_add_iso(NumberGame)
cam_add_iso(MechanismDesign)
cam_add_iso(PrisonerDilemma)
# The fires of the C# reach none of the bugs of the example; two fires side by side reach the empty plan, which stops
# the patrol
cam_add_iso(BDI 120 0)
cam_add_iso(BDI ORACLE BDIAdjacent 120 0 adjacent)
cam_add_iso(GameOfLife 60 0)
# The benchmarks, durations ignored
cam_add_iso(PingPong)
cam_add_iso(Skynet)
cam_add_iso(SkynetNumeric)
# Sequential, fixed order, with draws from the seeds of their oracles: exact traces
cam_add_iso(Zeuthen)
cam_add_iso(Voting)
cam_add_iso(EnglishWithBroadcast)
# Parallel: the lines of each agent in order, the messages of each round as a multiset
cam_add_iso(ReplicatorDynamics)
cam_add_iso(FictitiousPlay)
cam_add_iso(EnglishWithoutBroadcast)
cam_add_iso(Vickrey)
# Known defect: when an agent stops before the others read its last messages, the library formats these messages with
# its identifier instead of its name, if the agent is already removed when they are read, which depends on the
# interleaving. Such a test accepts the behaviour of the C#, or that difference alone (the multiset of the messages);
# the case of characterization.known_defects shows the defect every time, and signals its fix.
function(cam_iso_expect_removed_sender_name p_test)
	set_tests_properties(iso.${p_test} PROPERTIES
		PASS_REGULAR_EXPRESSION "\nsame behaviour;FAIL same message bag[^\n]*\n(OK[^\n]*\n)*1 difference\\(s\\)")
endfunction()
cam_iso_expect_removed_sender_name(FictitiousPlay)
cam_iso_expect_removed_sender_name(EnglishWithoutBroadcast)
cam_iso_expect_removed_sender_name(Vickrey)
# Millions of calls: with gcc, whose ASan detects stack-use-after-return by default (fake stacks), GameOfLife (390 000
# calls of the perception filter per turn) took more than 1 200 s, 39 s without, and PingPong (10 million messages per
# trial) more than 300 s, 37 s without; clang leaves that detection off by default
if (CAM_SANITIZER STREQUAL "asan")
	set_property(TEST iso.GameOfLife iso.PingPong APPEND PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_stack_use_after_return=0")
endif()
