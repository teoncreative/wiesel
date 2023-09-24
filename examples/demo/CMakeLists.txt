cmake_minimum_required(VERSION 3.25)
project(demo)

set(CMAKE_CXX_STANDARD 23)
#set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")

add_executable(demo w_demo.cpp)
target_link_libraries(demo PRIVATE wiesel)

target_include_assets(demo ${CMAKE_CURRENT_SOURCE_DIR}/assets OFF)