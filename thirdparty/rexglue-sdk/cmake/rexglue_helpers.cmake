#==========================================================
# rexglue_helpers.cmake
#
# Three helpers, each with a single responsibility:
#   rexglue_apply_target_settings(<target>)        - common compile/platform flags
#   rexglue_configure_target(<target>)             - host application
#   rexglue_configure_module_target(<target> ...)  - guest DLL module
#==========================================================

#==========================================================
# rexglue_apply_target_settings(<target>) - Common flags
#
# Applied to both host apps and guest DLL modules. Compile/link flags only;
# runtime DLL staging is the host's job (see rexglue_configure_target).
#==========================================================
function(rexglue_apply_target_settings target_name)
    if(UNIX AND NOT APPLE)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
        target_include_directories(${target_name} PRIVATE ${GTK3_INCLUDE_DIRS})
        target_link_libraries(${target_name} PRIVATE ${GTK3_LIBRARIES})
        # Large executable support
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
            target_link_options(${target_name} PRIVATE -Wl,--no-relax)
            target_compile_options(${target_name} PRIVATE -mcmodel=large)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
            target_compile_options(${target_name} PRIVATE -march=armv8-a)
        endif()
    endif()

    if(NOT MSVC)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
            target_compile_options(${target_name} PRIVATE -msse4.1)
        endif()
    endif()
endfunction()

#==========================================================
# rexglue_configure_target(<target>) - Host application
#
# Adds:
#   - Platform entry point source (windowed_app_main_*.cpp)
#   - ReXApp base class source (rex_app.cpp)
#   - Build-config define for the version stamp
#   - $ORIGIN RPATH on UNIX so the host finds librexruntime.so next to itself
#   - Windows POST_BUILD copy of TARGET_RUNTIME_DLLS and the FidelityFX DLLs.
#     Guest modules colocate with the host (see rexglue_configure_module_target),
#     so this single copy handles them transitively.
#==========================================================
function(rexglue_configure_target target_name)
    if(WIN32)
        target_sources(${target_name} PRIVATE
            ${REXGLUE_SHARE_DIR}/windowed_app_main_win.cpp)
    else()
        target_sources(${target_name} PRIVATE
            ${REXGLUE_SHARE_DIR}/windowed_app_main_posix.cpp)
    endif()

    target_sources(${target_name} PRIVATE
        ${REXGLUE_SHARE_DIR}/rex_app.cpp)

    target_compile_definitions(${target_name} PRIVATE
        REXGLUE_BUILD_CONFIG="$<CONFIG>")

    if(UNIX AND NOT APPLE)
        set_target_properties(${target_name} PROPERTIES
            INSTALL_RPATH "$ORIGIN"
            BUILD_WITH_INSTALL_RPATH ON
        )
    endif()

    rexglue_apply_target_settings(${target_name})

    if(WIN32)
        # Stage runtime DLLs (rexruntime, TracyClient, etc.) next to the host
        # binary on every link. copy_if_different is a no-op when up to date.
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND "$<$<BOOL:$<TARGET_RUNTIME_DLLS:${target_name}>>:${CMAKE_COMMAND};-E;copy_if_different;$<TARGET_RUNTIME_DLLS:${target_name}>;$<TARGET_FILE_DIR:${target_name}>>"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
        # FidelityFX is linked PRIVATE by rexui (to avoid propagating DLL
        # requirements to tool-mode targets), so copy its DLLs explicitly.
        foreach(_fx amd_fidelityfx_vk amd_fidelityfx_dx12)
            if(TARGET ${_fx})
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        $<TARGET_FILE:${_fx}>
                        $<TARGET_FILE_DIR:${target_name}>
                    VERBATIM
                )
            endif()
        endforeach()
    endif()
endfunction()

#==========================================================
# rexglue_configure_module_target(<target> [HOST <host_target>])
#   - Guest DLL module
#
# Output is colocated with HOST (or CMAKE_RUNTIME_OUTPUT_DIRECTORY) so the
# host's LoadUserModule finds it; the host is wired to depend on the module
# so a top-level build of the host pulls in all guest DLLs. No runtime DLL
# staging here; the host's POST_BUILD copy covers shared dependencies.
#==========================================================
function(rexglue_configure_module_target target_name)
    cmake_parse_arguments(ARG "" "HOST" "" ${ARGN})

    if(ARG_HOST)
        set_target_properties(${target_name} PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:${ARG_HOST}>
            RUNTIME_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:${ARG_HOST}>
            ARCHIVE_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:${ARG_HOST}>
        )
        # Defer add_dependencies so the host target may be declared after
        # this module call. Wrapping in EVAL CODE expands the variables now,
        # which DEFER CALL otherwise treats as literal argument text.
        cmake_language(EVAL CODE
            "cmake_language(DEFER CALL add_dependencies ${ARG_HOST} ${target_name})")
    elseif(CMAKE_RUNTIME_OUTPUT_DIRECTORY)
        set_target_properties(${target_name} PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
            ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        )
    endif()

    if(UNIX AND NOT APPLE)
        set_target_properties(${target_name} PROPERTIES
            INSTALL_RPATH "$ORIGIN"
            BUILD_WITH_INSTALL_RPATH ON
        )
    endif()

    rexglue_apply_target_settings(${target_name})
endfunction()
