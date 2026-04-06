cmake_minimum_required(VERSION 3.25)
project(editor)

set(SOURCES_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INCLUDE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB_RECURSE SOURCES "${SOURCES_DIRECTORY}/*.cc")
file(GLOB_RECURSE HEADERS "${INCLUDE_DIRECTORY}/*.h")

# Standalone editor executable
set(TEXTEDIT_DIR ${VENDOR_DIR}/ImGuiColorTextEdit)
if (WIN32)
    set(EDITOR_RC ${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc)
endif ()
add_executable(wiesel-editor ${SOURCES} ${HEADERS} ${TEXTEDIT_DIR}/TextEditor.cpp ${EDITOR_RC})
target_include_directories(wiesel-editor PRIVATE ${INCLUDE_DIRECTORY} ${TEXTEDIT_DIR})
target_link_libraries(wiesel-editor PRIVATE wiesel)