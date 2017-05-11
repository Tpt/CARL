cmake_minimum_required(VERSION 2.8)
project(carl)

set(CMAKE_CXX_FLAGS "--std=c++14 -Wall -Wextra ${CMAKE_CXX_FLAGS}")

set(SOURCE_FILES triplestore.cpp patterns_using_cardinalities.cpp)
add_executable(carl-patterns_using_cardinalities ${SOURCE_FILES})

set(SOURCE_FILES cardinality_patterns.cpp)
add_executable(carl-cardinality_patterns ${SOURCE_FILES})
