cmake_minimum_required(VERSION 3.25)
project(cargame)

add_executable(cargame w_cargame.cpp)
target_link_libraries(cargame PRIVATE wiesel editor)

