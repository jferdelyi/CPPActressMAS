# Tests of CPPActressMAS.
#
# CTest labels: unit, characterization, iso (reserved), integration, mqtt, zenoh (reserved), perf (tests/bench/).
#
# Characterization tests pin the behaviour of known defects (IDs from docs/port/00-analyse-initiale.md):
#   - crash probes run one process per probe (cam_probes <name>); the expected failure is the sanitizer
#     signature matched by PASS_REGULAR_EXPRESSION, so a probe that stops crashing fails the test and
#     signals that the defect is fixed (never WILL_FAIL);
#   - value probes are doctest cases asserting the C# behaviour and marked should_fail(); once the
#     defect is fixed doctest reports "should have failed" and the marker is removed.
# ThreadSanitizer suppressions (tests/tsan/suppressions.txt) apply to the ordinary tests only: the crash
# probes must see the races they document.

set(CAM_SANITIZER "" CACHE STRING "Sanitizer this build is instrumented with: asan, tsan or empty (selects the crash probes)")

set(CAM_TEST_LIBRARY CPPActressMAS)
if (NOT TARGET CPPActressMAS)
	set(CAM_TEST_LIBRARY CPPActressMASStatic)
endif()
set(CAM_DOCTEST_HEADER ${CMAKE_SOURCE_DIR}/ThirdParty/doctest/doctest/doctest.h)
if (NOT EXISTS ${CAM_DOCTEST_HEADER})
	message(FATAL_ERROR "ThirdParty/doctest is missing: run git submodule update --init --depth 1")
endif()
set(CAM_TSAN_SUPPRESSIONS ${CMAKE_CURRENT_LIST_DIR}/tsan/suppressions.txt)

# ---------------------------------------------------------------- support
add_library(cam_test_support INTERFACE)
target_include_directories(cam_test_support INTERFACE ${CMAKE_CURRENT_LIST_DIR}/support ${CMAKE_SOURCE_DIR}/ThirdParty/doctest)

# Ordinary tests run with the TSan suppressions of the known races (each entry carries its ID).
function(cam_apply_tsan_suppressions p_test)
	if (CAM_SANITIZER STREQUAL "tsan")
		set_property(TEST ${p_test} APPEND PROPERTY ENVIRONMENT "TSAN_OPTIONS=suppressions=${CAM_TSAN_SUPPRESSIONS}")
	endif()
endfunction()

# ---------------------------------------------------------------- crash probes
add_executable(cam_probes characterization/probes.cpp)
target_link_libraries(cam_probes ${CAM_TEST_LIBRARY} cam_test_support)

# A probe whose expected failure is a sanitizer signature; registered in the matching sanitizer build only.
function(cam_add_crash_probe p_name p_id p_sanitizer p_regex)
	if (CAM_SANITIZER STREQUAL p_sanitizer)
		add_test(NAME characterization.${p_sanitizer}.${p_name} COMMAND cam_probes ${p_name})
		# The sanitizer must exit instead of aborting: CTest reports an aborted subprocess as an exception
		# whatever its output, while PASS_REGULAR_EXPRESSION decides for an ordinary non-zero exit code.
		set_tests_properties(characterization.${p_sanitizer}.${p_name} PROPERTIES
			LABELS "characterization;${p_id}"
			PASS_REGULAR_EXPRESSION "${p_regex}"
			ENVIRONMENT "ASAN_OPTIONS=abort_on_error=0;UBSAN_OPTIONS=abort_on_error=0:print_stacktrace=1;TSAN_OPTIONS=abort_on_error=0"
			TIMEOUT 900)
	endif()
endfunction()

# A probe that must run to completion without any sanitizer report (witness or fixed defect).
function(cam_add_ok_probe p_name p_id)
	add_test(NAME characterization.ok.${p_name} COMMAND cam_probes ${p_name})
	set_tests_properties(characterization.ok.${p_name} PROPERTIES
		LABELS "characterization;${p_id}"
		PASS_REGULAR_EXPRESSION "OK no crash"
		FAIL_REGULAR_EXPRESSION "Sanitizer|runtime error"
		TIMEOUT 300)
