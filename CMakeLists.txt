cmake_minimum_required(VERSION 3.16.0)

project(test)

add_executable(test test.c)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/isotp)
