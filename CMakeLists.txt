cmake_minimum_required(VERSION 4.2.1)

set(CMAKE_CXX_STANDARD 20)

set(LUAU_BUILD_TESTS OFF)
set(LUAU_BUILD_CLI OFF)
set(LUAU_STATIC_CRT ON)
set(LUAU_EXTERN_C ON)
add_subdirectory(luau/)

project(bundler)
add_executable(bundler src/main.cpp)
target_link_libraries(bundler PUBLIC Luau.Ast)

# quick testing of non-luau dependant code
project(fast)
add_executable(fast src/fast.cpp)
