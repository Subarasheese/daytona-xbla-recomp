cmake_minimum_required(VERSION 3.25)
project(daytona LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Daytona XBLA requires the daytonaxbla branch of ReXGlue.
set(REXGLUE_SDK_SOURCE_DIR
    "${CMAKE_SOURCE_DIR}/../thirdparty/rexglue-sdk"
    CACHE PATH "Path to the required ReXGlue SDK source tree")

if(EXISTS "${REXGLUE_SDK_SOURCE_DIR}/CMakeLists.txt")
    add_subdirectory("${REXGLUE_SDK_SOURCE_DIR}" rexglue-sdk)
else()
    message(FATAL_ERROR
        "Required ReXGlue SDK submodule not found. Run:\n"
        "  git submodule update --init --recursive\n"
        "Expected branch: daytonaxbla\n"
        "Expected path: ${REXGLUE_SDK_SOURCE_DIR}")
endif()

# Include generated source list from rexglue codegen
include(${CMAKE_SOURCE_DIR}/../generated/sources.cmake)

add_executable(daytona
    src/main.cpp
    src/stubs.cpp
    src/keyboard_driver.cpp
    src/daytona_debug.cpp
    ${GENERATED_SOURCES}
)

# Let rexglue_configure_target add: entry point, rex_app.cpp, mcmodel=large,
# GTK3, RPATH, and other platform flags.
rexglue_configure_target(daytona)

target_include_directories(daytona PRIVATE
    ${CMAKE_SOURCE_DIR}/../generated
    ${CMAKE_SOURCE_DIR}/../ppc
    ${REXGLUE_SDK_SOURCE_DIR}/thirdparty/simde
    ${REXGLUE_SDK_SOURCE_DIR}/include/simde
    ${REXGLUE_SDK_SOURCE_DIR}/thirdparty/imgui
    ${CMAKE_BINARY_DIR}/rexglue-sdk/include
)

target_link_libraries(daytona PRIVATE
    rex::runtime
)

target_compile_definitions(daytona PRIVATE PPC_INCLUDE_DETAIL)

target_compile_options(daytona PRIVATE
    -fno-strict-aliasing
    -ffp-model=strict
    -fno-char8_t
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
    $<$<CONFIG:RelWithDebInfo>:-O2 -g -DNDEBUG>
)

# Force all kernel symbols into the executable (needed for hook registration)
if(WIN32)
    target_link_options(daytona PRIVATE
        "LINKER:/WHOLEARCHIVE:$<TARGET_FILE:rex::kernel>"
    )
else()
    target_link_options(daytona PRIVATE
        -Wl,--whole-archive $<TARGET_FILE:rex::runtime> -Wl,--no-whole-archive
    )
endif()

add_custom_target(daytona_codegen
    COMMAND $<TARGET_FILE:rex::rexglue> --force codegen ${CMAKE_SOURCE_DIR}/../config/daytona_manifest.toml
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/../config
    COMMENT "Regenerating Daytona codegen with vendored ReXGlue SDK"
    VERBATIM
)
