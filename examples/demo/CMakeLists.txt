cmake_minimum_required(VERSION 3.25)
project(demo)

set(CMAKE_CXX_STANDARD 23)

add_executable(demo w_demo.cpp)
target_link_libraries(demo PRIVATE wiesel)
