#include "daytona_host_compat.h"
// Daytona USA (XBLA) -- Game-specific Xbox 360 API stubs
// These override or supplement the ReXGlue SDK's built-in kernel stubs.

#include "daytona_init.h"
#include "daytona_debug.h"
#include "daytona_symbols.h"
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/system/xtypes.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

#include "daytona_render.h"

#include "daytona_winapi_macro_undef.h"
#include <rex/graphics/graphics_system.h>
#include <rex/graphics/command_processor.h>
#include <rex/system/kernel_state.h>
#include <rex/runtime.h>

namespace {
inline rex::graphics::CommandProcessor* GetCP() {
    auto* ks = rex::system::kernel_state();
    if (!ks) return nullptr;
    auto* gs = static_cast<rex::graphics::GraphicsSystem*>(ks->emulator()->graphics_system());
    return gs ? gs->command_processor() : nullptr;
}

}  // namespace

// Export EXE-local kernel-import overrides so the Windows import resolver can
// discover them via GetProcAddress, mirroring how Linux's dlsym(RTLD_DEFAULT)
// already finds them. mingw filters __imp_-prefixed symbols out of the PE export
// table (they look like IAT thunks), so an explicit dllexport is required for an
// override to win over the runtime DLL's default. No-op on Linux, where
// whole-archive + dlsym already give the executable's overrides priority.
// Apply this to any extern "C" REX_FUNC(__imp__Xxx) override below.
#if defined(_WIN32)
#define DAYTONA_EXPORT __declspec(dllexport)
#else
#define DAYTONA_EXPORT
#endif

// Return a zero result from a stub (r3 = 0)
#define DT_STUB_RETURN(name, val) \
    extern "C" REX_FUNC(name) { (void)base; ctx.r3.u64 = (val); }

#define DT_STUB(name) DT_STUB_RETURN(name, 0)

// ── XUI / Overlay stubs (no Xbox Live — all no-ops) ──────────────────────────
// Wrappers: UiShowFriends, UiShowAchievements, UiShowMarketplace, UiShowGamerCard
DT_STUB(__imp__XamShowGamerCardUI)
DT_STUB(__imp__XamShowAchievementsUI)
DT_STUB(__imp__XamShowMarketplaceUI)
DT_STUB(__imp__XamShowFriendsUI)
DT_STUB(__imp__XamShowMessageComposeUI)

// ── Network stubs (offline — XNetConnect/GetConnectStatus no-op) ─────────────
// Wrappers: NetXNetConnect, NetGetConnectStatus
DT_STUB(__imp__NetDll_XNetUnregisterInAddr)
DT_STUB(__imp__NetDll_XNetConnect)
DT_STUB(__imp__NetDll_XNetGetConnectStatus)

// ── Leaderboard / Achievement stubs (no Xbox Live) ───────────────────────────
DT_STUB(__imp__XUserWriteAchievements)
DT_STUB(__imp__XUserCreateStatsEnumeratorByRank)
DT_STUB(__imp__XUserCreateStatsEnumeratorByXuid)
DT_STUB(__imp__XamShowGamerCardUIForXUID)

// ── Kernel I/O stubs (device driver path unused on PC) ───────────────────────
DT_STUB(__imp__IoCheckShareAccess)
DT_STUB(__imp__IoCompleteRequest)
DT_STUB(__imp__IoDeleteDevice)
DT_STUB(__imp__IoInvalidDeviceRequest)
DT_STUB(__imp__IoRemoveShareAccess)
DT_STUB(__imp__IoSetShareAccess)

// ── Kernel Object Manager stubs ───────────────────────────────────────────────
DT_STUB(__imp__ObIsTitleObject)
DT_STUB(__imp__ObReferenceObject)

namespace {

bool DaytonaTraceVerbose() {
    static const bool enabled = [] {
        const char* value = std::getenv("DAYTONA_TRACE_VERBOSE");
        return value && value[0] && value[0] != '0';
    }();
    return enabled;
}

bool ShouldLog(uint32_t count) {
    return DaytonaTraceVerbose() && (count <= 16 || (count % 1000) == 0);
}

void LogGuestCall(const char* name, uint32_t count, const PPCContext& ctx) {
    REXLOG_INFO("Daytona hook {} call #{} lr={:08X} r3={:08X} r4={:08X} r5={:08X}",
                name,
                count,
                static_cast<uint32_t>(ctx.lr),
                ctx.r3.u32,
                ctx.r4.u32,
                ctx.r5.u32);
}

void ReadGuestString(uint8_t* base, uint32_t address, char* out, size_t out_size) {
    if (out_size == 0) return;
    size_t i = 0;
    for (; i + 1 < out_size && address != 0; ++i) {
        const uint8_t c = REX_LOAD_U8(address + static_cast<uint32_t>(i));
        if (c == 0) break;
        out[i] = c >= 32 && c < 127 ? static_cast<char>(c) : '.';
    }
    out[i] = 0;
}

}  // namespace

// ── PM4 IB diagnostic dump ───────────────────────────────────────────────────
namespace {

inline uint32_t IbRead(uint32_t phys, uint32_t dword_idx) {
    return daytona_render::GuestReadU32(
        0xA0000000u | ((phys + dword_idx * 4u) & 0x1FFFFFFFu));
}

bool LooksLikePhys(uint32_t v) {
    return v != 0 && v < 0x20000000u;
}

bool LooksLikeGuestBuffer(uint32_t phys, uint32_t size_bytes) {
    return phys >= 0x01000000u &&
           phys < 0x20000000u &&
           size_bytes > 0 &&
           size_bytes <= 8u * 1024u * 1024u;
}

float U32AsFloat(uint32_t v) {
    float f = 0.0f;
    std::memcpy(&f, &v, sizeof(f));
    return f;
}

inline bool IsDrawOp(uint8_t op) {
    return op==0x2C || op==0x2D || op==0x33 || op==0x36 || op==0x43;
}

void LogDrawDecode(const char* pfx, uint8_t op, uint32_t cnt, const uint32_t* w) {
    if (op == 0x2C) {
        REXLOG_ERROR("{}    2C: viz={:08X} init={:08X} prim={} sel={} idx={}",
            pfx, w[0], w[1], w[1]&0x3Fu, (w[1]>>6)&3u, w[1]>>10);
        if (((w[1]>>6)&3u) == 0)
            REXLOG_ERROR("{}    2C DMA: ib_phys={:08X} dw={}", pfx, w[2], w[3]);
    } else if (op == 0x36) {
        REXLOG_ERROR("{}    36: init={:08X} prim={} sel={} idx={}",
            pfx, w[0], w[0]&0x3Fu, (w[0]>>6)&3u, w[0]>>10);
    } else if (op == 0x2D) {
        // DRAW_INDX_BIN: still not fully decoded; log both likely initiator slots.
        REXLOG_ERROR("{}    2D d[0]-as-init: init={:08X} prim={} sel={} idx={}",
            pfx, w[0], w[0]&0x3Fu, (w[0]>>6)&3u, w[0]>>10);
        REXLOG_ERROR("{}    2D d[1]-as-init: init={:08X} prim={} sel={} idx={}",
            pfx, w[1], w[1]&0x3Fu, (w[1]>>6)&3u, w[1]>>10);
        if (cnt == 4) {
            // Observed in the live path: w[2]=y0:x0, w[3]=y1:x1 for
            // 1920x1080 screen strips.
            REXLOG_ERROR("{}    2D rect: x0={} y0={} x1={} y1={} flags0={:08X} flags1={:08X}",
                pfx,
                w[2] & 0xFFFFu, w[2] >> 16,
                w[3] & 0xFFFFu, w[3] >> 16,
                w[0], w[1]);
        } else if (cnt == 17) {
            REXLOG_ERROR("{}    2D strip-setup? y={:.3f} raw_w2={:08X}",
                pfx, U32AsFloat(w[2]), w[2]);
        } else if (cnt == 2) {
            REXLOG_ERROR("{}    2D tile-key? raw_w1={:08X}", pfx, w[1]);
        }
    }
}

struct DrawSig {
    uint8_t op = 0;
    uint32_t cnt = 0;
    uint32_t w0 = 0;
    uint32_t w1 = 0;
    uint32_t w2 = 0;
    uint32_t w3 = 0;
    uint32_t hits = 0;
    uint32_t first_off = 0;
};

struct PacketSig {
    uint32_t a = 0;
    uint32_t cnt = 0;
    uint32_t w0 = 0;
    uint32_t w1 = 0;
    uint32_t hits = 0;
    uint32_t first_off = 0;
};

struct ActiveVFetch {
    bool valid = false;
    uint32_t seq = 0;
    uint32_t phys = 0;
    uint32_t bytes = 0;
    uint32_t endian = 0;
};

struct ActiveTFetch {
    bool valid = false;
    uint32_t seq = 0;
    uint32_t base = 0;
    uint32_t mip = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t format = 0;
    uint32_t dim = 0;
    uint32_t pitch = 0;
    uint32_t tiled = 0;
};

struct ActiveFetchState {
    uint32_t seq = 0;
    ActiveVFetch v[96]{};
    ActiveTFetch t[32]{};
};

void AddDrawSig(DrawSig* sigs, uint32_t& sig_count, uint32_t max_sigs,
                uint8_t op, uint32_t cnt, uint32_t w0, uint32_t w1,
                uint32_t w2, uint32_t w3,
                uint32_t off) {
    for (uint32_t i = 0; i < sig_count; ++i) {
        if (sigs[i].op == op && sigs[i].cnt == cnt &&
            sigs[i].w0 == w0 && sigs[i].w1 == w1 &&
            sigs[i].w2 == w2 && sigs[i].w3 == w3) {
            ++sigs[i].hits;
            return;
        }
    }
    if (sig_count >= max_sigs) return;
    DrawSig& s = sigs[sig_count++];
    s.op = op;
    s.cnt = cnt;
    s.w0 = w0;
    s.w1 = w1;
    s.w2 = w2;
    s.w3 = w3;
    s.hits = 1;
    s.first_off = off;
}

void AddPacketSig(PacketSig* sigs, uint32_t& sig_count, uint32_t max_sigs,
                  uint32_t a, uint32_t cnt, uint32_t w0, uint32_t w1,
                  uint32_t off) {
    for (uint32_t i = 0; i < sig_count; ++i) {
        if (sigs[i].a == a && sigs[i].cnt == cnt &&
            sigs[i].w0 == w0 && sigs[i].w1 == w1) {
            ++sigs[i].hits;
            return;
        }
    }
    if (sig_count >= max_sigs) return;
    PacketSig& s = sigs[sig_count++];
    s.a = a;
    s.cnt = cnt;
    s.w0 = w0;
    s.w1 = w1;
    s.hits = 1;
    s.first_off = off;
}

bool IsInterestingRegBase(uint32_t base) {
    switch (base) {
        case 0x2000: case 0x2007: case 0x200D:
        case 0x2100: case 0x2102:
        case 0x2180:
        case 0x2200: case 0x2203: case 0x2204: case 0x2208:
        case 0x2280: case 0x2293:
        case 0x2300: case 0x2312: case 0x2318: case 0x231B:
        case 0x4800: case 0x5000:
            return true;
        default:
            return false;
    }
}

void DumpPhysSample(const char* label, const char* pfx, uint32_t phys, uint32_t max_words) {
    static uint32_t seen[64]{};
    static uint32_t seen_count = 0;
    const uint32_t aligned = phys & ~0x3u;
    if (!LooksLikePhys(aligned)) return;
    for (uint32_t i = 0; i < seen_count; ++i) {
        if (seen[i] == aligned) return;
    }
    if (seen_count < 64) seen[seen_count++] = aligned;

    uint32_t w[8]{};
    const uint32_t n = max_words < 8u ? max_words : 8u;
    for (uint32_t i = 0; i < n; ++i)
        w[i] = IbRead(aligned, i);
    REXLOG_ERROR("{}  {} phys={:08X} sample={} {:08X} {:08X} {:08X} {:08X} {:08X} {:08X} {:08X} {:08X}",
        pfx, label, aligned, n, w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]);
    REXLOG_ERROR("{}  {}F phys={:08X} {:.3f} {:.3f} {:.3f} {:.3f} {:.3f} {:.3f} {:.3f} {:.3f}",
        pfx, label, aligned,
        U32AsFloat(w[0]), U32AsFloat(w[1]), U32AsFloat(w[2]), U32AsFloat(w[3]),
        U32AsFloat(w[4]), U32AsFloat(w[5]), U32AsFloat(w[6]), U32AsFloat(w[7]));
}

