#pragma once

#ifdef _WIN32
#include <direct.h>
#include <locale.h>
#include <stdlib.h>

#ifndef LC_ALL_MASK
#define LC_ALL_MASK LC_ALL
#endif

#ifndef DAYTONA_HAS_POSIX_LOCALE_COMPAT
#define DAYTONA_HAS_POSIX_LOCALE_COMPAT 1

using locale_t = _locale_t;

static inline locale_t newlocale(int, const char* name, locale_t) {
    return _create_locale(LC_ALL, name);
}

static inline locale_t uselocale(locale_t loc) {
    return loc;
}

#endif

static inline int daytona_mkdir_compat(const char* path) {
    return _mkdir(path);
}

#else
#include <sys/stat.h>

static inline int daytona_mkdir_compat(const char* path) {
    return mkdir(path, 0755);
}

#endif
