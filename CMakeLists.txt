cmake_minimum_required(VERSION 3.22)
project(CPPActressMAS)

set(CMAKE_CXX_STANDARD 20)

find_package(OpenSSL REQUIRED)
find_package(PahoMqttCpp REQUIRED)

include_directories(Externals)
include_directories(CPPActressMAS)

file(GLOB SOURCES
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/*.h**
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/*.hpp
		${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/*.cpp
)

option(BUILD_STATIC "Enable building static library" ON)
option(BUILD_SHARED "Enable building shared library" ON)
option(BUILD_EXAMPLES "Enable building examples" OFF)

if (BUILD_STATIC)
	add_library(CPPActressMASStatic STATIC ${SOURCES})
	target_link_libraries(CPPActressMASStatic OpenSSL::SSL PahoMqttCpp::paho-mqttpp3-static)
	target_link_options(CPPActressMASStatic PRIVATE LINKER:-no_warn_duplicate_libraries)
	if (CMAKE_BUILD_TYPE STREQUAL "Release")
		target_compile_options(CPPActressMASStatic PRIVATE -march=native -O3 -march=native -flto -Wall -Wextra -Wpedantic)
	endif ()
	set_target_properties(CPPActressMASStatic PROPERTIES OUTPUT_NAME CPPActressMAS)
endif ()

if (BUILD_SHARED)
	add_library(CPPActressMAS SHARED ${SOURCES})
	target_link_libraries(CPPActressMAS OpenSSL::SSL PahoMqttCpp::paho-mqttpp3-static)
	target_link_options(CPPActressMAS PRIVATE LINKER:-no_warn_duplicate_libraries)
	if (CMAKE_BUILD_TYPE STREQUAL "Release")
		target_compile_options(CPPActressMASStatic PRIVATE -march=native -O3 -march=native -flto -Wall -Wextra -Wpedantic)
	endif ()
endif ()

if (BUILD_EXAMPLES)
	add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Examples)
endif ()