uint32_t EstimateTextureBytes(uint32_t base_phys, uint32_t mip_phys,
                              uint32_t width, uint32_t height,
                              uint32_t format) {
    if (mip_phys > base_phys) {
        const uint32_t level0_bytes = mip_phys - base_phys;
        if (level0_bytes > 0 && level0_bytes <= 16u * 1024u * 1024u)
            return level0_bytes;
    }

    uint64_t bytes = 0;
    switch (format) {
        case 6:   // observed render-target / packed surface path
        case 10:  // observed color texture path
            bytes = static_cast<uint64_t>(width) * height * 4u;
            break;
        case 20:  // observed large post-process / framebuffer-like path
            bytes = static_cast<uint64_t>(width) * height * 4u;
            break;
        default:
            bytes = static_cast<uint64_t>(width) * height * 4u;
            break;
    }
    if (bytes == 0 || bytes > 16ull * 1024ull * 1024ull) return 0;
    return static_cast<uint32_t>(bytes);
}

void DumpTextureRawOnce(const char* pfx, uint32_t base_phys, uint32_t mip_phys,
                        uint32_t width, uint32_t height, uint32_t format,
                        uint32_t dim, uint32_t pitch, uint32_t tiled) {
    struct SeenTexture {
        uint32_t base;
        uint32_t mip;
        uint32_t width;
        uint32_t height;
        uint32_t format;
    };
    static SeenTexture seen[128]{};
    static uint32_t seen_count = 0;

    if (!LooksLikePhys(base_phys) || width == 0 || height == 0) return;
    for (uint32_t i = 0; i < seen_count; ++i) {
        const SeenTexture& s = seen[i];
        if (s.base == base_phys && s.mip == mip_phys &&
            s.width == width && s.height == height && s.format == format) {
            return;
        }
    }
    if (seen_count >= 128) return;
    seen[seen_count++] = {base_phys, mip_phys, width, height, format};

    const uint32_t bytes = EstimateTextureBytes(base_phys, mip_phys, width, height, format);
    if (bytes == 0) return;

    (void)daytona_mkdir_compat("logs");
    (void)daytona_mkdir_compat("logs/texture_dumps");
    static bool manifest_started = false;
    std::FILE* manifest = std::fopen(
        "logs/texture_dumps/manifest.tsv", manifest_started ? "a" : "w");
    if (manifest && !manifest_started) {
        std::fprintf(manifest,
                     "base\tmip\twidth\theight\tformat\tdimension\tpitch\ttiled\tbytes\tpath\n");
    }
    manifest_started = true;
    char path[160]{};
    std::snprintf(path, sizeof(path),
                  "logs/texture_dumps/tex_%08X_%ux%u_fmt%u.raw",
                  base_phys, width, height, format);

    std::FILE* f = std::fopen(path, "wb");
    if (!f) {
        if (manifest) std::fclose(manifest);
        REXLOG_ERROR("{}  TEXDUMP failed path={} base={:08X}", pfx, path, base_phys);
        return;
    }

    uint8_t out[4096];
    uint32_t written = 0;
    while (written < bytes) {
        uint32_t pos = 0;
        while (pos < sizeof(out) && written + pos < bytes) {
            const uint32_t guest = 0xA0000000u | ((base_phys + written + pos) & 0x1FFFFFFFu);
            const uint32_t word = daytona_render::GuestReadU32(guest & ~3u);
            const uint32_t byte_sel = (base_phys + written + pos) & 3u;
            out[pos++] = static_cast<uint8_t>(word >> ((3u - byte_sel) * 8u));
        }
        const size_t n = std::fwrite(out, 1, pos, f);
        written += static_cast<uint32_t>(n);
        if (n != pos) break;
    }
    std::fclose(f);

    if (manifest) {
        std::fprintf(manifest,
                     "%08X\t%08X\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%s\n",
                     base_phys, mip_phys, width, height, format, dim, pitch,
                     tiled, written, path);
        std::fclose(manifest);
    }
    REXLOG_ERROR("{}  TEXDUMP base={:08X} mip={:08X} {}x{} fmt={} dim={} pitch={} tiled={} bytes={} path={}",
                 pfx, base_phys, mip_phys, width, height, format, dim, pitch,
                 tiled, written, path);
}

void LogDrawFetchState(const char* pfx, uint32_t off, uint8_t op, uint32_t cnt,
                       const ActiveFetchState& fs) {
    uint32_t v_order[6]{};
    uint32_t v_count = 0;
    for (uint32_t pick = 0; pick < 6; ++pick) {
        uint32_t best_idx = 0;
        uint32_t best_seq = 0;
        for (uint32_t i = 0; i < 96; ++i) {
            if (!fs.v[i].valid || fs.v[i].seq <= best_seq) continue;
            bool already = false;
            for (uint32_t j = 0; j < v_count; ++j)
                if (v_order[j] == i) already = true;
            if (already) continue;
            best_idx = i;
            best_seq = fs.v[i].seq;
        }
        if (!best_seq) break;
        v_order[v_count++] = best_idx;
    }

    uint32_t t_order[6]{};
    uint32_t t_count = 0;
    for (uint32_t pick = 0; pick < 6; ++pick) {
        uint32_t best_idx = 0;
        uint32_t best_seq = 0;
        for (uint32_t i = 0; i < 32; ++i) {
            if (!fs.t[i].valid || fs.t[i].seq <= best_seq) continue;
            bool already = false;
            for (uint32_t j = 0; j < t_count; ++j)
                if (t_order[j] == i) already = true;
            if (already) continue;
            best_idx = i;
            best_seq = fs.t[i].seq;
        }
        if (!best_seq) break;
        t_order[t_count++] = best_idx;
    }

    REXLOG_ERROR("{}    DRAWCORR off={:05X} op={:02X} cnt={} vf={} tf={}",
        pfx, off, op, cnt, v_count, t_count);
    for (uint32_t i = 0; i < v_count; ++i) {
        const uint32_t idx = v_order[i];
        const ActiveVFetch& v = fs.v[idx];
        REXLOG_ERROR("{}      V{} phys={:08X} bytes={} endian={} age={}",
            pfx, idx, v.phys, v.bytes, v.endian, fs.seq - v.seq);
    }
    for (uint32_t i = 0; i < t_count; ++i) {
        const uint32_t idx = t_order[i];
        const ActiveTFetch& t = fs.t[idx];
        daytona_render::RecordGuestTextureDrawUse(idx, op, t.base, t.mip,
            t.width, t.height, t.format, t.dim, t.pitch, t.tiled);
        REXLOG_ERROR("{}      T{} base={:08X} mip={:08X} {}x{} fmt={} dim={} pitch={} tiled={} age={}",
            pfx, idx, t.base, t.mip, t.width, t.height, t.format, t.dim,
            t.pitch, t.tiled, fs.seq - t.seq);
    }
}

void LogFetchConstants(const char* pfx, uint32_t base, const uint32_t* w, uint32_t cnt,
                       uint32_t off, ActiveFetchState* fs) {
    if (base < 0x4800 || base >= 0x4890 || cnt == 0) return;

    static uint32_t s_seen_vfetch[96]{};
    static uint32_t s_seen_vfetch_count = 0;
    static uint32_t s_seen_tfetch[96]{};
    static uint32_t s_seen_tfetch_count = 0;

    const uint32_t max_words = cnt < 192u ? cnt : 192u;
    for (uint32_t i = 0; i < max_words;) {
        const uint32_t reg = base + i;
        const uint32_t type = w[i] & 3u;
        if (type == 3u && i + 1 < max_words) {
            // xe_gpu_vertex_fetch_t: dword0 low 2 bits type, upper 30 bits
            // address in dwords; dword1 low 2 bits endian, next 24 bits size.
            const uint32_t phys = w[i] & ~3u;
            const uint32_t endian = w[i + 1] & 3u;
            const uint32_t size_words = (w[i + 1] >> 2) & 0x00FFFFFFu;
            const uint32_t size_bytes = size_words * 4u;
            const uint32_t vfetch = (reg - 0x4800u) / 2u;
            if (!LooksLikeGuestBuffer(phys, size_bytes)) {
                i += 2;
                continue;
            }

            if (fs && vfetch < 96) {
                ActiveVFetch& vf = fs->v[vfetch];
                vf.valid = true;
                vf.seq = ++fs->seq;
                vf.phys = phys;
                vf.bytes = size_bytes;
                vf.endian = endian;
            }

            REXLOG_ERROR("{}  VFETCH off={:05X} reg={:04X} idx={} phys={:08X} bytes={} endian={} raw={:08X} {:08X}",
                pfx, off, reg, vfetch, phys, size_bytes, endian, w[i], w[i + 1]);

            bool seen = false;
            for (uint32_t s = 0; s < s_seen_vfetch_count; ++s) {
                if (s_seen_vfetch[s] == phys) {
                    seen = true;
                    break;
                }
            }
            if (!seen) {
                if (s_seen_vfetch_count < 96) s_seen_vfetch[s_seen_vfetch_count++] = phys;
                DumpPhysSample("VFETCHSAMPLE", pfx, phys, 8);
            }
            i += 2;
            continue;
        }
        if (type == 2u && i + 5 < max_words && ((reg - 0x4800u) % 6u) == 0) {
            const uint32_t tfetch = (reg - 0x4800u) / 6u;
            const uint32_t pitch = (w[i] >> 22) & 0x1FFu;
            const uint32_t tiled = (w[i] >> 31) & 1u;
            const uint32_t format = w[i + 1] & 0x3Fu;
            const uint32_t endian = (w[i + 1] >> 6) & 3u;
            const uint32_t base_phys = ((w[i + 1] >> 12) & 0xFFFFFu) << 12;
            const uint32_t width = (w[i + 2] & 0x1FFFu) + 1u;
            const uint32_t height = ((w[i + 2] >> 13) & 0x1FFFu) + 1u;
            const uint32_t dim = (w[i + 5] >> 9) & 3u;
            const uint32_t mip_phys = ((w[i + 5] >> 12) & 0xFFFFFu) << 12;
            daytona_render::RecordGuestTexture(tfetch, base_phys, mip_phys,
                width, height, format, dim, pitch, tiled);
            if (fs && tfetch < 32) {
                ActiveTFetch& tf = fs->t[tfetch];
                tf.valid = true;
                tf.seq = ++fs->seq;
                tf.base = base_phys;
                tf.mip = mip_phys;
                tf.width = width;
                tf.height = height;
                tf.format = format;
                tf.dim = dim;
                tf.pitch = pitch;
                tf.tiled = tiled;
            }

            const uint32_t sig = w[i] ^ (w[i + 1] * 33u) ^ (w[i + 2] * 65537u);
            bool seen = false;
            for (uint32_t s = 0; s < s_seen_tfetch_count; ++s) {
                if (s_seen_tfetch[s] == sig) {
                    seen = true;
                    break;
                }
            }
            if (!seen) {
                if (s_seen_tfetch_count < 96) s_seen_tfetch[s_seen_tfetch_count++] = sig;
                REXLOG_ERROR(
                    "{}  TFETCH off={:05X} reg={:04X} idx={} base={:08X} mip={:08X} {}x{} fmt={} dim={} pitch={} tiled={} endian={} raw={:08X} {:08X} {:08X} {:08X} {:08X} {:08X}",
                    pfx, off, reg, tfetch, base_phys, mip_phys, width, height,
                    format, dim, pitch, tiled, endian,
                    w[i], w[i + 1], w[i + 2], w[i + 3], w[i + 4], w[i + 5]);
                DumpTextureRawOnce(pfx, base_phys, mip_phys, width, height,
                                   format, dim, pitch, tiled);
            }
            i += 6;
            continue;
        }
        ++i;
    }
}

