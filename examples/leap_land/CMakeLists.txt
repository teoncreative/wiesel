cmake_minimum_required(VERSION 3.25)
project(leap_land)

add_executable(leap_land w_game.cpp)
target_link_libraries(leap_land PRIVATE wiesel editor)

