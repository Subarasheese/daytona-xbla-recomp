if(NOT EXISTS "${CODEGEN_INIT_H}")
    message(FATAL_ERROR
        "generated/daytona_init.h not found — run codegen before building:\n"
        "  cmake --build <build-dir> --target daytona_codegen\n"
        "See README for details.")
endif()
