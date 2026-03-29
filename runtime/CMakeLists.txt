cmake_minimum_required(VERSION 3.25)
project(runtime)

add_executable(wiesel-runtime src/main.cc)
target_link_libraries(wiesel-runtime PRIVATE wiesel)

# No console window on Windows
if (WIN32)
    set_target_properties(wiesel-runtime PROPERTIES WIN32_EXECUTABLE TRUE)
endif ()