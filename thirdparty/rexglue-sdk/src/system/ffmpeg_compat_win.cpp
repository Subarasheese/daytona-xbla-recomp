#include <cstdarg>
#include <cstdio>
#include <cstdlib>

extern "C" double avpriv_strtod(const char* nptr, char** endptr) {
    return std::strtod(nptr, endptr);
}

extern "C" int avpriv_snprintf(char* s, size_t n, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = std::vsnprintf(s, n, fmt, ap);
    va_end(ap);
    return ret;
}
