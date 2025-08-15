cmake_minimum_required(VERSION 3.25)
project(demo)

add_executable(demo w_demo.cpp)
target_link_libraries(demo PRIVATE wiesel editor)

target_include_assets(demo "${CMAKE_CURRENT_SOURCE_DIR}/assets" OFF)
target_include_engine_assets(demo)