const char* Pm4OpShort(uint8_t op) {
    switch (op) {
        case 0x10: return "NOP";      case 0x12: return "IM_LOAD";
        case 0x19: return "LD_ALU_K"; case 0x1A: return "SET_CST2";
        case 0x21: return "REG_RMW";  case 0x23: return "VIZ_QRY";
        case 0x2B: return "SET_CST";  case 0x2C: return "DRAW_INDX";
        case 0x2D: return "DRAW_BIN"; case 0x2F: return "LD_CTX";
        case 0x31: return "IM_IMM";   case 0x33: return "DRAW_BN2";
        case 0x36: return "DRAW_2";   case 0x39: return "SET_SHDK";
        case 0x3B: return "INV_ST";   case 0x3C: return "WAIT_REG";
        case 0x3D: return "MEM_WR";   case 0x3E: return "REG_MEM";
        case 0x3F: return "IB";       case 0x43: return "DRAW_IMM";
        case 0x45: return "COND_WR";  case 0x46: return "EVT_WR";
        case 0x47: return "EOP";      case 0x64: return "SWAP";
        default:   return "?";
    }
}

bool LooksLikeGuestVirt(uint32_t addr) {
    return (addr & 3u) == 0 &&
           addr >= 0x70000000u &&
           addr < 0xC0000000u;
}

uint32_t SafeGuestLoadU32(uint8_t* base, uint32_t addr) {
    if (!LooksLikeGuestVirt(addr)) return 0;
    return REX_LOAD_U32(addr);
}

void LogGuestWords(uint8_t* base, const char* label, uint32_t seq, uint32_t addr, uint32_t words = 16) {
    if (!LooksLikeGuestVirt(addr)) return;
    uint32_t w[16]{};
    const uint32_t n = words < 16u ? words : 16u;
    for (uint32_t i = 0; i < n; ++i)
        w[i] = SafeGuestLoadU32(base, addr + i * 4u);
    REXLOG_ERROR(
        "{} #{} mem {:08X}: {:08X} {:08X} {:08X} {:08X}  {:08X} {:08X} {:08X} {:08X}  {:08X} {:08X} {:08X} {:08X}  {:08X} {:08X} {:08X} {:08X}",
        label, seq, addr,
        w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7],
        w[8], w[9], w[10], w[11], w[12], w[13], w[14], w[15]);
}

void LogGuestWordsAt(uint8_t* base, const char* label, uint32_t seq,
                     uint32_t addr, uint32_t word_off) {
    if (!LooksLikeGuestVirt(addr)) return;
    LogGuestWords(base, label, seq, addr + word_off * 4u);
}

void LogTexCall(uint8_t* base, const char* label, uint32_t seq, const PPCContext& ctx, bool after = false) {
    REXLOG_ERROR(
        "TEXPROBE {} {}#{} lr={:08X} r3={:08X} r4={:08X} r5={:08X} r6={:08X} r7={:08X} r8={:08X} r9={:08X} r10={:08X}",
        label, after ? "exit" : "enter", seq, static_cast<uint32_t>(ctx.lr),
        ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32,
        ctx.r8.u32, ctx.r9.u32, ctx.r10.u32);
    LogGuestWords(base, label, seq, ctx.r3.u32);
    LogGuestWords(base, label, seq, ctx.r4.u32);
    LogGuestWords(base, label, seq, ctx.r5.u32);
    LogGuestWords(base, label, seq, ctx.r6.u32);
    if (after) LogGuestWords(base, label, seq, ctx.r10.u32);
    if (after && std::strcmp(label, "TexFinalizeB") == 0) {
        constexpr uint32_t kOffsets[] = {
            0, 16, 32, 64, 128, 256, 384, 512,
            768, 1024, 1280, 1536, 1792
        };
        for (uint32_t off : kOffsets)
            LogGuestWordsAt(base, "TexFinalizeB.ctx", seq, ctx.r4.u32, off);
    }
}

// Scan an IB (and nested IBs) looking for any draw opcode.
bool IbHasDraws(uint32_t phys, uint32_t dwords, int depth = 0) {
    uint32_t off = 0;
    while (off < dwords && off < 8000) {
        const uint32_t hdr  = IbRead(phys, off);
        const uint32_t type = hdr >> 30;
        if (type == 2) { ++off; continue; }
        if (type != 0 && type != 3) break;
        const uint32_t cnt = ((hdr >> 16) & 0x3FFFu) + 1;
        if (type == 3) {
            const uint8_t op = (hdr >> 8) & 0xFF;
            if (IsDrawOp(op)) return true;
            if (op == 0x3F && cnt >= 2 && depth < 3) {
                const uint32_t n_phys = IbRead(phys, off + 1);
                const uint32_t n_dw   = IbRead(phys, off + 2);
                if (IbHasDraws(n_phys, n_dw, depth + 1)) return true;
            }
        }
        off += 1 + cnt;
    }
    return false;
}

void DumpIBToLog(uint32_t ib_phys, uint32_t ib_dwords, int ib_num, int depth = 0) {
    const char* pfx = depth == 0 ? "" : "  >>";
    REXLOG_ERROR("{}===IB#{} phys={:08X} dwords={} ({:.1f}KB)===",
                 pfx, ib_num, ib_phys, ib_dwords, ib_dwords * 4.0f / 1024.0f);
    uint32_t off = 0, pkts = 0;
    uint32_t draw_count = 0;
    uint32_t ib_count = 0;
    uint32_t state_count = 0;
    uint32_t state_suppressed = 0;
    uint32_t draw_suppressed = 0;
    ActiveFetchState fetch_state{};
    DrawSig sigs[32]{};
    uint32_t sig_count = 0;

    PacketSig state_sigs[96]{};
    uint32_t state_sig_count = 0;
    PacketSig ctx_sigs[32]{};
    uint32_t ctx_sig_count = 0;

    while (off < ib_dwords && pkts < 20000) {
        const uint32_t hdr  = IbRead(ib_phys, off);
        const uint32_t type = hdr >> 30;
        ++pkts;
        if (type == 2) { ++off; continue; }
        if (type != 0 && type != 3) {
            REXLOG_ERROR("{}  [{:05X}] UNKNOWN_TYPE{} {:08X}", pfx, off, type, hdr);
            break;
        }
        const uint32_t cnt = ((hdr >> 16) & 0x3FFFu) + 1;
        uint32_t w[192] = {};
        for (uint32_t i = 0; i < 192u && i < cnt; ++i)
            w[i] = IbRead(ib_phys, off + 1 + i);

        if (type == 0) {
            const uint32_t base = hdr & 0x7FFFu;
            if (IsInterestingRegBase(base))
                AddPacketSig(state_sigs, state_sig_count, 96, base, cnt, w[0], w[1], off);
            if (base >= 0x4800 && base < 0x4890)
                LogFetchConstants(pfx, base, w, cnt, off, &fetch_state);

            if (state_count < 12 || (depth > 0 && state_count < 24)) {
                REXLOG_ERROR("{}  [{:05X}] T0 base={:04X} cnt={:5}  {:08X} {:08X} {:08X} {:08X} ...",
                    pfx, off, base, cnt, w[0], w[1], w[2], w[3]);
            } else {
                ++state_suppressed;
            }
            ++state_count;
            off += 1 + cnt;
            continue;
        }

        const uint8_t op = (hdr >> 8) & 0xFF;
        if (IsDrawOp(op)) {
            ++draw_count;
            AddDrawSig(sigs, sig_count, 32, op, cnt, w[0], w[1], w[2], w[3], off);
            if (draw_count <= 16 || op != 0x36 || sig_count > 1) {
                REXLOG_ERROR(
                    "{}  [{:05X}] DRAW {:02X}({:8s}) cnt={:4}  "
                    "{:08X} {:08X} {:08X} {:08X}  {:08X} {:08X} {:08X} {:08X}",
                    pfx, off, op, Pm4OpShort(op), cnt,
                    w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]);
                LogDrawDecode(pfx, op, cnt, w);
                LogDrawFetchState(pfx, off, op, cnt, fetch_state);
            } else {
                ++draw_suppressed;
            }
        } else if (op == 0x3F && cnt >= 2 && depth < 3) {
            ++ib_count;
            REXLOG_ERROR("{}  [{:05X}] {:02X}({:8s}) cnt={:5}  {:08X} {:08X} {:08X} {:08X}",
                pfx, off, op, Pm4OpShort(op), cnt, w[0], w[1], w[2], w[3]);
            DumpIBToLog(w[0], w[1], ib_num, depth + 1);
        } else {
            const bool important =
                op == 0x2B || op == 0x2F || op == 0x39 ||
                op == 0x3D || op == 0x46 || op == 0x47 || op == 0x64;
            if (op == 0x2F)
                AddPacketSig(ctx_sigs, ctx_sig_count, 32, w[0], w[1], w[2], w[3], off);
            if (op == 0x2F)
                DumpPhysSample("LOADCTXSAMPLE", pfx, w[0] + w[2] * 4u, 8);
            if ((important && state_count < 48) || state_count < 12) {
                REXLOG_ERROR("{}  [{:05X}] {:02X}({:8s}) cnt={:5}  {:08X} {:08X} {:08X} {:08X}",
                    pfx, off, op, Pm4OpShort(op), cnt, w[0], w[1], w[2], w[3]);
            } else {
                ++state_suppressed;
            }
            ++state_count;
        }
        off += 1 + cnt;
    }
    for (uint32_t i = 0; i < sig_count; ++i) {
        const DrawSig& s = sigs[i];
        REXLOG_ERROR("{}  DRAWSUM op={:02X}({:8s}) cnt={} hits={} first={:05X} w0={:08X} w1={:08X} w2={:08X} w3={:08X}",
            pfx, s.op, Pm4OpShort(s.op), s.cnt, s.hits, s.first_off,
            s.w0, s.w1, s.w2, s.w3);
        if (s.op == 0x2D && s.cnt == 4) {
            REXLOG_ERROR("{}    DRAWSUM_RECT x0={} y0={} x1={} y1={}",
                pfx, s.w2 & 0xFFFFu, s.w2 >> 16, s.w3 & 0xFFFFu, s.w3 >> 16);
        } else if (s.op == 0x2D && s.cnt == 17) {
            REXLOG_ERROR("{}    DRAWSUM_STRIP_SETUP w0={:08X} w1={:08X} w1f={:.6f} w2f={:.6f} w3f={:.6f}",
                pfx, s.w0, s.w1, U32AsFloat(s.w1), U32AsFloat(s.w2), U32AsFloat(s.w3));
        } else if (s.op == 0x2D && s.cnt == 2) {
            REXLOG_ERROR("{}    DRAWSUM_TILE_KEY key={:08X} w0={:08X}",
                pfx, s.w1, s.w0);
        } else if (s.op == 0x36) {
            REXLOG_ERROR("{}    DRAWSUM_DRAW2 init={:08X} prim={} sel={} idx={}",
                pfx, s.w0, s.w0 & 0x3Fu, (s.w0 >> 6) & 3u, s.w0 >> 10);
        }
    }
    for (uint32_t i = 0; i < state_sig_count; ++i) {
        const PacketSig& s = state_sigs[i];
        REXLOG_ERROR("{}  STATESUM base={:04X} cnt={} hits={} first={:05X} w0={:08X} w1={:08X}",
            pfx, s.a, s.cnt, s.hits, s.first_off, s.w0, s.w1);
    }
    for (uint32_t i = 0; i < ctx_sig_count; ++i) {
        const PacketSig& s = ctx_sigs[i];
        REXLOG_ERROR("{}  LOADCTX phys={:08X} words={} offs={} hits={} first={:05X}",
            pfx, s.a, s.cnt, s.w0, s.hits, s.first_off);
    }
    if (draw_suppressed || state_suppressed) {
        REXLOG_ERROR("{}  SUPPRESSED draws={} state={} nested_ib={}",
            pfx, draw_suppressed, state_suppressed, ib_count);
    }
    REXLOG_ERROR("{}===IB#{} depth={} {} pkts={}===",
                 pfx, ib_num, depth, (off < ib_dwords) ? "TRUNCATED" : "END", pkts);
}

