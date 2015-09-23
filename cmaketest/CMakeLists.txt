cmake_minimum_required(VERSION 3.0)

project(cmtst)

set(CMAKE_CXX_FLAGS "-g -Wall")

set(COMMON_INCLUDES ${PROJECT_SOURCE_DIR}/include)

add_subdirectory(src)

