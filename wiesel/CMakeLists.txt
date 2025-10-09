cmake_minimum_required(VERSION 3.24)
project(Wiesel)

option(ENABLE_SANITIZE_ADDRESS "Enable AddressSanitizer for Engine" OFF)
option(ENABLE_VULKAN_VALIDATION "Enables Vulkan validation layer" OFF)
option(ENABLE_ID_BUFFER_PASS "Enables id buffer pass, specifically used for the editor" ON)
option(ENABLE_TRACY "Enables tracy profiler" OFF)

if (UNIX)
    EXECUTE_PROCESS(COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE DEVICEARCHITECTURE)
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
        GIT_TAG v0.12.2  # or whatever version you want
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(tracy)
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

file(GLOB_RECURSE SOURCES "${SOURCES_DIRECTORY}/*.cpp")
file(GLOB_RECURSE HEADERS "${INCLUDE_DIRECTORY}/*.hpp")

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
        "${VENDOR_DIR}/imgui/backends/imgui_impl_glfw.cpp"
        "${VENDOR_DIR}/imgui/misc/cpp/imgui_stdlib.cpp"
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
        "${VENDOR_DIR}/glslang/SPIRV/CInterface/*.cpp"
        "${VENDOR_DIR}/glslang/SPIRV/*.cpp"
)

add_library(wiesel STATIC ${SOURCES} ${HEADERS} ${IMGUI_FILES} ${IMGUIZMO_FILES} ${GLSLANG_FILES})

if (MSVC)
    target_compile_options(wiesel PRIVATE
            $<$<CONFIG:Release>:/O2 /GL /DNDEBUG>
            $<$<CONFIG:Debug>:/Od /Zi /DDEBUG>
    )
    if (ENABLE_SANITIZE_ADDRESS)
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
    if (ENABLE_SANITIZE_ADDRESS)
        target_compile_options(wiesel PRIVATE -fsanitize=address)
        target_link_options(wiesel PRIVATE -fsanitize=address)
    endif()
endif()

if (ENABLE_VULKAN_VALIDATION)
    target_compile_options(wiesel PUBLIC -DVULKAN_VALIDATION=1)
endif()
if (ENABLE_ID_BUFFER_PASS)
    target_compile_options(wiesel PUBLIC -DID_BUFFER_PASS=1)
endif()

target_precompile_headers(wiesel PUBLIC ${INCLUDE_DIRECTORY}/w_pch.hpp)
if (MSVC)
    # MSVC uses /FI to include precompiled header
    target_compile_options(wiesel PRIVATE /FI ${INCLUDE_DIRECTORY}/w_pch.hpp)
else()
    # Non-MSVC compilers use -include
    target_compile_options(wiesel PRIVATE -include ${INCLUDE_DIRECTORY}/w_pch.hpp)
endif()

target_include_directories(wiesel PUBLIC ${SOURCES_DIRECTORY} ${INCLUDE_DIRECTORY})

if (NOT APPLE AND NOT MSVC)
    target_link_options(wiesel PRIVATE -pthread)
endif()

## LIBRARIES

target_include_directories(wiesel PUBLIC ${LIBS_DIR}/tracy/public)
target_link_libraries(wiesel PUBLIC ${LIB_TRACY})
if(ENABLE_TRACY)
    target_compile_definitions(wiesel PUBLIC TRACY_ENABLE)
endif()

if (VULKAN_FOUND)
    message(STATUS "Found Vulkan. Including and linking...")
    target_include_directories(wiesel PUBLIC ${GLFW_INCLUDE_DIRS})
    target_link_libraries(wiesel PUBLIC Vulkan::Vulkan glfw ${GLFW_LIBRARIES})
endif (VULKAN_FOUND)

target_include_directories(wiesel PUBLIC ${VENDOR_DIR}/imgui)
target_include_directories(wiesel PUBLIC ${VENDOR_DIR}/imguizmo)
target_include_directories(wiesel PUBLIC ${VENDOR_DIR}/stb)
target_link_libraries(wiesel PUBLIC glfw)
target_link_libraries(wiesel PUBLIC assimp)
target_link_libraries(wiesel PUBLIC freetype)
target_link_libraries(wiesel PUBLIC EnTT)
target_link_libraries(wiesel PUBLIC nfd)
target_link_libraries(wiesel PUBLIC fmt)
target_link_libraries(wiesel PUBLIC glslang)
target_link_libraries(wiesel PUBLIC monolib)

function(_hash s outvar)
    string(MD5 _h "${s}")
    set(${outvar} "${_h}" PARENT_SCOPE)
endfunction()

