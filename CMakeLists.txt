cmake_minimum_required(VERSION 3.22)
project(CPPActressMAS)

set(CMAKE_CXX_STANDARD 20)

include_directories(Externals)
include_directories(CPPActressMAS)

# The core has no network dependency: the transports are separate libraries behind options (D1, D22).
file(GLOB SOURCES
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/*.h**
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/*.hpp
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/*.cpp
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/transport/*.h
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/transport/*.cpp
)
# The facade knows which transports are compiled in: it is a library of its own (cam_transport).
list(REMOVE_ITEM SOURCES
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/transport/Facade.h
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/transport/Facade.cpp
)

option(BUILD_STATIC "Enable building static library" ON)
option(BUILD_SHARED "Enable building shared library" ON)
option(BUILD_EXAMPLES "Enable building examples" OFF)
option(BUILD_BENCH "Enable building benchmarks (bench/, performance protocol)" OFF)
option(CAM_BUILD_TESTS "Enable building tests (tests/, CTest)" ${PROJECT_IS_TOP_LEVEL})
option(CAM_TRANSPORT_MQTT "Build the MQTT transport (Eclipse Paho fetched and built by the project, needs OpenSSL)" OFF)
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

if (CAM_TRANSPORT_MQTT)
	add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/transport/mqtt)
endif()
if (CAM_TRANSPORT_ZENOH)
	add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/transport/zenoh)
endif()

# Facade: cam::transport::open(url) and cam::transport::connect(environment, url) over the transports
# compiled in (loopback always, mqtt and mqtts with CAM_TRANSPORT_MQTT, zenoh with CAM_TRANSPORT_ZENOH).
add_library(cam_transport STATIC
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/transport/Facade.h
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/transport/Facade.cpp
)
target_link_libraries(cam_transport PUBLIC ${CAM_CORE_TARGET})
if (CAM_TRANSPORT_MQTT)
	target_compile_definitions(cam_transport PRIVATE CAM_TRANSPORT_MQTT=1)
	target_link_libraries(cam_transport PUBLIC cam_transport_mqtt)
endif()
if (CAM_TRANSPORT_ZENOH)
	target_compile_definitions(cam_transport PRIVATE CAM_TRANSPORT_ZENOH=1)
	target_link_libraries(cam_transport PUBLIC cam_transport_zenoh)
endif()

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
