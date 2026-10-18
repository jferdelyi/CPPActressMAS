# Tests of CPPActressMAS.
#
# CTest labels: unit, characterization, iso (reserved), integration, mqtt, zenoh (reserved), perf (bench/).
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

	# Generator of the seeded System.Random (D82), against the vectors produced by tools/csharp-ref.
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
