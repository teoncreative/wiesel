##organize this messy cmake file!

cmake_minimum_required(VERSION 3.24)
project(Wiesel)

set(CMAKE_CXX_STANDARD 23)

# todo other platforms

if(UNIX)
    EXECUTE_PROCESS( COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE DEVICEARCHITECTURE )
    message( STATUS "Arch: ${DEVICEARCHITECTURE}" )
endif()

message(STATUS "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -Wuninitialized -Wmultichar -ffunction-sections -fdata-sections -fexceptions -frtti -fpermissive -fPIC")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -DEIGEN_NO_CUDA -DNDEBUG -O2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -g -DDEBUG -O0")
if(NOT APPLE)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -v -pthread -Wl,--gc-sections")
endif()

##### SETTINGS #####
add_definitions(-D_ITERATOR_DEBUG_LEVEL=0)
SET(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(THREADS_PREFER_PTHREAD_FLAG ON)
set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/Install)
set(MACOSX_RPATH ON)
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH "@loader_path/../lib/")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
set(INJECT_DEBUG_POSTFIX OFF)

set(ENGINE_DIR ${CMAKE_SOURCE_DIR})
message("Engine Directory ${ENGINE_DIR}")

##### SOURCES #####
set(SOURCES_DIRECTORY ${ENGINE_DIR}/src)
set(INCLUDE_DIRECTORY ${ENGINE_DIR}/include)
set(DEMO_DIRECTORY ${ENGINE_DIR}/demo)

message("Engine Src: ${SOURCES_DIRECTORY}")
message("Engine Incl: ${INCLUDE_DIRECTORY}")
message("Demo Dir: ${DEMO_DIRECTORY}")

file(GLOB_RECURSE SOURCES "${SOURCES_DIRECTORY}/*.cpp" "${DEMO_DIRECTORY}/*.cpp")
file(GLOB_RECURSE HEADERS "${INCLUDE_DIRECTORY}/*.hpp" "${DEMO_DIRECTORY}/*.hpp")

# move imgui sources to somewhere else
include_directories(${ENGINE_DIR}/deps/imgui)
file(
        GLOB LibImGui
        "${ENGINE_DIR}/deps/imgui/imgui.cpp"
        "${ENGINE_DIR}/deps/imgui/imgui_draw.cpp"
        "${ENGINE_DIR}/deps/imgui/imgui_widgets.cpp"
        "${ENGINE_DIR}/deps/imgui/imgui_tables.cpp"
        "${ENGINE_DIR}/deps/imgui/imgui_demo.cpp" # remove
        "${ENGINE_DIR}/deps/imgui/backends/imgui_impl_metal.cpp"
        "${ENGINE_DIR}/deps/imgui/backends/imgui_impl_vulkan.cpp"
        "${ENGINE_DIR}/deps/imgui/backends/imgui_impl_glfw.cpp"
        "${ENGINE_DIR}/deps/imgui/misc/cpp/imgui_stdlib.cpp"
        "${ENGINE_DIR}/deps/imgui/backends/*.h"
        "${ENGINE_DIR}/deps/imgui/misc/cpp/*.h"
        "${ENGINE_DIR}/deps/imgui/*.h"
)
include_directories(${ENGINE_DIR}/deps/imguizmo)
file(
        GLOB LibImGuizmo
        "${ENGINE_DIR}/deps/imguizmo/GraphEditor.cpp"
        "${ENGINE_DIR}/deps/imguizmo/GraphEditor.h"
        "${ENGINE_DIR}/deps/imguizmo/ImCurveEdit.cpp"
        "${ENGINE_DIR}/deps/imguizmo/ImCurveEdit.h"
        "${ENGINE_DIR}/deps/imguizmo/ImGradient.cpp"
        "${ENGINE_DIR}/deps/imguizmo/ImGradient.h"
        "${ENGINE_DIR}/deps/imguizmo/ImGuizmo.cpp"
        "${ENGINE_DIR}/deps/imguizmo/ImGuizmo.h"
        "${ENGINE_DIR}/deps/imguizmo/ImSequencer.cpp"
        "${ENGINE_DIR}/deps/imguizmo/ImSequencer.h"
        "${ENGINE_DIR}/deps/imguizmo/ImZoomSlider.h"
)
file(
        GLOB LibGlslang
        "${ENGINE_DIR}/deps/glslang/glslang/CInterface/*.cpp"
        "${ENGINE_DIR}/deps/glslang/SPIRV/CInterface/*.cpp"
        "${ENGINE_DIR}/deps/glslang/SPIRV/*.cpp"
)
file(
        GLOB LibLua
        "${ENGINE_DIR}/deps/lua/*.c"
)
list(REMOVE_ITEM LibLua "${ENGINE_DIR}/deps/lua/onelua.c")
list(REMOVE_ITEM LibLua "${ENGINE_DIR}/deps/lua/lua.c")

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${LibImGui} ${LibImGuizmo} ${LibGlslang} ${LibLua})

target_precompile_headers(${PROJECT_NAME} PRIVATE ${INCLUDE_DIRECTORY}/w_pch.hpp)
target_include_directories(${PROJECT_NAME} PRIVATE ${SOURCES_DIRECTORY} ${INCLUDE_DIRECTORY})
target_include_directories(${PROJECT_NAME} PRIVATE ${DEMO_DIRECTORY})

