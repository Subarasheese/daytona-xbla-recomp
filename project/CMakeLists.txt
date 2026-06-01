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
    src/daytona_render.cpp
    src/daytona_inspector.cpp
    src/daytona_renderer.cpp
    src/daytona_vulkan_renderer.cpp
    src/daytona_import_wrapper_funcs.cpp
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

# Force all kernel symbols into the executable (needed for hook registration).
# On Windows the per-game __imp__ overrides in stubs.cpp are exported individually
# via DAYTONA_EXPORT (__declspec(dllexport)) so the import resolver
# (win_import_wrappers.cpp) can find them with GetProcAddress and prefer them over
# the runtime DLL's defaults — the same precedence Linux gets from whole-archive +
# dlsym(RTLD_DEFAULT). No blanket --export-all-symbols is needed (or wanted: it
# would bloat the PE export table with every generated sub_ function).
if(WIN32)
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

if(WIN32 AND TARGET daytona)
  set_target_properties(daytona PROPERTIES WIN32_EXECUTABLE OFF)
endif()

if(WIN32 AND TARGET daytona)
  target_link_options(daytona PRIVATE "-mconsole" "-Wl,--subsystem,console")
endif()

if(WIN32 AND TARGET daytona)
  target_sources(daytona PRIVATE src/winmain_shim.cpp)
  # winmm provides timeBeginPeriod/timeEndPeriod used by the WinMain shim to pin
  # the process timer resolution to 1ms (guest thread delays are built on
  # ::Sleep(ms) and would otherwise quantize to the ~15.6ms default).
  target_link_libraries(daytona PRIVATE winmm)
endif()


if(WIN32 AND TARGET daytona)
  target_sources(daytona PRIVATE src/win_import_wrappers.cpp)
endif()


if(UNIX AND NOT WIN32 AND TARGET daytona)
  target_sources(daytona PRIVATE src/linux_import_dispatch.cpp)
  target_link_libraries(daytona PRIVATE ${CMAKE_DL_LIBS})
endif()
