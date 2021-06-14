cmake_minimum_required(VERSION 3.19)
project(Filozof3Gerek C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_FLAGS -lpthread)

add_executable(Filozof3Gerek main.c)
target_link_libraries(${PROJECT_NAME} pthread)