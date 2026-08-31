cmake_minimum_required(VERSION 3.20)
project(graphics-course-practice LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(math)
add_subdirectory(wgpu_app)

include(cmake/AddPractice.cmake)

add_subdirectory(practice01)
