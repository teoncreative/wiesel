cmake_minimum_required(VERSION 3.24)
project(wiesel-parent)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)  # avoid compiler-specific extensions

if (NOT DEFINED PARENT_DIR)
    set(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif ()
if (NOT DEFINED PARENT_BINARY_DIR)
    set(PARENT_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
endif ()
if (NOT DEFINED VENDOR_DIR)
    set(VENDOR_DIR ${PARENT_DIR}/vendor)
endif ()

if(WIN32)
    add_compile_definitions(WIN32=1)
endif()

##### BUILD INFO #####
execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE WIESEL_GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
)
execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE WIESEL_GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
)
if(NOT WIESEL_GIT_COMMIT)
    set(WIESEL_GIT_COMMIT "unknown")
endif()
if(NOT WIESEL_GIT_BRANCH)
    set(WIESEL_GIT_BRANCH "unknown")
endif()
add_compile_definitions(
        WIESEL_GIT_COMMIT="${WIESEL_GIT_COMMIT}"
        WIESEL_GIT_BRANCH="${WIESEL_GIT_BRANCH}"
        WIESEL_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
)

include(${VENDOR_DIR}/CMakeLists.txt)
add_subdirectory(libs/wpak)
add_subdirectory(monolib)
add_subdirectory(wiesel)
add_subdirectory(editor)

# examples
add_subdirectory(examples/native_demo)

# tools
add_subdirectory(tools/asset_packer)

##### EDITOR BUNDLE #####
set(BUNDLE_DIR ${CMAKE_BINARY_DIR}/bundle)
add_custom_target(bundle-editor
    DEPENDS wiesel-editor assetpacker
    COMMENT "Creating editor bundle in ${BUNDLE_DIR}"

    # Clean and create bundle directory
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${BUNDLE_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${BUNDLE_DIR}

    # Copy editor executable
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:wiesel-editor> ${BUNDLE_DIR}/

    # Pack assets and compile core scripts
    COMMAND $<TARGET_FILE:assetpacker> bundle
        ${CMAKE_CURRENT_SOURCE_DIR}/wiesel/assets
        ${CMAKE_CURRENT_SOURCE_DIR}/editor/assets
        ${BUNDLE_DIR}

    # Copy Mono runtime
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_BINARY_DIR}/mono ${BUNDLE_DIR}/mono
)
