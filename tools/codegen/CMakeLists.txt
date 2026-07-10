cmake_minimum_required(VERSION 3.24)
project(wiesel-codegen)

add_executable(wiesel-codegen
        src/main.cc
        src/parser.cc
        src/generator.cc
)

target_include_directories(wiesel-codegen PRIVATE src)

# --- Code generation custom command ---
# Headers to reflect (add more as needed)
set(WIESEL_REFLECT_HEADERS
        ${PARENT_DIR}/wiesel/include/scene/w_components.h
        ${PARENT_DIR}/wiesel/include/rendering/w_sprite.h
        ${PARENT_DIR}/wiesel/include/rendering/w_camera.h
        ${PARENT_DIR}/wiesel/include/scene/w_lights.h
)

set(WIESEL_GENERATED_DIR ${PARENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${WIESEL_GENERATED_DIR})

add_custom_command(
        OUTPUT ${WIESEL_GENERATED_DIR}/w_reflect_all.generated.h
        COMMAND wiesel-codegen
        -o ${WIESEL_GENERATED_DIR}
        -r ${PARENT_DIR}/wiesel/include
        ${WIESEL_REFLECT_HEADERS}
        DEPENDS wiesel-codegen ${WIESEL_REFLECT_HEADERS}
        COMMENT "Running wiesel-codegen on reflected headers"
        VERBATIM
)

add_custom_target(wiesel-reflect
        DEPENDS ${WIESEL_GENERATED_DIR}/w_reflect_all.generated.h
)

# Export the generated dir so the engine can use it
set(WIESEL_GENERATED_DIR ${WIESEL_GENERATED_DIR} PARENT_SCOPE)
