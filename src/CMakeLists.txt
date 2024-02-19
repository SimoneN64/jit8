cmake_minimum_required(VERSION 3.25)
project(core)

add_library(core Chip8.cpp Chip8.hpp)

target_include_directories(core PRIVATE .)