# ---- project assets ----
function(target_include_assets target_project assets_dir)
    get_filename_component(ASSETS_DIR "${assets_dir}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")
    if(NOT EXISTS "${ASSETS_DIR}")
        return()  # optional: nothing to do
    endif()

    _hash("${ASSETS_DIR}-project" ASSET_ID)
    set(STAGE "${CMAKE_BINARY_DIR}/assetpacks/${ASSET_ID}")

    if(NOT TARGET assetpack_${ASSET_ID})
        set(all_outs)

        # 1) copy whole assets dir if present
        if(EXISTS "${ASSETS_DIR}")
            set(copy_stamp "${STAGE}/.copy_assets.stamp")
            add_custom_command(OUTPUT "${copy_stamp}"
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${STAGE}/assets"
                    COMMAND ${CMAKE_COMMAND} -E copy_directory "${ASSETS_DIR}" "${STAGE}/assets"
                    COMMAND ${CMAKE_COMMAND} -E touch "${copy_stamp}"
                    DEPENDS "${ASSETS_DIR}"
                    VERBATIM)
            list(APPEND all_outs "${copy_stamp}")
        endif()

        # 2) compile GLSL in assets/shaders -> assets/shaders/*.spv (optional)
        if(EXISTS "${ASSETS_DIR}/shaders")
            file(GLOB_RECURSE GLSL "${ASSETS_DIR}/shaders/*.frag" "${ASSETS_DIR}/shaders/*.vert")
            foreach(f IN LISTS GLSL)
                get_filename_component(fn "${f}" NAME)
                set(spv "${STAGE}/assets/shaders/${fn}.spv")
                add_custom_command(OUTPUT "${spv}"
                        COMMAND ${CMAKE_COMMAND} -E make_directory "${STAGE}/assets/shaders"
                        COMMAND glslc "${f}" -o "${spv}"
                        DEPENDS "${f}" VERBATIM)
                list(APPEND all_outs "${spv}")
            endforeach()
        endif()

        add_custom_target(assetpack_${ASSET_ID} DEPENDS ${all_outs})
    endif()

    add_dependencies(${target_project} assetpack_${ASSET_ID})

    # deploy (safe even if some dirs are missing)
    add_custom_command(TARGET ${target_project} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${STAGE}/assets" "$<TARGET_FILE_DIR:${target_project}>/assets"
            VERBATIM)
endfunction()

# ---- engine assets ----
# copies ${ENGINE_DIRECTORY}/assets if it exists; also compiles internal_shaders
function(target_include_engine_assets target_project)
    get_filename_component(ENG_DIR "${ENGINE_DIRECTORY}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")
    if(NOT EXISTS "${ENG_DIR}")
        return()
    endif()

    _hash("${ENG_DIR}-engine" ASSET_ID)
    set(STAGE "${CMAKE_BINARY_DIR}/assetpacks/${ASSET_ID}")

    if(NOT TARGET assetpack_${ASSET_ID})
        set(all_outs)

        # 1) copy engine assets tree if present
        if(EXISTS "${ENG_DIR}/assets")
            set(copy_stamp "${STAGE}/.copy_engine_assets.stamp")
            add_custom_command(OUTPUT "${copy_stamp}"
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${STAGE}/assets"
                    COMMAND ${CMAKE_COMMAND} -E copy_directory "${ENG_DIR}/assets" "${STAGE}/assets"
                    COMMAND ${CMAKE_COMMAND} -E touch "${copy_stamp}"
                    DEPENDS "${ENG_DIR}/assets" VERBATIM)
            list(APPEND all_outs "${copy_stamp}")
        endif()

        # 2) compile internal shaders -> assets/internal_shaders/*.spv (optional)
        if(EXISTS "${ENG_DIR}/assets/internal_shaders")
            file(GLOB_RECURSE IGLSL "${ENG_DIR}/assets/internal_shaders/*.frag" "${ENG_DIR}/assets/internal_shaders/*.vert")
            foreach(f IN LISTS IGLSL)
                get_filename_component(fn "${f}" NAME)
                set(spv "${STAGE}/assets/internal_shaders/${fn}.spv")
                add_custom_command(OUTPUT "${spv}"
                        COMMAND ${CMAKE_COMMAND} -E make_directory "${STAGE}/assets/internal_shaders"
                        COMMAND glslc "${f}" -o "${spv}"
                        DEPENDS "${f}" VERBATIM)
                list(APPEND all_outs "${spv}")
            endforeach()
        endif()

        add_custom_target(assetpack_${ASSET_ID} DEPENDS ${all_outs})
    endif()

    add_dependencies(${target_project} assetpack_${ASSET_ID})

    add_custom_command(TARGET ${target_project} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${STAGE}/assets" "$<TARGET_FILE_DIR:${target_project}>/assets"
            VERBATIM)
endfunction()
