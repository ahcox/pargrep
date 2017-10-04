#cmake_minimum_required(VERSION 3.3)
cmake_minimum_required(VERSION 2.8.12.2)

project(pargrep)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z")

add_subdirectory(external/benchmark)

set(SOURCE_FILES src/pargrep.cpp src/main.cpp)
add_executable(prep ${SOURCE_FILES})

#target_link_libraries(prep PUBLIC benchmark)
