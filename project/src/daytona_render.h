#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>

namespace daytona_render {

// Per-frame draw-level statistics. Published atomically at FrameEnd; safe to
// read from any thread via GetLastFrameStats().
struct FrameStats {
    uint64_t frame_index = 0;
    uint32_t draw_execute_count = 0;
    uint32_t draw_precheck_count = 0;
    uint32_t render_pass_count = 0;
    uint32_t texture_process_count = 0;
    uint32_t shader_build_count = 0;
    uint32_t state_change_count = 0;
    uint32_t rt_bind_count = 0;
    uint32_t clear_count = 0;
    uint32_t command_submit_count = 0;
};

// Tagged event kinds recorded per hook call.
enum class EventKind : uint8_t {
    kFrameBegin,
    kFrameEnd,
    kRenderTargetBind,
    kClear,
    kDrawPrecheck,
    kDrawExecute,
    kTextureProcess,
    kShaderBuildEnter,
    kShaderBuildExit,
    kRenderPassBegin,
    kRenderPassEnd,
    kStateChange,
    kCommandSubmit,
};

// One captured hook call. r[0..5] hold PPC r3-r8 at entry (or r[0] holds
// the return value for *Exit / *End events). slot disambiguates DeviceSetState
// variants ('A', 'B', 'C').
struct RenderEvent {
    EventKind kind;
    uint8_t slot = 0;
    uint16_t reserved = 0;
    uint32_t seq = 0;   // within-frame sequence counter
    uint32_t r[6] = {};
};

// Memory snapshot of a draw call, captured at GpuDrawExecute hook time.
// r[0..5] = r3-r8; r3_dump/r4_dump hold the first N guest words at those
// addresses (byteswapped to little-endian for easy printing).
struct DrawCallSnap {
    uint32_t r[6];        // raw PPC registers r3-r8
    uint32_t r3_dump[8];  // first 8 words at r3 (device/context), 0 if invalid
    uint32_t r4_dump[4];  // first 4 words at r4, 0 if not a valid guest addr
    uint32_t index_count; // decoded from r4_dump[0] or r5 (best-effort)
    uint32_t prim_type;   // decoded from r4_dump[1] (best-effort)
};

// Memory snapshot of a texture descriptor, captured at GpuTextureProcess time.
// desc_dump[0..7] = first 8 words of the descriptor struct at r3.
struct TexDescSnap {
    uint32_t r[4];         // r3-r6 (r3 = descriptor ptr, r4 = type)
    uint32_t desc_dump[8]; // first 8 words at r3 (width/height/fmt/data/…)
};

// Full single-frame capture (~750KB static storage). Valid after
// GetLastCapture() returns non-null. A new RequestCapture() invalidates
// the previous contents on the next FrameBegin().
struct FrameCapture {
    static constexpr uint32_t kMaxEvents    = 16384;
    static constexpr uint32_t kMaxDrawSnaps = 2048;
    static constexpr uint32_t kMaxTexSnaps  = 2048;

    FrameStats stats;
    uint32_t event_count    = 0;
    uint32_t overflow_count = 0;
    std::array<RenderEvent, kMaxEvents> events{};

    uint32_t draw_snap_count = 0;
    std::array<DrawCallSnap, kMaxDrawSnaps> draw_snaps{};

