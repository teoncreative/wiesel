cmake_minimum_required(VERSION 3.25)
project(editor)

set(SOURCES_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INCLUDE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB_RECURSE SOURCES "${SOURCES_DIRECTORY}/*.cpp")
file(GLOB_RECURSE HEADERS "${INCLUDE_DIRECTORY}/*.hpp")

# Standalone editor executable
add_executable(wiesel-editor ${SOURCES} ${HEADERS})
target_include_directories(wiesel-editor PRIVATE ${INCLUDE_DIRECTORY})
target_link_libraries(wiesel-editor PRIVATE wiesel)