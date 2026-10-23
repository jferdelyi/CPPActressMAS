# Characterization tests (label characterization): the known defects of the library, and the probes of the fixed ones,
# kept as regression tests.
#   - known_defects.cpp: defects whose symptom is a wrong value. Each case asserts the behaviour of ActressMAS C# v3.1
#     and is marked should_fail() while the defect exists; once it is fixed, doctest reports that the case "should
#     have failed" and the marker is removed. The assertions never change.
#   - probes.cpp: one process per probe (cam_probes <name>). A crash probe passes when the sanitizer reports the defect
#     (PASS_REGULAR_EXPRESSION, never WILL_FAIL), so a probe that stops crashing fails and signals the fix; an ok probe
#     must run to completion without any sanitizer report.
# The probes run without the TSan suppressions: they must see the races they document.
set(CAM_TEST_TYPE characterization)

add_executable(cam_known_defects known_defects.cpp)
target_link_libraries(cam_known_defects cam_doctest_main ${CAM_TEST_LIBRARY})
add_test(NAME characterization.known_defects COMMAND cam_known_defects)
set_tests_properties(characterization.known_defects PROPERTIES LABELS characterization TIMEOUT 600)

add_executable(cam_probes probes.cpp)
target_link_libraries(cam_probes ${CAM_TEST_LIBRARY} cam_test_support)

# A probe whose expected failure is a sanitizer signature; registered in the matching sanitizer build only.
function(cam_add_crash_probe p_name p_sanitizer p_regex)
	if (CAM_SANITIZER STREQUAL p_sanitizer)
		add_test(NAME characterization.${p_sanitizer}.${p_name} COMMAND cam_probes ${p_name})
		# The sanitizer must exit instead of aborting: CTest reports an aborted subprocess as an exception
		# whatever its output, while PASS_REGULAR_EXPRESSION decides for an ordinary non-zero exit code.
		set_tests_properties(characterization.${p_sanitizer}.${p_name} PROPERTIES
			LABELS characterization
			PASS_REGULAR_EXPRESSION "${p_regex}"
			ENVIRONMENT "ASAN_OPTIONS=abort_on_error=0;UBSAN_OPTIONS=abort_on_error=0:print_stacktrace=1;TSAN_OPTIONS=abort_on_error=0"
			TIMEOUT 900)
	endif()
endfunction()

# A probe that must run to completion without any sanitizer report (fixed defect).
function(cam_add_ok_probe p_name)
	add_test(NAME characterization.ok.${p_name} COMMAND cam_probes ${p_name})
	set_tests_properties(characterization.ok.${p_name} PROPERTIES
		LABELS characterization
		PASS_REGULAR_EXPRESSION "OK no crash"
		FAIL_REGULAR_EXPRESSION "Sanitizer|runtime error"
		TIMEOUT 300)
endfunction()

# Fixed: an empty turn in Parallel or SequentialRandom mode underflowed random_permutation(0)
cam_add_ok_probe(empty_start_sequential)
cam_add_ok_probe(empty_start_parallel)
cam_add_ok_probe(empty_start_sequential_random)
cam_add_ok_probe(continue_all_dead_parallel)
# Fixed: remove() of an unknown agent, or of an agent not inserted yet, dereferenced a null pointer; it is ignored now
cam_add_ok_probe(remove_unknown)
cam_add_ok_probe(remove_buffered)
# Fixed: remove() left the name index behind, then send_by_name() dereferenced a null pointer
cam_add_ok_probe(remove_then_send_by_name)
# Fixed: random_agent() included the upper bound and dereferenced end(); it draws once in [0, number of agents) now
cam_add_ok_probe(random_agent_bound)
# Fixed: remove() during a Parallel or Sequential turn erased from the collection that other threads, or the loop of
# the turn, were iterating; the agent is marked now, and the turn boundary erases it
cam_add_ok_probe(remove_during_parallel_turn)
cam_add_ok_probe(remove_during_sequential_turn)
# Fixed: add<T>() returned a reference to the identifier of the agent, dangling once the agent was destroyed; it returns
# the identifier by value now
cam_add_ok_probe(dangling_add_ref)
# Known: the live observables of an agent are read by the others while their owner writes them
cam_add_crash_probe(observables_parallel tsan "ThreadSanitizer: data race")
# Fixed: send_content() and remove() from another thread read the collection while the turn boundary inserted the agents
# added from outside or erased the removed ones, and lost messages; the public functions of the environment take a
# shared lock now, as the ConcurrentDictionary of the C# in Parallel mode
cam_add_ok_probe(outside_add_and_send_parallel)
cam_add_ok_probe(outside_remove_and_send_parallel)
# Fixed: stop() wrote the dead flag while send_by_name() read it from another thread; the flag is atomic now
cam_add_ok_probe(stop_and_send_parallel)
# Fixed: the random generator was static and shared by every environment of the process; each one has its own now
cam_add_ok_probe(rng_two_threads)