void DumpRingWindowToLog(uint32_t start_dw, uint32_t pm4_dw, int num,
                         const PPCContext& ctx) {
    REXLOG_ERROR("LIVERB#{} lr={:08X} regs r4={:08X} r6={:08X} r7={:08X} r8={:08X} r9={:08X} r10={:08X} rb=[{:08X},+{}]",
        num, static_cast<uint32_t>(ctx.lr), ctx.r4.u32, ctx.r6.u32,
        ctx.r7.u32, ctx.r8.u32, ctx.r9.u32, ctx.r10.u32, start_dw, pm4_dw);

    const uint32_t show = pm4_dw < 16u ? pm4_dw : 16u;
    for (uint32_t i = 0; i < show; ++i) {
        REXLOG_ERROR("  rb[{:05X}] {:08X}", start_dw + i,
                     daytona_render::ReadRingBufferDword(start_dw + i));
    }
    if (show < pm4_dw)
        REXLOG_ERROR("  ... ({} more ring dwords)", pm4_dw - show);

    uint32_t off = 0;
    while (off < pm4_dw) {
        const uint32_t hdr = daytona_render::ReadRingBufferDword(start_dw + off);
        const uint32_t type = hdr >> 30;
        if (type == 2) { ++off; continue; }
        if (type != 0 && type != 3) {
            REXLOG_ERROR("  rbpkt[{:05X}] UNKNOWN_TYPE{} {:08X}",
                         start_dw + off, type, hdr);
            break;
        }

        const uint32_t cnt = ((hdr >> 16) & 0x3FFFu) + 1;
        uint32_t w[8] = {};
        for (uint32_t i = 0; i < 8u && i < cnt && off + 1 + i < pm4_dw; ++i)
            w[i] = daytona_render::ReadRingBufferDword(start_dw + off + 1 + i);

        if (type == 0) {
            REXLOG_ERROR("  rbpkt[{:05X}] T0 base={:04X} cnt={} {:08X} {:08X} {:08X} {:08X}",
                start_dw + off, hdr & 0x7FFFu, cnt, w[0], w[1], w[2], w[3]);
        } else {
            const uint8_t op = (hdr >> 8) & 0xFF;
            REXLOG_ERROR("  rbpkt[{:05X}] {:02X}({:8s}) cnt={} {:08X} {:08X} {:08X} {:08X}",
                start_dw + off, op, Pm4OpShort(op), cnt, w[0], w[1], w[2], w[3]);
            if (IsDrawOp(op)) {
                LogDrawDecode("  ", op, cnt, w);
            } else if (op == 0x3F && cnt >= 2) {
                DumpIBToLog(w[0], w[1], num);
            }
        }

        if (cnt == 0) break;
        off += 1 + cnt;
    }
}

}  // namespace (IB helpers)

#define DT_TRACE_HOOK(name, id)                                              \
    REX_HOOK_RAW(name) {                                                     \
        uint32_t n = daytona_debug::RecordHook(id, ctx);                     \
        if (n <= 4)                                                          \
            REXLOG_ERROR("Daytona " #name " #{} lr={:08X} r3={:08X}",       \
                         n, static_cast<uint32_t>(ctx.lr), ctx.r3.u32);     \
        else if (ShouldLog(n)) LogGuestCall(#name, n, ctx);                  \
        __imp__##name(ctx, base);                                            \
    }

#define DT_TRACE_HOOK_AFTER(name, id)                   \
    REX_HOOK_RAW(name) {                                \
        __imp__##name(ctx, base);                       \
        uint32_t n = daytona_debug::RecordHook(id, ctx); \
        if (ShouldLog(n)) {                             \
            LogGuestCall(#name, n, ctx);                \
        }                                               \
    }

#define DT_TRACE_HOOK_AROUND(name, enter_id, exit_id)       \
    REX_HOOK_RAW(name) {                                    \
        uint32_t n = daytona_debug::RecordHook(enter_id, ctx); \
        if (ShouldLog(n)) {                                 \
            LogGuestCall(#name " enter", n, ctx);           \
        }                                                   \
        __imp__##name(ctx, base);                           \
        n = daytona_debug::RecordHook(exit_id, ctx);         \
        if (ShouldLog(n)) {                                 \
            LogGuestCall(#name " exit", n, ctx);            \
        }                                                   \
    }

#define DT_TRACE_HOOK_AFTER_EXCEPT(name, id)                \
    REX_HOOK_RAW(name) {                                    \
        uint32_t n = daytona_debug::RecordHook(id, ctx);    \
        if (ShouldLog(n)) {                                 \
            LogGuestCall(#name " enter", n, ctx);           \
        }                                                   \
        __imp__##name(ctx, base);                           \
    }

// ── Graphics / GPU trace hooks ────────────────────────────────────────────────
DT_TRACE_HOOK(sub_822346A0, daytona_debug::HookId::kGpuInterrupt)  // GpuInterruptHandler
REX_HOOK_RAW(sub_82234FA8) {
    // GpuDescriptorSubmit is the real GPU ring buffer submission (writes to CP_RB_WPTR).
    // Count it as the "Cmd" stat in the frame inspector.
    // Read write_ptr BEFORE the call so we can bracket the PM4 range written.
    auto* cp_ = GetCP();
    const uint32_t wptr_before = cp_ ? cp_->write_pointer_index() : 0;
    daytona_render::RecordCommandSubmit(ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kGpuDescriptorSubmit, ctx);

    // Descriptor submit is very hot. Log early calls and then sparse samples.
    const bool log = DaytonaTraceVerbose() && (
        (n <= 80) ||
        (n <= 2000 && (n % 100) == 0) ||
        ((n % 10000) == 0));

    if (log) {
        const uint32_t r3 = ctx.r3.u32;
        const uint32_t r4 = ctx.r4.u32;
        const uint32_t r5 = ctx.r5.u32;
        const uint32_t r6 = ctx.r6.u32;
        const uint32_t r7 = ctx.r7.u32;
        const uint32_t r8 = ctx.r8.u32;
        const uint32_t r9 = ctx.r9.u32;
        const uint32_t r10 = ctx.r10.u32;

        auto safe_load = [base](uint32_t a) -> uint32_t {
            if (a < 0x70000000u) return 0xDEAD0001u;
            return REX_LOAD_U32(a);
        };

        REXLOG_ERROR(
            "Daytona DESC sub_82234FA8 enter #{} lr={:08X} r3={:08X} r4={:08X} r5={:08X} r6={:08X} r7={:08X} r8={:08X} r9={:08X} r10={:08X}",
            n,
            static_cast<uint32_t>(ctx.lr),
            r3, r4, r5, r6, r7, r8, r9, r10);

        const uint32_t obj21812 = safe_load(r3 + 21812);
        const uint32_t obj23640 = safe_load(r3 + 23640);
        const uint32_t vt21812 = obj21812 >= 0x70000000u ? safe_load(obj21812 + 0) : 0;
        const uint32_t cb24 = vt21812 >= 0x70000000u ? safe_load(vt21812 + 24) : 0;
        const uint32_t cb28 = vt21812 >= 0x70000000u ? safe_load(vt21812 + 28) : 0;
        REXLOG_INFO(
            "Daytona DESC obj #{} obj21812={:08X} vt={:08X} cb24={:08X} cb28={:08X} obj23640={:08X}",
            n, obj21812, vt21812, cb24, cb28, obj23640);

        if (r4 >= 0x70000000u && r5 != 0) {
            for (uint32_t i = 0; i < r5 && i < 4; ++i) {
                const uint32_t d0 = safe_load(r4 + i * 8 + 0);
                const uint32_t d1 = safe_load(r4 + i * 8 + 4);
                const uint32_t len24 = d0 & 0x00FFFFFFu;
                const uint32_t conv =
                    (d1 < 0x20000000u) ? (d1 - 0x40000000u) : (d1 - 0x41000000u);

                REXLOG_INFO(
                    "Daytona DESC pair #{} i={} d0={:08X} len24={:08X} d1={:08X} conv={:08X}",
                    n, i, d0, len24, d1, conv);

                if (d1 >= 0x70000000u) {
                    REXLOG_INFO(
                        "Daytona DESC d1mem #{} i={} base={:08X}: +00={:08X} +04={:08X} +08={:08X} +0C={:08X}",
                        n, i, d1,
                        safe_load(d1 + 0), safe_load(d1 + 4), safe_load(d1 + 8), safe_load(d1 + 12));
                }
                if (conv >= 0x70000000u) {
                    REXLOG_INFO(
                        "Daytona DESC convmem #{} i={} base={:08X}: +00={:08X} +04={:08X} +08={:08X} +0C={:08X} +10={:08X} +14={:08X} +18={:08X} +1C={:08X} +20={:08X} +24={:08X} +28={:08X} +2C={:08X} +30={:08X} +34={:08X} +38={:08X} +3C={:08X}",
                        n, i, conv,
                        safe_load(conv + 0x00), safe_load(conv + 0x04), safe_load(conv + 0x08), safe_load(conv + 0x0C),
                        safe_load(conv + 0x10), safe_load(conv + 0x14), safe_load(conv + 0x18), safe_load(conv + 0x1C),
                        safe_load(conv + 0x20), safe_load(conv + 0x24), safe_load(conv + 0x28), safe_load(conv + 0x2C),
                        safe_load(conv + 0x30), safe_load(conv + 0x34), safe_load(conv + 0x38), safe_load(conv + 0x3C));
                }
            }
        }

        // Dump likely pointed-to guest structs. r3/r4 are the suspicious ones from the current log.
        for (uint32_t base_addr : {r3, r4, r8, r9, r10}) {
            if (base_addr >= 0x70000000u) {
                REXLOG_INFO(
                    "Daytona DESC mem #{} base={:08X}: +00={:08X} +04={:08X} +08={:08X} +0C={:08X} +10={:08X} +14={:08X} +18={:08X} +1C={:08X}",
                    n,
                    base_addr,
                    safe_load(base_addr + 0x00),
                    safe_load(base_addr + 0x04),
                    safe_load(base_addr + 0x08),
                    safe_load(base_addr + 0x0C),
                    safe_load(base_addr + 0x10),
                    safe_load(base_addr + 0x14),
                    safe_load(base_addr + 0x18),
                    safe_load(base_addr + 0x1C));
            }
        }
    }

    __imp__sub_82234FA8(ctx, base);

    // Capture the new write_ptr and store the [before, after) bracket in the event.
    const uint32_t wptr_after = cp_ ? cp_->write_pointer_index() : 0;
    daytona_render::UpdateLastCmdWritePtrs(wptr_before, wptr_after);

    if (log) {
        REXLOG_ERROR(
            "Daytona DESC sub_82234FA8 exit #{} result={:08X} wptr=[{:08X},{:08X})",
            n, ctx.r3.u32, wptr_before, wptr_after);
    }

    // The in-race render worker uses lr=8223CBC4 and writes tiny ring windows
    // that usually contain an unknown 0x62 packet followed by IT_INDIRECT_BUFFER.
    // Gate this to late submits / r6=3FFF so a 30s run keeps logging useful race
    // frames instead of exhausting the cap during the first second.
    {
        static int s_live_rb_dumps = 0;
        static uint32_t s_seen_live_phys[64]{};
        static uint32_t s_seen_live_dwords[64]{};
        static uint32_t s_seen_live_count = 0;
        if (DaytonaTraceVerbose() && s_live_rb_dumps < 48 && wptr_after != wptr_before) {
            const bool live_worker =
                static_cast<uint32_t>(ctx.lr) == 0x8223CBC4u &&
                ctx.r6.u32 == 0x3FFFu;
            const bool live_sample =
                live_worker &&
                (s_live_rb_dumps < 24 || (n % 10000u) == 0);
            if (live_worker) {
                const uint32_t rb_sz = daytona_render::GetRingBufferSize();
                const uint32_t pm4_dw = (rb_sz > 0)
                    ? ((wptr_after >= wptr_before)
                        ? (wptr_after - wptr_before)
                        : (rb_sz / 4u - wptr_before + wptr_after))
                    : 0u;
                if (live_sample && pm4_dw > 0) {
                    uint32_t root_ib_phys = 0;
                    uint32_t root_ib_dwords = 0;
                    uint32_t rb_off = 0;
                    while (rb_off < pm4_dw) {
                        const uint32_t hdr = daytona_render::ReadRingBufferDword(wptr_before + rb_off);
                        const uint32_t type = hdr >> 30;
                        if (type == 2) { ++rb_off; continue; }
                        if (type != 0 && type != 3) break;
                        const uint32_t cnt = ((hdr >> 16) & 0x3FFFu) + 1;
                        if (type == 3 && ((hdr >> 8) & 0xFFu) == 0x3F && cnt >= 2) {
                            root_ib_phys = daytona_render::ReadRingBufferDword(wptr_before + rb_off + 1);
                            root_ib_dwords = daytona_render::ReadRingBufferDword(wptr_before + rb_off + 2);
                            break;
                        }
                        rb_off += 1 + cnt;
                    }

                    bool seen_root = false;
                    if (root_ib_phys != 0 && root_ib_dwords != 0) {
                        for (uint32_t i = 0; i < s_seen_live_count; ++i) {
                            if (s_seen_live_phys[i] == root_ib_phys &&
                                s_seen_live_dwords[i] == root_ib_dwords) {
                                seen_root = true;
                                break;
                            }
                        }
                        if (!seen_root && s_seen_live_count < 64) {
                            s_seen_live_phys[s_seen_live_count] = root_ib_phys;
                            s_seen_live_dwords[s_seen_live_count] = root_ib_dwords;
                            ++s_seen_live_count;
                        }
                    }

                    if (!seen_root) {
                        ++s_live_rb_dumps;
                        DumpRingWindowToLog(wptr_before, pm4_dw, s_live_rb_dumps, ctx);
                    }
                }
            }
        }
    }
}
DT_TRACE_HOOK(sub_824140B8, daytona_debug::HookId::kGpuCacheFlush)  // GpuCacheFlush
DT_TRACE_HOOK(sub_82130D68, daytona_debug::HookId::kBootPresentA)   // BootPresentA
REX_HOOK_RAW(sub_82131088) {
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kBootPresentB, ctx);
    if (ShouldLog(n)) {
        REXLOG_INFO(
            "Daytona boot present B enter #{} lr={:08X} r3={:08X} r4={:08X} r5={:08X}",
            n,
            static_cast<uint32_t>(ctx.lr),
            ctx.r3.u32,
            ctx.r4.u32,
            ctx.r5.u32);
    }

    __imp__sub_82131088(ctx, base);

    if (ShouldLog(n)) {
        REXLOG_INFO(
            "Daytona boot present B exit #{} result={:08X} lr={:08X}",
            n,
            ctx.r3.u32,
            static_cast<uint32_t>(ctx.lr));
    }
}
DT_TRACE_HOOK(sub_8223CF68, daytona_debug::HookId::kRenderPresentWorker)  // RenderPresentWorker
DT_TRACE_HOOK(sub_82233C78, daytona_debug::HookId::kFrameFenceReset)      // FrameFenceReset
REX_HOOK_RAW(sub_822337C0) {
    __imp__sub_822337C0(ctx, base);
}
DT_TRACE_HOOK(sub_82235B20, daytona_debug::HookId::kCommandFlush)  // CommandFlush
DT_TRACE_HOOK(sub_82235D58, daytona_debug::HookId::kCommandAlloc)  // CommandAlloc
REX_HOOK_RAW(sub_82233C80) {
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kVdSwapCaller, ctx);

    if (ShouldLog(n)) {
        REXLOG_INFO(
            "Daytona VdSwap caller enter #{} lr={:08X} r3={:08X} r4={:08X} r5={:08X} r6={:08X} r7={:08X} r8={:08X}",
            n,
            static_cast<uint32_t>(ctx.lr),
            ctx.r3.u32,
            ctx.r4.u32,
            ctx.r5.u32,
            ctx.r6.u32,
            ctx.r7.u32,
            ctx.r8.u32);
    }

    __imp__sub_82233C80(ctx, base);

    if (ShouldLog(n)) {
        REXLOG_INFO(
            "Daytona VdSwap caller exit #{} result={:08X}",
            n,
            ctx.r3.u32);
    }
}
DT_TRACE_HOOK_AFTER(sub_822297D8, daytona_debug::HookId::kCreateDeviceLike)   // D3DDevice-like create
REX_HOOK_RAW(sub_8221E940) {  // DeviceSetStateA
    const uint32_t r3 = ctx.r3.u32, r4 = ctx.r4.u32;
    daytona_render::RecordStateChange('A', r3, r4);
    __imp__sub_8221E940(ctx, base);
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kSetRenderStateA, ctx);
    if (ShouldLog(n)) LogGuestCall("DeviceSetStateA", n, ctx);
}
REX_HOOK_RAW(sub_8221E950) {  // DeviceSetStateB
    const uint32_t r3 = ctx.r3.u32, r4 = ctx.r4.u32;
    daytona_render::RecordStateChange('B', r3, r4);
    __imp__sub_8221E950(ctx, base);
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kSetRenderStateB, ctx);
    if (ShouldLog(n)) LogGuestCall("DeviceSetStateB", n, ctx);
}
REX_HOOK_RAW(sub_8221E658) {  // DeviceSetStateC
    const uint32_t r3 = ctx.r3.u32, r4 = ctx.r4.u32;
    daytona_render::RecordStateChange('C', r3, r4);
    __imp__sub_8221E658(ctx, base);
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kSetRenderStateC, ctx);
    if (ShouldLog(n)) LogGuestCall("DeviceSetStateC", n, ctx);
}
DT_TRACE_HOOK_AFTER(sub_822337E8, daytona_debug::HookId::kBackbufferSetup)    // BackbufferSetup
DT_TRACE_HOOK_AFTER(sub_8222C4C8, daytona_debug::HookId::kSceneSetup)         // SceneSetup
REX_HOOK_RAW(sub_8222EBC0) {  // RenderTargetSetup
    const uint32_t r3 = ctx.r3.u32, r4 = ctx.r4.u32, r5 = ctx.r5.u32, r6 = ctx.r6.u32;
    __imp__sub_8222EBC0(ctx, base);
    daytona_render::RecordRenderTarget(r3, r4, r5, r6);
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kRenderTargetSetup, ctx);
    if (ShouldLog(n)) LogGuestCall("RenderTargetSetup", n, ctx);
}
DT_TRACE_HOOK_AFTER(sub_8212F028, daytona_debug::HookId::kBeginSceneLike)     // BeginScene
REX_HOOK_RAW(sub_82223F28) {  // ClearLike
    const uint32_t r3 = ctx.r3.u32, r4 = ctx.r4.u32, r5 = ctx.r5.u32, r6 = ctx.r6.u32;
    __imp__sub_82223F28(ctx, base);
    daytona_render::RecordClear(r3, r4, r5, r6);
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kClearLike, ctx);
    if (ShouldLog(n)) LogGuestCall("ClearLike", n, ctx);
}
DT_TRACE_HOOK(sub_824DC8A8, daytona_debug::HookId::kMemcpyLikeDc8)            // Memcpy-like (DC8)
DT_TRACE_HOOK(sub_824EB2B8, daytona_debug::HookId::kMemcpyLikeEb2)            // Memcpy-like (EB2)
// ── Boot / Init sequence hooks ────────────────────────────────────────────────
DT_TRACE_HOOK_AROUND(sub_82242A30,
                     daytona_debug::HookId::kPostStateGateEnter,
                     daytona_debug::HookId::kPostStateGateExit)   // PostStateGate
