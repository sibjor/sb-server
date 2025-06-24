cmake_minimum_required(VERSION 3.16)

project(SB-SERVER
        VERSION 1.0
        LANGUAGES C CXX)

# Skapa compile_commands.json för clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C-program
add_executable(sb-server-c main.c)

# C++-program
add_executable(sb-server-cpp main.cpp)
