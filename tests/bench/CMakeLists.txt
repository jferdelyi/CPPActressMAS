# Benchmarks: same workloads and same measured scope as the C# v3.1 E30 benchmarks, atomic counters, steady_clock in microseconds,
# a deterministic variant (--sequential), linked to the static library. Built with -DBUILD_BENCH=ON,
# normally through the "bench" preset (-O3 -DNDEBUG, no -march=native, no -flto).
# tests/iso/ports/ holds the faithful ports of the C# benchmarks; these are the measurement copies.

if (NOT TARGET CPPActressMASStatic)
	message(FATAL_ERROR "tests/bench/ links to CPPActressMASStatic: configure with -DBUILD_STATIC=ON")
endif()

function(cam_add_bench p_name)
	add_executable(bench_${p_name} ${CMAKE_CURRENT_LIST_DIR}/${p_name}.cpp)
	target_include_directories(bench_${p_name} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
	target_link_libraries(bench_${p_name} CPPActressMASStatic)
endfunction()

cam_add_bench(PingPong)
cam_add_bench(PingPongPairs)
cam_add_bench(Skynet)
cam_add_bench(SkynetNumeric)
# Wolf, Sheep and Grass and Flockers of Antelmi et al. (ABM_Comparison), 1 000 to 128 000 agents
cam_add_bench(Wsg)
cam_add_bench(Flockers)
# Agents added and removed at every turn: the guard of their cost against a complexity regression (perf.scaling.Churn)
cam_add_bench(Churn)
# Micro-benchmark of the envelope codecs (encode, decode, bytes on the wire), linked to the core only.
cam_add_bench(Envelope)

# End-to-end transport benchmark: two environments, two nodes, round trips through the directory, over the
# transports compiled in the core (loopback always, mqtt and zenoh with their options).
add_executable(bench_Transport ${CMAKE_CURRENT_LIST_DIR}/Transport.cpp)
target_include_directories(bench_Transport PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(bench_Transport CPPActressMASStatic)

# Smoke tests (label perf): the benchmarks run once and their invariant is checked, behind CAM_RUN_CHECKED (an error
# exit code fails them). Not a measurement.
if (CAM_BUILD_TESTS)
	add_test(NAME perf.smoke.Skynet COMMAND ${CAM_RUN_CHECKED} $<TARGET_FILE:bench_Skynet>)
	set_tests_properties(perf.smoke.Skynet PROPERTIES LABELS perf PASS_REGULAR_EXPRESSION "Sum = 49995000" TIMEOUT 300)
	add_test(NAME perf.smoke.SkynetNumeric COMMAND ${CAM_RUN_CHECKED} $<TARGET_FILE:bench_SkynetNumeric>)
	set_tests_properties(perf.smoke.SkynetNumeric PROPERTIES LABELS perf PASS_REGULAR_EXPRESSION "Long Sum = 199990000" TIMEOUT 300)
	add_test(NAME perf.smoke.PingPong COMMAND ${CAM_RUN_CHECKED} $<TARGET_FILE:bench_PingPong> --sequential --turns 100 --trials 1)
	set_tests_properties(perf.smoke.PingPong PROPERTIES LABELS perf PASS_REGULAR_EXPRESSION "msg/ms" TIMEOUT 300)
	add_test(NAME perf.smoke.Transport COMMAND ${CAM_RUN_CHECKED} $<TARGET_FILE:bench_Transport> --transport loopback --pings 50 --trials 1)
	set_tests_properties(perf.smoke.Transport PROPERTIES LABELS perf PASS_REGULAR_EXPRESSION "msg/ms" TIMEOUT 300)
	add_test(NAME perf.smoke.Envelope COMMAND ${CAM_RUN_CHECKED} $<TARGET_FILE:bench_Envelope> --count 2000)
	set_tests_properties(perf.smoke.Envelope PROPERTIES LABELS perf PASS_REGULAR_EXPRESSION "json content=1024 encode_ns" TIMEOUT 300)
	# The sum of Skynet is N(N - 1) / 2 for N agents, whatever their number
	add_test(NAME perf.smoke.Skynet.N1000 COMMAND ${CAM_RUN_CHECKED} $<TARGET_FILE:bench_Skynet> --agents 1000)
	set_tests_properties(perf.smoke.Skynet.N1000 PROPERTIES LABELS perf PASS_REGULAR_EXPRESSION "Sum = 499500\n" TIMEOUT 300)
	add_test(NAME perf.smoke.Skynet.N50000 COMMAND ${CAM_RUN_CHECKED} $<TARGET_FILE:bench_Skynet> --agents 50000)
	set_tests_properties(perf.smoke.Skynet.N50000 PROPERTIES LABELS perf PASS_REGULAR_EXPRESSION "Sum = 1249975000\n" TIMEOUT 300)
	# PingPong by pairs ends with exactly pairs * messages messages, in both modes
	add_test(NAME perf.smoke.PingPongPairs COMMAND ${CAM_RUN_CHECKED} $<TARGET_FILE:bench_PingPongPairs> --trials 1)
	set_tests_properties(perf.smoke.PingPongPairs PROPERTIES LABELS perf PASS_REGULAR_EXPRESSION "Trial 1\n50000 msg\n" TIMEOUT 300)
	add_test(NAME perf.smoke.PingPongPairs.Sequential
		COMMAND ${CAM_RUN_CHECKED} $<TARGET_FILE:bench_PingPongPairs> --sequential --pairs 7 --messages 10 --trials 1)
	set_tests_properties(perf.smoke.PingPongPairs.Sequential PROPERTIES LABELS perf PASS_REGULAR_EXPRESSION "Trial 1\n70 msg\n" TIMEOUT 300)
	# The models of Antelmi et al. run their 200 turns; the birds of Flockers are never lost. Each writes world-state.txt
	# into its own folder, as the C#.
	foreach (l_model IN ITEMS Wsg Flockers)
		file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${l_model})
		add_test(NAME perf.smoke.${l_model} COMMAND ${CAM_RUN_CHECKED} $<TARGET_FILE:bench_${l_model}>
			WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${l_model})
	endforeach()
	set_tests_properties(perf.smoke.Wsg PROPERTIES LABELS perf PASS_REGULAR_EXPRESSION "Turn 200: [0-9]+ wolves, [0-9]+ sheeps\n" TIMEOUT 300)
	set_tests_properties(perf.smoke.Flockers PROPERTIES LABELS perf PASS_REGULAR_EXPRESSION "Turn 200: 1000 ants\n" TIMEOUT 300)
	# Churn ends with its spawners and the children of their last action
	add_test(NAME perf.smoke.Churn COMMAND ${CAM_RUN_CHECKED} $<TARGET_FILE:bench_Churn> --agents 100)
	set_tests_properties(perf.smoke.Churn PROPERTIES LABELS perf PASS_REGULAR_EXPRESSION "^200 agents\n" TIMEOUT 300)
	cam_fail_on_sanitizer_report(perf.smoke.Skynet perf.smoke.Skynet.N1000 perf.smoke.Skynet.N50000 perf.smoke.SkynetNumeric perf.smoke.PingPong
		perf.smoke.PingPongPairs perf.smoke.PingPongPairs.Sequential perf.smoke.Wsg perf.smoke.Flockers perf.smoke.Churn perf.smoke.Transport
		perf.smoke.Envelope)

	# Growth with the number of agents (label perf), whatever the machine: four times the agents of Churn in less than
	# eight times the time. A linear cost gives about 4; removals that search every agent added in the turn give about
	# 16. Not under a sanitizer, which changes the costs; run alone.
	add_executable(cam_bench_scaling ${CMAKE_CURRENT_LIST_DIR}/scaling.cpp)
	if (NOT CAM_SANITIZER)
		add_test(NAME perf.scaling.Churn COMMAND cam_bench_scaling 8 5 -- $<TARGET_FILE:bench_Churn> --agents 2000)
		set_tests_properties(perf.scaling.Churn PROPERTIES LABELS perf RUN_SERIAL ON TIMEOUT 900)
	endif()

	# Faster than the C# (label perf): a benchmark runs 10 times and the median of its runs is compared with the median
	# of its C# original measured on the same machine (CAM_BENCH_REFERENCE, written by tools/csharp-ref benchref);
	# skipped without that reference. Run alone, so that no other test shares the machine.
	add_executable(cam_bench_compare ${CMAKE_CURRENT_LIST_DIR}/compare.cpp)
	function(cam_add_bench_comparison p_name p_program)
		add_test(NAME perf.vs_csharp.${p_name} COMMAND cam_bench_compare ${p_name} 10 -- $<TARGET_FILE:bench_${p_program}> ${ARGN})
		set_tests_properties(perf.vs_csharp.${p_name} PROPERTIES LABELS perf SKIP_RETURN_CODE 77 RUN_SERIAL ON TIMEOUT 1800)
	endfunction()
	cam_add_bench_comparison(Skynet Skynet)
	cam_add_bench_comparison(SkynetNumeric SkynetNumeric)
	cam_add_bench_comparison(PingPong PingPong)
	cam_add_bench_comparison(PingPongSequential PingPong --sequential)
	cam_add_bench_comparison(PingPongPairs PingPongPairs)
	cam_add_bench_comparison(PingPongPairsSequential PingPongPairs --sequential)
	cam_add_bench_comparison(Wsg Wsg)
	cam_add_bench_comparison(Flockers Flockers)

	# The comparison tool on fixture references: skipped without a reference, then each verdict on small runs
	set(l_fixtures ${CMAKE_CURRENT_LIST_DIR}/fixtures)
	add_test(NAME perf.compare.skip COMMAND cam_bench_compare Skynet 2 -- $<TARGET_FILE:bench_Skynet> --sequential --agents 100)
	set_tests_properties(perf.compare.skip PROPERTIES LABELS perf ENVIRONMENT "CAM_BENCH_REFERENCE=" PASS_REGULAR_EXPRESSION "SKIP no C# reference")
	foreach (l_case IN ITEMS "slow|\nfaster than the C#" "fast|FAIL not faster than the C#")
		string(REPLACE "|" ";" l_parts "${l_case}")
		list(GET l_parts 0 l_reference)
		list(GET l_parts 1 l_expected)
		# A "faster" verdict exits with 0, behind CAM_RUN_CHECKED; a "not faster" one exits with 1, as expected
		set(l_runner)
		if (l_reference STREQUAL "slow")
			set(l_runner ${CAM_RUN_CHECKED})
		endif()
		add_test(NAME perf.compare.time_${l_reference}
			COMMAND ${l_runner} $<TARGET_FILE:cam_bench_compare> Skynet 3 -- $<TARGET_FILE:bench_Skynet> --sequential --agents 100)
		add_test(NAME perf.compare.throughput_${l_reference}
			COMMAND ${l_runner} $<TARGET_FILE:cam_bench_compare> PingPongSequential 2 -- $<TARGET_FILE:bench_PingPong> --sequential --turns 3000
				--trials 3)
		set_tests_properties(perf.compare.time_${l_reference} perf.compare.throughput_${l_reference} PROPERTIES LABELS perf
			ENVIRONMENT "CAM_BENCH_REFERENCE=${l_fixtures}/reference-${l_reference}.txt" PASS_REGULAR_EXPRESSION "${l_expected}" TIMEOUT 300)
	endforeach()
	cam_fail_on_sanitizer_report(perf.compare.skip perf.compare.time_slow perf.compare.time_fast perf.compare.throughput_slow
		perf.compare.throughput_fast)
endif()