DT_TRACE_HOOK_AROUND(sub_8212C7A8,
                     daytona_debug::HookId::kBootLogoSetupEnter,
                     daytona_debug::HookId::kBootLogoSetupExit)   // BootLogoSetup
DT_TRACE_HOOK_AROUND(sub_82130870,
                     daytona_debug::HookId::kBootPollEnter,
                     daytona_debug::HookId::kBootPollExit)        // BootPoll
DT_TRACE_HOOK_AFTER(sub_8212F000, daytona_debug::HookId::kEndSceneLike)       // EndScene
DT_TRACE_HOOK_AFTER(sub_8222AF38, daytona_debug::HookId::kBootCreateTarget)   // BootCreateTarget
DT_TRACE_HOOK_AFTER(sub_8222AE18, daytona_debug::HookId::kBootCreateTarget2)  // BootCreateTarget2
DT_TRACE_HOOK_AFTER(sub_822203B8, daytona_debug::HookId::kBootBindTarget)     // BootBindTarget
DT_TRACE_HOOK_AFTER(sub_822200E0, daytona_debug::HookId::kBootSetTarget)      // BootSetTarget
DT_TRACE_HOOK_AFTER(sub_8212E068, daytona_debug::HookId::kBootShaderPath)     // BootShaderPath
DT_TRACE_HOOK_AFTER(sub_82220730, daytona_debug::HookId::kBootAllocA)         // BootAllocA
DT_TRACE_HOOK_AFTER(sub_82221780, daytona_debug::HookId::kBootClearAllocA)    // BootClearAllocA
DT_TRACE_HOOK_AFTER(sub_822207F8, daytona_debug::HookId::kBootUnlockAllocA)   // BootUnlockAllocA
DT_TRACE_HOOK_AFTER(sub_82223EA8, daytona_debug::HookId::kBootMakeDraw)       // BootMakeDraw
DT_TRACE_HOOK_AFTER(sub_82220808, daytona_debug::HookId::kBootAllocB)         // BootAllocB
DT_TRACE_HOOK_AFTER(sub_822217D0, daytona_debug::HookId::kBootClearAllocB)    // BootClearAllocB
DT_TRACE_HOOK_AFTER(sub_822208B8, daytona_debug::HookId::kBootUnlockAllocB)   // BootUnlockAllocB
DT_TRACE_HOOK_AFTER(sub_8212C408, daytona_debug::HookId::kBootObjectSetup)    // BootObjectSetup
DT_TRACE_HOOK_AFTER(sub_82129DE0, daytona_debug::HookId::kBootUiSetupA)       // BootUiSetupA
DT_TRACE_HOOK_AFTER(sub_82126280, daytona_debug::HookId::kBootUiSetupB)       // BootUiSetupB
DT_TRACE_HOOK_AFTER(sub_8212A850, daytona_debug::HookId::kBootUiSetupC)       // BootUiSetupC
DT_TRACE_HOOK_AFTER(sub_82128638, daytona_debug::HookId::kBootUiSetupD)       // BootUiSetupD
DT_TRACE_HOOK_AROUND(sub_8212A928,
                     daytona_debug::HookId::kBootUiSetupF,
                     daytona_debug::HookId::kBootUiSetupFExit)    // BootUiSetupF
DT_TRACE_HOOK_AROUND(sub_8212C7B8,
                     daytona_debug::HookId::kBootUiSetupG,
                     daytona_debug::HookId::kBootUiSetupGExit)    // BootUiSetupG

// ── UI texture hooks ──────────────────────────────────────────────────────────
DT_TRACE_HOOK_AFTER(sub_82241750, daytona_debug::HookId::kUiTextureSizeA)         // UiTextureSizeA
DT_TRACE_HOOK_AFTER(sub_82241740, daytona_debug::HookId::kUiTextureSizeB)         // UiTextureSizeB

REX_HOOK_RAW(sub_82225658) {  // TexFinalizeA
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kUiTextureFinalizeA, ctx);
    const bool log = DaytonaTraceVerbose() && (n <= 64 || (n % 512u) == 0);
    if (log) LogTexCall(base, "TexFinalizeA", n, ctx);
    __imp__sub_82225658(ctx, base);
    if (log) LogTexCall(base, "TexFinalizeA", n, ctx, true);
}

REX_HOOK_RAW(sub_82225470) {  // TexFinalizeB
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kUiTextureFinalizeB, ctx);
    const bool log = DaytonaTraceVerbose() && (n <= 64 || (n % 512u) == 0);
    if (log) LogTexCall(base, "TexFinalizeB", n, ctx);
    __imp__sub_82225470(ctx, base);
    if (log) LogTexCall(base, "TexFinalizeB", n, ctx, true);
}

REX_HOOK_RAW(sub_824DC270) {  // TexCacheFind
    static uint32_t s_calls = 0;
    ++s_calls;
    const bool log = DaytonaTraceVerbose() && (s_calls <= 96 || (s_calls % 1024u) == 0);
    if (log) LogTexCall(base, "TexCacheFind", s_calls, ctx);
    __imp__sub_824DC270(ctx, base);
    if (log) LogTexCall(base, "TexCacheFind", s_calls, ctx, true);
}

REX_HOOK_RAW(sub_824DC420) {  // TexSlotLookup
    static uint32_t s_calls = 0;
    ++s_calls;
    const bool log = DaytonaTraceVerbose() && (s_calls <= 96 || (s_calls % 1024u) == 0);
    if (log) LogTexCall(base, "TexSlotLookup", s_calls, ctx);
    __imp__sub_824DC420(ctx, base);
    if (log) LogTexCall(base, "TexSlotLookup", s_calls, ctx, true);
}

