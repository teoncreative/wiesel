cmake_minimum_required(VERSION 3.24)
project(wpak)

add_library(wpak STATIC src/wpak.cc)
target_include_directories(wpak PUBLIC include)
target_compile_features(wpak PUBLIC cxx_std_23)
target_link_libraries(wpak PUBLIC urkern)