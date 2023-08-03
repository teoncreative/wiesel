cmake_minimum_required(VERSION 3.24)
project(wiesel-parent)

set(CMAKE_CXX_STANDARD 23)

set(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(PARENT_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(VENDOR_DIR ${PARENT_DIR}/vendor)

include(${VENDOR_DIR}/CMakeLists.txt)
add_subdirectory(wiesel)

# examples
add_subdirectory(examples/demo)