REX_HOOK_RAW(sub_824E4990) {  // TexHash
    static uint32_t s_calls = 0;
    ++s_calls;
    const bool log = DaytonaTraceVerbose() && (s_calls <= 96 || (s_calls % 1024u) == 0);
    if (log) LogTexCall(base, "TexHash", s_calls, ctx);
    __imp__sub_824E4990(ctx, base);
    if (log) LogTexCall(base, "TexHash", s_calls, ctx, true);
}

REX_HOOK_RAW(sub_8212F990) {
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kMainShaderSetup, ctx);
    if (ShouldLog(n)) {
        LogGuestCall("sub_8212F990 enter", n, ctx);
    }
    // Do not clear the built-in shader source pointers here. sub_8212F990
    // still walks those strings before the compile calls, so zeroing them can
    // stall inside the initializer. Let the original function run so the next
    // failing child call can be observed.
    __imp__sub_8212F990(ctx, base);
    n = daytona_debug::RecordHook(daytona_debug::HookId::kMainShaderSetup, ctx);
    if (ShouldLog(n)) {
        LogGuestCall("sub_8212F990 exit", n, ctx);
    }
}

REX_HOOK_RAW(sub_8212AD80) {
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kBootUiSetupE, ctx);
    if (ShouldLog(n)) {
        LogGuestCall("sub_8212AD80 bypass enter", n, ctx);
    }
    // This boot UI helper compiles built-in D3DX HLSL shaders. The current
    // guest D3DX path returns 0x8000FFFF and the title asserts before first
    // present. Leave the zeroed shader fields in place so boot can continue.
    n = daytona_debug::RecordHook(daytona_debug::HookId::kBootUiSetupEExit, ctx);
    if (ShouldLog(n)) {
        LogGuestCall("sub_8212AD80 bypass exit", n, ctx);
    }
}

// sub_822429C8 (roadmap: "TexLoad") — r3 is actually a shader source string,
// not a texture file path. The roadmap symbol was misidentified; this function
// takes HLSL-like DSL source in r3 and byte length in r4.
REX_HOOK_RAW(sub_822429C8) {
    const uint32_t src_addr = ctx.r3.u32;
    const uint32_t src_len  = ctx.r4.u32;
    char src_preview[160]{};
    ReadGuestString(base, src_addr, src_preview, sizeof(src_preview));
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kUiTextureLoad, ctx);
    if (ShouldLog(n)) {
        REXLOG_INFO("Daytona hook sub_822429C8 (ShaderLoad?) enter #{} src={:08X} len={} r5={:08X} r7={:08X}",
                    n, src_addr, src_len, ctx.r5.u32, ctx.r7.u32);
        if (src_preview[0] >= 0x20)
            REXLOG_ERROR("  sub_822429C8 source: '{:.120s}'", src_preview);
    }
    __imp__sub_822429C8(ctx, base);
    // Source string in r3 — store in shader registry, not texture registry.
    if (src_preview[0] >= 0x20)
        daytona_render::RecordShaderSource(src_addr, src_preview, src_len);
    if (ShouldLog(n))
        REXLOG_INFO("Daytona hook sub_822429C8 exit #{} result={:08X}", n, ctx.r3.u32);
}

REX_HOOK_RAW(sub_82176F48) {
    REXLOG_INFO("Daytona parent sub_82176F48 enter lr={:08X} r3={:08X} r4={:08X} r5={:08X}",
                static_cast<uint32_t>(ctx.lr),
                ctx.r3.u32,
                ctx.r4.u32,
                ctx.r5.u32);

    __imp__sub_82176F48(ctx, base);

    REXLOG_INFO("Daytona parent sub_82176F48 exit result={:08X} lr={:08X}",
                ctx.r3.u32,
                static_cast<uint32_t>(ctx.lr));
}


