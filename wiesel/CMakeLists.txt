cmake_minimum_required(VERSION 3.24)
project(Wiesel)

option(ENABLE_SANITIZE_ADDRESS "Enable AddressSanitizer for Engine" OFF)
option(ENABLE_VULKAN_VALIDATION "Enables Vulkan validation layer" OFF)

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

##### SOURCES #####
set(SOURCES_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INCLUDE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB_RECURSE SOURCES "${SOURCES_DIRECTORY}/*.cpp")
file(GLOB_RECURSE HEADERS "${INCLUDE_DIRECTORY}/*.hpp")

message("SOURCES_DIRECTORY: " ${SOURCES_DIRECTORY})
message("INCLUDE_DIRECTORY: " ${INCLUDE_DIRECTORY})

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

add_library(wiesel STATIC ${SOURCES} ${HEADERS} ${LibImGui} ${LibImGuizmo} ${LibGlslang})

if (MSVC)
    target_compile_options(wiesel PRIVATE
            $<$<CONFIG:Release>:/O2 /GL /DNDEBUG>
            $<$<CONFIG:Debug>:/Od /Zi>
    )
    if (ENABLE_SANITIZE_ADDRESS)
        target_compile_options(wiesel PRIVATE /fsanitize=address)
    endif()
else()
    # We are using exceptions right now, but I want to replace them with state values to provide more safety
    # And add -fno-exceptions to release builds.
    # Additionally we might also add -fno-rtti if I can replace the existing rtti usages with some other stuff
    target_compile_options(wiesel PRIVATE
            $<$<CONFIG:Release>:-O2 -march=native -DNDEBUG -ffast-math >
            $<$<CONFIG:Debug>:-g -O0>
    )
    if (ENABLE_SANITIZE_ADDRESS)
        target_compile_options(wiesel PRIVATE -fsanitize=address)
        target_link_options(wiesel PRIVATE -fsanitize=address)
    endif()
endif()

if (ENABLE_VULKAN_VALIDATION)
    target_compile_options(wiesel PUBLIC -DVULKAN_VALIDATION=1)
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

if (VULKAN_FOUND)
    message(STATUS "Found Vulkan. Including and linking...")
    target_include_directories(wiesel PUBLIC ${GLFW_INCLUDE_DIRS})
    target_link_libraries(wiesel Vulkan::Vulkan glfw ${GLFW_LIBRARIES})
endif (VULKAN_FOUND)

target_include_directories(wiesel PUBLIC ${VENDOR_DIR}/imgui)
target_include_directories(wiesel PUBLIC ${VENDOR_DIR}/imguizmo)
target_include_directories(wiesel PUBLIC ${VENDOR_DIR}/stb)
target_link_libraries(wiesel glfw)
target_link_libraries(wiesel assimp)
target_link_libraries(wiesel freetype)
target_link_libraries(wiesel EnTT)
target_link_libraries(wiesel nfd)
target_link_libraries(wiesel fmt)
target_link_libraries(wiesel glslang)
target_link_libraries(wiesel monolib)

function(target_include_assets target_project assets_directory internal)
    set(TARGET_PROJECT ${target_project})
    set(ASSETS_DIRECTORY ${assets_directory})
    set(IS_INTERNAL ${internal})
    ##### SHADERS #####
    file(GLOB_RECURSE GLSL_SOURCE_FILES
            "${ASSETS_DIRECTORY}/shaders/*.frag"
            "${ASSETS_DIRECTORY}/shaders/*.vert"
    )

    foreach (GLSL ${GLSL_SOURCE_FILES})
        get_filename_component(FILE_NAME ${GLSL} NAME)
        set(SPIRV "${PARENT_BINARY_DIR}/assets/shaders/${FILE_NAME}.spv")
        message("Compiling shader ${FILE_NAME}")
        add_custom_command(TARGET ${TARGET_PROJECT} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "${PARENT_BINARY_DIR}/assets/shaders/"
                COMMAND ${CMAKE_COMMAND} -E copy "${ASSETS_DIRECTORY}/shaders/${FILE_NAME}" "${PARENT_BINARY_DIR}/assets/shaders/${FILE_NAME}"
                COMMAND glslc ${GLSL} -o ${SPIRV}
                DEPENDS ${GLSL}
                COMMENT "Compiling shader ${FILE_NAME}"
        )
    endforeach (GLSL)

    add_custom_target(
            ${TARGET_PROJECT}_shaders
            DEPENDS ${SPIRV_BINARY_FILES}
    )

    ##### TEXTURES #####

    file(GLOB_RECURSE TEXTURE_FILES
            "${ASSETS_DIRECTORY}/textures/*.jpg"
            "${ASSETS_DIRECTORY}/textures/*.png"
            "${ASSETS_DIRECTORY}/textures/*.bmp"
            "${ASSETS_DIRECTORY}/textures/*.jpeg"
    )
    # todo copy images as some other easier to load format

    foreach (TEXTURE ${TEXTURE_FILES})
        file(RELATIVE_PATH REL_PATH "${ASSETS_DIRECTORY}/textures" "${TEXTURE}")
        set(OUT_FILE "${PARENT_BINARY_DIR}/assets/textures/${REL_PATH}")

        add_custom_command(TARGET ${TARGET_PROJECT} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${TARGET_PROJECT}>/assets/textures/${REL_PATH}"
                COMMAND ${CMAKE_COMMAND} -E copy "${TEXTURE}" "${OUT_FILE}"
                DEPENDS ${TEXTURE}
                COMMENT "Copying texture ${REL_PATH}"
        )
    endforeach (TEXTURE)

    ##### MODELS #####

    if (IS_DIRECTORY ${ASSETS_DIRECTORY}/models)
        file(COPY ${ASSETS_DIRECTORY}/models/ DESTINATION ${PARENT_BINARY_DIR}/assets/models/)
    endif ()

    ##### SCRIPTS COPY #####

    if (IS_INTERNAL)
        add_custom_command(TARGET ${TARGET_PROJECT} PRE_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory ${ASSETS_DIRECTORY}/scripts ${PARENT_BINARY_DIR}/assets/internal_scripts
                DEPENDS ${ASSETS_DIRECTORY}/scripts
                COMMENT "Copying scripts to ${PARENT_BINARY_DIR}/assets/internal_scripts"
        )
    else()
        add_custom_command(TARGET ${TARGET_PROJECT} PRE_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory ${ASSETS_DIRECTORY}/scripts ${PARENT_BINARY_DIR}/assets/scripts
                DEPENDS ${ASSETS_DIRECTORY}/scripts
                COMMENT "Copying scripts to ${PARENT_BINARY_DIR}/assets/scripts"
        )
    endif ()
endfunction()

target_include_assets(wiesel ${CMAKE_CURRENT_SOURCE_DIR}/assets ON)