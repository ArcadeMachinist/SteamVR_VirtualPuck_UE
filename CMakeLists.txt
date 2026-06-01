cmake_minimum_required(VERSION 3.20)
project(driver_arcadecabvr VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OPENVR_SDK "${CMAKE_SOURCE_DIR}/vendor/openvr")

add_library(driver_arcadecabvr SHARED
    src/driver_main.cpp
    src/tracker_device.cpp
    src/config.cpp
    src/udp_listener.cpp
)

target_include_directories(driver_arcadecabvr PRIVATE
    "${OPENVR_SDK}/headers"
    "${CMAKE_SOURCE_DIR}/vendor"
)

target_link_libraries(driver_arcadecabvr PRIVATE
    "${OPENVR_SDK}/lib/win64/openvr_api.lib"
    ws2_32
)

set_target_properties(driver_arcadecabvr PROPERTIES
    PREFIX ""
    OUTPUT_NAME "driver_arcadecabvr"
)

add_executable(device_list tools/device_list.cpp)
target_include_directories(device_list PRIVATE "${OPENVR_SDK}/headers")
target_link_libraries(device_list PRIVATE "${OPENVR_SDK}/lib/win64/openvr_api.lib")

set(STAGE_DIR "${CMAKE_SOURCE_DIR}/driver_stage/arcadecabvr")

add_custom_command(TARGET driver_arcadecabvr POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${STAGE_DIR}/bin/win64"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:driver_arcadecabvr>" "${STAGE_DIR}/bin/win64/"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/driver.vrdrivermanifest" "${STAGE_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/resources" "${STAGE_DIR}/resources"
    # Copy default config only if not already present (preserve user edits)
    COMMAND ${CMAKE_COMMAND}
        -DSRC="${CMAKE_SOURCE_DIR}/config.json.default"
        -DDST="${STAGE_DIR}/config.json"
        -P "${CMAKE_SOURCE_DIR}/cmake/copy_if_missing.cmake"
    COMMENT "Staging driver to driver_stage/arcadecabvr/"
)