## LIBRARIES

find_package(Vulkan REQUIRED)
if (VULKAN_FOUND)
    message(STATUS "Found Vulkan. Including and linking...")
    include_directories(${GLFW_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan glfw ${GLFW_LIBRARIES})
endif(VULKAN_FOUND)

include_directories(${ENGINE_DIR}/deps/stb)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${ENGINE_DIR}/deps/glfw)
target_link_libraries(${PROJECT_NAME} glfw)

set(ASSIMP_BUILD_NO_EXPORT ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_NO_M3D_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_NO_M3D_EXPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_NO_PBRT_EXPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
add_subdirectory(${ENGINE_DIR}/deps/assimp)
target_link_libraries(${PROJECT_NAME} assimp)

add_subdirectory(${ENGINE_DIR}/deps/freetype)
target_link_libraries(${PROJECT_NAME} freetype)

add_subdirectory(${ENGINE_DIR}/deps/entt)
target_link_libraries(${PROJECT_NAME} EnTT)

add_subdirectory(${ENGINE_DIR}/deps/filedialog)
target_link_libraries(${PROJECT_NAME} nfd)

add_subdirectory(${ENGINE_DIR}/deps/fmt)
target_link_libraries(${PROJECT_NAME} fmt)

set(ENABLE_HLSL ON CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_INSTALL ON CACHE BOOL "" FORCE)
add_subdirectory(${ENGINE_DIR}/deps/glslang)
target_link_libraries(${PROJECT_NAME} glslang)

target_include_directories(${PROJECT_NAME} PRIVATE ${ENGINE_DIR}/deps/lua)

add_subdirectory(${ENGINE_DIR}/deps/luabridge3)
target_link_libraries(${PROJECT_NAME} LuaBridge)


if(APPLE)
    message("Platform: Apple")
    set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
elseif(WIN32)
    message("Platform: Win32")
    set(CMAKE_CXX_COMPILER "clang++")
endif()

##### SHADERS #####
file(GLOB_RECURSE GLSL_SOURCE_FILES
        "${ENGINE_DIR}/assets/shaders/*.frag"
        "${ENGINE_DIR}/assets/shaders/*.vert"
        )

foreach(GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_BINARY_DIR}/assets/shaders/${FILE_NAME}.spv")
    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/assets/shaders/"
            COMMAND ${CMAKE_COMMAND} -E copy "${ENGINE_DIR}/assets/shaders/${FILE_NAME}" "${PROJECT_BINARY_DIR}/assets/shaders/${FILE_NAME}"
            COMMAND glslc ${GLSL} -o ${SPIRV}
            DEPENDS ${GLSL}
    )
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
        ${PROJECT_NAME}_Shaders
        DEPENDS ${SPIRV_BINARY_FILES}
)

add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_Shaders)

##### TEXTURES #####

file(GLOB_RECURSE TEXTURE_FILES
        "${ENGINE_DIR}/assets/textures/*.jpg"
        "${ENGINE_DIR}/assets/textures/*.png"
        "${ENGINE_DIR}/assets/textures/*.bmp"
        "${ENGINE_DIR}/assets/textures/*.jpeg"
        )
# todo copy images as some other easier to load format

foreach(TEXTURE ${TEXTURE_FILES})
    get_filename_component(FILE_NAME ${TEXTURE} NAME)
    set(OUT_FILE "${PROJECT_BINARY_DIR}/assets/textures/${FILE_NAME}")
    add_custom_command(
            OUTPUT ${OUT_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/assets/textures/"
            COMMAND ${CMAKE_COMMAND} -E copy "${ENGINE_DIR}/assets/textures/${FILE_NAME}" ${OUT_FILE}
            DEPENDS ${TEXTURE}
    )
    list(APPEND COPIED_TEXTURE_FILES ${OUT_FILE})
endforeach(TEXTURE)

add_custom_target(
        ${PROJECT_NAME}_Textures
        DEPENDS ${COPIED_TEXTURE_FILES}
)

add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_Textures)

##### MODELS #####

file(COPY ${ENGINE_DIR}/assets/models/ DESTINATION ${PROJECT_BINARY_DIR}/assets/models/)

##### SCRIPTS COPY #####

file(GLOB_RECURSE SCRIPT_FILES
        "${ENGINE_DIR}/assets/scripts/*.lua"
        )

foreach(SCRIPT ${SCRIPT_FILES})
    get_filename_component(FILE_NAME ${SCRIPT} NAME)
    set(OUT_FILE "${PROJECT_BINARY_DIR}/assets/scripts/${FILE_NAME}")
    add_custom_command(
            OUTPUT ${OUT_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/assets/scripts/"
            COMMAND ${CMAKE_COMMAND} -E copy ${ENGINE_DIR}/assets/scripts/${FILE_NAME} ${OUT_FILE}
            DEPENDS ${GLSL}
    )
    list(APPEND COPIED_SCRIPT_FILES ${OUT_FILE})
endforeach(SCRIPT)

add_custom_target(
        ${PROJECT_NAME}_Scripts
        DEPENDS ${COPIED_SCRIPT_FILES}
)

add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_Scripts)