#define DT_LOG_AROUND_ONLY(name) \
    REX_HOOK_RAW(name) { \
        REXLOG_INFO("Daytona around " #name " enter lr={:08X} r3={:08X} r4={:08X} r5={:08X}", static_cast<uint32_t>(ctx.lr), ctx.r3.u32, ctx.r4.u32, ctx.r5.u32); \
        __imp__##name(ctx, base); \
        REXLOG_INFO("Daytona around " #name " exit result={:08X} lr={:08X}", ctx.r3.u32, static_cast<uint32_t>(ctx.lr)); \
    }

// ── Boot sub-path probes ──────────────────────────────────────────────────────
DT_LOG_AROUND_ONLY(sub_821303D8)   // BootSubA
DT_LOG_AROUND_ONLY(sub_8212DC68)   // BootSubB
DT_LOG_AROUND_ONLY(sub_82130FD0)   // BootSubC
DT_LOG_AROUND_ONLY(sub_82131418)   // BootSubD
DT_LOG_AROUND_ONLY(sub_821F1888)   // BootSubE


#define DT_LOG_SHADER_CHILD(name) \
    REX_HOOK_RAW(name) { \
        REXLOG_INFO("Daytona shader child " #name " enter lr={:08X} r3={:08X} r4={:08X} r5={:08X} r6={:08X} r7={:08X} r8={:08X} r9={:08X} r10={:08X}", static_cast<uint32_t>(ctx.lr), ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, ctx.r8.u32, ctx.r9.u32, ctx.r10.u32); \
        __imp__##name(ctx, base); \
        REXLOG_INFO("Daytona shader child " #name " exit result={:08X} lr={:08X}", ctx.r3.u32, static_cast<uint32_t>(ctx.lr)); \
    }
#define DT_LOG_65E00_CHILD(name) \
    REX_HOOK_RAW(name) { \
        REXLOG_INFO("Daytona 65E00 child " #name " enter lr={:08X} r3={:08X} r4={:08X} r5={:08X} r6={:08X} r7={:08X} r8={:08X} r9={:08X} r10={:08X}", static_cast<uint32_t>(ctx.lr), ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, ctx.r8.u32, ctx.r9.u32, ctx.r10.u32); \
        __imp__##name(ctx, base); \
        REXLOG_INFO("Daytona 65E00 child " #name " exit result={:08X} lr={:08X}", ctx.r3.u32, static_cast<uint32_t>(ctx.lr)); \
    }
static void DumpGuestBacktrace(uint8_t* base, const PPCContext& ctx, const char* reason) {
    REXLOG_ERROR("Daytona guest backtrace begin: {} current_lr={:08X} current_sp={:08X}",
                 reason,
                 static_cast<uint32_t>(ctx.lr),
                 ctx.r1.u32);

    uint32_t sp = ctx.r1.u32;
    for (uint32_t depth = 0; depth < 32; ++depth) {
        if (sp == 0 || (sp & 3) != 0) {
            REXLOG_ERROR("  bt#{:02} stop invalid sp={:08X}", depth, sp);
            break;
        }

        uint32_t caller_sp = REX_LOAD_U32(sp);
        if (caller_sp == 0 || caller_sp <= sp || (caller_sp & 3) != 0) {
            REXLOG_ERROR("  bt#{:02} stop sp={:08X} caller_sp={:08X}", depth, sp, caller_sp);
            break;
        }

        uint32_t saved_lr = REX_LOAD_U32(caller_sp - 8);
        REXLOG_ERROR("  bt#{:02} sp={:08X} caller_sp={:08X} saved_lr={:08X}",
                     depth,
                     sp,
                     caller_sp,
                     saved_lr);

        sp = caller_sp;
    }

    REXLOG_ERROR("Daytona guest backtrace end");
}


REX_HOOK_RAW(sub_82266808) {
    REXLOG_ERROR("Daytona failure sink sub_82266808 enter lr={:08X} r3={:08X} r4={:08X} r5={:08X} r6={:08X} r7={:08X} r8={:08X} r9={:08X} r10={:08X}",
                 static_cast<uint32_t>(ctx.lr),
                 ctx.r3.u32,
                 ctx.r4.u32,
                 ctx.r5.u32,
                 ctx.r6.u32,
                 ctx.r7.u32,
                 ctx.r8.u32,
                 ctx.r9.u32,
                 ctx.r10.u32);

    if (ctx.r3.u32 & 0x80000000u) {
        DumpGuestBacktrace(base, ctx, "negative HRESULT entering sub_82266808");
    }

    __imp__sub_82266808(ctx, base);

    REXLOG_ERROR("Daytona failure sink sub_82266808 exit result={:08X} lr={:08X}",
                 ctx.r3.u32,
                 static_cast<uint32_t>(ctx.lr));
}
#define DT_LOG_F9288_CHILD(name) \
    REX_HOOK_RAW(name) { \
        REXLOG_ERROR("Daytona F9288 child " #name " enter lr={:08X} r3={:08X} r4={:08X} r5={:08X} r6={:08X} r7={:08X} r8={:08X} r9={:08X} r10={:08X}", static_cast<uint32_t>(ctx.lr), ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, ctx.r8.u32, ctx.r9.u32, ctx.r10.u32); \
        __imp__##name(ctx, base); \
        REXLOG_ERROR("Daytona F9288 child " #name " exit result={:08X} lr={:08X}", ctx.r3.u32, static_cast<uint32_t>(ctx.lr)); \
    }
REX_HOOK_RAW(sub_822F8D48) {
    const uint32_t obj = ctx.r3.u32;
    const uint32_t obj0 = obj ? REX_LOAD_U32(obj + 0) : 0;
    const uint32_t state = obj ? REX_LOAD_U32(obj + 4) : 0;

    if (state) {
        const uint32_t flags = REX_LOAD_U32(state + 40);
        if ((flags & 0x10000000u) == 0) {
            REX_STORE_U32(state + 40, flags | 0x10000000u);
        }

        const uint32_t expected = REX_LOAD_U32(state + 52);
        if (obj0 == expected) {
            REX_STORE_U32(state + 52, expected ^ 1u);
        }
    }

    __imp__sub_822F8D48(ctx, base);
}

// ---- Daytona auto-culprit tracer ----



// depth=1 parent=sub_822F8D48 file=daytona/generated/daytona_recomp.15.cpp
// depth=2 parent=sub_822F88C8 file=daytona/generated/daytona_recomp.15.cpp
// depth=3 parent=sub_822F8910 file=daytona/generated/daytona_recomp.15.cpp
// depth=4 parent=sub_822F894C file=daytona/generated/daytona_recomp.15.cpp
// depth=5 parent=sub_822F8790 file=daytona/generated/daytona_recomp.3.cpp
// depth=5 parent=sub_822F8790 file=daytona/generated/daytona_recomp.15.cpp
// depth=3 parent=sub_822F8910 file=daytona/generated/daytona_recomp.8.cpp
// depth=4 parent=sub_82228790 file=daytona/generated/daytona_recomp.8.cpp
// depth=5 parent=sub_82228108 file=daytona/generated/daytona_recomp.8.cpp
// depth=2 parent=sub_822F88C8 file=daytona/generated/daytona_recomp.14.cpp
// depth=3 parent=sub_822F86C8 file=daytona/generated/daytona_recomp.10.cpp
// depth=1 parent=sub_822F8D48 file=daytona/generated/daytona_recomp.9.cpp

REX_HOOK_RAW(sub_8219BE00) {
    const uint32_t lr = static_cast<uint32_t>(ctx.lr);
    const bool interesting = DaytonaTraceVerbose() && (
        (lr >= 0x822F0000 && lr < 0x82300000) ||
        (lr >= 0x82240000 && lr < 0x82270000) ||
        (lr >= 0x82190000 && lr < 0x821B0000));

    if (interesting) {
        REXLOG_ERROR("Daytona WAITPROBE alloc sub_8219BE00 enter lr={:08X} r3={:08X} r4={:08X} r5={:08X} r6={:08X} r7={:08X} r8={:08X} r9={:08X} r10={:08X}",
                     lr, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, ctx.r8.u32, ctx.r9.u32, ctx.r10.u32);
    }

    __imp__sub_8219BE00(ctx, base);

    if (interesting) {
        REXLOG_ERROR("Daytona WAITPROBE alloc sub_8219BE00 exit result={:08X} lr={:08X}",
                     ctx.r3.u32, static_cast<uint32_t>(ctx.lr));
    }
}

REX_HOOK_RAW(sub_822122C8) {
    __imp__sub_822122C8(ctx, base);
}

REX_HOOK_RAW(sub_824DFC40) {
    const uint32_t lr = static_cast<uint32_t>(ctx.lr);
    const bool f9288_site =
        (lr == 0x822F9304 || lr == 0x822F9484) &&
        ctx.r4.u32 == 0 &&
        ctx.r5.u32 == 0x000009C0;

    if (DaytonaTraceVerbose() && f9288_site) {
        REXLOG_ERROR("Daytona F9288 memzero-return probe enter lr={:08X} dst/r3={:08X} value/r4={:08X} len/r5={:08X} r6={:08X} r7={:08X} r8={:08X} r9={:08X} r10={:08X}",
                     lr, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, ctx.r8.u32, ctx.r9.u32, ctx.r10.u32);
    }

    __imp__sub_824DFC40(ctx, base);

    if (f9288_site) {
        const uint32_t original_result = ctx.r3.u32;
        if (DaytonaTraceVerbose()) {
            REXLOG_ERROR("Daytona F9288 memzero-return probe original result={:08X}; forcing 00000000 so sub_822F9288 reaches build path",
                         original_result);
        }
        ctx.r3.u64 = 0;
    }
}

// ── Shader build-path probes (sub_822F9288 and callees) ──────────────────────
DT_LOG_AROUND_ONLY(sub_822F8EB8)   // ShaderBuildAlloc
DT_LOG_AROUND_ONLY(sub_822F8F98)   // ShaderBuildFinalizer
DT_LOG_AROUND_ONLY(sub_822F9288)   // ShaderBuildRoot

// Trampoline: single `b 0x824C78D0` instruction.
extern "C" REX_FUNC(sub_824C8D80) {
    sub_824C78D0(ctx, base);
}

// Daytona manual missing leaf sub_82342258.
// The recompiler skipped this valid PPC leaf routine, but guest code calls it
// indirectly as a comparator from sub_824DEC68.
extern "C" REX_FUNC(sub_82342258) {
    static uint32_t calls = 0;
    ++calls;

    const uint32_t left_obj = REX_LOAD_U32(ctx.r3.u32 + 0);
    const uint32_t right_obj = REX_LOAD_U32(ctx.r4.u32 + 0);

    auto key_from_word = [](uint32_t word) -> uint32_t {
        // PPC: rlwinm rX,rX,19,16,31
        return ((word << 19) | (word >> 13)) & 0xFFFFu;
    };

    uint32_t left_max = 0;
    for (uint32_t node = REX_LOAD_U32(left_obj + 0x20); node != 0; node = REX_LOAD_U32(node + 4)) {
        const uint32_t payload = REX_LOAD_U32(node + 0);
        const uint32_t word = REX_LOAD_U32(payload + 0);
        const uint32_t key = key_from_word(word);
        if (key > left_max) {
            left_max = key;
        }
    }

    uint32_t right_max = 0;
    for (uint32_t node = REX_LOAD_U32(right_obj + 0x20); node != 0; node = REX_LOAD_U32(node + 4)) {
        const uint32_t payload = REX_LOAD_U32(node + 0);
        const uint32_t word = REX_LOAD_U32(payload + 0);
        const uint32_t key = key_from_word(word);
        if (key > right_max) {
            right_max = key;
        }
    }

    if (left_max == right_max) {
        ctx.r3.s64 = 0;
    } else {
        // Matches the small helper at 0x823422CC: descending order.
        ctx.r3.s64 = (right_max > left_max) ? 1 : -1;
    }

    if (ShouldLog(calls)) {
        REXLOG_ERROR("Daytona manual missing leaf sub_82342258 call #{} lr={:08X} left={:08X} right={:08X} left_max={:08X} right_max={:08X} result={:08X}",
                     calls,
                     static_cast<uint32_t>(ctx.lr),
                     ctx.r3.u32,
                     ctx.r4.u32,
                     left_max,
                     right_max,
                     ctx.r3.u32);
    }
}


// License flag: set to true for full version (trial mode when false)
static constexpr bool kFullVersion = true;

// Override the default license mask stub.
extern "C" DAYTONA_EXPORT REX_FUNC(__imp__XamContentGetLicenseMask) {
    uint32_t mask_addr = ctx.r3.u32;
    if (mask_addr) {
        REX_STORE_U32(mask_addr, kFullVersion ? 0xFFFFFFFF : 0);
    }
    ctx.r3.u64 = 0;
}

// ── Audio domain hooks ────────────────────────────────────────────────────────
// AudioInitDriver: XAudioGetSpeakerConfig + XAudioRegisterRenderDriverClient
REX_HOOK_RAW(AudioInitDriver) {
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kAudioInitDriver, ctx);
    if (ShouldLog(n))
        LogGuestCall("AudioInitDriver enter", n, ctx);
    __imp__sub_8242E438(ctx, base);
    if (ShouldLog(n))
        REXLOG_INFO("Daytona AudioInitDriver exit #{} result={:08X}", n, ctx.r3.u32);
}

// AudioSubmitRenderFrame: XAudioSubmitRenderDriverFrame
REX_HOOK_RAW(AudioSubmitRenderFrame) {
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kAudioSubmitRenderFrame, ctx);
    const bool log = n <= 8 || (n % 5000) == 0;
    if (log)
        LogGuestCall("AudioSubmitRenderFrame", n, ctx);
    __imp__sub_8242E038(ctx, base);
}

// ── Input domain hooks ────────────────────────────────────────────────────────
// InputGetRawState: XamInputGetState + XamInputRawState + XNotifyGetNext
REX_HOOK_RAW(InputGetRawState) {
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kInputGetRawState, ctx);
    const bool log = n <= 8 || (n % 2000) == 0;
    if (log)
        LogGuestCall("InputGetRawState", n, ctx);
    __imp__sub_8245D500(ctx, base);
}

// InputGetCapabilitiesEx: XamInputGetCapabilitiesEx
REX_HOOK_RAW(InputGetCapabilitiesEx) {
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kInputGetCapabilitiesEx, ctx);
    if (ShouldLog(n))
        LogGuestCall("InputGetCapabilitiesEx", n, ctx);
    __imp__sub_8245D348(ctx, base);
}

// ── Video (Vd*) domain hooks ──────────────────────────────────────────────────
// GpuSetDisplayMode: VdGetCurrentDisplayInformation + VdSetDisplayMode + VdSetDisplayModeOverride
REX_HOOK_RAW(GpuSetDisplayMode) {
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kGpuSetDisplayMode, ctx);
    if (ShouldLog(n))
        LogGuestCall("GpuSetDisplayMode enter", n, ctx);
    __imp__sub_82237F18(ctx, base);
    if (ShouldLog(n))
        REXLOG_INFO("Daytona GpuSetDisplayMode exit #{} result={:08X}", n, ctx.r3.u32);
}

// GpuInitEngines: VdInitializeEngines + VdSetGraphicsInterruptCallback
REX_HOOK_RAW(GpuInitEngines) {
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kGpuInitEngines, ctx);
    if (ShouldLog(n))
        LogGuestCall("GpuInitEngines enter", n, ctx);
    __imp__sub_82238028(ctx, base);
    if (ShouldLog(n))
        REXLOG_INFO("Daytona GpuInitEngines exit #{} result={:08X}", n, ctx.r3.u32);
}

// GpuInitRingBuffer: VdInitializeRingBuffer + VdEnableRingBufferRPtrWriteBack
REX_HOOK_RAW(GpuInitRingBuffer) {
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kGpuInitRingBuffer, ctx);
    if (ShouldLog(n))
        LogGuestCall("GpuInitRingBuffer enter", n, ctx);
    __imp__sub_82235EA0(ctx, base);
    if (ShouldLog(n))
        REXLOG_INFO("Daytona GpuInitRingBuffer exit #{} result={:08X}", n, ctx.r3.u32);
}

// Intercept VdInitializeRingBuffer to learn the ring buffer physical address.
// Forwards to the SDK's GraphicsSystem::InitializeRingBuffer directly.
extern "C" DAYTONA_EXPORT REX_FUNC(__imp__VdInitializeRingBuffer) {
    const uint32_t phys     = ctx.r3.u32;  // result of MmGetPhysicalAddress
    const uint32_t size_log2 = ctx.r4.u32;
    REXLOG_ERROR("VdInitializeRingBuffer: phys={:08X} size_log2={} size_bytes={}",
                 phys, size_log2, uint32_t(1) << (size_log2 + 3));
    daytona_render::SetRingBufferPhys(phys, size_log2);
    auto* gs = static_cast<rex::graphics::GraphicsSystem*>(
        rex::system::kernel_state()->emulator()->graphics_system());
    if (gs) gs->InitializeRingBuffer(phys, size_log2);
}

// ── Network domain hooks ──────────────────────────────────────────────────────
// NetXNetStartup: NetDll_XNetStartup + XamGetSystemVersion
REX_HOOK_RAW(NetXNetStartup) {
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kNetXNetStartup, ctx);
    if (ShouldLog(n))
        LogGuestCall("NetXNetStartup enter", n, ctx);
    __imp__sub_8240DA60(ctx, base);
    if (ShouldLog(n))
        REXLOG_INFO("Daytona NetXNetStartup exit #{} result={:08X}", n, ctx.r3.u32);
}

// NetWsaStartup: NetDll_WSAStartup + XamGetSystemVersion
REX_HOOK_RAW(NetWsaStartup) {
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kNetWsaStartup, ctx);
    if (ShouldLog(n))
        LogGuestCall("NetWsaStartup enter", n, ctx);
    __imp__sub_8240DC38(ctx, base);
    if (ShouldLog(n))
        REXLOG_INFO("Daytona NetWsaStartup exit #{} result={:08X}", n, ctx.r3.u32);
}

// ── XUI / Overlay domain hooks ────────────────────────────────────────────────
// UiShowMessageBoxEx: XamShowMessageBoxUIEx — used for in-game error dialogs
REX_HOOK_RAW(UiShowMessageBoxEx) {
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kUiShowMessageBoxEx, ctx);
    if (ShouldLog(n))
        LogGuestCall("UiShowMessageBoxEx enter", n, ctx);
    __imp__sub_82210B78(ctx, base);
    if (ShouldLog(n))
        REXLOG_INFO("Daytona UiShowMessageBoxEx exit #{} result={:08X}", n, ctx.r3.u32);
}

// UiShowSignin: XamShowSigninUI
REX_HOOK_RAW(UiShowSignin) {
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kUiShowSignin, ctx);
    if (ShouldLog(n))
        LogGuestCall("UiShowSignin enter", n, ctx);
    __imp__sub_8220F1D0(ctx, base);
    if (ShouldLog(n))
        REXLOG_INFO("Daytona UiShowSignin exit #{} result={:08X}", n, ctx.r3.u32);
}

// ── User / Profile domain hooks ───────────────────────────────────────────────
// UserGetSigninInfo: XamUserGetSigninInfo
REX_HOOK_RAW(UserGetSigninInfo) {
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kUserGetSigninInfo, ctx);
    if (ShouldLog(n))
        LogGuestCall("UserGetSigninInfo", n, ctx);
    __imp__sub_8220EC88(ctx, base);
}

// VoiceCheckHeadset: XamVoiceHeadsetPresent + XamVoiceIsActiveProcess + XamUserReadProfileSettings
REX_HOOK_RAW(VoiceCheckHeadset) {
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kVoiceCheckHeadset, ctx);
    if (ShouldLog(n))
        LogGuestCall("VoiceCheckHeadset", n, ctx);
    __imp__sub_82461C58(ctx, base);
}

// ── Session domain hooks ──────────────────────────────────────────────────────
// SessionCreate: XamSessionCreateHandle + XamSessionRefObjByHandle + XMsgStartIORequest
REX_HOOK_RAW(SessionCreate) {
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kSessionCreate, ctx);
    if (ShouldLog(n))
        LogGuestCall("SessionCreate enter", n, ctx);
    __imp__sub_82459FD0(ctx, base);
    if (ShouldLog(n))
        REXLOG_INFO("Daytona SessionCreate exit #{} result={:08X}", n, ctx.r3.u32);
}

