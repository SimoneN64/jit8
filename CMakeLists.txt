cmake_minimum_required(VERSION 3.25)

project(jit8 CXX)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(src)

add_executable(jit8 main.cpp)
target_link_libraries(jit8 PUBLIC core)
target_include_directories(jit8 PUBLIC src)