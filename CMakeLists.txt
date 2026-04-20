cmake_minimum_required(VERSION 3.16)
project(JVAV)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(jvavc/front)
add_subdirectory(jvavc/back)
add_subdirectory(jvavc/tools)
add_subdirectory(jvm)
add_subdirectory(tests/jvavc/back)
add_subdirectory(tests/jvavc/front)
