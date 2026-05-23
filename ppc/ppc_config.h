#pragma once
#ifndef PPC_CONFIG_H_INCLUDED
#define PPC_CONFIG_H_INCLUDED

// Daytona USA (XBLA) - Title ID 58410B1D
// Image base: 0x82000000
// Entry point: 0x82210E98
// .text section: VA=0x82120000, VSize=0x3DF72C
// .idata section: VA=0x83440000 (import thunks)
// Full image size from XEX: 0x1460000

#define PPC_IMAGE_BASE 0x82000000ull
#define PPC_IMAGE_SIZE 0x1460000ull
#define PPC_CODE_BASE  0x82120000ull
#define PPC_CODE_SIZE  0x3E0000ull

#ifdef PPC_INCLUDE_DETAIL
#include "ppc_detail.h"
#endif

#endif
