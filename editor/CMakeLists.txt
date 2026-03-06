cmake_minimum_required(VERSION 3.25)
project(editor)

set(SOURCES_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INCLUDE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Editor library (used by game executables)
add_library(editor
    ${SOURCES_DIRECTORY}/w_editor.cpp
    ${INCLUDE_DIRECTORY}/w_editor.hpp
)
target_include_directories(editor PUBLIC ${INCLUDE_DIRECTORY})
target_link_libraries(editor PRIVATE wiesel)

# Standalone editor executable
add_executable(wiesel-editor
    ${SOURCES_DIRECTORY}/w_editor_app.cpp
    ${SOURCES_DIRECTORY}/w_editor.cpp
    ${INCLUDE_DIRECTORY}/w_editor.hpp
)
target_include_directories(wiesel-editor PRIVATE ${INCLUDE_DIRECTORY})
target_link_libraries(wiesel-editor PRIVATE wiesel)