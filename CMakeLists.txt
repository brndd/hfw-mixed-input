cmake_minimum_required(VERSION 3.25)
project(hfw_mixed_input VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# Git version string
if(NOT DEFINED MOD_VERSION)
    find_package(Git QUIET)
    if(GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE MOD_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif()
    if(NOT MOD_VERSION)
        set(MOD_VERSION "v${PROJECT_VERSION}")
    endif()
endif()

add_definitions(-DMOD_VERSION="${MOD_VERSION}")

option(ENABLE_VERBOSE_INPUT_LOGGING "Enable verbose frame-by-frame mouse and touchpad logging" OFF)
if(ENABLE_VERBOSE_INPUT_LOGGING)
    add_compile_definitions(VERBOSE_INPUT_LOGGING)
endif()

# SafetyHook + Zydis static library
add_library(safetyhook STATIC
    external/safetyhook/Zydis.c
    external/safetyhook/safetyhook.cpp
)
target_include_directories(safetyhook PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/external/safetyhook
)
target_compile_features(safetyhook PUBLIC cxx_std_23)

# version.dll target
add_library(version SHARED
    src/main.cpp
    src/config.cpp
    src/logger.cpp
    src/scanner.cpp
    src/patches.cpp
    src/proxy.cpp
    src/camera_hook.cpp
    src/context_hook.cpp
    src/steam_input.cpp
    src/version.def
)

target_include_directories(version PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/safetyhook
)

target_link_libraries(version PRIVATE
    safetyhook
)

target_link_options(version PRIVATE
    -static
    -static-libgcc
    -static-libstdc++
    -s
    ${CMAKE_CURRENT_SOURCE_DIR}/src/version.def
)

set_target_properties(version PROPERTIES
    OUTPUT_NAME "version"
    PREFIX ""
    SUFFIX ".dll"
)

# Package target
file(GLOB VDF_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/vdf/*.vdf")

set(DIST_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/mixed_input_fix.ini
    ${VDF_FILES}
)

set(DIST_ZIP "${CMAKE_CURRENT_BINARY_DIR}/hfw-mixed-input-${MOD_VERSION}.zip")
set(DIST_DIR "${CMAKE_CURRENT_BINARY_DIR}/dist")

add_custom_target(package_zip
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${DIST_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:version> ${DIST_FILES} "${DIST_DIR}"
    COMMAND ${CMAKE_COMMAND} -E chdir "${DIST_DIR}" ${CMAKE_COMMAND} -E tar cf "${DIST_ZIP}" --format=zip .
    DEPENDS version ${DIST_FILES}
    COMMENT "Building distributable zip: ${DIST_ZIP}"
    VERBATIM
)

add_custom_target(dist DEPENDS package_zip)