endfunction()

# C1 (fixed in 1A): empty turn in Parallel or SequentialRandom mode (random_permutation(0) underflow).
# The probes stay as regression tests; continue_all_dead_parallel still shows C24 (known_defects.cpp).
cam_add_ok_probe(empty_start_sequential C1)
cam_add_ok_probe(empty_start_parallel C1)
cam_add_ok_probe(empty_start_sequential_random C1)
cam_add_ok_probe(continue_all_dead_parallel C1)
# C5 (fixed in 3B, D80): remove() of an unknown or not yet inserted agent dereferenced a null pointer; it is now
# ignored. The probes stay as regression tests.
cam_add_ok_probe(remove_unknown C5)
cam_add_ok_probe(remove_buffered C5)
# C6 (fixed in 1A): remove() left the name index behind, send_by_name() then dereferenced a null pointer.
cam_add_ok_probe(remove_then_send_by_name C6)
# C4 (fixed in 3B, D82): random_agent() included the upper bound and dereferenced end(); it now draws once in
# [0, number of agents) and returns an id. The probe stays as a regression test.
cam_add_ok_probe(random_agent_bound C4)
# C10 and C12 (fixed in 3B, D80): remove() during a Parallel or Sequential turn erased from the collection other
# threads, or the range-for of run_turn(), were iterating; it now marks the agent and the turn boundary erases it.
# The probes stay as regression tests.
cam_add_ok_probe(remove_during_parallel_turn C10)
cam_add_ok_probe(remove_during_sequential_turn C12)
# C16 (fixed in 3B, D76): add<T>() returned a reference to the agent's id, dangling once the agent was destroyed;
# it now returns the AgentId by value. The probe stays as a regression test.
cam_add_ok_probe(dangling_add_ref C16)
# T1: live observables map read by other agents while its owner writes it.
cam_add_crash_probe(observables_parallel T1 tsan "ThreadSanitizer: data race")
# T2 (fixed in 3B, D80): stop() wrote the dead flag while send_by_name() read it from another thread; the flag is
# now atomic. The probe stays as a regression test.
cam_add_ok_probe(stop_and_send_parallel T2)
# T7 (fixed in 3B, D82): the random generator was static and shared by all environments of the process; each
# environment now has its own. The probe stays as a regression test.
cam_add_ok_probe(rng_two_threads T7)

# ---------------------------------------------------------------- ISO comparison with the C# (D74, D82)
add_executable(cam_iso_check iso/iso_check.cpp)
set(CAM_ISO_FIXTURES ${CMAKE_CURRENT_LIST_DIR}/iso/fixtures)

# The tool itself, on fixtures: the same behaviour, then each kind of difference it must report.
add_test(NAME unit.iso_check.same COMMAND cam_iso_check ${CAM_ISO_FIXTURES}/basic.rules ${CAM_ISO_FIXTURES}/basic.oracle --actual ${CAM_ISO_FIXTURES}/same.txt)
set_tests_properties(unit.iso_check.same PROPERTIES LABELS unit PASS_REGULAR_EXPRESSION "same behaviour" FAIL_REGULAR_EXPRESSION "FAIL")
foreach (l_case IN ITEMS "order|FAIL same msg seq" "uncovered|FAIL cover all on C\\+\\+" "missing|FAIL same total bag")
	string(REPLACE "|" ";" l_parts "${l_case}")
	list(GET l_parts 0 l_name)
	list(GET l_parts 1 l_expected)
	add_test(NAME unit.iso_check.${l_name} COMMAND cam_iso_check ${CAM_ISO_FIXTURES}/basic.rules ${CAM_ISO_FIXTURES}/basic.oracle --actual ${CAM_ISO_FIXTURES}/${l_name}.txt)
	set_tests_properties(unit.iso_check.${l_name} PROPERTIES LABELS unit PASS_REGULAR_EXPRESSION "${l_expected}")
