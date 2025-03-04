cmake_minimum_required(VERSION 3.22)
project(CPPActressMASExamples)

set(CMAKE_CXX_STANDARD 20)

# Fonction pour appliquer les options de compilation
function(apply_compile_options target)
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        if (WIN32)
            target_compile_options(${target} PRIVATE /O2 /W4 /LTCG /arch:AVX2)
        else()
            target_compile_options(${target} PRIVATE -march=native -O3 -flto -Wall -Wextra -Wpedantic)
            target_link_options(${target} PRIVATE LINKER:-no_warn_duplicate_libraries)
        endif()
    endif()
endfunction()

add_executable(Exemple1 ${CMAKE_CURRENT_LIST_DIR}/Simples/Exemple1.cpp)
target_link_libraries(Exemple1 CPPActressMAS)
apply_compile_options(Exemple1)

add_executable(Exemple2 ${CMAKE_CURRENT_LIST_DIR}/Simples/Exemple2.cpp)
target_link_libraries(Exemple2 CPPActressMAS)
apply_compile_options(Exemple2)

add_executable(Exemple3 ${CMAKE_CURRENT_LIST_DIR}/Simples/Exemple3.cpp)
target_link_libraries(Exemple3 CPPActressMAS)
apply_compile_options(Exemple3)

add_executable(Exemple4 ${CMAKE_CURRENT_LIST_DIR}/Simples/Exemple4.cpp)
target_link_libraries(Exemple4 CPPActressMAS)
apply_compile_options(Exemple4)

add_executable(Exemple5 ${CMAKE_CURRENT_LIST_DIR}/Simples/Exemple5.cpp)
target_link_libraries(Exemple5 CPPActressMAS)
apply_compile_options(Exemple5)

add_executable(MultipleMessages ${CMAKE_CURRENT_LIST_DIR}/Simples/MultipleMessages.cpp)
target_link_libraries(MultipleMessages CPPActressMAS)
apply_compile_options(MultipleMessages)

add_executable(SendingObjects ${CMAKE_CURRENT_LIST_DIR}/Simples/SendingObjects.cpp)
target_link_libraries(SendingObjects CPPActressMAS)
apply_compile_options(SendingObjects)

add_executable(PingPong ${CMAKE_CURRENT_LIST_DIR}/Benchmarks/PingPong.cpp)
target_link_libraries(PingPong CPPActressMAS)
apply_compile_options(PingPong)

add_executable(Skynet ${CMAKE_CURRENT_LIST_DIR}/Benchmarks/Skynet.cpp)
target_link_libraries(Skynet CPPActressMAS)
apply_compile_options(Skynet)

add_executable(NumberGame ${CMAKE_CURRENT_LIST_DIR}/Observables/NumberGame.cpp)
target_link_libraries(NumberGame CPPActressMAS)
apply_compile_options(NumberGame)

add_executable(ColorGame ${CMAKE_CURRENT_LIST_DIR}/Observables/ColorGame.cpp)
target_link_libraries(ColorGame CPPActressMAS)
apply_compile_options(ColorGame)

add_executable(Serialization ${CMAKE_CURRENT_LIST_DIR}/Serialization/Serialization.cpp)
target_link_libraries(Serialization CPPActressMAS)
apply_compile_options(Serialization)

add_executable(Mobile ${CMAKE_CURRENT_LIST_DIR}/Mobiles/Mobile.cpp)
target_link_libraries(Mobile CPPActressMAS)
apply_compile_options(Mobile)

add_executable(MobilePingPong ${CMAKE_CURRENT_LIST_DIR}/Mobiles/PingPong.cpp)
target_link_libraries(MobilePingPong CPPActressMAS)
apply_compile_options(MobilePingPong)

add_executable(Reactive ${CMAKE_CURRENT_LIST_DIR}/MAS/Reactive.cpp)
target_link_libraries(Reactive CPPActressMAS)
apply_compile_options(Reactive)
