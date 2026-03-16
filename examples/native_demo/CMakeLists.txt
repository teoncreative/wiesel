cmake_minimum_required(VERSION 3.25)
project(native_demo)

add_executable(native_demo native_demo.cpp)
# You don't have to add the editor
target_link_libraries(native_demo PRIVATE wiesel lib-wiesel-editor)

