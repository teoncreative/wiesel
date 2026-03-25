cmake_minimum_required(VERSION 3.24)
project(assetpacker)

add_executable(assetpacker main.cc)
target_link_libraries(assetpacker PRIVATE mono-compiler wpak)