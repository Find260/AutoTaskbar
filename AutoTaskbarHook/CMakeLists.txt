cmake_minimum_required(VERSION 3.30)
project(AutoTaskbarHook)

set(CMAKE_CXX_STANDARD 20)
add_library(AutoTaskbarHook SHARED main.cpp)
target_link_libraries(AutoTaskbarHook PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/lib/detours.lib")
//atb-tmp