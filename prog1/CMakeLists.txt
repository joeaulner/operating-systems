cmake_minimum_required(VERSION 3.6)
project(prog1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES main.c)
add_executable(prog1 ${SOURCE_FILES})