endforeach()
add_test(NAME unit.iso_check.typo COMMAND cam_iso_check ${CAM_ISO_FIXTURES}/typo.rules ${CAM_ISO_FIXTURES}/basic.oracle --actual ${CAM_ISO_FIXTURES}/same.txt)
set_tests_properties(unit.iso_check.typo PROPERTIES LABELS unit PASS_REGULAR_EXPRESSION "FAIL fact msg: no tuple in the C# oracle")

# ---------------------------------------------------------------- harness programs (tests/iso/ports, D98)
# One program per C# project, named after it: the C# original as it is, its bugs included, on the mechanics of the
# library, with the .NET helpers of the harness (tests/iso/DotNet.hpp). They keep the floating point contract and the
# warnings of the examples (-ffp-contract=off; -Wall -Wextra -Wpedantic in Release).
function(cam_add_iso_port p_project)
	add_executable(iso_${p_project} iso/ports/${p_project}.cpp)
	target_link_libraries(iso_${p_project} ${CAM_TEST_LIBRARY})
	target_include_directories(iso_${p_project} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/iso)
	if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
		target_compile_options(iso_${p_project} PRIVATE -ffp-contract=off $<$<CONFIG:Release>:-Wall -Wextra -Wpedantic>)
	endif()
endfunction()

foreach (l_project IN ITEMS Agents1 Agents2 Agents3 Agents4 Agents5 MultipleMessages SendingObjects ColorGame NumberGame
		MechanismDesign PrisonerDilemma BDI GameOfLife PingPong Skynet)
	cam_add_iso_port(${l_project})
endforeach()
# GameOfLife reads its initial pattern from the working directory, as the C#
add_custom_command(TARGET iso_GameOfLife POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/Examples/GameOfLife/patterns.txt $<TARGET_FILE_DIR:iso_GameOfLife>)

# Runs the harness program of a C# project and compares its facts with the output of the C# original
# (tests/iso/oracles/<oracle>.txt, produced by tests/tools/csharp-ref) under tests/iso/rules/<oracle>.rules. The oracle is
# named after the project unless ORACLE names another one (a scenario of the same project), the test is iso.<oracle>
# unless NAME says otherwise; the other arguments go to the program, which runs from its own folder, where its data
# files are copied. Label iso.
function(cam_add_iso p_project)
	cmake_parse_arguments(PARSE_ARGV 1 l "" "NAME;ORACLE" "")
	if (NOT l_ORACLE)
		set(l_ORACLE ${p_project})
	endif()
	if (NOT l_NAME)
		set(l_NAME ${l_ORACLE})
	endif()
	add_test(NAME iso.${l_NAME}
		COMMAND cam_iso_check ${CMAKE_CURRENT_LIST_DIR}/iso/rules/${l_ORACLE}.rules ${CMAKE_CURRENT_LIST_DIR}/iso/oracles/${l_ORACLE}.txt
			-- $<TARGET_FILE:iso_${p_project}> ${l_UNPARSED_ARGUMENTS}
		WORKING_DIRECTORY $<TARGET_FILE_DIR:iso_${p_project}>)
	set_tests_properties(iso.${l_NAME} PROPERTIES LABELS iso TIMEOUT 300)
	cam_apply_tsan_suppressions(iso.${l_NAME})
endfunction()

