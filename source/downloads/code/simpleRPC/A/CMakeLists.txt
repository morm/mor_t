project(A)
cmake_minimum_required(VERSION 2.8)

enable_language(CXX)

SET(SOURCES main_A.cpp)
SET(HEADERS rpc_caller.hpp)

add_executable(A ${SOURCES} ${HEADERS})