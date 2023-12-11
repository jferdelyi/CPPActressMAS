cmake_minimum_required(VERSION 3.22)
project(CPPActressMAS)

set(CMAKE_CXX_STANDARD 20)

include_directories(Externals)
include_directories(CPPActressMAS)

file(GLOB SOURCES ${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/*.h ${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/*.hpp ${CMAKE_CURRENT_LIST_DIR}/CPPActressMAS/*.cpp)

add_library(CPPActressMAS STATIC ${SOURCES})

if(MSVC)
    target_compile_options(CPPActressMAS PRIVATE /W4 /WX /EHsc)
else()
    target_compile_options(CPPActressMAS PRIVATE -Wall -Wextra -Wpedantic -fno-exceptions)
endif()

add_executable(Exemple1 ${CMAKE_CURRENT_LIST_DIR}/Examples/Simple/Exemple1.cpp)
target_link_libraries(Exemple1 CPPActressMAS)

add_executable(Exemple2 ${CMAKE_CURRENT_LIST_DIR}/Examples/Simple/Exemple2.cpp)
target_link_libraries(Exemple2 CPPActressMAS)

add_executable(Exemple3 ${CMAKE_CURRENT_LIST_DIR}/Examples/Simple/Exemple3.cpp)
target_link_libraries(Exemple3 CPPActressMAS)

add_executable(Exemple4 ${CMAKE_CURRENT_LIST_DIR}/Examples/Simple/Exemple4.cpp)
target_link_libraries(Exemple4 CPPActressMAS)

add_executable(Exemple5 ${CMAKE_CURRENT_LIST_DIR}/Examples/Simple/Exemple5.cpp)
target_link_libraries(Exemple5 CPPActressMAS)

add_executable(MultipleMessages ${CMAKE_CURRENT_LIST_DIR}/Examples/Simple/MultipleMessages.cpp)
target_link_libraries(MultipleMessages CPPActressMAS)

add_executable(SendingObjects ${CMAKE_CURRENT_LIST_DIR}/Examples/Simple/SendingObjects.cpp)
target_link_libraries(SendingObjects CPPActressMAS)

add_executable(PingPong ${CMAKE_CURRENT_LIST_DIR}/Examples/Benchmarks/PingPong.cpp)
target_link_libraries(PingPong CPPActressMAS)

add_executable(Skynet ${CMAKE_CURRENT_LIST_DIR}/Examples/Benchmarks/Skynet.cpp)
target_link_libraries(Skynet CPPActressMAS)
