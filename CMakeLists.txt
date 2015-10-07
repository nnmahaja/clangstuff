cmake_minimum_required(VERSION 3.0)

project(clangutils)

set(CMAKE_CXX_FLAGS "-O2 -Wall -std=c++11")

set(COMMON_INCLUDES ${PROJECT_SOURCE_DIR}/include)

add_subdirectory(src)
add_subdirectory(tests)
