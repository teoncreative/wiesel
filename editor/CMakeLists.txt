cmake_minimum_required(VERSION 3.25)
project(demo)

set(SOURCES_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INCLUDE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB_RECURSE SOURCES "${SOURCES_DIRECTORY}/*.cpp")
file(GLOB_RECURSE HEADERS "${INCLUDE_DIRECTORY}/*.hpp")

add_library(editor ${SOURCES} ${HEADERS})

target_include_directories(editor PUBLIC ${INCLUDE_DIRECTORY})
target_link_libraries(editor PRIVATE wiesel)
#target_include_assets(editor ${CMAKE_CURRENT_SOURCE_DIR}/assets OFF)