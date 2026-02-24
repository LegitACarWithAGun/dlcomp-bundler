cmake_minimum_required(VERSION 4.2.1)
set(CMAKE_CXX_STANDARD 20)

option(LUAU_BUILD_TESTS OFF)
option(LUAU_BUILD_CLI OFF)
option(LUAU_STATIC_CRT ON)
option(LUAU_EXTERN_C ON)
add_subdirectory(extern/luau/)

# quick testing of non-luau dependant code
project(fast)
add_executable(fast src/fast.cpp)

# tests
project(test)
add_executable(test test/main.cpp)
target_link_libraries(test PUBLIC Luau.Ast)
target_include_directories(test PRIVATE src/)

# put executables in ./bin
# any projects above this will still be put in ./build
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

project(bundler)
add_executable(bundler src/main.cpp)
target_link_libraries(bundler PUBLIC Luau.Ast)
target_include_directories(bundler PUBLIC extern/single/)
