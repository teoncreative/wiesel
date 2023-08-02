cmake_minimum_required(VERSION 3.24)
project(Wiesel)

set(CMAKE_CXX_STANDARD 23)

# todo other platforms

if (UNIX)
    EXECUTE_PROCESS(COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE DEVICEARCHITECTURE)
    message(STATUS "Arch: ${DEVICEARCHITECTURE}")
endif ()

message(STATUS "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -Wuninitialized -Wmultichar -ffunction-sections -fdata-sections -fexceptions -frtti -fpermissive -fPIC")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -DEIGEN_NO_CUDA -DNDEBUG -O2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -g -DDEBUG -O0")
if (NOT APPLE)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -v -pthread -Wl,--gc-sections")
endif ()

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

##### SOURCES #####
set(SOURCES_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INCLUDE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB_RECURSE SOURCES "${SOURCES_DIRECTORY}/*.cpp")
file(GLOB_RECURSE HEADERS "${INCLUDE_DIRECTORY}/*.hpp")

# move imgui sources to somewhere else
file(
        GLOB LibImGui
        "${VENDOR_DIR}/imgui/imgui.cpp"
        "${VENDOR_DIR}/imgui/imgui_draw.cpp"
        "${VENDOR_DIR}/imgui/imgui_widgets.cpp"
        "${VENDOR_DIR}/imgui/imgui_tables.cpp"
        "${VENDOR_DIR}/imgui/imgui_demo.cpp" # remove
        "${VENDOR_DIR}/imgui/backends/imgui_impl_metal.cpp"
        "${VENDOR_DIR}/imgui/backends/imgui_impl_vulkan.cpp"
        "${VENDOR_DIR}/imgui/backends/imgui_impl_glfw.cpp"
        "${VENDOR_DIR}/imgui/misc/cpp/imgui_stdlib.cpp"
        "${VENDOR_DIR}/imgui/backends/*.h"
        "${VENDOR_DIR}/imgui/misc/cpp/*.h"
        "${VENDOR_DIR}/imgui/*.h"
)
file(
        GLOB LibImGuizmo
        "${VENDOR_DIR}/imguizmo/GraphEditor.cpp"
        "${VENDOR_DIR}/imguizmo/GraphEditor.h"
        "${VENDOR_DIR}/imguizmo/ImCurveEdit.cpp"
        "${VENDOR_DIR}/imguizmo/ImCurveEdit.h"
        "${VENDOR_DIR}/imguizmo/ImGradient.cpp"
        "${VENDOR_DIR}/imguizmo/ImGradient.h"
        "${VENDOR_DIR}/imguizmo/ImGuizmo.cpp"
        "${VENDOR_DIR}/imguizmo/ImGuizmo.h"
        "${VENDOR_DIR}/imguizmo/ImSequencer.cpp"
        "${VENDOR_DIR}/imguizmo/ImSequencer.h"
        "${VENDOR_DIR}/imguizmo/ImZoomSlider.h"
)
file(
        GLOB LibGlslang
        "${VENDOR_DIR}/glslang/glslang/CInterface/*.cpp"
        "${VENDOR_DIR}/glslang/SPIRV/CInterface/*.cpp"
        "${VENDOR_DIR}/glslang/SPIRV/*.cpp"
)
file(
        GLOB LibLua
        "${VENDOR_DIR}/lua/*.c"
)
list(REMOVE_ITEM LibLua "${VENDOR_DIR}/lua/onelua.c")
list(REMOVE_ITEM LibLua "${VENDOR_DIR}/lua/lua.c")

add_library(wiesel ${SOURCES} ${HEADERS} ${LibImGui} ${LibImGuizmo} ${LibGlslang} ${LibLua})

target_precompile_headers(wiesel PUBLIC ${INCLUDE_DIRECTORY}/w_pch.hpp)
target_include_directories(wiesel PUBLIC ${SOURCES_DIRECTORY} ${INCLUDE_DIRECTORY})

## LIBRARIES