# Sequential examples are compared line by line (exact trace), the parallel ones on what does not depend on the
# interleaving; Agents2, ColorGame and NumberGame draw from the seeds of their oracles (D96)
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
# The fires of the C# reach none of the bugs of E02; two fires side by side reach the empty plan, which stops the patrol
cam_add_iso(BDI 120 0)
cam_add_iso(BDI ORACLE BDIAdjacent 120 0 adjacent)
cam_add_iso(GameOfLife 60 0)
# The benchmarks, durations ignored
cam_add_iso(PingPong)
cam_add_iso(Skynet)
# 390 000 calls of the perception filter per turn: with gcc, whose ASan detects stack-use-after-return by default (fake
# stacks), the test took more than 1 200 s, 39 s without; clang leaves it off by default (D95)
if (CAM_SANITIZER STREQUAL "asan")
	set_property(TEST iso.GameOfLife APPEND PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_stack_use_after_return=0")
endif()

# ---------------------------------------------------------------- corrected behaviour of the examples
# Until the examples are rewritten (D98), their "fixed" mode corrects the bugs of the C#: on the scenarios of the C# it
# matches the same oracles; where a bug is reached, it is checked alone. The patterns stay linear: CTest matches them
# with backtracking, and "(.|\n)*" repeated blows up on the whole output.
function(cam_add_example_iso p_example)
	cmake_parse_arguments(PARSE_ARGV 1 l "" "NAME;ORACLE" "")
	if (TARGET ${p_example})
		add_test(NAME iso.${l_NAME}
			COMMAND cam_iso_check ${CMAKE_CURRENT_LIST_DIR}/iso/rules/${l_ORACLE}.rules ${CMAKE_CURRENT_LIST_DIR}/iso/oracles/${l_ORACLE}.txt
				-- $<TARGET_FILE:${p_example}> ${l_UNPARSED_ARGUMENTS}
			WORKING_DIRECTORY $<TARGET_FILE_DIR:${p_example}>)
		set_tests_properties(iso.${l_NAME} PROPERTIES LABELS iso TIMEOUT 300)
		cam_apply_tsan_suppressions(iso.${l_NAME})
	endif()
endfunction()
cam_add_example_iso(Exemple4 NAME Agents4.fixed ORACLE Agents4 fixed)
cam_add_example_iso(BDI NAME BDI.fixed ORACLE BDI 120 0 fixed)
if (TARGET Exemple4)
	add_test(NAME example.Exemple4.fixed COMMAND Exemple4 fixed)
	set_tests_properties(example.Exemple4.fixed PROPERTIES LABELS integration TIMEOUT 120
		PASS_REGULAR_EXPRESSION "Agent a5 turn 3 index 3" FAIL_REGULAR_EXPRESSION "turn 4")
endif()
if (TARGET MultipleMessages)
	add_test(NAME example.MultipleMessages.fixed COMMAND MultipleMessages fixed)
	set_tests_properties(example.MultipleMessages.fixed PROPERTIES LABELS integration TIMEOUT 120
		PASS_REGULAR_EXPRESSION "\\[Manager\\]: Worker[0-9] replying" FAIL_REGULAR_EXPRESSION "out of range|Exception")
endif()
if (TARGET BDI)
	add_test(NAME example.BDI.fixed_adjacent COMMAND BDI 120 0 fixed-adjacent)
	set_tests_properties(example.BDI.fixed_adjacent PROPERTIES LABELS integration TIMEOUT 120
		PASS_REGULAR_EXPRESSION "fire-out\n\t\\[terrain -> patrol\\]: percepts 3 0 0 0" FAIL_REGULAR_EXPRESSION "out of range")
endif()

# Examples of the features of the C++ port (Examples/Features): they run and report what they measure.
if (TARGET TransportRoundTrip)
	add_test(NAME example.TransportRoundTrip COMMAND TransportRoundTrip loopback://smoke-roundtrip loopback://smoke-roundtrip 50)
	set_tests_properties(example.TransportRoundTrip PROPERTIES LABELS integration PASS_REGULAR_EXPRESSION "50 round trips in" TIMEOUT 120)
	cam_apply_tsan_suppressions(example.TransportRoundTrip)
endif()
if (TARGET SharedText)
	add_test(NAME example.SharedText COMMAND SharedText 10 4)
	set_tests_properties(example.SharedText PROPERTIES LABELS integration PASS_REGULAR_EXPRESSION "cam::Text [^\n]* 40960 characters received"
		FAIL_REGULAR_EXPRESSION "MISSING" TIMEOUT 120)
	cam_apply_tsan_suppressions(example.SharedText)
endif()

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

	# Transport layer without any environment: codec (round trips, binary vectors of
	# tests/vectors/envelope/<codec>/v1, malformed input) and Endpoint (sequences, deduplication,
	# reordering, overflows, counters). CAM_WRITE_VECTORS=1 in the environment regenerates the vectors.
	# The ActressMAS wire codec is checked against the bytes captured from the C# code (actressmas/v1).
	add_executable(cam_transport_tests unit/envelope_codec.cpp unit/endpoint.cpp unit/actressmas_codec.cpp)
	target_link_libraries(cam_transport_tests cam_doctest_main ${CAM_TEST_LIBRARY})
	target_compile_definitions(cam_transport_tests PRIVATE CAM_VECTORS_DIR="${CMAKE_CURRENT_LIST_DIR}/vectors")
	add_test(NAME unit.transport COMMAND cam_transport_tests)
	set_tests_properties(unit.transport PROPERTIES LABELS unit TIMEOUT 300)

	# Environment::connect(url): the URL kept by the environment, the default factory over the schemes compiled
	# in, a custom factory, and the shared loopback of a namespace.
	add_executable(cam_connect_tests unit/connect.cpp)
	target_link_libraries(cam_connect_tests cam_doctest_main ${CAM_TEST_LIBRARY})
	add_test(NAME unit.connect COMMAND cam_connect_tests)
	set_tests_properties(unit.connect PROPERTIES LABELS unit TIMEOUT 300)

	# Queue of the agent messages (P8): arrival order, several producers at once, nodes recycled per thread.
	add_executable(cam_mpsc_queue_tests unit/mpsc_queue.cpp)
	target_link_libraries(cam_mpsc_queue_tests cam_doctest_main ${CAM_TEST_LIBRARY})
	add_test(NAME unit.mpsc_queue COMMAND cam_mpsc_queue_tests)
	set_tests_properties(unit.mpsc_queue PROPERTIES LABELS unit TIMEOUT 300)
	cam_apply_tsan_suppressions(unit.mpsc_queue)

	# Blocks recycled per thread (P8): reuse, release from another thread, threads that end, arrays.
	add_executable(cam_recycling_allocator_tests unit/recycling_allocator.cpp)
	target_link_libraries(cam_recycling_allocator_tests cam_doctest_main ${CAM_TEST_LIBRARY})
	add_test(NAME unit.recycling_allocator COMMAND cam_recycling_allocator_tests)
	set_tests_properties(unit.recycling_allocator PROPERTIES LABELS unit TIMEOUT 300)
	cam_apply_tsan_suppressions(unit.recycling_allocator)

	# Pointer to a message with its counter inside (D76): copies, moves, kept messages, release across threads.
	add_executable(cam_message_pointer_tests unit/message_pointer.cpp)
	target_link_libraries(cam_message_pointer_tests cam_doctest_main ${CAM_TEST_LIBRARY})
	add_test(NAME unit.message_pointer COMMAND cam_message_pointer_tests)
	set_tests_properties(unit.message_pointer PROPERTIES LABELS unit TIMEOUT 300)
	cam_apply_tsan_suppressions(unit.message_pointer)

	# Identifier of an agent (D76): size, capacity, literals, comparisons, hash, stream, JSON.
	add_executable(cam_agent_id_tests unit/agent_id.cpp)
	target_link_libraries(cam_agent_id_tests cam_doctest_main ${CAM_TEST_LIBRARY})
	add_test(NAME unit.agent_id COMMAND cam_agent_id_tests)
	set_tests_properties(unit.agent_id PROPERTIES LABELS unit TIMEOUT 300)
	cam_apply_tsan_suppressions(unit.agent_id)

	# Removal of agents (C10, C12, C5, D80): deferred to the turn boundary during a turn, at once outside a turn.
	add_executable(cam_removal_tests unit/removal.cpp)
	target_link_libraries(cam_removal_tests cam_doctest_main ${CAM_TEST_LIBRARY})
	add_test(NAME unit.removal COMMAND cam_removal_tests)
	set_tests_properties(unit.removal PROPERTIES LABELS unit TIMEOUT 300)
	cam_apply_tsan_suppressions(unit.removal)

	# Generator of the seeded System.Random (D82), against the vectors produced by tests/tools/csharp-ref.
	add_executable(cam_dotnet_random_tests unit/dotnet_random.cpp)
	target_link_libraries(cam_dotnet_random_tests cam_doctest_main ${CAM_TEST_LIBRARY})
	target_compile_definitions(cam_dotnet_random_tests PRIVATE CAM_VECTORS_DIR="${CMAKE_CURRENT_LIST_DIR}/vectors")
	add_test(NAME unit.dotnet_random COMMAND cam_dotnet_random_tests)
	set_tests_properties(unit.dotnet_random PROPERTIES LABELS unit TIMEOUT 300)
	cam_apply_tsan_suppressions(unit.dotnet_random)

	# Random order of the sequential modes (D82): C# traces with the DotNet generator, portable Mt19937, seeds.
	add_executable(cam_random_order_tests unit/random_order.cpp)
	target_link_libraries(cam_random_order_tests cam_doctest_main ${CAM_TEST_LIBRARY})
	target_compile_definitions(cam_random_order_tests PRIVATE CAM_VECTORS_DIR="${CMAKE_CURRENT_LIST_DIR}/vectors")
	add_test(NAME unit.random_order COMMAND cam_random_order_tests)
	set_tests_properties(unit.random_order PROPERTIES LABELS unit TIMEOUT 300)
	cam_apply_tsan_suppressions(unit.random_order)

	# Text of a message as the C# reads it (G1, G2, D82): Parse, Parse1P, Format with names.
	add_executable(cam_message_parse_tests unit/message_parse.cpp)
	target_link_libraries(cam_message_parse_tests cam_doctest_main ${CAM_TEST_LIBRARY})
	add_test(NAME unit.message_parse COMMAND cam_message_parse_tests)
	set_tests_properties(unit.message_parse PROPERTIES LABELS unit TIMEOUT 300)
	cam_apply_tsan_suppressions(unit.message_parse)

	# Catalogue of the agent types of the process (Q27, D82): automatic names, CAM_AGENT_TYPE, conflicts, migration.
	add_executable(cam_agent_types_tests unit/agent_types.cpp)
	target_link_libraries(cam_agent_types_tests cam_doctest_main ${CAM_TEST_LIBRARY})
	add_test(NAME unit.agent_types COMMAND cam_agent_types_tests)
	set_tests_properties(unit.agent_types PROPERTIES LABELS unit TIMEOUT 300)
	cam_apply_tsan_suppressions(unit.agent_types)

	# Number formatting and parsing of .NET for the harness programs (tests/iso/DotNet.hpp, D82), against the C# vectors.
	add_executable(cam_dotnet_format_tests unit/dotnet_format.cpp)
	target_link_libraries(cam_dotnet_format_tests cam_doctest_main)
	target_include_directories(cam_dotnet_format_tests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/iso)
	target_compile_definitions(cam_dotnet_format_tests PRIVATE CAM_VECTORS_DIR="${CMAKE_CURRENT_LIST_DIR}/vectors")
	add_test(NAME unit.dotnet_format COMMAND cam_dotnet_format_tests)
	set_tests_properties(unit.dotnet_format PROPERTIES LABELS unit TIMEOUT 300)
	cam_apply_tsan_suppressions(unit.dotnet_format)

	add_executable(cam_dotnet_seed_tests unit/dotnet_seed.cpp)
	target_link_libraries(cam_dotnet_seed_tests cam_doctest_main)
	target_include_directories(cam_dotnet_seed_tests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/iso)
	add_test(NAME unit.dotnet_site_seed COMMAND cam_dotnet_seed_tests)
	set_tests_properties(unit.dotnet_site_seed PROPERTIES LABELS unit TIMEOUT 300)
	cam_apply_tsan_suppressions(unit.dotnet_site_seed)

	# Identifiers (D58): version 4 UUIDs, short agent identifiers, uniqueness from several threads.
	add_executable(cam_identifier_tests unit/identifier.cpp)
	target_link_libraries(cam_identifier_tests cam_doctest_main ${CAM_TEST_LIBRARY})
	add_test(NAME unit.identifier COMMAND cam_identifier_tests)
	set_tests_properties(unit.identifier PROPERTIES LABELS unit TIMEOUT 300)
	cam_apply_tsan_suppressions(unit.identifier)

	# Environments connected through the loopback transport, in memory and serializing.
	add_executable(cam_loopback_tests integration/loopback.cpp)
	target_link_libraries(cam_loopback_tests cam_doctest_main ${CAM_TEST_LIBRARY})
	add_test(NAME integration.loopback COMMAND cam_loopback_tests)
	set_tests_properties(integration.loopback PROPERTIES LABELS integration TIMEOUT 300)
	cam_apply_tsan_suppressions(integration.loopback)

	# Three-step agent migration between environments connected through the loopback transport.
	add_executable(cam_migration_tests integration/migration.cpp)
	target_link_libraries(cam_migration_tests cam_doctest_main ${CAM_TEST_LIBRARY})
	add_test(NAME integration.migration COMMAND cam_migration_tests)
	set_tests_properties(integration.migration PROPERTIES LABELS integration TIMEOUT 300)
	cam_apply_tsan_suppressions(integration.migration)

	add_executable(cam_known_defects characterization/known_defects.cpp)
	target_link_libraries(cam_known_defects cam_doctest_main ${CAM_TEST_LIBRARY})
	add_test(NAME characterization.known_defects COMMAND cam_known_defects)
	set_tests_properties(characterization.known_defects PROPERTIES LABELS characterization TIMEOUT 600)

	# Needs a broker: CAM_MQTT_HOST and CAM_MQTT_PORT in the environment, otherwise the test skips (exit 77).
	add_executable(cam_network_tests network/network_tests.cpp)
	target_link_libraries(cam_network_tests cam_test_support ${CAM_TEST_LIBRARY})
	add_test(NAME mqtt.network COMMAND cam_network_tests)
	set_tests_properties(mqtt.network PROPERTIES LABELS "mqtt;integration" SKIP_RETURN_CODE 77 TIMEOUT 120)
	cam_apply_tsan_suppressions(mqtt.network)

	# Two Zenoh peers of the process on 127.0.0.1 (explicit endpoints, no multicast); skips (77) without the
	# Zenoh transport. Opens sockets: excluded with -LE zenoh when the network must stay silent.
	add_executable(cam_zenoh_tests network/zenoh_tests.cpp)
	target_link_libraries(cam_zenoh_tests cam_test_support ${CAM_TEST_LIBRARY})
	add_test(NAME zenoh.network COMMAND cam_zenoh_tests)
	set_tests_properties(zenoh.network PROPERTIES LABELS "zenoh;integration" SKIP_RETURN_CODE 77 TIMEOUT 180)
	cam_apply_tsan_suppressions(zenoh.network)
else()
	message(WARNING "${CAM_DOCTEST_HEADER} is missing: the unit, known-defect and network tests are not built")
endif()
