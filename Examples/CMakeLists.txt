cmake_minimum_required(VERSION 3.22)
project(CPPActressMASExamples)

set(CMAKE_CXX_STANDARD 20)

add_executable(Exemple1 ${CMAKE_CURRENT_LIST_DIR}/Simples/Exemple1.cpp)
target_link_libraries(Exemple1 CPPActressMAS)
#target_compile_options(Exemple1 PRIVATE -march=native -O3 -march=native -flto -Wall -Wextra -Wpedantic)

add_executable(Exemple2 ${CMAKE_CURRENT_LIST_DIR}/Simples/Exemple2.cpp)
target_link_libraries(Exemple2 CPPActressMAS)
target_compile_options(Exemple2 PRIVATE -march=native -O3 -march=native -flto -Wall -Wextra -Wpedantic)

add_executable(Exemple3 ${CMAKE_CURRENT_LIST_DIR}/Simples/Exemple3.cpp)
target_link_libraries(Exemple3 CPPActressMAS)
target_compile_options(Exemple3 PRIVATE -march=native -O3 -march=native -flto -Wall -Wextra -Wpedantic)

add_executable(Exemple4 ${CMAKE_CURRENT_LIST_DIR}/Simples/Exemple4.cpp)
target_link_libraries(Exemple4 CPPActressMAS)
target_compile_options(Exemple4 PRIVATE -march=native -O3 -march=native -flto -Wall -Wextra -Wpedantic)

add_executable(Exemple5 ${CMAKE_CURRENT_LIST_DIR}/Simples/Exemple5.cpp)
target_link_libraries(Exemple5 CPPActressMAS)
target_compile_options(Exemple5 PRIVATE -march=native -O3 -march=native -flto -Wall -Wextra -Wpedantic)

add_executable(MultipleMessages ${CMAKE_CURRENT_LIST_DIR}/Simples/MultipleMessages.cpp)
target_link_libraries(MultipleMessages CPPActressMAS)
target_compile_options(MultipleMessages PRIVATE -march=native -O3 -march=native -flto -Wall -Wextra -Wpedantic)

add_executable(SendingObjects ${CMAKE_CURRENT_LIST_DIR}/Simples/SendingObjects.cpp)
target_link_libraries(SendingObjects CPPActressMAS)
target_compile_options(SendingObjects PRIVATE -march=native -O3 -march=native -flto -Wall -Wextra -Wpedantic)

add_executable(PingPong ${CMAKE_CURRENT_LIST_DIR}/Benchmarks/PingPong.cpp)
target_link_libraries(PingPong CPPActressMAS)
target_link_options(PingPong PRIVATE LINKER:-no_warn_duplicate_libraries)
target_compile_options(PingPong PRIVATE -march=native -O3 -march=native -flto -Wall -Wextra -Wpedantic)

add_executable(Skynet ${CMAKE_CURRENT_LIST_DIR}/Benchmarks/Skynet.cpp)
target_link_libraries(Skynet CPPActressMAS)
target_link_options(Skynet PRIVATE LINKER:-no_warn_duplicate_libraries)
target_compile_options(Skynet PRIVATE -march=native -O3 -march=native -flto -Wall -Wextra -Wpedantic)

add_executable(NumberGame ${CMAKE_CURRENT_LIST_DIR}/Observables/NumberGame.cpp)
target_link_libraries(NumberGame CPPActressMAS)
target_compile_options(NumberGame PRIVATE -march=native -O3 -march=native -flto -Wall -Wextra -Wpedantic)

add_executable(ColorGame ${CMAKE_CURRENT_LIST_DIR}/Observables/ColorGame.cpp)
target_link_libraries(ColorGame CPPActressMAS)
#target_compile_options(ColorGame PRIVATE -march=native -O3 -march=native -flto -Wall -Wextra -Wpedantic)

add_executable(Serialization ${CMAKE_CURRENT_LIST_DIR}/Serialization/Serialization.cpp)
target_link_libraries(Serialization CPPActressMAS)
#target_compile_options(Serialization PRIVATE -march=native -O3 -march=native -flto -Wall -Wextra -Wpedantic)

add_executable(Mobile ${CMAKE_CURRENT_LIST_DIR}/Mobiles/Mobile.cpp)
target_link_libraries(Mobile CPPActressMAS)
target_link_options(Mobile PRIVATE LINKER:-no_warn_duplicate_libraries)


add_executable(MobilePingPong ${CMAKE_CURRENT_LIST_DIR}/Mobiles/PingPong.cpp)
target_link_libraries(MobilePingPong CPPActressMAS)

add_executable(Reactive ${CMAKE_CURRENT_LIST_DIR}/MAS/Reactive.cpp)
target_link_libraries(Reactive CPPActressMAS)
