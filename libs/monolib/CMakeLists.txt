cmake_minimum_required(VERSION 3.26)
project(monolib)

#set(COMPILE_DLL_RUNTIME ON)
message("CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

if(WIN32)
    # Use the MONO_ROOT environment variable to locate Mono
    if(DEFINED ENV{MONO_ROOT})
        set(MONO_ROOT $ENV{MONO_ROOT})
    else()
        message(FATAL_ERROR "Environment variable MONO_ROOT is not set. Please set it to the path of your Mono installation.")
    endif()

    message("Mono Root: ${MONO_ROOT}")
    set(MONO_INCLUDE_DIRS ${MONO_ROOT}/include/mono-2.0)
    set(MONO_LIBRARY_DIRS ${MONO_ROOT}/lib)
    set(MONO_LIBRARIES ${MONO_ROOT}/lib/libmono-static-sgen.lib)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(MONO REQUIRED mono-2)
endif()

message("Include dirs: ${MONO_INCLUDE_DIRS}")
message("Lib dirs: ${MONO_LIBRARY_DIRS}")
message("Libs: ${MONO_LIBRARIES}")

# Standalone C# compiler helper (no Mono runtime dependency)
add_library(mono-compiler STATIC src/mono_compiler.cc)
target_include_directories(mono-compiler PUBLIC include)
target_compile_features(mono-compiler PUBLIC cxx_std_17)

add_library(monolib STATIC src/mono_wrappers.cc)
target_link_libraries(monolib PUBLIC mono-compiler)

message("MONO_LDFLAGS: ${MONO_LDFLAGS}")
message("MONO_INCLUDE_DIRS: ${MONO_INCLUDE_DIRS}")

target_include_directories(monolib PUBLIC ${MONO_INCLUDE_DIRS} include)
target_link_directories(monolib PUBLIC ${MONO_LIBRARY_DIRS})
target_link_libraries(monolib PUBLIC ${MONO_LIBRARIES} ${MONO_LDFLAGS})

# Link/copy Mono runtime (etc + lib) to the build directory.
# Windows: copy (no symlinks). Unix: symlink for fast dev builds.
if(WIN32)
    target_link_libraries(monolib PUBLIC winmm Ws2_32 Version bcrypt)
    add_custom_command(TARGET monolib PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${MONO_ROOT}/etc ${CMAKE_BINARY_DIR}/mono/etc
            COMMENT "Copying mono etc to the build directory"
    )
    add_custom_command(TARGET monolib PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${MONO_ROOT}/lib ${CMAKE_BINARY_DIR}/mono/lib
            COMMENT "Copying mono lib to the build directory"
    )
else()
    add_custom_command(TARGET monolib PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/mono
            COMMAND ln -sfn ${MONO_ROOT}/etc ${CMAKE_BINARY_DIR}/mono/etc
            COMMAND ln -sfn ${MONO_ROOT}/lib ${CMAKE_BINARY_DIR}/mono/lib
            COMMENT "Symlinking mono etc/lib to the build directory"
    )
endif()
