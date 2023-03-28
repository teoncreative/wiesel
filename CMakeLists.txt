cmake_minimum_required(VERSION 3.24)
project(Wiesel)

set(CMAKE_CXX_STANDARD 23)

if(APPLE)
    set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
endif()
# todo other platforms

if(UNIX)
    EXECUTE_PROCESS( COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE DEVICEARCHITECTURE )
    message( STATUS "Arch: ${DEVICEARCHITECTURE}" )
endif()

message(STATUS "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -std=c++23 -DEIGEN_NO_CUDA -DNDEBUG -DENABLE_BOOST_WORKAROUND=ON -Wuninitialized -Wmultichar -ffunction-sections -fdata-sections -fexceptions -frtti -fpermissive -fPIC -O2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -g -std=c++23 -DDEBUG -DENABLE_BOOST_WORKAROUND=ON -Wuninitialized -Wmultichar -ffunction-sections -fdata-sections -fexceptions -frtti -fpermissive -fPIC -O0")
if(NOT APPLE)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -v -pthread -Wl,--gc-sections")
endif()
# todo other platforms

##### SETTINGS #####
if(NOT DEFINED CMAKE_SUPPRESS_DEVELOPER_WARNINGS)
    set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1 CACHE INTERNAL "No dev warnings")
endif()
set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_WIN32_THREADS_INIT 0)
set(CMAKE_USE_PTHREADS_INIT 1)
set(THREADS_PREFER_PTHREAD_FLAG ON)
set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/Install)
set(MACOSX_RPATH ON)
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH "@loader_path/../lib/")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

##### SOURCES #####
set(SOURCES_DIRECTORY ${PROJECT_SOURCE_DIR}/src)
set(INCLUDE_DIRECTORY ${PROJECT_SOURCE_DIR}/include)

message("Sources Dir: ${SOURCES_DIRECTORY}")
message("Includes Dir: ${INCLUDE_DIRECTORY}")

set(SOURCES
        ${SOURCES_DIRECTORY}/main.cpp

        ${SOURCES_DIRECTORY}/util/w_profiler.cpp
        ${SOURCES_DIRECTORY}/util/w_logger.cpp
        ${SOURCES_DIRECTORY}/util/w_utils.cpp
        ${SOURCES_DIRECTORY}/window/w_window.cpp
        ${SOURCES_DIRECTORY}/window/w_glfwwindow.cpp
        ${SOURCES_DIRECTORY}/w_object.cpp
        ${SOURCES_DIRECTORY}/w_mesh.cpp
        ${SOURCES_DIRECTORY}/w_camera.cpp
        ${SOURCES_DIRECTORY}/w_keymanager.cpp
        ${SOURCES_DIRECTORY}/w_engine.cpp

        ${INCLUDE_DIRECTORY}/util/w_attributes.h
        ${INCLUDE_DIRECTORY}/util/w_profiler.h
        ${INCLUDE_DIRECTORY}/util/w_logger.h
        ${INCLUDE_DIRECTORY}/util/w_utils.h
        ${INCLUDE_DIRECTORY}/util/w_keycodes.h
        ${INCLUDE_DIRECTORY}/util/w_mousecodes.h
        ${INCLUDE_DIRECTORY}/events/w_events.h
        ${INCLUDE_DIRECTORY}/events/w_mouseevents.h
        ${INCLUDE_DIRECTORY}/events/w_keyevents.h
        ${INCLUDE_DIRECTORY}/window/w_window.h
        ${INCLUDE_DIRECTORY}/window/w_glfwwindow.h
        ${SOURCES_DIRECTORY}/w_renderer.h
        ${INCLUDE_DIRECTORY}/w_object.h
        ${INCLUDE_DIRECTORY}/w_camera.h
        ${INCLUDE_DIRECTORY}/w_mesh.h
        ${INCLUDE_DIRECTORY}/w_keymanager.h
        ${INCLUDE_DIRECTORY}/w_engine.h
        )

add_executable(${PROJECT_NAME} ${SOURCES})

target_precompile_headers(${PROJECT_NAME} PRIVATE ${INCLUDE_DIRECTORY}/w_pch.h)
target_include_directories(${PROJECT_NAME} PRIVATE ${SOURCES_DIRECTORY} ${INCLUDE_DIRECTORY})

## LIBRARIES

find_package(Vulkan REQUIRED)
if (VULKAN_FOUND)
    message(STATUS "Found Vulkan. Including and linking...")
    include_directories(${GLFW_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan glfw ${GLFW_LIBRARIES})
endif(VULKAN_FOUND)

add_subdirectory(deps/glfw)
target_link_libraries(${PROJECT_NAME} glfw)
include_directories(deps/stb)

##### SHADERS #####
file(GLOB_RECURSE GLSL_SOURCE_FILES
        "shaders/*.frag"
        "shaders/*.vert"
        )

foreach(GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
            COMMAND glslc ${GLSL} -o ${SPIRV}
            DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
        ${PROJECT_NAME}_SHADERS
        DEPENDS ${SPIRV_BINARY_FILES}
)

add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_SHADERS)

##### TEXTURES #####

file(GLOB_RECURSE TEXTURE_FILES
        "textures/*.jpg"
        "textures/*.png"
        "textures/*.bmp"
        "textures/*.jpeg"
        )
# todo copy images as some other easier to load format

foreach(TEXTURE ${TEXTURE_FILES})
    get_filename_component(FILE_NAME ${TEXTURE} NAME)
    set(OUT_FILE "${PROJECT_BINARY_DIR}/textures/${FILE_NAME}")
    add_custom_command(
            OUTPUT ${OUT_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/textures/"
            COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/textures/${FILE_NAME} ${OUT_FILE}
            DEPENDS ${TEXTURE})
    list(APPEND COPIED_TEXTURE_FILES ${OUT_FILE})
endforeach(TEXTURE)

add_custom_target(
        ${PROJECT_NAME}_TEXTURES
        DEPENDS ${COPIED_TEXTURE_FILES}
)

add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_TEXTURES)