    uint32_t tex_snap_count = 0;
    std::array<TexDescSnap, kMaxTexSnaps> tex_snaps{};
};

// Per-texture-load record. Captured once at TexLoad time from the path argument
// and function return value (guest texture object pointer). Session-wide.
struct TexEntry {
    char path[128];       // null-terminated, truncated if longer
    uint32_t guest_obj;   // TexLoad return value (game texture object ptr)
    uint64_t frame_index; // frame in which first loaded
};

// Texture object candidate decoded from live PM4 TFETCH constants. This is the
// first renderer-owned identity boundary: base/mip/format/extent/pitch/tile.
struct GuestTextureEntry {
    uint32_t base_phys;
    uint32_t mip_phys;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t dimension;
    uint32_t pitch;
    uint32_t tiled;
    uint32_t first_slot;
    uint32_t last_slot;
    uint32_t tfetch_count;
    uint32_t draw_use_count;
    uint32_t op_2d_count;
    uint32_t op_36_count;
    uint32_t other_draw_count;
    uint32_t host_cache_id;       // Stable runtime cache handle for this identity.
    uint32_t host_format;         // Daytona host-format class, see TextureHostFormatName.
    uint32_t estimated_bytes;     // Level-0 upload size estimate.
    uint32_t cache_state;         // 1=metadata, 2=upload-candidate, 3=linearized.
    uint32_t cache_hit_count;     // TFETCH/draw touches through this cache record.
    uint64_t first_frame;
    uint64_t last_frame;
    // Content stability measurement: FNV-1a hash of the first 64 guest-phys
    // bytes.  hash_change_count is the number of times the hash differed from
    // the previous draw-use sample.  0 means the texture has been static since
    // first observation.
    uint32_t content_hash = 0;
    uint32_t hash_change_count = 0;
    uint32_t uploaded_hash = 0;  // Hash at time of last debug-texture upload; 0 = never uploaded.
};

// Per-shader-build record. Captured at ShaderSourceCompile time.
struct ShaderEntry {
    uint32_t guest_shader_ptr; // guest pointer passed to ShaderCompile
    uint32_t source_len;       // byte length of source string (r4 at hook)
    char preview[128];         // first 127 chars of source, null-terminated
    uint64_t frame_index;
};

// ── Guest memory access ───────────────────────────────────────────────────────
// Called once from the FrameRender hook with the SDK `base` pointer so that
// the render module can snapshot guest memory during capture.
void SetGuestBase(uint8_t* base);

// Read one big-endian uint32_t from guest memory. Returns 0 for invalid addrs.
// Safe to call from any thread while the game is running.
uint32_t GuestReadU32(uint32_t guest_addr);

// ── Frame boundaries (called from the FrameRender hook) ──────────────────────
void FrameBegin(uint32_t r3);
void FrameEnd(uint32_t result);

// ── Per-hook recording (called from each instrumented hook) ──────────────────
void RecordRenderTarget(uint32_t r3, uint32_t r4, uint32_t r5, uint32_t r6);
void RecordClear(uint32_t r3, uint32_t r4, uint32_t r5, uint32_t r6);
void RecordDrawPrecheck(uint32_t r3, uint32_t r4, uint32_t r5,
                        uint32_t r6, uint32_t r7, uint32_t r8);
void RecordDrawExecute(uint32_t r3, uint32_t r4, uint32_t r5,
                       uint32_t r6, uint32_t r7, uint32_t r8);
void RecordTextureProcess(uint32_t r3, uint32_t r4, uint32_t r5,
                          uint32_t r6, uint32_t r7, uint32_t r8);
void RecordShaderBuildEnter(uint32_t r3, uint32_t r4, uint32_t r5);
void RecordShaderBuildExit(uint32_t result);
void RecordRenderPassBegin(uint32_t r3, uint32_t r4);
void RecordRenderPassEnd(uint32_t result);
void RecordStateChange(char slot, uint32_t r3, uint32_t r4);
void RecordCommandSubmit(uint32_t r3, uint32_t r4, uint32_t r5);
// Called after GpuDescriptorSubmit executes — stores CP_RB_WPTR bracket in the
// most recently pushed CMD_SUB event's r[3] (start) and r[4] (end).
void UpdateLastCmdWritePtrs(uint32_t start_wptr, uint32_t end_wptr);

// ── Ring buffer access ────────────────────────────────────────────────────────
// Called once from the VdInitializeRingBuffer hook.
void SetRingBufferPhys(uint32_t phys_addr, uint32_t size_log2);
// Read one big-endian dword at dword_offset into the ring buffer (wraps).
// Returns 0 if the ring buffer has not been initialised yet.
uint32_t ReadRingBufferDword(uint32_t dword_offset);
uint32_t GetRingBufferPhys();
uint32_t GetRingBufferSize();

// ── Texture registry (session-wide, thread-safe reads) ───────────────────────
static constexpr uint32_t kMaxTexEntries = 512;
// Record a texture load. Called after TexLoad returns, with the path that was
// passed in and the resulting guest texture object pointer.
void RecordTexLoad(const char* path, uint32_t result_obj);
uint32_t GetTexEntryCount();
const TexEntry* GetTexEntries(); // stable pointer; pair with GetTexEntryCount
// Linear scan by guest_obj; returns nullptr if not found.
const TexEntry* FindTexByObj(uint32_t guest_obj);

// ── Guest texture registry from live TFETCH state ────────────────────────────
static constexpr uint32_t kMaxGuestTextures = 512;
void RecordGuestTexture(uint32_t slot, uint32_t base_phys, uint32_t mip_phys,
                        uint32_t width, uint32_t height, uint32_t format,
                        uint32_t dimension, uint32_t pitch, uint32_t tiled);
uint32_t RecordGuestTextureDrawUse(uint32_t slot, uint8_t draw_op,
                                   uint32_t base_phys, uint32_t mip_phys,
                                   uint32_t width, uint32_t height, uint32_t format,
                                   uint32_t dimension, uint32_t pitch, uint32_t tiled);
uint32_t GetGuestTextureCount();
const GuestTextureEntry* GetGuestTextures();
// ── Shader registry (session-wide, thread-safe reads) ────────────────────────
static constexpr uint32_t kMaxShaderEntries = 512;
// Record a shader source-compile event. src may be nullptr if not yet decoded.
void RecordShaderSource(uint32_t guest_ptr, const char* src, uint32_t len);
uint32_t GetShaderEntryCount();
const ShaderEntry* GetShaderEntries();

// ── Query API (safe from any thread) ─────────────────────────────────────────

// Trigger a single-frame event capture. The next FrameBegin activates it.
void RequestCapture();

// Stats for the last completed frame. Protected by an internal mutex.
FrameStats GetLastFrameStats();

// Pointer to the last captured frame, or nullptr if none is ready yet.
// The pointer remains stable (static storage); reuse after a new RequestCapture
// invalidates the previous capture contents.
const FrameCapture* GetLastCapture();

}  // namespace daytona_render
