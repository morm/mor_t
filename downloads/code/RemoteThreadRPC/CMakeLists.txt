project(paramRPC)
cmake_minimum_required(VERSION 2.8) 

SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_UNICODE -DUNICODE)

add_subdirectory("A")
add_subdirectory("B")