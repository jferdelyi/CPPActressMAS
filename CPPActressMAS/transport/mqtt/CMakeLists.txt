# MQTT transport of CPPActressMAS (cam_transport_mqtt), behind CAM_TRANSPORT_MQTT.
#
# Eclipse Paho is built by the project from pinned archives (D22): Paho MQTT C++ 1.4.0 with its bundled
# Paho MQTT C 1.3.13, static, with SSL (OpenSSL of the system), compiled with the C++ standard of the
# project (B19: a Paho built in C++11 and linked to C++20 code crashes at the first publish on macOS).

include(FetchContent)

if (POLICY CMP0135)
	cmake_policy(SET CMP0135 NEW)  # timestamps of the extracted archives: time of extraction
endif()
# Paho C 1.3.13 still declares cmake_minimum_required(VERSION 3.1), refused by CMake 4: allow it.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

FetchContent_Declare(paho_mqtt_cpp
	URL https://github.com/eclipse/paho.mqtt.cpp/archive/refs/tags/v1.4.0.tar.gz
	URL_HASH SHA256=758c504b585402fbeb47822e52897239bbd53b4236ac3908cc040e09d2a8d8e1
	SOURCE_SUBDIR not-added-here)
FetchContent_MakeAvailable(paho_mqtt_cpp)

# Paho C++ builds Paho C from its externals/paho-mqtt-c directory (empty in the archive): fill it.
FetchContent_Declare(paho_mqtt_c
	URL https://github.com/eclipse/paho.mqtt.c/archive/refs/tags/v1.3.13.tar.gz
	URL_HASH SHA256=47c77e95609812da82feee30db435c3b7c720d4fd3147d466ead126e657b6d9c
	SOURCE_DIR ${paho_mqtt_cpp_SOURCE_DIR}/externals/paho-mqtt-c
	SOURCE_SUBDIR not-added-here)
FetchContent_MakeAvailable(paho_mqtt_c)

set(PAHO_WITH_MQTT_C TRUE CACHE BOOL "Paho C built with Paho C++" FORCE)
set(PAHO_WITH_SSL TRUE CACHE BOOL "Paho with SSL" FORCE)
set(PAHO_BUILD_STATIC TRUE CACHE BOOL "Paho static" FORCE)
set(PAHO_BUILD_SHARED FALSE CACHE BOOL "Paho shared" FORCE)
set(PAHO_BUILD_SAMPLES FALSE CACHE BOOL "" FORCE)
set(PAHO_BUILD_EXAMPLES FALSE CACHE BOOL "" FORCE)
set(PAHO_BUILD_TESTS FALSE CACHE BOOL "" FORCE)
set(PAHO_BUILD_DOCUMENTATION FALSE CACHE BOOL "" FORCE)
set(PAHO_ENABLE_TESTING FALSE CACHE BOOL "" FORCE)
set(PAHO_HIGH_PERFORMANCE TRUE CACHE BOOL "" FORCE)
add_subdirectory(${paho_mqtt_cpp_SOURCE_DIR} ${paho_mqtt_cpp_BINARY_DIR} EXCLUDE_FROM_ALL)

# Paho C++ forces C++11 in its own directory: align its targets on the project (B19).
set_target_properties(paho-mqttpp3-static PROPERTIES CXX_STANDARD ${CMAKE_CXX_STANDARD} CXX_STANDARD_REQUIRED ON)

add_library(cam_transport_mqtt STATIC ${CMAKE_CURRENT_LIST_DIR}/MqttTransport.cpp ${CMAKE_CURRENT_LIST_DIR}/MqttTransport.h)
# Paho headers are a dependency: system headers, their warnings are not the project's (D36).
target_include_directories(cam_transport_mqtt SYSTEM PUBLIC ${paho_mqtt_cpp_SOURCE_DIR}/include)
target_link_libraries(cam_transport_mqtt PUBLIC ${CAM_CORE_TARGET} paho-mqttpp3-static)
# Apple clang searches /usr/local/include before the -isystem directories: a Paho installed there shadows the
# fetched headers (B20). Say so at configure time rather than compile against another copy silently.
if (APPLE AND EXISTS /usr/local/include/mqtt/async_client.h)
	message(WARNING "A Paho MQTT C++ installation in /usr/local/include/mqtt shadows the headers fetched by the project with Apple clang (B20): remove it, or expect its version and its warnings")
endif()
