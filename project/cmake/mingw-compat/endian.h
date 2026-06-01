#pragma once

#include <stdint.h>

#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN 1234
#endif

#ifndef __BIG_ENDIAN
#define __BIG_ENDIAN 4321
#endif

#ifndef __BYTE_ORDER
#define __BYTE_ORDER __LITTLE_ENDIAN
#endif

#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN __LITTLE_ENDIAN
#endif

#ifndef BIG_ENDIAN
#define BIG_ENDIAN __BIG_ENDIAN
#endif

#ifndef BYTE_ORDER
#define BYTE_ORDER __BYTE_ORDER
#endif

#ifndef htobe16
#define htobe16(x) __builtin_bswap16((uint16_t)(x))
#endif

#ifndef htole16
#define htole16(x) ((uint16_t)(x))
#endif

#ifndef be16toh
#define be16toh(x) __builtin_bswap16((uint16_t)(x))
#endif

#ifndef le16toh
#define le16toh(x) ((uint16_t)(x))
#endif

#ifndef htobe32
#define htobe32(x) __builtin_bswap32((uint32_t)(x))
#endif

#ifndef htole32
#define htole32(x) ((uint32_t)(x))
#endif

#ifndef be32toh
#define be32toh(x) __builtin_bswap32((uint32_t)(x))
#endif

#ifndef le32toh
#define le32toh(x) ((uint32_t)(x))
#endif

#ifndef htobe64
#define htobe64(x) __builtin_bswap64((uint64_t)(x))
#endif

#ifndef htole64
#define htole64(x) ((uint64_t)(x))
#endif

#ifndef be64toh
#define be64toh(x) __builtin_bswap64((uint64_t)(x))
#endif

#ifndef le64toh
#define le64toh(x) ((uint64_t)(x))
#endif
