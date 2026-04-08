cmake_minimum_required(VERSION 3.24)
project(Wiesel)

option(WIESEL_ENABLE_SANITIZE_ADDRESS "Enable AddressSanitizer for Engine" OFF)
option(ENABLE_VULKAN_VALIDATION "Enables Vulkan validation layer" OFF)
option(WIESEL_ENABLE_TRACY "Enables tracy profiler" OFF)
option(WIESEL_ENABLE_GPU_PROFILING "Enable per-pass GPU timing queries" OFF)
option(WIESEL_ENABLE_DISCORD_RPC "Enable Discord Rich Presence" ON)

if (UNIX)
    execute_process(COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE DEVICEARCHITECTURE)
    message(STATUS "Arch: ${DEVICEARCHITECTURE}")
endif ()

##### SETTINGS #####
#add_definitions(-D_ITERATOR_DEBUG_LEVEL=0)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(THREADS_PREFER_PTHREAD_FLAG ON)
set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/Install)
set(MACOSX_RPATH ON)
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH "@loader_path/../lib/")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
set(INJECT_DEBUG_POSTFIX OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

##### TRACY #####
include(FetchContent)
FetchContent_Declare(
        tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG v0.13.1
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(tracy)

##### NLOHMANN JSON #####
FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        sdl3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.14
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(sdl3)

FetchContent_GetProperties(tracy)
if(tracy_POPULATED)
    message(STATUS "Tracy successfully downloaded and configured")
    # Optional: Verify Tracy target exists
    if(TARGET Tracy::TracyClient)
        message(STATUS "Tracy::TracyClient target available")
        set(LIB_TRACY Tracy::TracyClient)
    elseif(TARGET tracy)
        message(STATUS "tracy target available")
        set(LIB_TRACY tracy)
    else()
        message(FATAL_ERROR "Tracy downloaded but no target found")
    endif()
else()
    message(FATAL_ERROR "Failed to download/configure Tracy")
endif()

##### SOURCES #####
set(ENGINE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} CACHE INTERNAL "wiesel root")
set(SOURCES_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INCLUDE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB_RECURSE SOURCES "${SOURCES_DIRECTORY}/*.cc")
file(GLOB_RECURSE HEADERS "${INCLUDE_DIRECTORY}/*.h")

message("ENGINE_DIRECTORY: " ${ENGINE_DIRECTORY})
message("SOURCES_DIRECTORY: " ${SOURCES_DIRECTORY})
message("INCLUDE_DIRECTORY: " ${INCLUDE_DIRECTORY})

# move imgui sources to somewhere else
file(
        GLOB IMGUI_FILES
        "${VENDOR_DIR}/imgui/imgui.cpp"
        "${VENDOR_DIR}/imgui/imgui_draw.cpp"
        "${VENDOR_DIR}/imgui/imgui_widgets.cpp"
        "${VENDOR_DIR}/imgui/imgui_tables.cpp"
        "${VENDOR_DIR}/imgui/imgui_demo.cpp" # remove
        "${VENDOR_DIR}/imgui/backends/imgui_impl_metal.cpp" # todo implement our own imgui backend instead
        "${VENDOR_DIR}/imgui/backends/imgui_impl_vulkan.cpp"
        "${VENDOR_DIR}/imgui/backends/imgui_impl_sdl3.cpp"
        "${VENDOR_DIR}/imgui/misc/cpp/imgui_stdlib.cpp"
        "${VENDOR_DIR}/imgui/misc/freetype/imgui_freetype.cpp"
        "${VENDOR_DIR}/imgui/backends/*.h"
        "${VENDOR_DIR}/imgui/misc/cpp/*.h"
        "${VENDOR_DIR}/imgui/*.h"
)

file(
        GLOB IMGUIZMO_FILES
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
        GLOB GLSLANG_FILES
        "${VENDOR_DIR}/glslang/glslang/CInterface/*.cpp"
        "${VENDOR_DIR}/glslang/glslang/ResourceLimits/*.cpp"
        "${VENDOR_DIR}/glslang/SPIRV/CInterface/*.cpp"
        "${VENDOR_DIR}/glslang/SPIRV/*.cpp"
)

add_library(wiesel STATIC ${SOURCES} ${HEADERS} ${IMGUI_FILES} ${IMGUIZMO_FILES} ${GLSLANG_FILES})

if (MSVC)
    target_compile_options(wiesel PRIVATE
            $<$<CONFIG:Release>:/O2 /GL /DNDEBUG>
            $<$<CONFIG:Debug>:/Od /Zi /DDEBUG>
    )
    if (WIESEL_ENABLE_SANITIZE_ADDRESS)
        target_compile_options(wiesel PRIVATE /fsanitize=address)
    endif()
else()
    # We are using exceptions right now, but I want to replace them with state values to provide more safety
    # And add -fno-exceptions to release builds.
    # Additionally we might also add -fno-rtti if I can replace the existing rtti usages with some other stuff
    target_compile_options(wiesel PRIVATE
            $<$<CONFIG:Release>:-O2 -march=native -DNDEBUG -ffast-math>
            $<$<CONFIG:Debug>:-g -O0 -DDEBUG>
    )
    if (WIESEL_ENABLE_SANITIZE_ADDRESS)
        target_compile_options(wiesel PRIVATE -fsanitize=address)
        target_link_options(wiesel PRIVATE -fsanitize=address)
    endif()
endif()

if (WIESEL_ENABLE_VULKAN_VALIDATION)
    target_compile_options(wiesel PUBLIC -DVULKAN_VALIDATION=1)
endif()
target_compile_definitions(wiesel PUBLIC GLM_ENABLE_EXPERIMENTAL)
target_compile_definitions(wiesel PUBLIC GLM_FORCE_DEFAULT_ALIGNED_GENTYPES)
target_compile_definitions(wiesel PUBLIC GLM_FORCE_LEFT_HANDED)
target_compile_definitions(wiesel PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_compile_definitions(wiesel PUBLIC GLM_FORCE_RADIANS)

target_precompile_headers(wiesel PUBLIC ${INCLUDE_DIRECTORY}/w_pch.h)
if (MSVC)
    # MSVC uses /FI to include precompiled header
    target_compile_options(wiesel PRIVATE /FI ${INCLUDE_DIRECTORY}/w_pch.h)
else()
    # Non-MSVC compilers use -include
    target_compile_options(wiesel PRIVATE -include ${INCLUDE_DIRECTORY}/w_pch.h)
endif()

target_include_directories(wiesel PUBLIC ${SOURCES_DIRECTORY} ${INCLUDE_DIRECTORY})

# Generated reflection headers (from wiesel-codegen)
if (DEFINED WIESEL_GENERATED_DIR)
    target_include_directories(wiesel PUBLIC ${WIESEL_GENERATED_DIR})
    add_dependencies(wiesel wiesel-reflect)
endif ()

if (NOT APPLE AND NOT MSVC)
    target_link_options(wiesel PRIVATE -pthread)
endif()

## LIBRARIES

target_include_directories(wiesel PUBLIC ${LIBS_DIR}/tracy/public)
target_link_libraries(wiesel PUBLIC ${LIB_TRACY})
if (WIESEL_ENABLE_TRACY)
    target_compile_definitions(wiesel PUBLIC TRACY_ENABLE)
endif()
if (WIESEL_ENABLE_GPU_PROFILING)
    target_compile_definitions(wiesel PUBLIC WIESEL_GPU_PROFILING=1)
endif()
target_compile_definitions(wiesel PUBLIC IMGUI_ENABLE_FREETYPE)

target_include_directories(wiesel PUBLIC ${VENDOR_DIR}/glm)

if (VULKAN_FOUND)
    message(STATUS "Found Vulkan. Including and linking...")
    target_link_libraries(wiesel PUBLIC Vulkan::Vulkan)
    target_link_libraries(wiesel PUBLIC SDL3::SDL3-static)
endif (VULKAN_FOUND)

target_include_directories(wiesel PUBLIC ${VENDOR_DIR}/imgui)
target_include_directories(wiesel PUBLIC ${VENDOR_DIR}/imguizmo)
target_include_directories(wiesel PUBLIC ${VENDOR_DIR}/stb)
target_include_directories(wiesel PUBLIC ${VENDOR_DIR}/miniaudio)
if (WIESEL_ENABLE_DISCORD_RPC)
    target_compile_definitions(wiesel PUBLIC WIESEL_DISCORD_RPC=1)
    target_link_libraries(wiesel PUBLIC discord-rpc)
endif()
target_link_libraries(wiesel PUBLIC assimp)
target_link_libraries(wiesel PUBLIC freetype)
target_link_libraries(wiesel PUBLIC EnTT)
target_link_libraries(wiesel PUBLIC nfd)
target_link_libraries(wiesel PUBLIC glslang)
target_link_libraries(wiesel PUBLIC monolib)
target_link_libraries(wiesel PUBLIC wpak)
target_link_libraries(wiesel PUBLIC cxxopts::cxxopts)
target_link_libraries(wiesel PUBLIC GPUOpen::VulkanMemoryAllocator)
target_link_libraries(wiesel PUBLIC Jolt)
target_link_libraries(wiesel PUBLIC Recast Detour)
target_link_libraries(wiesel PUBLIC rmlui)
target_link_libraries(wiesel PUBLIC efsw-static)
target_include_directories(wiesel PUBLIC ${VENDOR_DIR}/efsw/include)
target_link_libraries(wiesel PUBLIC nlohmann_json::nlohmann_json)

if (WIN32)
    target_link_libraries(wiesel PUBLIC shell32 ole32)
endif()

function(_hash s outvar)
    string(MD5 _h "${s}")
    set(${outvar} "${_h}" PARENT_SCOPE)
endfunction()
