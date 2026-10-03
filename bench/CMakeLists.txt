# Benchmarks that follow the performance protocol (docs/port/perf/PROTOCOLE.md): same workloads and
# same measured scope as the C# v3.1 E30 benchmarks, atomic counters, steady_clock in microseconds,
# a deterministic variant (--sequential), linked to the static library. Built with -DBUILD_BENCH=ON,
# normally through the "bench" preset (-O3 -DNDEBUG, no -march=native, no -flto).
# Examples/Benchmarks/ stays the faithful port of the C# examples; these are the measurement copies.

if (NOT TARGET CPPActressMASStatic)
	message(FATAL_ERROR "bench/ links to CPPActressMASStatic: configure with -DBUILD_STATIC=ON")
endif()

function(cam_add_bench p_name)
	add_executable(bench_${p_name} ${CMAKE_CURRENT_LIST_DIR}/${p_name}.cpp)
	target_include_directories(bench_${p_name} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
	target_link_libraries(bench_${p_name} CPPActressMASStatic)
endfunction()

cam_add_bench(PingPong)
cam_add_bench(Skynet)
cam_add_bench(SkynetNumeric)

# Smoke tests (label perf): the benchmarks run once and their invariant is checked. Not a measurement.
if (CAM_BUILD_TESTS)
	add_test(NAME perf.smoke.Skynet COMMAND bench_Skynet)
	set_tests_properties(perf.smoke.Skynet PROPERTIES LABELS perf PASS_REGULAR_EXPRESSION "Sum = 49995000" TIMEOUT 300)
	add_test(NAME perf.smoke.SkynetNumeric COMMAND bench_SkynetNumeric)
	set_tests_properties(perf.smoke.SkynetNumeric PROPERTIES LABELS perf PASS_REGULAR_EXPRESSION "Long Sum = 199990000" TIMEOUT 300)
	add_test(NAME perf.smoke.PingPong COMMAND bench_PingPong --sequential --turns 100 --trials 1)
	set_tests_properties(perf.smoke.PingPong PROPERTIES LABELS perf PASS_REGULAR_EXPRESSION "msg/ms" TIMEOUT 300)
endif()
