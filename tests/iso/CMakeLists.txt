# Oracle tests (label iso): the behaviour of ActressMAS v3.1 (C#). Each C# example is ported as it is, its bugs included,
# in ports/<Project>.cpp, and its output is compared with the output of the C# original (oracles/<Oracle>.txt, produced
# by tools/csharp-ref) on the facts of rules/<Oracle>.rules (README.md).
set(CAM_TEST_TYPE iso)

# ---------------------------------------------------------------- the tools of the harness, and their own tests
add_executable(cam_iso_check iso_check.cpp)
set(CAM_ISO_FIXTURES ${CMAKE_CURRENT_LIST_DIR}/fixtures)

# The comparison tool on fixtures: the same behaviour, then each kind of difference it must report. A case that expects
# a passing verdict runs behind CAM_RUN_CHECKED: a FAIL line or an error exit code fails it.
function(cam_add_iso_check_case p_name p_rules p_oracle p_actual p_expected)
	set(l_arguments ${CAM_ISO_FIXTURES}/${p_rules} ${CAM_ISO_FIXTURES}/${p_oracle} --actual ${CAM_ISO_FIXTURES}/${p_actual})
	if (p_expected MATCHES "^FAIL")
		add_test(NAME iso.check.${p_name} COMMAND cam_iso_check ${l_arguments})
		set_tests_properties(iso.check.${p_name} PROPERTIES LABELS iso PASS_REGULAR_EXPRESSION "${p_expected}")
	else()
		add_test(NAME iso.check.${p_name} COMMAND ${CAM_RUN_CHECKED} $<TARGET_FILE:cam_iso_check> ${l_arguments})
		set_tests_properties(iso.check.${p_name} PROPERTIES LABELS iso PASS_REGULAR_EXPRESSION "${p_expected}"
			FAIL_REGULAR_EXPRESSION "FAIL;cam: exit code")
	endif()
endfunction()
cam_add_iso_check_case(same basic.rules basic.oracle same.txt "same behaviour")
cam_add_iso_check_case(order basic.rules basic.oracle order.txt "FAIL same msg seq")
cam_add_iso_check_case(uncovered basic.rules basic.oracle uncovered.txt "FAIL cover all on C\\+\\+")
cam_add_iso_check_case(missing basic.rules basic.oracle missing.txt "FAIL same total bag")
# "first": a difference after the first tuples is not compared, a difference among them is
cam_add_iso_check_case(first first.rules basic.oracle first.txt "same behaviour")
cam_add_iso_check_case(first_order first.rules basic.oracle order.txt "FAIL same msg seq")
cam_add_iso_check_case(typo typo.rules basic.oracle same.txt "FAIL fact msg: no tuple in the C# oracle")
# "alias": a sender shown as an identifier is paired, a changed content is not
cam_add_iso_check_case(alias alias.rules alias.oracle alias-same.txt "OK   same msg bag \\(4 tuples\\): 1 paired by the alias of column 0\n")
cam_add_iso_check_case(alias_changed alias.rules alias.oracle alias-changed.txt "FAIL same msg bag \\(4 tuples\\): 1 tuples only in C#")
# "aggregate": the sum of a column, the C# against the C++
cam_add_iso_check_case(aggregate aggregate.rules basic.oracle same.txt "same behaviour")
cam_add_iso_check_case(aggregate_missing aggregate.rules basic.oracle missing.txt "FAIL same total aggregate \\(2 tuples\\): sum\\(0\\) 30 in C#, 10 in C\\+\\+")
# "expect": a published value in both outputs
cam_add_iso_check_case(expect expect.rules basic.oracle same.txt "same behaviour")
cam_add_iso_check_case(expect_missing expect.rules basic.oracle expect-missing.txt "FAIL expect msg \\(a\\|b\\|hello\\) on C\\+\\+")
# "each": a predicate on every tuple, with the previous one of its group
cam_add_iso_check_case(each each.rules each.oracle each-same.txt "same behaviour")
cam_add_iso_check_case(each_step each.rules each.oracle each-jump.txt "FAIL check pos each by 0 [^\n]* on C\\+\\+: fails on \\(a\\|3\\) after \\(a\\|1\\)")
cam_add_iso_check_case(each_sum each.rules each.oracle each-jump.txt "FAIL check sum each [^\n]* on C\\+\\+: fails on \\(2\\|3\\|6\\)")