if (VULKAN_FOUND)
    message(STATUS "Found Vulkan. Including and linking...")
    target_include_directories(wiesel PUBLIC ${GLFW_INCLUDE_DIRS})
    target_link_libraries(wiesel Vulkan::Vulkan glfw ${GLFW_LIBRARIES})
endif (VULKAN_FOUND)

target_include_directories(wiesel PUBLIC ${VENDOR_DIR}/imgui)
target_include_directories(wiesel PUBLIC ${VENDOR_DIR}/imguizmo)
target_include_directories(wiesel PUBLIC ${VENDOR_DIR}/stb)
target_include_directories(wiesel PUBLIC ${VENDOR_DIR}/lua)
target_link_libraries(wiesel glfw)
target_link_libraries(wiesel assimp)
target_link_libraries(wiesel freetype)
target_link_libraries(wiesel EnTT)
target_link_libraries(wiesel nfd)
target_link_libraries(wiesel fmt)
target_link_libraries(wiesel glslang)
target_link_libraries(wiesel LuaBridge)


if (APPLE)
    message("Platform: Apple")
    set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
elseif (WIN32)
    message("Platform: Win32")
    set(CMAKE_CXX_COMPILER "clang++")
endif ()

##### SHADERS #####
file(GLOB_RECURSE GLSL_SOURCE_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/*.frag"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/*.vert"
)

foreach (GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PARENT_BINARY_DIR}/assets/shaders/${FILE_NAME}.spv")
    message("Compiling shader ${FILE_NAME}")
    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${PARENT_BINARY_DIR}/assets/shaders/"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/${FILE_NAME}" "${PARENT_BINARY_DIR}/assets/shaders/${FILE_NAME}"
            COMMAND glslc ${GLSL} -o ${SPIRV}
            DEPENDS ${GLSL}
    )
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach (GLSL)

add_custom_target(
        wiesel_shaders
        DEPENDS ${SPIRV_BINARY_FILES}
)

add_dependencies(wiesel wiesel_shaders)

##### TEXTURES #####

file(GLOB_RECURSE TEXTURE_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/textures/*.jpg"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/textures/*.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/textures/*.bmp"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/textures/*.jpeg"
)
# todo copy images as some other easier to load format

foreach (TEXTURE ${TEXTURE_FILES})
    get_filename_component(FILE_NAME ${TEXTURE} NAME)
    set(OUT_FILE "${PARENT_BINARY_DIR}/assets/textures/${FILE_NAME}")
    add_custom_command(
            OUTPUT ${OUT_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${PARENT_BINARY_DIR}/assets/textures/"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/assets/textures/${FILE_NAME}" ${OUT_FILE}
            DEPENDS ${TEXTURE}
    )
    list(APPEND COPIED_TEXTURE_FILES ${OUT_FILE})
endforeach (TEXTURE)

add_custom_target(
        wiesel_textures
        DEPENDS ${COPIED_TEXTURE_FILES}
)

add_dependencies(wiesel wiesel_textures)

##### MODELS #####

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets/models/ DESTINATION ${PARENT_BINARY_DIR}/assets/models/)

##### SCRIPTS COPY #####

file(GLOB_RECURSE SCRIPT_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/scripts/*.lua"
)

foreach (SCRIPT ${SCRIPT_FILES})
    get_filename_component(FILE_NAME ${SCRIPT} NAME)
    set(OUT_FILE "${PARENT_BINARY_DIR}/assets/scripts/${FILE_NAME}")
    add_custom_command(
            OUTPUT ${OUT_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${PARENT_BINARY_DIR}/assets/scripts/"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/assets/scripts/${FILE_NAME}" ${OUT_FILE}
            DEPENDS ${GLSL}
    )
    list(APPEND COPIED_SCRIPT_FILES ${OUT_FILE})
endforeach (SCRIPT)

add_custom_target(
        wiesel_scripts
        DEPENDS ${COPIED_SCRIPT_FILES}
)

add_dependencies(wiesel wiesel_scripts)