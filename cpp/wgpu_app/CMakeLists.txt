find_package(SDL3 REQUIRED CONFIG)

# TODO: check wgpu-native version to be 29.0.1.1 exactly?
find_path(WGPU_NATIVE_INCLUDE_DIR
    NAMES webgpu.h wgpu.h
    HINTS ${WGPU_NATIVE_ROOT}/include/webgpu
    REQUIRED)
find_library(WGPU_NATIVE_LIBRARY
    NAMES wgpu_native
    HINTS ${WGPU_NATIVE_ROOT}/lib
    REQUIRED)

add_library(wgpu-native UNKNOWN IMPORTED)
set_target_properties(wgpu-native PROPERTIES
    IMPORTED_LOCATION "${WGPU_NATIVE_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${WGPU_NATIVE_INCLUDE_DIR}"
)
if(APPLE)
    target_link_libraries(wgpu-native INTERFACE
        "-framework Metal"
        "-framework QuartzCore"
        "-framework Foundation"
        "-framework CoreFoundation"
    )
endif()

add_library(wgpu_app
    source/wgpu_app.cpp
    source/file_utils.cpp
    source/obj_loader.cpp
)
target_include_directories(wgpu_app PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_link_libraries(wgpu_app PUBLIC math SDL3::SDL3 wgpu-native)