// ── Platform / system domain hooks ───────────────────────────────────────────
// SysPlatformInfo: DbgPrint + ExGetXConfigSetting + XGetAVPack + XGetLanguage — read platform config
REX_HOOK_RAW(SysPlatformInfo) {
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kSysPlatformInfo, ctx);
    if (ShouldLog(n))
        LogGuestCall("SysPlatformInfo enter", n, ctx);
    __imp__sub_82210CB0(ctx, base);
    if (ShouldLog(n))
        REXLOG_INFO("Daytona SysPlatformInfo exit #{} result={:08X}", n, ctx.r3.u32);
}

// LaunchTitle: XamLoaderLaunchTitle
REX_HOOK_RAW(LaunchTitle) {
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kLaunchTitle, ctx);
    if (ShouldLog(n))
        LogGuestCall("LaunchTitle", n, ctx);

    char launch_path[256];
    ReadGuestString(base, ctx.r3.u32, launch_path, sizeof(launch_path));
    REXLOG_INFO("Daytona requested title launch/exit path='{}'; closing PC port", launch_path);
    std::fflush(nullptr);
    std::_Exit(0);
}

// ── Crypto hook ───────────────────────────────────────────────────────────────
// CryptoSha: XeCryptSha
REX_HOOK_RAW(CryptoSha) {
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kCryptoSha, ctx);
    if (ShouldLog(n))
        LogGuestCall("CryptoSha", n, ctx);
    __imp__sub_824F0110(ctx, base);
}

// sub_8223CCA8: render-job dispatcher called by RenderPresentWorker.
// Reads a function pointer from [r31+16] and calls it via CTR.
REX_HOOK_RAW(sub_8223CCA8) {
    static uint32_t s_calls = 0;
    ++s_calls;
    const bool log = DaytonaTraceVerbose() && (s_calls <= 16 || (s_calls % 10000) == 0);
    if (log) {
        const uint32_t queue_obj = ctx.r3.u32;
        const uint32_t job_root  = queue_obj >= 0x70000000u
                                   ? REX_LOAD_U32(queue_obj + 0)
                                   : 0;
        const uint32_t fn_ptr    = job_root >= 0x70000000u
                                   ? REX_LOAD_U32(job_root + 16)
                                   : 0;
        REXLOG_ERROR("Daytona RenderJobDispatch #{} queue={:08X} root={:08X} fn_ptr={:08X}",
                    s_calls, queue_obj, job_root, fn_ptr);
    }
    __imp__sub_8223CCA8(ctx, base);
}

// ScalerPass (sub_8223AB80): the expected render-job dispatch target.
// Hook to confirm whether the render-job dispatcher ever calls it.
REX_HOOK_RAW(ScalerPass) {
    static uint32_t s_calls = 0;
    ++s_calls;
    if (DaytonaTraceVerbose() && (s_calls <= 4 || (s_calls % 5000) == 0))
        REXLOG_ERROR("Daytona ScalerPass #{} lr={:08X} r3={:08X} r4={:08X}",
                     s_calls, static_cast<uint32_t>(ctx.lr), ctx.r3.u32, ctx.r4.u32);
    __imp__sub_8223AB80(ctx, base);
}

// ── Render pipeline orchestrator hooks ───────────────────────────────────────
// FrameRender (sub_8212B828): called once per frame from FrameEntry.
// Logs on first 5 calls and every 1000 thereafter to trace frame-render cadence.
REX_HOOK_RAW(FrameRender) {
    daytona_render::SetGuestBase(base);
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kFrameRender, ctx);
    const bool log = DaytonaTraceVerbose() && (n <= 5 || (n % 1000) == 0);
    if (log)
        REXLOG_ERROR("Daytona FrameRender enter #{} lr={:08X} r3={:08X}",
                     n, static_cast<uint32_t>(ctx.lr), ctx.r3.u32);
    daytona_render::FrameBegin(ctx.r3.u32);
    __imp__sub_8212B828(ctx, base);
    daytona_render::FrameEnd(ctx.r3.u32);
    if (log)
        REXLOG_ERROR("Daytona FrameRender exit #{} result={:08X}", n, ctx.r3.u32);

    // Always-on lightweight perf probe: every 120 frames log the measured frame
    // rate plus the per-frame GPU work counts. Lets the log reveal what changes
    // when the frame rate tanks across repeated races (draw/pass/tex volume vs.
    // shader-pipeline churn vs. steady counts = GPU memory / throttle).
    {
        static uint32_t s_frames = 0;
        static auto s_last = std::chrono::steady_clock::now();
        if (++s_frames >= 120) {
            const auto now = std::chrono::steady_clock::now();
            const double secs =
                std::chrono::duration<double>(now - s_last).count();
            const double fps = secs > 0.0 ? s_frames / secs : 0.0;
            const auto rs = daytona_render::GetLastFrameStats();
            REXLOG_ERROR("Daytona PERF: {:.1f} fps ({:.2f} ms/frame) | "
                         "draw={} pass={} tex={} shdr={} state={} rt={} clear={} cmd={}",
                         fps, secs * 1000.0 / s_frames,
                         rs.draw_execute_count, rs.render_pass_count,
                         rs.texture_process_count, rs.shader_build_count,
                         rs.state_change_count, rs.rt_bind_count,
                         rs.clear_count, rs.command_submit_count);
            s_frames = 0;
            s_last = now;
        }
    }
}

// SceneRender (sub_8212B3C8): dispatches to Scene3dRender; checks render-object dirty flags.
REX_HOOK_RAW(SceneRender) {
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kSceneRender, ctx);
    const bool log = n <= 5 || (n % 1000) == 0;
    if (log)
        LogGuestCall("SceneRender", n, ctx);
    __imp__sub_8212B3C8(ctx, base);
}

// Scene3dRender (sub_8222CB88): builds the 3D draw list; calls RenderTargetSetup.
REX_HOOK_RAW(Scene3dRender) {
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kScene3dRender, ctx);
    const bool log = n <= 5 || (n % 1000) == 0;
    if (log)
        LogGuestCall("Scene3dRender", n, ctx);
    __imp__sub_8222CB88(ctx, base);
}

// ── Render pipeline detail hooks ─────────────────────────────────────────────

// GpuDrawPrecheck (sub_82258F20): validates vertex/index buffer sizes before draw.
// Called immediately before GpuDrawExecute; records draw-precheck event.
REX_HOOK_RAW(GpuDrawPrecheck) {
    daytona_render::RecordDrawPrecheck(
        ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, ctx.r8.u32);
    __imp__sub_82258F20(ctx, base);
}

// GpuDrawExecute (sub_8225A0B0): executes the prepared draw call.
// Counted in frame stats and logged sparsely.
REX_HOOK_RAW(GpuDrawExecute) {
    daytona_render::RecordDrawExecute(
        ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, ctx.r8.u32);
    uint32_t n = daytona_debug::RecordHook(daytona_debug::HookId::kGpuDrawExecute, ctx);
    const bool log = n <= 5 || (n % 5000) == 0;
    if (log) REXLOG_ERROR("Daytona GpuDrawExecute #{} lr={:08X} r3={:08X} r4={:08X} r5={:08X}",
                          n, static_cast<uint32_t>(ctx.lr), ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
    __imp__sub_8225A0B0(ctx, base);
    if (log) REXLOG_ERROR("Daytona GpuDrawExecute exit #{} result={:08X}", n, ctx.r3.u32);
}

// GpuTextureProcess (sub_8225A8F0): processes a type-14 texture descriptor.
// Moderately hot (once per texture per draw call); count only in debug ring.
REX_HOOK_RAW(GpuTextureProcess) {
    daytona_render::RecordTextureProcess(
        ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, ctx.r8.u32);
    __imp__sub_8225A8F0(ctx, base);
}

// GpuSubmitCommand (sub_8219BF38): writes one D3D command packet to the ring buffer.
// Not on the primary 3D draw path (GpuDescriptorSubmit is); pass through silently.
REX_HOOK_RAW(GpuSubmitCommand) {
    __imp__sub_8219BF38(ctx, base);
}

// RenderPass (sub_82239F30): full 1160-line render pass — shader bind, draw, state flush.
REX_HOOK_RAW(RenderPass) {
    daytona_debug::RecordHook(daytona_debug::HookId::kRenderPassBegin, ctx);
    daytona_render::RecordRenderPassBegin(ctx.r3.u32, ctx.r4.u32);
    __imp__sub_82239F30(ctx, base);
    daytona_debug::RecordHook(daytona_debug::HookId::kRenderPassEnd, ctx);
    daytona_render::RecordRenderPassEnd(ctx.r3.u32);
}

// ShaderBuildOrReuse (sub_823C4350): main shader build/reuse orchestrator.
// StateReset → ParamInit → JobAlloc → ProgLookup → compile on cache miss.
REX_HOOK_RAW(ShaderBuildOrReuse) {
    daytona_debug::RecordHook(daytona_debug::HookId::kShaderBuildOrReuse, ctx);
    daytona_render::RecordShaderBuildEnter(ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
    __imp__sub_823C4350(ctx, base);
    daytona_render::RecordShaderBuildExit(ctx.r3.u32);
}

// ── Phase 4: Shader source capture ───────────────────────────────────────────

// ShaderCompile (sub_822F9180): top-level shader compile entry point.
// Log all args so we can identify which register holds the source string.
REX_HOOK_RAW(sub_822F9180) {
    static uint32_t s_calls = 0;
    ++s_calls;
    const bool log = s_calls <= 32 || (s_calls % 500) == 0;
    if (log) {
        REXLOG_ERROR(
            "Daytona ShaderCompile #{} lr={:08X} r3={:08X} r4={:08X} r5={:08X} r6={:08X} r7={:08X} r8={:08X} r9={:08X} r10={:08X}",
            s_calls, static_cast<uint32_t>(ctx.lr),
            ctx.r3.u32, ctx.r4.u32, ctx.r5.u32,
            ctx.r6.u32, ctx.r7.u32, ctx.r8.u32,
            ctx.r9.u32, ctx.r10.u32);
        // Try reading r3 as a string (may be shader source or object ptr).
        char src_peek[128]{};
        ReadGuestString(base, ctx.r3.u32, src_peek, sizeof(src_peek));
        if (src_peek[0] >= 0x20) {
            REXLOG_ERROR("  ShaderCompile r3-as-str: '{:.80s}'", src_peek);
            daytona_render::RecordShaderSource(ctx.r3.u32, src_peek, ctx.r4.u32);
        }
    }
    __imp__sub_822F9180(ctx, base);
    if (log)
        REXLOG_ERROR("Daytona ShaderCompile exit #{} result={:08X}", s_calls, ctx.r3.u32);
}

// ShaderBuildLoop (sub_822F92D8): iterates pending shader compile jobs.
REX_HOOK_RAW(sub_822F92D8) {
    static uint32_t s_calls = 0;
    ++s_calls;
    const bool log = s_calls <= 16 || (s_calls % 2000) == 0;
    if (log)
        REXLOG_ERROR("Daytona ShaderBuildLoop #{} lr={:08X} r3={:08X} r4={:08X} r5={:08X}",
                     s_calls, static_cast<uint32_t>(ctx.lr),
                     ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
    __imp__sub_822F92D8(ctx, base);
}

// ShaderSourceCompile (sub_823AFB08): compiles one HLSL-like source blob.
// r3 is expected to be the source string ptr; r4 the byte length.
REX_HOOK_RAW(sub_823AFB08) {
    static uint32_t s_calls = 0;
    ++s_calls;
    const bool log = s_calls <= 32 || (s_calls % 500) == 0;
    char src_peek[128]{};
    ReadGuestString(base, ctx.r3.u32, src_peek, sizeof(src_peek));
    const bool looks_like_src = src_peek[0] >= 0x20;
    if (log) {
        REXLOG_ERROR(
            "Daytona ShaderSourceCompile #{} lr={:08X} r3={:08X} r4={:08X} r5={:08X} r6={:08X} src='{:.60s}'",
            s_calls, static_cast<uint32_t>(ctx.lr),
            ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32,
            looks_like_src ? src_peek : "(not a string)");
    }
    if (looks_like_src)
        daytona_render::RecordShaderSource(ctx.r3.u32, src_peek, ctx.r4.u32);
    __imp__sub_823AFB08(ctx, base);
    if (log)
        REXLOG_ERROR("Daytona ShaderSourceCompile exit #{} result={:08X}", s_calls, ctx.r3.u32);
}
