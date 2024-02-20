cmake_minimum_required(VERSION 3.25)
project(jit8 CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(SDL2 REQUIRED)

add_subdirectory(src)

add_executable(jit8 main.cpp)

target_link_libraries(jit8 PUBLIC SDL2::SDL2main SDL2::SDL2 core)
target_compile_definitions(jit8 PUBLIC SDL_MAIN_HANDLED)
target_include_directories(jit8 PUBLIC src externals/xbyak/xbyak)