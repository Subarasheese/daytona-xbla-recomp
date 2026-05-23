#pragma once

// Daytona USA (XBLA) -- PPC runtime overrides.
// Included via daytona_init.h AFTER the SDK headers.

#include <cstdio>
#include <cstdint>

// Override __builtin_trap() for out-of-range switch indices.
// The recompiler emits these as safety nets; some are legitimately reached at runtime.
#ifdef __builtin_trap
#undef __builtin_trap
#endif
#define __builtin_trap() do { \
    static int _tc = 0; \
    if (++_tc <= 10) \
        fprintf(stderr, "[WARN] Switch OOB (LR=0x%08X) -- continuing\n", (uint32_t)ctx.lr); \
} while(0)

// Safe indirect call: null check + range validation + import thunk resolution.
// Overrides PPC_CALL_INDIRECT_FUNC from SDK context.h.
#undef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(x) do { \
    uint32_t _target = (x); \
    if (_target == 0) { \
        static int _nc = 0; \
        if (++_nc <= 5) \
            fprintf(stderr, "[WARN] Indirect call NULL (LR=0x%08X)\n", (uint32_t)ctx.lr); \
        ctx.r3.u32 = 0; break; \
    } \
    if (_target < (uint32_t)PPC_CODE_BASE || _target >= (uint32_t)(PPC_CODE_BASE + PPC_CODE_SIZE)) { \
        if (_target >= (uint32_t)PPC_IMAGE_BASE && _target < (uint32_t)PPC_CODE_BASE) { \
            uint32_t i0 = PPC_LOAD_U32(base, _target); \
            uint32_t i1 = PPC_LOAD_U32(base, _target + 4); \
            uint16_t hi = (uint16_t)(i0 & 0xFFFF); \
            int16_t lo = (int16_t)(i1 & 0xFFFF); \
            uint32_t iat_addr = ((uint32_t)hi << 16) + lo; \
            uint32_t resolved = PPC_LOAD_U32(base, iat_addr); \
            if (resolved >= (uint32_t)PPC_CODE_BASE && resolved < (uint32_t)(PPC_CODE_BASE + PPC_CODE_SIZE)) { \
                PPCFunc* _fn = PPC_LOOKUP_FUNC(base, resolved); \
                if (_fn) { _fn(ctx, base); break; } \
            } \
            static int _imp = 0; \
            if (++_imp <= 20) \
                fprintf(stderr, "[WARN] Import thunk 0x%08X unresolved (LR=0x%08X)\n", \
                        _target, (uint32_t)ctx.lr); \
            ctx.r3.u32 = 0; break; \
        } \
        static int _oor = 0; \
        if (++_oor <= 20) \
            fprintf(stderr, "[WARN] Indirect call OOR 0x%08X (LR=0x%08X)\n", \
                    _target, (uint32_t)ctx.lr); \
        ctx.r3.u32 = 0; break; \
    } \
    PPCFunc* _fn = PPC_LOOKUP_FUNC(base, _target); \
    if (!_fn) { \
        static int _nf = 0; \
        if (++_nf <= 50) \
            fprintf(stderr, "[WARN] Indirect 0x%08X: no func (LR=0x%08X)\n", \
                    _target, (uint32_t)ctx.lr); \
        ctx.r3.u32 = 0; break; \
    } \
    _fn(ctx, base); \
} while(0)
