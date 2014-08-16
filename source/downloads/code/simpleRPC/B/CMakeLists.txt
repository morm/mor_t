project(B)
cmake_minimum_required(VERSION 2.8)

enable_language(CXX)

SET(SOURCES main_B.cpp)

add_executable(B WIN32 ${SOURCES} ${HEADERS} export.def)