cmake_minimum_required(VERSION 3.22)
project(CPPActressMAS)

set(CMAKE_CXX_STANDARD 20)

# Third-party libraries are git submodules (ThirdParty/README.md): stop with a clear message when they are not checked out
foreach (l_header json/single_include/nlohmann/json.hpp ordered-map/include/tsl/ordered_map.h)
	if (NOT EXISTS ${CMAKE_CURRENT_LIST_DIR}/ThirdParty/${l_header})
		message(FATAL_ERROR "ThirdParty/${l_header} is missing: run git submodule update --init --depth 1")
	endif()
endforeach()
include_directories(ThirdParty/json/single_include ThirdParty/ordered-map/include)
include_directories(CPPActressMAS)

# The core has no network dependency by default: each network transport joins the library behind its
# option (D1, D22, D48), with its own dependencies, and Environment::connect(url) selects the schemes
# compiled in (CAM_TRANSPORT_* definitions, no registry).
file(GLOB SOURCES
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/*.h**
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/*.hpp
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/*.cpp
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/transport/*.h
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/transport/*.cpp
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/transport/actressmas/*.h
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/transport/actressmas/*.cpp
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/transport/codec/*.h
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/transport/codec/*.cpp
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/transport/loopback/*.h
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/transport/loopback/*.cpp
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/transport/network/*.h
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/transport/network/*.cpp
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/util/*.h
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/util/*.hpp
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/util/*.cpp
)

option(BUILD_STATIC "Enable building static library" ON)
option(BUILD_SHARED "Enable building shared library" ON)
option(BUILD_EXAMPLES "Enable building examples" OFF)
option(BUILD_BENCH "Enable building benchmarks (bench/, performance protocol)" OFF)
option(CAM_BUILD_TESTS "Enable building tests (tests/, CTest)" ${PROJECT_IS_TOP_LEVEL})
option(CAM_TRANSPORT_MQTT "Build the MQTT transport (libmosquitto fetched and built by the project, needs OpenSSL)" OFF)
option(CAM_TRANSPORT_ZENOH "Build the Zenoh transport (zenoh-c and zenoh-cpp 1.7.1 from the system)" OFF)

if (BUILD_STATIC)
	add_library(CPPActressMASStatic STATIC ${SOURCES})
	if (CMAKE_BUILD_TYPE STREQUAL "Release")
		if (WIN32)
			target_compile_options(CPPActressMASStatic PRIVATE /O2 /W4 /LTCG)
		else()
			target_compile_options(CPPActressMASStatic PRIVATE -march=native -O3 -flto -Wall -Wextra -Wpedantic)
		endif()
	endif()
	set_target_properties(CPPActressMASStatic PROPERTIES OUTPUT_NAME CPPActressMAS)
endif()

if (BUILD_SHARED)
	add_library(CPPActressMAS SHARED ${SOURCES})
	if (CMAKE_BUILD_TYPE STREQUAL "Release")
		if (WIN32)
			target_compile_options(CPPActressMAS PRIVATE /O2 /W4 /LTCG)
		else()
			target_compile_options(CPPActressMAS PRIVATE -march=native -O3 -flto -Wall -Wextra -Wpedantic)
			# Objects compiled with -flto are LLVM bitcode under clang: GNU ld needs -flto at link time too (B18).
			target_link_options(CPPActressMAS PRIVATE -flto)
		endif()
	endif()
endif()

# Core target the transports, the facade, the examples and the tests link to.
if (TARGET CPPActressMAS)
	set(CAM_CORE_TARGET CPPActressMAS)
else()
	set(CAM_CORE_TARGET CPPActressMASStatic)
endif()

# Network transports: each subdirectory declares its sources, include directories and libraries
# (CAM_<NAME>_SOURCES, CAM_<NAME>_INCLUDES, CAM_<NAME>_LIBRARIES, CAM_<NAME>_DEFINITIONS) and the core targets
# take them.
set(CAM_CORE_TARGETS)
foreach (l_target IN ITEMS CPPActressMASStatic CPPActressMAS)
	if (TARGET ${l_target})
		list(APPEND CAM_CORE_TARGETS ${l_target})
	endif()
endforeach()
function(cam_add_transport p_name p_define)
	foreach (l_target IN LISTS CAM_CORE_TARGETS)
		target_sources(${l_target} PRIVATE ${CAM_${p_name}_SOURCES})
		if (CAM_${p_name}_INCLUDES)
			target_include_directories(${l_target} SYSTEM PRIVATE ${CAM_${p_name}_INCLUDES})
		endif()
		target_link_libraries(${l_target} PRIVATE ${CAM_${p_name}_LIBRARIES})
		target_compile_definitions(${l_target} PRIVATE ${p_define}=1 ${CAM_${p_name}_DEFINITIONS})
	endforeach()
endfunction()
if (CAM_TRANSPORT_MQTT)
	add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/transport/mqtt)
	cam_add_transport(MQTT CAM_TRANSPORT_MQTT)
endif()
if (CAM_TRANSPORT_ZENOH)
	add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/transport/zenoh)
	cam_add_transport(ZENOH CAM_TRANSPORT_ZENOH)
endif()

# Compatibility name of the former facade library (1C to 2B): the facade is part of the core now.
add_library(cam_transport INTERFACE)
target_link_libraries(cam_transport INTERFACE ${CAM_CORE_TARGET})

if (BUILD_EXAMPLES)
	add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Examples)
endif()

if (CAM_BUILD_TESTS)
	enable_testing()
	add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tests)
endif()

if (BUILD_BENCH)
	add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/bench)
endif()
