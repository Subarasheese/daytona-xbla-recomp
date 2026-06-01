#pragma once

#ifdef _WIN32
#include <cstdio>

static inline void daytona_boot_trace(const char* msg) {
    FILE* f = std::fopen("daytona_boot_trace.txt", "ab");
    if (f) {
        std::fprintf(f, "%s\n", msg);
        std::fclose(f);
    }
}
#else
static inline void daytona_boot_trace(const char*) {}
#endif
