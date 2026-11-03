# MQTT transport of CPPActressMAS, behind CAM_TRANSPORT_MQTT (sources joined to the core library).
#
# libmosquitto is built by the project from a pinned archive: Eclipse Mosquitto 2.0.22, client library
# only (no broker, no clients, no cJSON, no C++ wrapper), static, threaded (the simulation thread publishes
# while the loop thread reads), with TLS (OpenSSL of the system). The project publishes no checksum: the
# SHA-256 below was computed on the GitHub tag archive the day it was pinned (2026-10-04).

include(FetchContent)

if (POLICY CMP0135)
	cmake_policy(SET CMP0135 NEW)  # timestamps of the extracted archive: time of extraction
endif()

FetchContent_Declare(libmosquitto
	URL https://github.com/eclipse-mosquitto/mosquitto/archive/refs/tags/v2.0.22.tar.gz
	URL_HASH SHA256=a2850bd168cd9f1baa954c30994167f9f8c54ee4966ef34738620a48f63657d5
	SOURCE_SUBDIR not-added-here)
FetchContent_MakeAvailable(libmosquitto)

set(WITH_BROKER OFF CACHE BOOL "Mosquitto broker" FORCE)
set(WITH_CLIENTS OFF CACHE BOOL "Mosquitto command line clients" FORCE)
set(WITH_APPS OFF CACHE BOOL "Mosquitto apps" FORCE)
set(WITH_PLUGINS OFF CACHE BOOL "Mosquitto plugins" FORCE)
set(WITH_CJSON OFF CACHE BOOL "Mosquitto cJSON support" FORCE)
set(DOCUMENTATION OFF CACHE BOOL "Mosquitto documentation" FORCE)
set(WITH_LIB_CPP OFF CACHE BOOL "Mosquitto C++ wrapper" FORCE)
set(WITH_STATIC_LIBRARIES ON CACHE BOOL "libmosquitto static" FORCE)
set(WITH_PIC ON CACHE BOOL "libmosquitto static with PIC" FORCE)
set(WITH_THREADING ON CACHE BOOL "libmosquitto locks (publish from another thread)" FORCE)
set(WITH_TLS ON CACHE BOOL "libmosquitto TLS" FORCE)
set(WITH_TLS_PSK OFF CACHE BOOL "" FORCE)
set(WITH_SRV OFF CACHE BOOL "" FORCE)
set(WITH_SOCKS OFF CACHE BOOL "" FORCE)
set(WITH_UNIX_SOCKETS OFF CACHE BOOL "" FORCE)
set(WITH_DLT OFF CACHE BOOL "" FORCE)
add_subdirectory(${libmosquitto_SOURCE_DIR} ${libmosquitto_BINARY_DIR} EXCLUDE_FROM_ALL)

# Sources and dependencies taken by the core targets (root CMakeLists.txt, cam_add_transport). libmosquitto
# headers are a dependency: system headers, their warnings are not the project's.
set(CAM_MQTT_SOURCES ${CMAKE_CURRENT_LIST_DIR}/MqttTransport.cpp ${CMAKE_CURRENT_LIST_DIR}/MqttTransport.h PARENT_SCOPE)
set(CAM_MQTT_INCLUDES ${libmosquitto_SOURCE_DIR}/include PARENT_SCOPE)
set(CAM_MQTT_LIBRARIES libmosquitto_static PARENT_SCOPE)
