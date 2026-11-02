cmake_minimum_required(VERSION 3.20)
project(realtime-graphics-course-practice LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(math)
add_subdirectory(wgpu_app)

include(cmake/AddPractice.cmake)

add_subdirectory(practice01)
add_subdirectory(practice02)
add_subdirectory(practice03)
add_subdirectory(practice04)
add_subdirectory(practice05)
add_subdirectory(practice06)
add_subdirectory(practice07)
add_subdirectory(practice08)
add_subdirectory(practice09)

