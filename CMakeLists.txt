#cmake_minimum_required(VERSION 3.3)
cmake_minimum_required(VERSION 2.8.12.2)

project(pargrep)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z")

add_subdirectory(external/benchmark)

set(SOURCE_FILES src/pargrep.cpp src/regex_functions.cpp src/regex_functions.h)

add_executable(benchmarks ${SOURCE_FILES} src/benchmarks.cpp)
target_link_libraries(benchmarks PUBLIC benchmark)

add_executable(prep ${SOURCE_FILES} src/main.cpp)