# The .NET behaviour of the ports (DotNet.hpp): number formatting and parsing against the vectors of the C#
# (vectors/format.txt, produced by tools/csharp-ref), and the seeds of the oracles.
cam_add_doctest(dotnet_format cam_dotnet_format_tests dotnet_format.cpp)
target_include_directories(cam_dotnet_format_tests PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_compile_definitions(cam_dotnet_format_tests PRIVATE CAM_VECTORS_DIR="${CMAKE_CURRENT_LIST_DIR}/vectors")
cam_add_doctest(dotnet_site_seed cam_dotnet_seed_tests dotnet_seed.cpp)
target_include_directories(cam_dotnet_seed_tests PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# ---------------------------------------------------------------- the ports of the C# examples
# One program per C# project, named after it: the C# original as it is, its bugs included, on the mechanics of the
# library, with the .NET helpers of DotNet.hpp. Floating point as the C# (-ffp-contract=off: no fused multiply-add).
function(cam_add_iso_port p_project)
	add_executable(iso_${p_project} ports/${p_project}.cpp)
	target_link_libraries(iso_${p_project} ${CAM_TEST_LIBRARY})
	target_include_directories(iso_${p_project} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
	if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
		target_compile_options(iso_${p_project} PRIVATE -ffp-contract=off)
	endif()
endfunction()

foreach (l_project IN ITEMS Agents1 Agents2 Agents3 Agents4 Agents5 MultipleMessages SendingObjects ColorGame NumberGame
		MechanismDesign PrisonerDilemma BDI GameOfLife PingPong Skynet SkynetNumeric Zeuthen Voting EnglishWithBroadcast
		FictitiousPlay ReplicatorDynamics LrtaStar EnglishWithoutBroadcast Vickrey YellowPages Shapley ContractNet
		Reactive PredatorPrey PredatorPreyGui RemoteMessages MasMobileConsole MasMobileGui)
	cam_add_iso_port(${l_project})
endforeach()
# GameOfLife reads its initial pattern from the working directory, as the C#
add_custom_command(TARGET iso_GameOfLife POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/Examples/GameOfLife/patterns.txt $<TARGET_FILE_DIR:iso_GameOfLife>)
# LrtaStar reads its map from the working directory, as the C#: the map files of the C# example (data/README.md), when
# they are in data/
set(CAM_ISO_LRTA_MAPS ${CMAKE_CURRENT_LIST_DIR}/data/Pendulum.grx ${CMAKE_CURRENT_LIST_DIR}/data/Pendulum.grh)
set(CAM_ISO_HAS_LRTA_MAPS ON)
foreach (l_map IN LISTS CAM_ISO_LRTA_MAPS)
	if (NOT EXISTS ${l_map})
		set(CAM_ISO_HAS_LRTA_MAPS OFF)
	endif()
endforeach()
if (CAM_ISO_HAS_LRTA_MAPS)
	add_custom_command(TARGET iso_LrtaStar POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CAM_ISO_LRTA_MAPS} $<TARGET_FILE_DIR:iso_LrtaStar>)
endif()

# Runs the port of a C# project and compares its facts with the output of the C# original (oracles/<oracle>.txt) under
# rules/<oracle>.rules. The oracle is named after the project unless ORACLE names another one (a scenario of the same
# project), the test is iso.<oracle> unless NAME says otherwise; SEED runs it with CAM_SEED=<seed> against the oracle of
# that seed (oracles/seed<seed>/, test iso.<name>.seed<seed>); the other arguments go to the program, which runs from
# its own folder, where its data files are copied.
function(cam_add_iso p_project)
	cmake_parse_arguments(PARSE_ARGV 1 l "" "NAME;ORACLE;SEED" "")
	if (NOT l_ORACLE)
		set(l_ORACLE ${p_project})
	endif()
	if (NOT l_NAME)
		set(l_NAME ${l_ORACLE})
	endif()
	set(l_oracle_file ${CMAKE_CURRENT_LIST_DIR}/oracles/${l_ORACLE}.txt)
	if (DEFINED l_SEED)
		set(l_NAME ${l_NAME}.seed${l_SEED})
		set(l_oracle_file ${CMAKE_CURRENT_LIST_DIR}/oracles/seed${l_SEED}/${l_ORACLE}.txt)
	endif()
	add_test(NAME iso.${l_NAME}
		COMMAND cam_iso_check ${CMAKE_CURRENT_LIST_DIR}/rules/${l_ORACLE}.rules ${l_oracle_file}
			-- $<TARGET_FILE:iso_${p_project}> ${l_UNPARSED_ARGUMENTS}
		WORKING_DIRECTORY $<TARGET_FILE_DIR:iso_${p_project}>)
	set_tests_properties(iso.${l_NAME} PROPERTIES LABELS iso TIMEOUT 300)
	if (DEFINED l_SEED)
		set_property(TEST iso.${l_NAME} APPEND PROPERTY ENVIRONMENT "CAM_SEED=${l_SEED}")
	endif()
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
cam_add_iso(YellowPages)
cam_add_iso(Shapley)
cam_add_iso(ContractNet)
# The example never stops: 100 turns, as its oracle, without the delay of 100 ms after each turn
cam_add_iso(Reactive 100 0)
cam_add_iso(PredatorPrey)
# Without the pause of 100 ms after each turn; both versions write world-state.txt next to the programs, and the GUI
# version prints it back at the end
cam_add_iso(PredatorPreyGui 0)
# E20: one environment per container of the C#, on the loopback transport, stopped once the scenario is over
cam_add_iso(RemoteMessages)
cam_add_iso(MasMobileConsole)
if (CAM_ISO_HAS_LRTA_MAPS)
	cam_add_iso(LrtaStar)
endif()

# The same comparisons with the oracles of two other seeds, for the examples whose output depends on the draws (a
# program fitted to one seed fails them). Not at seed 7: Agents2 (the same output as seed 0), ContractNet (the C# race on
# the shared CNPMessage: a letter assigned twice inside one tour, which the port cannot reproduce without undefined
# behaviour) and EnglishWithoutBroadcast (two bidders at the last price: the winner depends on the order of arrival).
set(CAM_ISO_SEEDED Agents1 Agents2 Agents3 Agents4 Agents5 MultipleMessages SendingObjects ColorGame NumberGame Voting
	EnglishWithBroadcast ReplicatorDynamics FictitiousPlay EnglishWithoutBroadcast Vickrey YellowPages Shapley ContractNet
	SkynetNumeric "Reactive 100 0" PredatorPrey "PredatorPreyGui 0")
foreach (l_seed IN ITEMS 42 7)
	foreach (l_entry IN LISTS CAM_ISO_SEEDED)
		separate_arguments(l_words UNIX_COMMAND "${l_entry}")
		list(GET l_words 0 l_project)
		if (l_seed STREQUAL "7" AND l_project MATCHES "^(Agents2|ContractNet|EnglishWithoutBroadcast)$")
			continue()
		endif()
		cam_add_iso(${l_words} SEED ${l_seed})
	endforeach()
	if (CAM_ISO_HAS_LRTA_MAPS)
		cam_add_iso(LrtaStar SEED ${l_seed})
	endif()
endforeach()
set_tests_properties(iso.PredatorPrey iso.PredatorPreyGui iso.PredatorPrey.seed42 iso.PredatorPreyGui.seed42 iso.PredatorPrey.seed7
	iso.PredatorPreyGui.seed7 PROPERTIES RESOURCE_LOCK world-state.txt)

# The variants of the oracles: the original sources but one line (tests/tools/csharp-ref/ref/Oracles/<Variant>/), the
# variants commented in the C# examples, and a sequential version of the parallel ones, where the C# fixes every line;
# the port takes the argument of the variant (DotNet.hpp, scenario_mode and has_scenario)
foreach (l_variant IN ITEMS "Agents5 Agents5Seq sequential" "Agents3 Agents3Seq sequential" "PrisonerDilemma PrisonerDilemmaConfess confess"
		"PrisonerDilemma PrisonerDilemmaRandom random" "Vickrey VickreySeqRandom sequential-random" "SkynetNumeric SkynetNumericParallel parallel"
		"Skynet SkynetSeqRandom sequential-random" "PingPong PingPongSeqRandom sequential-random" "Shapley ShapleySeq sequential"
		"ContractNet ContractNetSeq sequential" "YellowPages YellowPagesSeq sequential" "FictitiousPlay FictitiousPlaySeq sequential"
		"ReplicatorDynamics ReplicatorDynamicsSeq sequential" "EnglishWithoutBroadcast EnglishWithoutBroadcastSeq sequential"
		"MultipleMessages MultipleMessagesSeq sequential" "Agents1 Agents1Seq sequential" "SendingObjects SendingObjectsSeq sequential"
		"Reactive ReactiveSeq 100 0 sequential")
	separate_arguments(l_words UNIX_COMMAND "${l_variant}")
	list(POP_FRONT l_words l_project l_oracle)
	cam_add_iso(${l_project} ORACLE ${l_oracle} ${l_words})
endforeach()

# ---------------------------------------------------------------- distributions
# Where the C# itself is not deterministic: statistics of 30 runs of the port (CAM_SEED 1 to 30) against those of its
# C# original (distributions/<Oracle>.txt, from tools/csharp-ref distrib, and the spec of the statistics), by a
# two-sample Kolmogorov-Smirnov test; with the generator of the oracles (dotnet) and with the default one of the
# library (mt19937), so that the equivalence does not rest on the reproduction of the draws. Reactive is not among them:
# the number of its exchanges per turn depends on the scheduling (a known divergence of throughput), and its logic is
# compared line by line by its sequential variant. Long: not under the sanitizers.
add_executable(cam_iso_stats iso_stats.cpp)
add_test(NAME iso.stats.self COMMAND ${CAM_RUN_CHECKED} $<TARGET_FILE:cam_iso_stats> --self-test)
set_tests_properties(iso.stats.self PROPERTIES LABELS iso PASS_REGULAR_EXPRESSION "self-test ok" FAIL_REGULAR_EXPRESSION "FAIL;cam: exit code")
if (NOT CAM_SANITIZER)
	foreach (l_example IN ITEMS PredatorPrey ContractNet Shapley)
		foreach (l_engine IN ITEMS dotnet mt19937)
			add_test(NAME iso.${l_example}.distribution.${l_engine}
				COMMAND cam_iso_stats ${CMAKE_CURRENT_LIST_DIR}/distributions/${l_example}.spec ${CMAKE_CURRENT_LIST_DIR}/distributions/${l_example}.txt 30
					-- $<TARGET_FILE:iso_${l_example}>
				WORKING_DIRECTORY $<TARGET_FILE_DIR:iso_${l_example}>)
			set_tests_properties(iso.${l_example}.distribution.${l_engine} PROPERTIES LABELS "iso;stat" TIMEOUT 900
				ENVIRONMENT "CAM_RANDOM_ENGINE=${l_engine}")
		endforeach()
	endforeach()
	set_tests_properties(iso.PredatorPrey.distribution.dotnet iso.PredatorPrey.distribution.mt19937 PROPERTIES RESOURCE_LOCK world-state.txt)
endif()

# ---------------------------------------------------------------- oracles published with the examples
# The game of life of the port of E13 against the patterns of Gardner (1970), generation by generation
cam_add_doctest(gardner cam_iso_gardner gardner.cpp)
target_compile_definitions(cam_iso_gardner PRIVATE CAM_GAME_OF_LIFE="$<TARGET_FILE:iso_GameOfLife>")
add_dependencies(cam_iso_gardner iso_GameOfLife)
set_tests_properties(iso.gardner PROPERTIES TIMEOUT 900)
# The calculator of E04 alone on the examples of the Shapley value: 2 players (1, 3, 6: 2 and 4) and 3 players (0, 0, 0,
# 2, 4, 6, 7: 4/3, 7/3, 10/3), normalized; the calculator takes times, the inverse of the values (1e9 for a value of 0)
add_test(NAME iso.shapley.two COMMAND ${CAM_RUN_CHECKED} $<TARGET_FILE:iso_Shapley>
	calculate 1 0.3333333333333333 1000 0.16666666666666666 1000 1000 1000)
set_tests_properties(iso.shapley.two PROPERTIES LABELS iso
	PASS_REGULAR_EXPRESSION "\\[shapley -> manager\\]: results 0\\.333333333333333[0-9]* 0\\.666666666666666[0-9]* 0\n")
add_test(NAME iso.shapley.three COMMAND ${CAM_RUN_CHECKED} $<TARGET_FILE:iso_Shapley>
	calculate 1000000000 1000000000 1000000000 0.5 0.25 0.16666666666666666 0.14285714285714285)
set_tests_properties(iso.shapley.three PROPERTIES LABELS iso
	PASS_REGULAR_EXPRESSION "\\[shapley -> manager\\]: results 0\\.190476190[0-9]* 0\\.33333333[0-9]* 0\\.476190476[0-9]*\n")

# Millions of calls: with gcc, whose ASan detects stack-use-after-return by default (fake stacks), GameOfLife (390 000
# calls of the perception filter per turn) took more than 1 200 s, 39 s without, and PingPong (10 million messages per
# trial) more than 300 s, 37 s without; clang leaves that detection off by default. Predator-Prey too (thousands of
# insects, small vectors at each move): the GUI version more than 300 s, 3 s without
if (CAM_SANITIZER STREQUAL "asan")
	set_property(TEST iso.GameOfLife iso.gardner iso.PingPong iso.PingPongSeqRandom iso.PredatorPrey iso.PredatorPreyGui iso.PredatorPrey.seed42
				 iso.PredatorPreyGui.seed42 iso.PredatorPrey.seed7 iso.PredatorPreyGui.seed7 APPEND PROPERTY ENVIRONMENT
				 "ASAN_OPTIONS=detect_stack_use_after_return=0")
endif()
# Ten million messages in a sequential order take about four times the parallel PingPong: more than 300 s under TSan.
# The game of life of Gardner took 885 s alone under the TSan of gcc, close to its 900 s on a loaded host
if (CAM_SANITIZER)
	set_tests_properties(iso.PingPongSeqRandom iso.gardner PROPERTIES TIMEOUT 1500)
endif()
# MSVC in Debug (no optimisation, checked iterators): on the Windows runner of the CI, PingPong took 116 s, so
# PingPongSeqRandom passed 300 s, and GameOfLife took 226 s, close to its 300 s
if (MSVC)
	set_tests_properties(iso.PingPongSeqRandom iso.GameOfLife PROPERTIES TIMEOUT 900)
endif()
# A test that passes on a regular expression ignores its exit code: an error exit code or a report of a sanitizer still
# fails it
cam_fail_on_sanitizer_report(iso.shapley.two iso.shapley.three)
