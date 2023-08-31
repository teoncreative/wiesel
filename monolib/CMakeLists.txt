cmake_minimum_required(VERSION 3.26)
project(monolib)

set(COMPILE_DLL_RUNTIME ON)

if(WIN32)
    # Use the MONO_ROOT environment variable to locate Mono
    if(DEFINED ENV{MONO_ROOT})
        set(MONO_ROOT $ENV{MONO_ROOT})
    else()
        message(FATAL_ERROR "Environment variable MONO_ROOT is not set. Please set it to the path of your Mono installation.")
    endif()

    set(MONO_INCLUDE_DIRS ${MONO_ROOT}/include/mono-2.0)
    set(MONO_LIBRARY_DIRS ${MONO_ROOT}/lib)
    set(MONO_LIBRARIES ${MONO_ROOT}/lib/mono-2.0.lib)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(MONO REQUIRED mono-2)
endif()

#message("include dirs: ${MONO_INCLUDE_DIRS}")
#message("lib dirs: ${MONO_LIBRARY_DIRS}")
#message("libs: ${MONO_LIBRARIES}")

add_library(monolib STATIC src/mono_util.cpp)

target_include_directories(monolib PUBLIC ${MONO_INCLUDE_DIRS} include)
target_link_directories(monolib PUBLIC ${MONO_LIBRARY_DIRS})
target_link_libraries(monolib PUBLIC ${MONO_LIBRARIES})

# copy mcs to the build directory
if(WIN32)
    # todo
else()
    add_custom_command(TARGET monolib PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory /opt/homebrew/opt/mono/bin ${CMAKE_BINARY_DIR}/mono/bin
            COMMENT "Copying mono bin to the build directory"
    )
    add_custom_command(TARGET monolib PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory /opt/homebrew/opt/mono/etc ${CMAKE_BINARY_DIR}/mono/etc
            COMMENT "Copying mono etc to the build directory"
    )
    add_custom_command(TARGET monolib PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory /opt/homebrew/opt/mono/lib ${CMAKE_BINARY_DIR}/mono/lib
            COMMENT "Copying mono lib to the build directory"
    )
endif()


function(target_compile_csharp target_project output_dll sources)
    set(TARGET_PROJECT ${target_project})
    set(ASSETS_DIRECTORY ${assets_directory})

    set(OUTPUT_DLL ${output_dll})
    get_filename_component(OUTPUT_DLL_FOLDER ${OUTPUT_DLL} DIRECTORY)

    # Create a custom target to compile C# sources into a DLL
    add_custom_command(TARGET ${TARGET_PROJECT} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DLL_FOLDER})

    add_custom_target("CSharpCompile-${TARGET_PROJECT}"
            COMMAND mcs -target:library -out:${OUTPUT_DLL} ${CSHARP_SOURCES}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Compiling C# sources into ${OUTPUT_DLL}"
    )

    add_dependencies(${TARGET_PROJECT} "CSharpCompile-${TARGET_PROJECT}")
endfunction()