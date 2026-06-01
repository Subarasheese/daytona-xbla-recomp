#include "daytona_render.h"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>

namespace daytona_render {

// ── Guest memory access ───────────────────────────────────────────────────────

namespace {
// Pointer to the guest virtual address space base, set once from FrameRender.
// Mapping: host_ptr = g_guest_base + guest_virtual_addr (for addrs < 0xE0000000).
// Xbox 360 is big-endian so 32-bit loads need __builtin_bswap32.
std::atomic<uint8_t*> g_guest_base_atomic{nullptr};
uint8_t* g_guest_base = nullptr;

inline bool IsValidGuestAddr(uint32_t addr) noexcept {
    return g_guest_base != nullptr && addr >= 0x70000000u && addr < 0xE0000000u;
}

inline uint32_t ReadBE32(uint32_t addr) noexcept {
    if (!IsValidGuestAddr(addr)) return 0;
    uint32_t raw;
    std::memcpy(&raw, g_guest_base + addr, sizeof(raw));
    return __builtin_bswap32(raw);
}

template<uint32_t N>
void SnapWords(uint32_t addr, uint32_t (&out)[N]) noexcept {
    for (uint32_t i = 0; i < N; ++i) out[i] = ReadBE32(addr + i * 4);
}
}  // namespace

void SetGuestBase(uint8_t* base) {
    g_guest_base = base;
    g_guest_base_atomic.store(base, std::memory_order_relaxed);
}

uint32_t GuestReadU32(uint32_t guest_addr) {
    uint8_t* base = g_guest_base_atomic.load(std::memory_order_relaxed);
    if (!base || guest_addr < 0x70000000u || guest_addr >= 0xE0000000u) return 0;
    uint32_t raw;
    std::memcpy(&raw, base + guest_addr, sizeof(raw));
    return __builtin_bswap32(raw);
}

namespace {

// Live per-frame counters. Written exclusively from the game thread inside a
// FrameBegin..FrameEnd window, so no synchronisation is needed for writes.
// Published to g_last_stats under g_stats_mutex at FrameEnd.
struct LiveStats {
    uint64_t frame_index = 0;
    uint32_t draw_execute = 0;
    uint32_t draw_precheck = 0;
    uint32_t render_pass = 0;
    uint32_t texture_process = 0;
    uint32_t shader_build = 0;
    uint32_t state_change = 0;
    uint32_t rt_bind = 0;
    uint32_t clear = 0;
    uint32_t command_submit = 0;
    uint32_t event_seq = 0;
};

LiveStats g_live;

std::mutex g_stats_mutex;
FrameStats g_last_stats;

// On-demand single-frame capture. Written only from the game thread while
// g_capturing == true. g_capture_complete is the release/acquire fence that
// makes the completed data visible to reader threads.
std::atomic<bool> g_capture_requested{false};
bool g_capturing = false;
std::atomic<bool> g_capture_complete{false};
FrameCapture g_capture;

FrameStats ToStats(const LiveStats& live) {
    FrameStats s;
    s.frame_index = live.frame_index;
    s.draw_execute_count = live.draw_execute;
    s.draw_precheck_count = live.draw_precheck;
    s.render_pass_count = live.render_pass;
    s.texture_process_count = live.texture_process;
    s.shader_build_count = live.shader_build;
    s.state_change_count = live.state_change;
    s.rt_bind_count = live.rt_bind;
    s.clear_count = live.clear;
    s.command_submit_count = live.command_submit;
    return s;
}

void PushEvent(EventKind kind, uint8_t slot, const uint32_t* r, uint32_t n) {
    if (!g_capturing) return;
    if (g_capture.event_count >= FrameCapture::kMaxEvents) {
        ++g_capture.overflow_count;
        return;
    }
    RenderEvent& ev = g_capture.events[g_capture.event_count++];
    ev.kind = kind;
    ev.slot = slot;
    ev.reserved = 0;
    ev.seq = g_live.event_seq;
    for (uint32_t i = 0; i < 6; ++i) ev.r[i] = (i < n) ? r[i] : 0;
}

}  // namespace

void FrameBegin(uint32_t r3) {
    const bool requested = g_capture_requested.exchange(false, std::memory_order_relaxed);
    if (requested) {
        g_capture_complete.store(false, std::memory_order_relaxed);
        g_capture.stats = {};
        g_capture.event_count = 0;
        g_capture.overflow_count = 0;
        g_capture.draw_snap_count = 0;
        g_capture.tex_snap_count = 0;
        g_capturing = true;
    } else {
        g_capturing = false;
    }

    const uint64_t next_frame = g_last_stats.frame_index + 1;
    g_live = {};
    g_live.frame_index = next_frame;

    if (g_capturing) {
        const uint32_t r[] = {r3, 0, 0, 0, 0, 0};
        PushEvent(EventKind::kFrameBegin, 0, r, 1);
    }
}

void FrameEnd(uint32_t result) {
    if (g_capturing) {
        const uint32_t r[] = {result, 0, 0, 0, 0, 0};
        PushEvent(EventKind::kFrameEnd, 0, r, 1);
        g_capture.stats = ToStats(g_live);
        g_capture_complete.store(true, std::memory_order_release);
        g_capturing = false;
    }

    {
        std::lock_guard<std::mutex> lock(g_stats_mutex);
        g_last_stats = ToStats(g_live);
    }
}

void RecordRenderTarget(uint32_t r3, uint32_t r4, uint32_t r5, uint32_t r6) {
    ++g_live.rt_bind;
    ++g_live.event_seq;
    if (g_capturing) {
        const uint32_t r[] = {r3, r4, r5, r6, 0, 0};
        PushEvent(EventKind::kRenderTargetBind, 0, r, 4);
    }
}

void RecordClear(uint32_t r3, uint32_t r4, uint32_t r5, uint32_t r6) {
    ++g_live.clear;
    ++g_live.event_seq;
    if (g_capturing) {
        const uint32_t r[] = {r3, r4, r5, r6, 0, 0};
        PushEvent(EventKind::kClear, 0, r, 4);
    }
}

void RecordDrawPrecheck(uint32_t r3, uint32_t r4, uint32_t r5,
                        uint32_t r6, uint32_t r7, uint32_t r8) {
    ++g_live.draw_precheck;
    ++g_live.event_seq;
    if (g_capturing) {
        const uint32_t r[] = {r3, r4, r5, r6, r7, r8};
        PushEvent(EventKind::kDrawPrecheck, 0, r, 6);
    }
}

void RecordDrawExecute(uint32_t r3, uint32_t r4, uint32_t r5,
                       uint32_t r6, uint32_t r7, uint32_t r8) {
    ++g_live.draw_execute;
    ++g_live.event_seq;
    if (g_capturing) {
        const uint32_t r[] = {r3, r4, r5, r6, r7, r8};
        PushEvent(EventKind::kDrawExecute, 0, r, 6);
        if (g_capture.draw_snap_count < FrameCapture::kMaxDrawSnaps) {
            DrawCallSnap& s = g_capture.draw_snaps[g_capture.draw_snap_count++];
            for (uint32_t i = 0; i < 6; ++i) s.r[i] = r[i];
            SnapWords(r3, s.r3_dump);
            SnapWords(r4, s.r4_dump);
            // Best-effort: r5 is often index_count, r6 often primitive type.
            s.index_count = r5;
            s.prim_type   = r6;
        }
    }
}

void RecordTextureProcess(uint32_t r3, uint32_t r4, uint32_t r5,
                          uint32_t r6, uint32_t r7, uint32_t r8) {
    ++g_live.texture_process;
    ++g_live.event_seq;
    if (g_capturing) {
        const uint32_t r[] = {r3, r4, r5, r6, r7, r8};
        PushEvent(EventKind::kTextureProcess, 0, r, 6);
        if (g_capture.tex_snap_count < FrameCapture::kMaxTexSnaps) {
            TexDescSnap& s = g_capture.tex_snaps[g_capture.tex_snap_count++];
            s.r[0] = r3; s.r[1] = r4; s.r[2] = r5; s.r[3] = r6;
            SnapWords(r3, s.desc_dump);
        }
    }
}

void RecordShaderBuildEnter(uint32_t r3, uint32_t r4, uint32_t r5) {
    ++g_live.shader_build;
    ++g_live.event_seq;
    if (g_capturing) {
        const uint32_t r[] = {r3, r4, r5, 0, 0, 0};
        PushEvent(EventKind::kShaderBuildEnter, 0, r, 3);
    }
}

void RecordShaderBuildExit(uint32_t result) {
    ++g_live.event_seq;
    if (g_capturing) {
        const uint32_t r[] = {result, 0, 0, 0, 0, 0};
        PushEvent(EventKind::kShaderBuildExit, 0, r, 1);
    }
}

void RecordRenderPassBegin(uint32_t r3, uint32_t r4) {
    ++g_live.render_pass;
    ++g_live.event_seq;
    if (g_capturing) {
        const uint32_t r[] = {r3, r4, 0, 0, 0, 0};
        PushEvent(EventKind::kRenderPassBegin, 0, r, 2);
    }
}

void RecordRenderPassEnd(uint32_t result) {
    ++g_live.event_seq;
    if (g_capturing) {
        const uint32_t r[] = {result, 0, 0, 0, 0, 0};
        PushEvent(EventKind::kRenderPassEnd, 0, r, 1);
    }
}

void RecordStateChange(char slot, uint32_t r3, uint32_t r4) {
    ++g_live.state_change;
    ++g_live.event_seq;
    if (g_capturing) {
        const uint32_t r[] = {r3, r4, 0, 0, 0, 0};
        PushEvent(EventKind::kStateChange, static_cast<uint8_t>(slot), r, 2);
    }
}

// CommandSubmit is very hot (called per GPU packet). Count it but do not bump
// the sequence counter to avoid artificially overflowing the capture ring.
void RecordCommandSubmit(uint32_t r3, uint32_t r4, uint32_t r5) {
    ++g_live.command_submit;
    if (g_capturing) {
        const uint32_t r[] = {r3, r4, r5, 0, 0, 0};
        PushEvent(EventKind::kCommandSubmit, 0, r, 3);
    }
}

void UpdateLastCmdWritePtrs(uint32_t start_wptr, uint32_t end_wptr) {
    if (!g_capturing || g_capture.event_count == 0) return;
    auto& ev = g_capture.events[g_capture.event_count - 1];
    if (ev.kind != EventKind::kCommandSubmit) return;
    ev.r[3] = start_wptr;
    ev.r[4] = end_wptr;
}

// ── Ring buffer ───────────────────────────────────────────────────────────────
namespace {
uint32_t g_rb_phys = 0;
uint32_t g_rb_size = 0;
}

void SetRingBufferPhys(uint32_t phys_addr, uint32_t size_log2) {
    g_rb_phys = phys_addr;
    g_rb_size = uint32_t(1) << (size_log2 + 3);
}

uint32_t GetRingBufferPhys() { return g_rb_phys; }
uint32_t GetRingBufferSize() { return g_rb_size; }

uint32_t ReadRingBufferDword(uint32_t dword_offset) {
    if (!g_rb_phys || !g_rb_size) return 0;
    uint32_t byte_off = (dword_offset * 4) % g_rb_size;
    // Physical memory is accessible at guest virtual 0xA0000000 + phys (uncached alias).
    uint32_t guest_addr = 0xA0000000u | ((g_rb_phys + byte_off) & 0x1FFFFFFFu);
    return ReadBE32(guest_addr);
}

void RequestCapture() {
    g_capture_requested.store(true, std::memory_order_relaxed);
}

// ── Texture registry ──────────────────────────────────────────────────────────
namespace {
std::mutex g_tex_mutex;
uint32_t   g_tex_count = 0;
TexEntry   g_tex_entries[kMaxTexEntries];
}

void RecordTexLoad(const char* path, uint32_t result_obj) {
    std::lock_guard<std::mutex> lock(g_tex_mutex);
    // Update in-place if the same object is reloaded.
    for (uint32_t i = 0; i < g_tex_count; ++i) {
        if (g_tex_entries[i].guest_obj == result_obj) {
            std::strncpy(g_tex_entries[i].path, path, 127);
            g_tex_entries[i].path[127] = '\0';
            g_tex_entries[i].frame_index = g_last_stats.frame_index;
            return;
        }
    }
    if (g_tex_count >= kMaxTexEntries) return;
    TexEntry& e = g_tex_entries[g_tex_count++];
    std::strncpy(e.path, path, 127);
    e.path[127] = '\0';
    e.guest_obj    = result_obj;
    e.frame_index  = g_last_stats.frame_index;
}

uint32_t GetTexEntryCount() {
    std::lock_guard<std::mutex> lock(g_tex_mutex);
    return g_tex_count;
}

const TexEntry* GetTexEntries() { return g_tex_entries; }

const TexEntry* FindTexByObj(uint32_t guest_obj) {
    const uint32_t n = g_tex_count; // benign read; worst case miss
    for (uint32_t i = 0; i < n; ++i) {
        if (g_tex_entries[i].guest_obj == guest_obj) return &g_tex_entries[i];
    }
    return nullptr;
}

// ── Guest texture registry from live TFETCH state ────────────────────────────
namespace {
std::mutex g_guest_tex_mutex;
uint32_t g_guest_tex_count = 0;
uint32_t g_next_guest_tex_cache_id = 1;
GuestTextureEntry g_guest_textures[kMaxGuestTextures];

uint32_t HostFormatForTexture(uint32_t format) {
    switch (format) {
        case 6:  return 1; // RGBA8/BGRA8-class uncompressed surface.
        case 10: return 2; // RG8-class two-channel surface.
        case 20: return 3; // BC3/DXT5 block-compressed surface.
        default: return 0;
    }
}

uint32_t EstimateGuestTextureBytes(uint32_t base_phys, uint32_t mip_phys,
                                   uint32_t width, uint32_t height,
                                   uint32_t format) {
    if (mip_phys > base_phys) {
        const uint32_t level0_bytes = mip_phys - base_phys;
        if (level0_bytes > 0 && level0_bytes <= 16u * 1024u * 1024u)
            return level0_bytes;
    }

    uint64_t bytes = 0;
    if (format == 20) {
        const uint64_t blocks_x = (uint64_t(width) + 3u) / 4u;
        const uint64_t blocks_y = (uint64_t(height) + 3u) / 4u;
        bytes = blocks_x * blocks_y * 16u;
    } else if (format == 10) {
        bytes = uint64_t(width) * height * 2u;
    } else {
        bytes = uint64_t(width) * height * 4u;
    }
    if (bytes == 0 || bytes > 16ull * 1024ull * 1024ull) return 0;
    return uint32_t(bytes);
}

uint32_t BytesPerGuestBlock(uint32_t format) {
    switch (format) {
        case 6:  return 4;
        case 10: return 2;
        case 20: return 16;
        default: return 4;
    }
}

uint32_t BlockWidth(uint32_t format) {
    return format == 20 ? 4u : 1u;
}

uint32_t BlockHeight(uint32_t format) {
    return format == 20 ? 4u : 1u;
}

uint32_t Log2BytesPerBlock(uint32_t bytes_per_block) {
    return (bytes_per_block / 4u) + ((bytes_per_block / 2u) >> (bytes_per_block / 4u));
}

uint32_t AlignPow2(uint32_t value, uint32_t alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

uint32_t TiledOffset2D(uint32_t x, uint32_t y, uint32_t pitch, uint32_t log2_bpp) {
    pitch = AlignPow2(pitch, 32u);
    const uint32_t macro = ((x >> 5) + (y >> 5) * (pitch >> 5)) << (log2_bpp + 7);
    const uint32_t micro = ((x & 7u) + ((y & 0xEu) << 2)) << log2_bpp;
    const uint32_t offset = macro + ((micro & ~0xFu) << 1) + (micro & 0xFu) + ((y & 1u) << 4);
    return ((offset & ~0x1FFu) << 3) +
           ((y & 16u) << 7) +
           ((offset & 0x1C0u) << 2) +
           (((((y & 8u) >> 2) + (x >> 3)) & 3u) << 6) +
           (offset & 0x3Fu);
}


void ReadGuestPhysBytes(uint32_t phys, uint32_t offset, uint8_t* out, uint32_t bytes) {
    for (uint32_t i = 0; i < bytes; ++i) {
        const uint32_t addr = phys + offset + i;
        const uint32_t word = GuestReadU32(0xA0000000u | (addr & 0x1FFFFFFCu));
        const uint32_t byte_sel = addr & 3u;
        out[i] = static_cast<uint8_t>(word >> ((3u - byte_sel) * 8u));
    }
}

bool SameGuestTexture(const GuestTextureEntry& e,
                      uint32_t base_phys, uint32_t mip_phys,
                      uint32_t width, uint32_t height, uint32_t format,
                      uint32_t dimension, uint32_t pitch, uint32_t tiled) {
    return e.base_phys == base_phys &&
           e.mip_phys == mip_phys &&
           e.width == width &&
           e.height == height &&
           e.format == format &&
           e.dimension == dimension &&
           e.pitch == pitch &&
           e.tiled == tiled;
}


GuestTextureEntry* FindOrCreateGuestTexture(uint32_t slot,
                                            uint32_t base_phys, uint32_t mip_phys,
                                            uint32_t width, uint32_t height,
                                            uint32_t format, uint32_t dimension,
                                            uint32_t pitch, uint32_t tiled) {
    for (uint32_t i = 0; i < g_guest_tex_count; ++i) {
        GuestTextureEntry& e = g_guest_textures[i];
        if (SameGuestTexture(e, base_phys, mip_phys, width, height,
                             format, dimension, pitch, tiled)) {
            e.last_slot = slot;
            e.last_frame = g_last_stats.frame_index;
            return &e;
        }
    }
    if (g_guest_tex_count >= kMaxGuestTextures) return nullptr;

    GuestTextureEntry& e = g_guest_textures[g_guest_tex_count++];
    e = {};
    e.base_phys = base_phys;
    e.mip_phys = mip_phys;
    e.width = width;
    e.height = height;
    e.format = format;
    e.dimension = dimension;
    e.pitch = pitch;
    e.tiled = tiled;
    e.first_slot = slot;
    e.last_slot = slot;
    e.host_cache_id = g_next_guest_tex_cache_id++;
    e.host_format = HostFormatForTexture(format);
    e.estimated_bytes = EstimateGuestTextureBytes(base_phys, mip_phys, width, height, format);
    e.cache_state = 1;
    e.first_frame = g_last_stats.frame_index;
    e.last_frame = g_last_stats.frame_index;
    return &e;
}
}  // namespace

void RecordGuestTexture(uint32_t slot, uint32_t base_phys, uint32_t mip_phys,
                        uint32_t width, uint32_t height, uint32_t format,
                        uint32_t dimension, uint32_t pitch, uint32_t tiled) {
    if (!base_phys || !width || !height) return;
    std::lock_guard<std::mutex> lock(g_guest_tex_mutex);
    GuestTextureEntry* e = FindOrCreateGuestTexture(slot, base_phys, mip_phys,
        width, height, format, dimension, pitch, tiled);
    if (!e) return;
    ++e->tfetch_count;
    ++e->cache_hit_count;
}

uint32_t RecordGuestTextureDrawUse(uint32_t slot, uint8_t draw_op,
                                   uint32_t base_phys, uint32_t mip_phys,
                                   uint32_t width, uint32_t height, uint32_t format,
                                   uint32_t dimension, uint32_t pitch, uint32_t tiled) {
    if (!base_phys || !width || !height) return 0;
    std::lock_guard<std::mutex> lock(g_guest_tex_mutex);
    GuestTextureEntry* e = FindOrCreateGuestTexture(slot, base_phys, mip_phys,
        width, height, format, dimension, pitch, tiled);
    if (!e) return 0;
    ++e->draw_use_count;
    ++e->cache_hit_count;
    if (e->estimated_bytes) e->cache_state = 2;
    if (draw_op == 0x2D) ++e->op_2d_count;
    else if (draw_op == 0x36) ++e->op_36_count;
    else ++e->other_draw_count;
    return e->host_cache_id;
}

uint32_t GetGuestTextureCount() {
    std::lock_guard<std::mutex> lock(g_guest_tex_mutex);
    return g_guest_tex_count;
}

const GuestTextureEntry* GetGuestTextures() { return g_guest_textures; }


// ── Shader registry ───────────────────────────────────────────────────────────
namespace {
std::mutex   g_shader_mutex;
uint32_t     g_shader_count = 0;
ShaderEntry  g_shader_entries[kMaxShaderEntries];
}

void RecordShaderSource(uint32_t guest_ptr, const char* src, uint32_t len) {
    std::lock_guard<std::mutex> lock(g_shader_mutex);
    // Avoid storing the same shader address twice.
    for (uint32_t i = 0; i < g_shader_count; ++i) {
        if (g_shader_entries[i].guest_shader_ptr == guest_ptr) return;
    }
    if (g_shader_count >= kMaxShaderEntries) return;
    ShaderEntry& e  = g_shader_entries[g_shader_count++];
    e.guest_shader_ptr = guest_ptr;
    e.source_len       = len;
    e.frame_index      = g_last_stats.frame_index;
    if (src) {
        std::strncpy(e.preview, src, 127);
        e.preview[127] = '\0';
    } else {
        e.preview[0] = '\0';
    }
}

uint32_t GetShaderEntryCount() {
    std::lock_guard<std::mutex> lock(g_shader_mutex);
    return g_shader_count;
}

const ShaderEntry* GetShaderEntries() { return g_shader_entries; }

FrameStats GetLastFrameStats() {
    std::lock_guard<std::mutex> lock(g_stats_mutex);
    return g_last_stats;
}

const FrameCapture* GetLastCapture() {
    if (!g_capture_complete.load(std::memory_order_acquire)) return nullptr;
    return &g_capture;
}

}  // namespace daytona_render
