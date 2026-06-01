#include "daytona_host_compat.h"
#include "daytona_renderer.h"
#include "daytona_vulkan_renderer.h"
#include <locale.h>

#include "daytona_render.h"

#include <rex/graphics/pipeline/shader/shader.h>
#include <rex/logging.h>
#include <rex/cvar.h>

#include <charconv>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <sys/stat.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <vector>

// Register enum values needed for direct register reads.
// These come from <rex/graphics/xenos.h> via the XE_GPU_REG_* constants
// defined there.  We use the RegisterFile::Get<T>() typed accessors where
// available and fall back to values[] for constants we need numerically.
#include <rex/graphics/xenos.h>
#include <rex/graphics/registers.h>

namespace daytona {

namespace reg = rex::graphics::reg;

// ── Catalog state (command-processor thread only, no locking needed) ──────────

namespace {
// Keep enough room for a long all-tracks capture, but avoid unbounded growth.
static constexpr uint32_t kMaxRecords = 65536;
// Writing JSON is not free on the render thread; flush periodically and at shutdown.
static constexpr uint32_t kFlushEveryNewRecords = 256;
static DrawRecord g_records[kMaxRecords];
static uint32_t  g_record_count = 0;
static uint64_t  g_total_draws  = 0;
static uint64_t  g_overflow_draws = 0;
static uint64_t  g_dumped_shader_hashes[64];
static uint32_t  g_dumped_shader_count = 0;
static uint64_t  g_native_shadow_quadlist_hits = 0;
static uint64_t  g_native_shadow_quadlist_enabled_hits = 0;
static uint64_t  g_native_quadlist_takeover_count = 0;
static uint64_t  g_native_shadow_pointlist_hits = 0;
static uint64_t  g_native_shadow_pointlist_enabled_hits = 0;
static uint64_t  g_native_pointlist_takeover_count = 0;
static uint64_t  g_native_shadow_mesh_hits = 0;
static uint64_t  g_native_shadow_mesh_enabled_hits = 0;
static uint64_t  g_native_mesh_takeover_count = 0;
static uint64_t  g_native_quadlist_zero_sample_skips = 0;
static uint64_t  g_native_pointlist_zero_sample_skips = 0;
static bool      g_native_quadlist_sample_valid = false;
static bool      g_native_pointlist_sample_valid = false;

static constexpr uint64_t kNativeTargetQuadListVs  = 0x39076F2082E5AE16ull;
static constexpr uint64_t kNativeTargetQuadListPs  = 0xB7562ACD8E8C5B11ull;
// Vertex-color + alpha-kill variant; same VS/vertex layout, no texture sampling.
static constexpr uint64_t kNativeTargetQuadListPs2 = 0x7F233B2859368673ull;
static constexpr uint32_t kNativeTargetQuadListTexMask = 0x00000003u;
static constexpr uint32_t kNativeTargetQuadListVtxMaskLo = 0x00000016u;
static constexpr uint32_t kNativeTargetQuadListVtxMaskMid = 0x00000000u;
static constexpr uint32_t kNativeTargetQuadListVtxMaskHi = 0x90C00000u;
static constexpr uint32_t kNativeTargetQuadListRtPitch = 1920u;
static constexpr uint32_t kNativeTargetQuadListShaderStrideDwords = 9u;
static constexpr uint32_t kNativeTargetQuadListSampleCount = 64u;
static constexpr uint32_t kNativeTargetQuadListSampleVertices = 4u;
static constexpr uint32_t kNativeTargetQuadListSampleDwords = 9u;
static constexpr uint32_t kNativeTargetQuadListScanSlots = 16u;

static constexpr uint64_t kNativeTargetPointListVs = 0xB6C9863F710683ECull;
static constexpr uint64_t kNativeTargetPointListPs = 0xA4A965C189287B99ull;
static constexpr uint32_t kNativeTargetPointListSampleVertices = 8u;
static constexpr uint32_t kNativeTargetPointListSampleDwords = 16u;
static constexpr uint32_t kNativeTargetPointListScanSlots = 24u;

// Mesh (TriangleStrip) target: 3 separate vertex streams, single-texture PS.
static constexpr uint64_t kNativeTargetMeshVs    = 0x63A523F2409137F3ull;
// Secondary 3D-world TriangleStrip shader (billboard sprites: trees, crowds,
// Sonic-on-cliff, signage). Uses the SAME perspective MVP in c72-c75 as the
// Mesh-3D track (verified col3=(0,0,-1,~2000)), so it must receive the same
// aspect-ratio (Hor+) correction or its sprites misalign with the widened world.
static constexpr uint64_t kNativeTargetWorldStripVs = 0xA210F533F3EBDE24ull;
// Mesh (QuadList) variant: same vf0/vf3/vf8 fetch slots + same MVP c72-c75 + same PS,
// but tighter stream strides (3/1/2 dwords) and QuadList prim topology.
static constexpr uint64_t kNativeTargetMeshFanVs = 0xFA44706CA015263Dull;
static constexpr uint64_t kNativeTargetMeshPs    = 0x7ABDFEFBCADB4BF9ull;
static constexpr uint32_t kNativeTargetMeshTexMask   = 0x00000001u;
static constexpr uint32_t kNativeTargetMeshVtxMaskLo = 0x00000012u;
static constexpr uint32_t kNativeTargetMeshSampleCount = 64u;
static constexpr uint32_t kNativeTargetMeshSampleVertices = 8u;
static constexpr uint32_t kNativeTargetMeshPosDwords = 3u;
static constexpr uint32_t kNativeTargetMeshColorDwords = 1u;
static constexpr uint32_t kNativeTargetMeshUvDwords = 2u;
static constexpr uint32_t kNativeTargetMeshIndexSampleCount = 16u;

// All native pipelines use VK_FORMAT_R8G8B8A8_UNORM — only accept k_8_8_8_8 (=0) RT draws.
static constexpr uint8_t  kNativeTargetColorFmt      = 0u;

struct NativeQuadListSample {
    DrawSig sig = {};
    uint64_t draw_number = 0;
    uint32_t index_offset = 0;
    uint32_t draw_initiator = 0;
    uint32_t raw_regs[DrawRecord::kRawRegCount] = {};
    uint32_t texture_base[2] = {};
    uint32_t texture_mip[2] = {};
    uint32_t texture_width[2] = {};
    uint32_t texture_height[2] = {};
    uint32_t texture_format[2] = {};
    uint32_t texture_tiled[2] = {};
    uint32_t texture_endian[2] = {};
    float c72_c75[4][4] = {};
    float c0[4] = {};
    float c255[4] = {};
    uint32_t raw_vfetch_slots[3] = {};
    uint32_t raw_vfetch_address_dwords[3] = {};
    uint32_t raw_vfetch_bases[3] = {};
    uint32_t raw_vfetch_sizes[3] = {};
    uint32_t base_rows[3][kNativeTargetQuadListSampleVertices][kNativeTargetQuadListSampleDwords] = {};
    uint32_t indexed_rows[3][kNativeTargetQuadListSampleVertices][kNativeTargetQuadListSampleDwords] = {};
    uint32_t unshifted_rows[3][kNativeTargetQuadListSampleVertices][kNativeTargetQuadListSampleDwords] = {};
    uint32_t scan_count = 0;
    uint32_t scan_slots[kNativeTargetQuadListScanSlots] = {};
    uint32_t scan_address_dwords[kNativeTargetQuadListScanSlots] = {};
    uint32_t scan_base_phys[kNativeTargetQuadListScanSlots] = {};
    uint32_t scan_size_bytes[kNativeTargetQuadListScanSlots] = {};
    uint32_t scan_first_dwords[kNativeTargetQuadListScanSlots][4] = {};
    uint32_t scan_rows[kNativeTargetQuadListScanSlots]
                      [kNativeTargetQuadListSampleVertices]
                      [kNativeTargetQuadListSampleDwords] = {};
    uint32_t shader_binding_count = 0;
    uint32_t shader_binding_fetch_constants[8] = {};
    uint32_t shader_binding_stride_words[8] = {};
    uint32_t shader_binding_attribute_counts[8] = {};
    uint32_t shader_binding_base_phys[8] = {};
    uint32_t shader_binding_size_bytes[8] = {};
    uint32_t shader_binding_rows[8]
                                [kNativeTargetQuadListSampleVertices]
                                [kNativeTargetQuadListSampleDwords] = {};
};
static NativeQuadListSample g_native_quadlist_sample;
static NativeQuadListSample g_native_quadlist_samples[kNativeTargetQuadListSampleCount];
static uint32_t g_native_quadlist_sample_count = 0;

struct NativePointListSample {
    uint64_t draw_number = 0;
    uint32_t index_offset = 0;
    uint32_t draw_initiator = 0;
    uint32_t index_count = 0;
    uint32_t tex_mask = 0;
    uint32_t vtx_mask = 0;
    uint32_t vtx_mask_mid = 0;
    uint32_t vtx_mask_hi = 0;
    uint32_t scan_count = 0;
    uint32_t scan_slots[kNativeTargetPointListScanSlots] = {};
    uint32_t scan_address_dwords[kNativeTargetPointListScanSlots] = {};
    uint32_t scan_base_phys[kNativeTargetPointListScanSlots] = {};
    uint32_t scan_size_bytes[kNativeTargetPointListScanSlots] = {};
    uint32_t scan_endian[kNativeTargetPointListScanSlots] = {};
    uint32_t base_rows[kNativeTargetPointListScanSlots]
                      [kNativeTargetPointListSampleVertices]
                      [kNativeTargetPointListSampleDwords] = {};
    uint32_t indexed_rows[kNativeTargetPointListScanSlots]
                         [kNativeTargetPointListSampleVertices]
                         [kNativeTargetPointListSampleDwords] = {};
    uint32_t shader_binding_count = 0;
    uint32_t shader_binding_fetch_constants[8] = {};
    uint32_t shader_binding_stride_words[8] = {};
    uint32_t shader_binding_attribute_counts[8] = {};
};
static NativePointListSample g_native_pointlist_sample;

struct NativeMeshSample {
    bool valid = false;
    DrawSig sig;
    uint64_t draw_number = 0;
    uint32_t index_offset = 0;
    uint32_t draw_initiator = 0;
    uint32_t raw_regs[DrawRecord::kRawRegCount] = {};
    uint32_t texture0_base = 0;
    uint32_t texture0_mip = 0;
    uint32_t texture0_width = 0;
    uint32_t texture0_height = 0;
    uint32_t texture0_format = 0;
    uint32_t texture0_tiled = 0;
    uint32_t texture0_endian = 0;
    uint32_t shader_binding_count = 0;
    uint32_t shader_binding_fetch_constants[8] = {};
    uint32_t shader_binding_stride_words[8] = {};
    uint32_t shader_binding_attribute_counts[8] = {};
    uint32_t pos_fetch_constant = 0xFFFFFFFFu;
    uint32_t col_fetch_constant = 0xFFFFFFFFu;
    uint32_t uv_fetch_constant = 0xFFFFFFFFu;
    uint32_t pos_base_phys = 0;
    uint32_t col_base_phys = 0;
    uint32_t uv_base_phys = 0;
    uint32_t pos_size_bytes = 0;
    uint32_t col_size_bytes = 0;
    uint32_t uv_size_bytes = 0;
    uint32_t pos_stride_words = 0;
    uint32_t col_stride_words = 0;
    uint32_t uv_stride_words = 0;
    uint32_t pos_endian = 0;
    uint32_t col_endian = 0;
    uint32_t uv_endian = 0;
    bool indexed = false;
    uint32_t index_format = 0;
    uint32_t index_endianness = 0;
    uint32_t index_guest_base = 0;
    uint64_t index_length = 0;
    uint32_t decoded_index_count = 0;
    uint32_t decoded_indices[kNativeTargetMeshIndexSampleCount] = {};
    uint32_t c72_c75[4][4] = {};
    uint32_t pos_rows[kNativeTargetMeshSampleVertices][kNativeTargetMeshPosDwords] = {};
    uint32_t col_rows[kNativeTargetMeshSampleVertices][kNativeTargetMeshColorDwords] = {};
    uint32_t uv_rows[kNativeTargetMeshSampleVertices][kNativeTargetMeshUvDwords] = {};
};
static NativeMeshSample g_native_mesh_samples[kNativeTargetMeshSampleCount];
static uint32_t g_native_mesh_sample_count = 0;

static bool EnvEnabled(const char* name) {
    const char* v = std::getenv(name);
    return v && v[0] && v[0] != '0';
}

static bool NativeSuppressionUnlocked() {
    return true;
}

// Aspect-ratio correction factor = gameAR / windowAR, multiplied into the
// entire c72 projection row (clip.x). Set in Install() from window_width/
// window_height. 1.0 = exactly 16:9 (no correction). For wider windows
// (ultrawide) it is <1 → Hor+ (wider FOV). For narrower windows (4:3, 16:10)
// it is >1 → narrower horizontal FOV. Combined with present_letterbox=false
// (uniform fill), this keeps geometry proportionally correct at ANY aspect
// ratio with no black bars and no stretching.
static float g_ar_scale = 1.0f;

static bool RendererDiagnosticsEnabled() {
    return EnvEnabled("DAYTONA_RENDERER_DIAGNOSTICS") ||
           EnvEnabled("DAYTONA_NATIVE_ALL") ||
           EnvEnabled("DAYTONA_NATIVE_SHADOW_QUADLIST") ||
           EnvEnabled("DAYTONA_NATIVE_SHADOW_POINTLIST") ||
           EnvEnabled("DAYTONA_NATIVE_SHADOW_MESH");
}

static bool IsPowerOfTwo(uint64_t v) {
    return v && ((v & (v - 1)) == 0);
}

static uint32_t ReadPhysU32(rex::graphics::CommandProcessor* cp, uint32_t phys) {
    if (cp) {
        return cp->DaytonaReadPhysicalU32(phys);
    }
    return daytona_render::GuestReadU32(0xA0000000u | (phys & 0x1FFFFFFCu));
}

static float FloatFromU32(uint32_t bits) {
    float f = 0.0f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

static void WriteJsonFloat(std::FILE* f, float value) {
    if (std::isfinite(value)) {
        // Format under the C locale so JSON always uses '.' as the
        // decimal separator, regardless of the system LC_NUMERIC.
        char buf[64];
        static locale_t c_locale = newlocale(LC_ALL_MASK, "C", (locale_t)0);
        locale_t prev = uselocale(c_locale);
        std::snprintf(buf, sizeof(buf), "%.9g", value);
        uselocale(prev);
        std::fprintf(f, "%s", buf);
    } else {
        std::fprintf(f, "null");
    }
}

static void WriteJsonFloat3(std::FILE* f, float x, float y, float z) {
    std::fprintf(f, "[");
    WriteJsonFloat(f, x);
    std::fprintf(f, ",");
    WriteJsonFloat(f, y);
    std::fprintf(f, ",");
    WriteJsonFloat(f, z);
    std::fprintf(f, "]");
}

static void WriteJsonFloat2(std::FILE* f, float x, float y) {
    std::fprintf(f, "[");
    WriteJsonFloat(f, x);
    std::fprintf(f, ",");
    WriteJsonFloat(f, y);
    std::fprintf(f, "]");
}

static void WriteDecodedQuadListVertexJson(std::FILE* f, const uint32_t* row) {
    const uint32_t packed = row[3];
    std::fprintf(f, "{\"position\":");
    WriteJsonFloat3(f, FloatFromU32(row[0]), FloatFromU32(row[1]), FloatFromU32(row[2]));
    std::fprintf(f,
                 ",\"packed_dword3\":\"0x%08X\","
                 "\"packed_dword3_rgba8\":[%u,%u,%u,%u],"
                 "\"scalar_x\":",
                 packed,
                 packed & 0xFFu, (packed >> 8) & 0xFFu,
                 (packed >> 16) & 0xFFu, (packed >> 24) & 0xFFu);
    WriteJsonFloat(f, FloatFromU32(row[4]));
    std::fprintf(f, ",\"uv0\":");
    WriteJsonFloat2(f, FloatFromU32(row[5]), FloatFromU32(row[6]));
    std::fprintf(f, ",\"uv1\":");
    WriteJsonFloat2(f, FloatFromU32(row[7]), FloatFromU32(row[8]));
    std::fprintf(f, "}");
}

static std::array<float, 4> ReadVsFloatConstant(const rex::graphics::RegisterFile& regs,
                                                uint32_t index) {
    std::array<float, 4> value = {};
    const auto base_and_size =
        regs.Get<reg::SQ_VS_CONST>(rex::graphics::XE_GPU_REG_SQ_VS_CONST);
    if (index > base_and_size.size) {
        return value;
    }
    const uint32_t absolute_index = base_and_size.base + index;
    if (absolute_index >= 512) {
        return value;
    }
    std::memcpy(value.data(),
                &regs[rex::graphics::XE_GPU_REG_SHADER_CONSTANT_000_X + 4 * absolute_index],
                sizeof(float) * 4);
    return value;
}

static std::array<float, 4> ReadPsFloatConstant(const rex::graphics::RegisterFile& regs,
                                                uint32_t index) {
    std::array<float, 4> value = {};
    const auto base_and_size =
        regs.Get<reg::SQ_PS_CONST>(rex::graphics::XE_GPU_REG_SQ_PS_CONST);
    if (index > base_and_size.size) {
        return value;
    }
    const uint32_t absolute_index = base_and_size.base + index;
    if (absolute_index >= 512) {
        return value;
    }
    std::memcpy(value.data(),
                &regs[rex::graphics::XE_GPU_REG_SHADER_CONSTANT_000_X + 4 * absolute_index],
                sizeof(float) * 4);
    return value;
}

static uint32_t ReadPhysicalIndex(rex::graphics::CommandProcessor* cp, uint32_t addr,
                                  xenos::IndexFormat format, xenos::Endian endianness) {
    if (format == xenos::IndexFormat::kInt16) {
        const uint32_t word = ReadPhysU32(cp, addr & ~uint32_t(3));
        const uint32_t shift = (addr & 2u) ? 0u : 16u;
        const uint16_t raw = uint16_t((word >> shift) & 0xFFFFu);
        return xenos::GpuSwap(raw, endianness);
    }
    const uint32_t raw = ReadPhysU32(cp, addr & ~uint32_t(3));
    return xenos::GpuSwap(raw, endianness);
}

static void WriteDecodedMeshVertexJson(std::FILE* f,
                                       const uint32_t* pos_row,
                                       const uint32_t* col_row,
                                       const uint32_t* uv_row,
                                       const uint32_t c72_c75[4][4]) {
    const float x = FloatFromU32(pos_row[0]);
    const float y = FloatFromU32(pos_row[1]);
    const float z = FloatFromU32(pos_row[2]);
    const uint32_t packed = col_row[0];
    const float u = FloatFromU32(uv_row[0]);
    const float v = FloatFromU32(uv_row[1]);
    float m[16] = {};
    for (uint32_t row = 0; row < 4; ++row) {
        for (uint32_t col = 0; col < 4; ++col) {
            m[row * 4 + col] = FloatFromU32(c72_c75[row][col]);
        }
    }
    float r2[4] = {m[12], m[15], m[14], m[13]};        // c75.xwzy
    r2[0] = z * m[8]  + r2[0];                         // c74.x
    r2[1] = z * m[11] + r2[1];                         // c74.w
    r2[2] = z * m[10] + r2[2];                         // c74.z
    r2[3] = z * m[9]  + r2[3];                         // c74.y
    const float prev[4] = {r2[0], r2[2], r2[3], r2[1]}; // r2.xzwy
    r2[0] = y * m[4] + prev[0];                         // c73.x
    r2[1] = y * m[6] + prev[1];                         // c73.z
    r2[2] = y * m[5] + prev[2];                         // c73.y
    r2[3] = y * m[7] + prev[3];                         // c73.w
    const float prev2[4] = {r2[0], r2[2], r2[3], r2[1]}; // r2.xzyw
    const float clip[4] = {
        x * m[0] + prev2[0],
        x * m[1] + prev2[1],
        x * m[2] + prev2[2],
        x * m[3] + prev2[3],
    };
    const float w = clip[3] != 0.0f ? clip[3] : 1.0f;

    std::fprintf(f, "{\"position\":");
    WriteJsonFloat3(f, x, y, z);
    std::fprintf(f, ",\"packed_color\":\"0x%08X\",\"rgba8\":[%u,%u,%u,%u],\"uv\":",
                 packed, packed & 0xFFu, (packed >> 8) & 0xFFu,
                 (packed >> 16) & 0xFFu, (packed >> 24) & 0xFFu);
    WriteJsonFloat2(f, u, v);
    std::fprintf(f, ",\"clip\":");
    std::fprintf(f, "[");
    for (uint32_t i = 0; i < 4; ++i) {
        if (i) std::fprintf(f, ",");
        WriteJsonFloat(f, clip[i]);
    }
    std::fprintf(f, "],\"ndc\":");
    WriteJsonFloat3(f, clip[0] / w, clip[1] / w, clip[2] / w);
    std::fprintf(f, "}");
}

static uint64_t HashMix(uint64_t h, uint64_t v) {
    h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
    return h;
}

template <typename TextureFetchT>
static bool IsTextureFetchActive(const TextureFetchT& f) {
    return f.type == xenos::FetchConstantType::kTexture && f.base_address != 0;
}

template <typename VertexFetchT>
static bool IsVertexFetchActive(const VertexFetchT& f) {
    return f.type == xenos::FetchConstantType::kVertex && f.address != 0;
}

struct RawRegDef {
    const char* name;
    uint32_t index;
};

const char* PrimName(uint8_t p);

static const RawRegDef kRawRegs[DrawRecord::kRawRegCount] = {
    {"SQ_PROGRAM_CNTL", reg::SQ_PROGRAM_CNTL::register_index},
    {"SQ_CONTEXT_MISC", reg::SQ_CONTEXT_MISC::register_index},
    {"VGT_DMA_SIZE", reg::VGT_DMA_SIZE::register_index},
    {"VGT_DRAW_INITIATOR", reg::VGT_DRAW_INITIATOR::register_index},
    {"VGT_MULTI_PRIM_IB_RESET_INDX", reg::VGT_MULTI_PRIM_IB_RESET_INDX::register_index},
    {"VGT_INDX_OFFSET", reg::VGT_INDX_OFFSET::register_index},
    {"VGT_MIN_VTX_INDX", reg::VGT_MIN_VTX_INDX::register_index},
    {"VGT_MAX_VTX_INDX", reg::VGT_MAX_VTX_INDX::register_index},
    {"VGT_OUTPUT_PATH_CNTL", reg::VGT_OUTPUT_PATH_CNTL::register_index},
    {"VGT_HOS_CNTL", reg::VGT_HOS_CNTL::register_index},
    {"PA_SU_SC_MODE_CNTL", reg::PA_SU_SC_MODE_CNTL::register_index},
    {"PA_SC_MPASS_PS_CNTL", reg::PA_SC_MPASS_PS_CNTL::register_index},
    {"PA_SC_VIZ_QUERY", reg::PA_SC_VIZ_QUERY::register_index},
    {"PA_CL_CLIP_CNTL", reg::PA_CL_CLIP_CNTL::register_index},
    {"PA_CL_VTE_CNTL", reg::PA_CL_VTE_CNTL::register_index},
    {"PA_SC_SCREEN_SCISSOR_TL", reg::PA_SC_SCREEN_SCISSOR_TL::register_index},
    {"PA_SC_SCREEN_SCISSOR_BR", reg::PA_SC_SCREEN_SCISSOR_BR::register_index},
    {"PA_SC_WINDOW_OFFSET", reg::PA_SC_WINDOW_OFFSET::register_index},
    {"PA_SC_WINDOW_SCISSOR_TL", reg::PA_SC_WINDOW_SCISSOR_TL::register_index},
    {"PA_SC_WINDOW_SCISSOR_BR", reg::PA_SC_WINDOW_SCISSOR_BR::register_index},
    {"RB_MODECONTROL", reg::RB_MODECONTROL::register_index},
    {"RB_SURFACE_INFO", reg::RB_SURFACE_INFO::register_index},
    {"RB_COLORCONTROL", reg::RB_COLORCONTROL::register_index},
    {"RB_COLOR_INFO", reg::RB_COLOR_INFO::register_index},
    {"RB_COLOR_MASK", reg::RB_COLOR_MASK::register_index},
    {"RB_BLENDCONTROL0", reg::RB_BLENDCONTROL::rt_register_indices[0]},
    {"RB_BLENDCONTROL1", reg::RB_BLENDCONTROL::rt_register_indices[1]},
    {"RB_BLENDCONTROL2", reg::RB_BLENDCONTROL::rt_register_indices[2]},
    {"RB_BLENDCONTROL3", reg::RB_BLENDCONTROL::rt_register_indices[3]},
    {"RB_DEPTHCONTROL", reg::RB_DEPTHCONTROL::register_index},
    {"RB_STENCILREFMASK", reg::RB_STENCILREFMASK::register_index},
    {"RB_DEPTH_INFO", reg::RB_DEPTH_INFO::register_index},
};

static void CopyPath(char* dst, size_t dst_size, const std::filesystem::path& path) {
    if (!dst_size) return;
    const std::string s = path.string();
    std::strncpy(dst, s.c_str(), dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static void CaptureShaderInfo(DrawRecord::ShaderInfo& out, rex::graphics::Shader* shader) {
    if (!shader) return;
    out.valid = true;
    out.hash = shader->ucode_data_hash();
    out.type = static_cast<uint32_t>(shader->type());
    out.ucode_dwords = static_cast<uint32_t>(shader->ucode_dword_count());
    out.ucode_storage_index = shader->ucode_storage_index();
    out.analyzed = shader->is_ucode_analyzed() ? 1u : 0u;
    out.texture_binding_count = static_cast<uint32_t>(shader->texture_bindings().size());
    out.vertex_binding_count = static_cast<uint32_t>(shader->vertex_bindings().size());
    out.memexport_written_mask = shader->memexport_eM_written();
    out.writes_color_targets = shader->writes_color_targets();
    out.writes_interpolators = shader->writes_interpolators();
    out.writes_depth = shader->writes_depth() ? 1u : 0u;
    out.kills_pixels = shader->kills_pixels() ? 1u : 0u;

    bool dumped = false;
    for (uint32_t i = 0; i < g_dumped_shader_count; ++i) {
        if (g_dumped_shader_hashes[i] == out.hash) {
            dumped = true;
            break;
        }
    }
    if (!dumped && g_dumped_shader_count < 64) {
        const auto paths = shader->DumpUcode("logs/daytona_shaders");
        CopyPath(out.ucode_path, sizeof(out.ucode_path), paths.first);
        CopyPath(out.disasm_path, sizeof(out.disasm_path), paths.second);
        g_dumped_shader_hashes[g_dumped_shader_count++] = out.hash;
    } else {
        const char* type_extension = shader->type() == xenos::ShaderType::kVertex ? "vert" : "frag";
        std::snprintf(out.ucode_path, sizeof(out.ucode_path),
                      "logs/daytona_shaders/shader_%016llX.ucode.bin.%s",
                      (unsigned long long)out.hash, type_extension);
        if (shader->is_ucode_analyzed()) {
            std::snprintf(out.disasm_path, sizeof(out.disasm_path),
                          "logs/daytona_shaders/shader_%016llX.ucode.%s",
                          (unsigned long long)out.hash, type_extension);
        }
    }
}
}  // namespace

// ── DrawSig construction ──────────────────────────────────────────────────────

DrawSig DaytonaRenderer::BuildSig(const rex::graphics::RegisterFile& regs,
                                   rex::graphics::Shader* vs,
                                   rex::graphics::Shader* ps,
                                   xenos::PrimitiveType prim_type,
                                   uint32_t index_count) {
    DrawSig sig;
    sig.vs_hash   = vs ? vs->ucode_data_hash() : 0;
    sig.ps_hash   = ps ? ps->ucode_data_hash() : 0;
    sig.prim_type = static_cast<uint8_t>(prim_type);
    sig.index_count = index_count;

    // Texture fetch slots 0..31.  Include the descriptor identity in the
    // signature; a mask alone collapses unrelated draws that merely use the
    // same slots.
    uint64_t tex_hash = 0xDAD70A7E00000001ull;
    for (uint32_t i = 0; i < 32; ++i) {
        const auto f = regs.GetTextureFetch(i);
        if (IsTextureFetchActive(f)) {
            sig.tex_mask |= (1u << i);
            tex_hash = HashMix(tex_hash, i);
            tex_hash = HashMix(tex_hash, uint32_t(f.base_address));
            tex_hash = HashMix(tex_hash, uint32_t(f.mip_address));
            tex_hash = HashMix(tex_hash, static_cast<uint32_t>(f.format));
            tex_hash = HashMix(tex_hash, static_cast<uint32_t>(f.dimension));
            tex_hash = HashMix(tex_hash, static_cast<uint32_t>(f.tiled));
            tex_hash = HashMix(tex_hash, static_cast<uint32_t>(f.endianness));
            tex_hash = HashMix(tex_hash, uint32_t(f.size_1d.width));
            tex_hash = HashMix(tex_hash, uint32_t(f.size_2d.width));
            tex_hash = HashMix(tex_hash, uint32_t(f.size_2d.height));
            tex_hash = HashMix(tex_hash, uint32_t(f.size_2d.stack_depth));
            tex_hash = HashMix(tex_hash, uint32_t(f.size_3d.depth));
        }
    }
    sig.tex_desc_hash = tex_hash;

    // Vertex fetch slots 0..95.
    // Do NOT include f.address here. Daytona streams many tiny vertex buffers
    // through changing guest addresses; address-sensitive signatures explode into
    // thousands of one-off records and obscure the stable pipeline shapes.
    // The first observed address/size is still written in DrawRecord for auditing.
    uint64_t vtx_hash = 0xDAD70A7E00000002ull;
    for (uint32_t i = 0; i < 96; ++i) {
        const auto f = regs.GetVertexFetch(i);
        if (IsVertexFetchActive(f)) {
            if (i < 32) sig.vtx_mask |= (1u << i);
            else if (i < 64) sig.vtx_mask_mid |= (1u << (i - 32));
            else sig.vtx_mask_hi |= (1u << (i - 64));
            vtx_hash = HashMix(vtx_hash, i);
            vtx_hash = HashMix(vtx_hash, static_cast<uint32_t>(f.endian));
        }
    }
    sig.vtx_desc_hash = vtx_hash;

    // Render target color/depth state.
    {
        const auto color = regs.Get<reg::RB_COLOR_INFO>();
        const auto surf  = regs.Get<reg::RB_SURFACE_INFO>();
        const auto depth = regs.Get<reg::RB_DEPTH_INFO>();
        sig.rt_color_format = static_cast<uint8_t>(color.color_format);
        sig.rt_depth_format = static_cast<uint8_t>(depth.depth_format);
        sig.rt_edram_base = color.color_base | (color.color_base_bit_11 << 11);
        sig.depth_edram_base = depth.depth_base | (depth.depth_base_bit_11 << 11);
        sig.rt_surface_pitch = surf.surface_pitch;
    }

    return sig;
}

bool DaytonaRenderer::IsRecommendedNativeQuadListTarget(const DrawSig& sig) {
    // vtx_mask_hi is omitted: it varies draw-to-draw within this class (0x90800000 vs 0x90C00000).
    // VS+PS+prim+tex/vtx masks identify the class; index_count is batched in groups of quads.
    // rt_color_format == 0: pipeline uses VK_FORMAT_R8G8B8A8_UNORM (k_8_8_8_8 only).
    // Depth is disabled explicitly in the native issue path (zeroed normalized_depth_control),
    // so we do not gate on rt_depth_format here.
    return sig.prim_type == static_cast<uint8_t>(xenos::PrimitiveType::kQuadList) &&
           sig.vs_hash == kNativeTargetQuadListVs &&
           (sig.ps_hash == kNativeTargetQuadListPs ||
            sig.ps_hash == kNativeTargetQuadListPs2) &&
           sig.tex_mask == kNativeTargetQuadListTexMask &&
           sig.vtx_mask == kNativeTargetQuadListVtxMaskLo &&
           sig.vtx_mask_mid == kNativeTargetQuadListVtxMaskMid &&
           (sig.index_count % 4u) == 0 &&
           sig.rt_color_format == 0 &&
           sig.rt_surface_pitch == kNativeTargetQuadListRtPitch;
}

bool DaytonaRenderer::IsRecommendedNativePointListTarget(const DrawSig& sig) {
    return sig.prim_type == static_cast<uint8_t>(xenos::PrimitiveType::kPointList) &&
           sig.vs_hash == kNativeTargetPointListVs &&
           sig.ps_hash == kNativeTargetPointListPs &&
           sig.index_count == 1 &&
           sig.rt_color_format == 0 &&
           sig.rt_surface_pitch == kNativeTargetQuadListRtPitch;
}

bool DaytonaRenderer::IsRecommendedNativeMeshTarget(const DrawSig& sig) {
    // vtx_mask_hi omitted: varies draw-to-draw within this class.
    // rt_color_format == 0: pipeline uses VK_FORMAT_R8G8B8A8_UNORM (k_8_8_8_8 only).
    // Depth is disabled explicitly in the native issue path (zeroed normalized_depth_control).
    return sig.prim_type == static_cast<uint8_t>(xenos::PrimitiveType::kTriangleStrip) &&
           sig.vs_hash == kNativeTargetMeshVs &&
           sig.ps_hash == kNativeTargetMeshPs &&
           (sig.tex_mask & kNativeTargetMeshTexMask) != 0 &&
           (sig.vtx_mask & kNativeTargetMeshVtxMaskLo) == kNativeTargetMeshVtxMaskLo &&
           sig.rt_color_format == 0 &&
           sig.rt_surface_pitch == kNativeTargetQuadListRtPitch;
}

bool DaytonaRenderer::IsRecommendedNativeMeshFanTarget(const DrawSig& sig) {
    // VS FA44706CA015263D: same vf0/vf3/vf8 + MVP c72-c75 + mesh PS, but tighter
    // per-stream strides (3/1/2 dwords) and QuadList prim topology. DaytonaNativeIssueMesh
    // reads strides from vertex_bindings() so no separate issue path is needed.
    return sig.prim_type == static_cast<uint8_t>(xenos::PrimitiveType::kQuadList) &&
           sig.vs_hash == kNativeTargetMeshFanVs &&
           sig.ps_hash == kNativeTargetMeshPs &&
           (sig.tex_mask & kNativeTargetMeshTexMask) != 0 &&
           (sig.vtx_mask & kNativeTargetMeshVtxMaskLo) == kNativeTargetMeshVtxMaskLo &&
           sig.rt_color_format == 0 &&
           sig.rt_surface_pitch == kNativeTargetQuadListRtPitch;
}

void DaytonaRenderer::CaptureRecommendedQuadListSample(
    rex::graphics::CommandProcessor* cp, const rex::graphics::RegisterFile& regs,
    rex::graphics::Shader* vs, const DrawSig& sig) {
    bool duplicate_multi_sample = false;
    for (uint32_t i = 0; i < g_native_quadlist_sample_count; ++i) {
        const DrawSig& existing = g_native_quadlist_samples[i].sig;
        if (existing.vs_hash == sig.vs_hash &&
            existing.ps_hash == sig.ps_hash &&
            existing.prim_type == sig.prim_type &&
            existing.index_count == sig.index_count &&
            existing.tex_mask == sig.tex_mask &&
            existing.vtx_mask == sig.vtx_mask &&
            existing.vtx_mask_mid == sig.vtx_mask_mid &&
            existing.vtx_mask_hi == sig.vtx_mask_hi &&
            existing.rt_surface_pitch == sig.rt_surface_pitch &&
            existing.rt_color_format == sig.rt_color_format &&
            existing.rt_depth_format == sig.rt_depth_format) {
            duplicate_multi_sample = true;
            break;
        }
    }
    const bool capture_primary = !g_native_quadlist_sample_valid;
    const bool capture_multi =
        !duplicate_multi_sample &&
        g_native_quadlist_sample_count < kNativeTargetQuadListSampleCount;
    if (!capture_primary && !capture_multi) return;

    NativeQuadListSample sample;
    bool any_nonzero = false;
    sample.sig = sig;
    sample.draw_number = g_total_draws;
    sample.index_offset = regs[reg::VGT_INDX_OFFSET::register_index];
    sample.draw_initiator = regs[reg::VGT_DRAW_INITIATOR::register_index];
    for (uint32_t i = 0; i < DrawRecord::kRawRegCount; ++i) {
        sample.raw_regs[i] = regs[kRawRegs[i].index];
    }
    for (uint32_t row = 0; row < 4; ++row) {
        const auto c = ReadVsFloatConstant(regs, 72 + row);
        std::memcpy(sample.c72_c75[row], c.data(), sizeof(float) * 4);
    }
    const auto c0 = ReadVsFloatConstant(regs, 0);
    const auto c255 = ReadPsFloatConstant(regs, 255);
    std::memcpy(sample.c0, c0.data(), sizeof(sample.c0));
    std::memcpy(sample.c255, c255.data(), sizeof(sample.c255));
    for (uint32_t ti = 0; ti < 2; ++ti) {
        const auto tex = regs.GetTextureFetch(ti);
        if (!IsTextureFetchActive(tex)) continue;
        sample.texture_base[ti] = uint32_t(tex.base_address) << 12;
        sample.texture_mip[ti] = uint32_t(tex.mip_address) << 12;
        sample.texture_format[ti] = static_cast<uint32_t>(tex.format);
        sample.texture_tiled[ti] = tex.tiled;
        sample.texture_endian[ti] = static_cast<uint32_t>(tex.endianness);
        if (tex.dimension == xenos::DataDimension::k2DOrStacked ||
            tex.dimension == xenos::DataDimension::k3D) {
            sample.texture_width[ti] = tex.size_2d.width + 1;
            sample.texture_height[ti] = tex.size_2d.height + 1;
        }
    }

    if (vs) {
        const auto& bindings = vs->vertex_bindings();
        sample.shader_binding_count =
            static_cast<uint32_t>(bindings.size() < 8 ? bindings.size() : 8);
        for (uint32_t i = 0; i < sample.shader_binding_count; ++i) {
            sample.shader_binding_fetch_constants[i] = bindings[i].fetch_constant;
            sample.shader_binding_stride_words[i] = bindings[i].stride_words;
            sample.shader_binding_attribute_counts[i] =
                static_cast<uint32_t>(bindings[i].attributes.size());
            const auto binding_fetch = regs.GetVertexFetch(bindings[i].fetch_constant);
            sample.shader_binding_base_phys[i] = uint32_t(binding_fetch.address) << 2;
            sample.shader_binding_size_bytes[i] = uint32_t(binding_fetch.size) << 2;
            for (uint32_t vi = 0; vi < kNativeTargetQuadListSampleVertices; ++vi) {
                const uint32_t row_phys =
                    sample.shader_binding_base_phys[i] + vi * bindings[i].stride_words * 4u;
                for (uint32_t wi = 0; wi < kNativeTargetQuadListSampleDwords; ++wi) {
                    sample.shader_binding_rows[i][vi][wi] =
                        ReadPhysU32(cp, row_phys + wi * 4u);
                    any_nonzero = any_nonzero || sample.shader_binding_rows[i][vi][wi] != 0;
                }
            }
        }
    }

    const uint32_t slots[3] = {1, 2, 4};
    for (uint32_t si = 0; si < 3; ++si) {
        const uint32_t slot = slots[si];
        const auto fetch = regs.GetVertexFetch(slot);
        sample.raw_vfetch_slots[si] = slot;
        sample.raw_vfetch_address_dwords[si] = uint32_t(fetch.address);
        sample.raw_vfetch_bases[si] = uint32_t(fetch.address) << 2;
        sample.raw_vfetch_sizes[si] = uint32_t(fetch.size) << 2;

        const uint32_t base_phys = sample.raw_vfetch_bases[si];
        const uint32_t unshifted_phys = sample.raw_vfetch_address_dwords[si];
        const uint32_t indexed_phys =
            base_phys + sample.index_offset * kNativeTargetQuadListShaderStrideDwords * 4u;
        for (uint32_t vi = 0; vi < kNativeTargetQuadListSampleVertices; ++vi) {
            const uint32_t base_row =
                base_phys + vi * kNativeTargetQuadListShaderStrideDwords * 4u;
            const uint32_t indexed_row =
                indexed_phys + vi * kNativeTargetQuadListShaderStrideDwords * 4u;
            const uint32_t unshifted_row =
                unshifted_phys + vi * kNativeTargetQuadListShaderStrideDwords * 4u;
            for (uint32_t wi = 0; wi < kNativeTargetQuadListSampleDwords; ++wi) {
                sample.base_rows[si][vi][wi] = ReadPhysU32(cp, base_row + wi * 4u);
                sample.indexed_rows[si][vi][wi] = ReadPhysU32(cp, indexed_row + wi * 4u);
                sample.unshifted_rows[si][vi][wi] = ReadPhysU32(cp, unshifted_row + wi * 4u);
                any_nonzero = any_nonzero ||
                              sample.base_rows[si][vi][wi] != 0 ||
                              sample.indexed_rows[si][vi][wi] != 0 ||
                              sample.unshifted_rows[si][vi][wi] != 0;
            }
        }
    }

    for (uint32_t slot = 0; slot < 96 && sample.scan_count < kNativeTargetQuadListScanSlots; ++slot) {
        const auto fetch = regs.GetVertexFetch(slot);
        if (!IsVertexFetchActive(fetch)) continue;
        uint32_t shifted[4] = {};
        bool shifted_nonzero = false;
        const uint32_t base_phys = uint32_t(fetch.address) << 2;
        for (uint32_t wi = 0; wi < 4; ++wi) {
            shifted[wi] = ReadPhysU32(cp, base_phys + wi * 4u);
            shifted_nonzero = shifted_nonzero || shifted[wi] != 0;
        }
        if (!shifted_nonzero) continue;
        const uint32_t si = sample.scan_count++;
        sample.scan_slots[si] = slot;
        sample.scan_address_dwords[si] = uint32_t(fetch.address);
        sample.scan_base_phys[si] = base_phys;
        sample.scan_size_bytes[si] = uint32_t(fetch.size) << 2;
        for (uint32_t wi = 0; wi < 4; ++wi)
            sample.scan_first_dwords[si][wi] = shifted[wi];
        for (uint32_t vi = 0; vi < kNativeTargetQuadListSampleVertices; ++vi) {
            const uint32_t row_phys =
                base_phys + vi * kNativeTargetQuadListShaderStrideDwords * 4u;
            for (uint32_t wi = 0; wi < kNativeTargetQuadListSampleDwords; ++wi) {
                sample.scan_rows[si][vi][wi] = ReadPhysU32(cp, row_phys + wi * 4u);
            }
        }
        any_nonzero = true;
    }

    if (!any_nonzero) {
        ++g_native_quadlist_zero_sample_skips;
        if (g_native_quadlist_zero_sample_skips <= 8 ||
            IsPowerOfTwo(g_native_quadlist_zero_sample_skips)) {
            REXLOG_ERROR("DaytonaRenderer: skipped zero QuadList target sample #{} draw={} index_offset={}",
                         g_native_quadlist_zero_sample_skips, sample.draw_number,
                         sample.index_offset);
        }
        return;
    }

    if (capture_primary) {
        g_native_quadlist_sample = sample;
        g_native_quadlist_sample_valid = true;
    }
    if (capture_multi) {
        g_native_quadlist_samples[g_native_quadlist_sample_count++] = sample;
    }
    REXLOG_ERROR("DaytonaRenderer: captured QuadList target sample draw={} index_offset={} stride_dw={}",
                 sample.draw_number, sample.index_offset, kNativeTargetQuadListShaderStrideDwords);
}

void DaytonaRenderer::CaptureRecommendedPointListSample(
    rex::graphics::CommandProcessor* cp, const rex::graphics::RegisterFile& regs,
    rex::graphics::Shader* vs, const DrawSig& sig) {
    if (g_native_pointlist_sample_valid) return;

    NativePointListSample sample;
    bool any_nonzero = false;
    sample.draw_number = g_total_draws;
    sample.index_offset = regs[reg::VGT_INDX_OFFSET::register_index];
    sample.draw_initiator = regs[reg::VGT_DRAW_INITIATOR::register_index];
    sample.index_count = sig.index_count;
    sample.tex_mask = sig.tex_mask;
    sample.vtx_mask = sig.vtx_mask;
    sample.vtx_mask_mid = sig.vtx_mask_mid;
    sample.vtx_mask_hi = sig.vtx_mask_hi;

    if (vs) {
        const auto& bindings = vs->vertex_bindings();
        sample.shader_binding_count =
            static_cast<uint32_t>(bindings.size() < 8 ? bindings.size() : 8);
        for (uint32_t i = 0; i < sample.shader_binding_count; ++i) {
            sample.shader_binding_fetch_constants[i] = bindings[i].fetch_constant;
            sample.shader_binding_stride_words[i] = bindings[i].stride_words;
            sample.shader_binding_attribute_counts[i] =
                static_cast<uint32_t>(bindings[i].attributes.size());
        }
    }

    for (uint32_t slot = 0; slot < 96 && sample.scan_count < kNativeTargetPointListScanSlots; ++slot) {
        const auto fetch = regs.GetVertexFetch(slot);
        if (!IsVertexFetchActive(fetch)) continue;
        const uint32_t si = sample.scan_count++;
        const uint32_t base_phys = uint32_t(fetch.address) << 2;
        sample.scan_slots[si] = slot;
        sample.scan_address_dwords[si] = uint32_t(fetch.address);
        sample.scan_base_phys[si] = base_phys;
        sample.scan_size_bytes[si] = uint32_t(fetch.size) << 2;
        sample.scan_endian[si] = static_cast<uint32_t>(fetch.endian);

        // No stride is known yet for this shader. Capture contiguous dwords at
        // the base and at VGT_INDX_OFFSET so the next native decoder can infer
        // whether the stream is a tiny constant row, a per-point buffer, or an
        // offset-indexed table.
        const uint32_t indexed_phys =
            base_phys + sample.index_offset * kNativeTargetPointListSampleDwords * 4u;
        for (uint32_t vi = 0; vi < kNativeTargetPointListSampleVertices; ++vi) {
            const uint32_t base_row =
                base_phys + vi * kNativeTargetPointListSampleDwords * 4u;
            const uint32_t indexed_row =
                indexed_phys + vi * kNativeTargetPointListSampleDwords * 4u;
            for (uint32_t wi = 0; wi < kNativeTargetPointListSampleDwords; ++wi) {
                sample.base_rows[si][vi][wi] = ReadPhysU32(cp, base_row + wi * 4u);
                sample.indexed_rows[si][vi][wi] = ReadPhysU32(cp, indexed_row + wi * 4u);
                any_nonzero = any_nonzero ||
                              sample.base_rows[si][vi][wi] != 0 ||
                              sample.indexed_rows[si][vi][wi] != 0;
            }
        }
    }

    if (!any_nonzero) {
        ++g_native_pointlist_zero_sample_skips;
        if (g_native_pointlist_zero_sample_skips <= 8 ||
            IsPowerOfTwo(g_native_pointlist_zero_sample_skips)) {
            REXLOG_ERROR("DaytonaRenderer: skipped zero PointList target sample #{} draw={} index_offset={} vtx={:08X}/{:08X}/{:08X}",
                         g_native_pointlist_zero_sample_skips, sample.draw_number,
                         sample.index_offset, sample.vtx_mask, sample.vtx_mask_mid,
                         sample.vtx_mask_hi);
        }
        return;
    }

    g_native_pointlist_sample = sample;
    g_native_pointlist_sample_valid = true;
    REXLOG_ERROR("DaytonaRenderer: captured PointList target sample draw={} index_offset={} scans={}",
                 sample.draw_number, sample.index_offset, sample.scan_count);
}

void DaytonaRenderer::CaptureRecommendedMeshSample(
    rex::graphics::CommandProcessor* cp, const rex::graphics::RegisterFile& regs,
    rex::graphics::Shader* vs, const DrawSig& sig,
    const rex::graphics::CommandProcessor::DaytonaIndexBufferInfo* index_buffer_info) {
    for (uint32_t i = 0; i < g_native_mesh_sample_count; ++i) {
        if (g_native_mesh_samples[i].valid && g_native_mesh_samples[i].sig == sig) {
            return;
        }
    }
    if (g_native_mesh_sample_count >= kNativeTargetMeshSampleCount) {
        return;
    }

    NativeMeshSample sample;
    sample.valid = true;
    sample.sig = sig;
    sample.draw_number = g_total_draws;
    sample.index_offset = regs[reg::VGT_INDX_OFFSET::register_index];
    sample.draw_initiator = regs[reg::VGT_DRAW_INITIATOR::register_index];
    for (uint32_t i = 0; i < DrawRecord::kRawRegCount; ++i) {
        sample.raw_regs[i] = regs[kRawRegs[i].index];
    }

    const auto tex0 = regs.GetTextureFetch(0);
    if (IsTextureFetchActive(tex0)) {
        sample.texture0_base = uint32_t(tex0.base_address) << 12;
        sample.texture0_mip = uint32_t(tex0.mip_address) << 12;
        sample.texture0_format = static_cast<uint32_t>(tex0.format);
        sample.texture0_tiled = tex0.tiled;
        sample.texture0_endian = static_cast<uint32_t>(tex0.endianness);
        if (tex0.dimension == xenos::DataDimension::k2DOrStacked ||
            tex0.dimension == xenos::DataDimension::kCube) {
            sample.texture0_width = tex0.size_2d.width + 1u;
            sample.texture0_height = tex0.size_2d.height + 1u;
        } else {
            sample.texture0_width = tex0.size_1d.width + 1u;
            sample.texture0_height = 1u;
        }
    }

    const rex::graphics::Shader::VertexBinding* pos_binding = nullptr;
    const rex::graphics::Shader::VertexBinding* col_binding = nullptr;
    const rex::graphics::Shader::VertexBinding* uv_binding = nullptr;
    if (vs) {
        const auto& bindings = vs->vertex_bindings();
        sample.shader_binding_count =
            static_cast<uint32_t>(bindings.size() < 8 ? bindings.size() : 8);
        for (uint32_t i = 0; i < sample.shader_binding_count; ++i) {
            sample.shader_binding_fetch_constants[i] = bindings[i].fetch_constant;
            sample.shader_binding_stride_words[i] = bindings[i].stride_words;
            sample.shader_binding_attribute_counts[i] =
                static_cast<uint32_t>(bindings[i].attributes.size());
        }
        for (const auto& binding : bindings) {
            if (binding.fetch_constant == 95 && !pos_binding) {
                pos_binding = &binding;
            } else if (binding.fetch_constant == 92 && !col_binding) {
                col_binding = &binding;
            } else if (binding.fetch_constant == 87 && !uv_binding) {
                uv_binding = &binding;
            }
        }
        if ((!pos_binding || !col_binding || !uv_binding) && bindings.size() >= 3) {
            pos_binding = &bindings[0];
            col_binding = &bindings[1];
            uv_binding = &bindings[2];
        }
    }
    if (!pos_binding || !col_binding || !uv_binding ||
        pos_binding->stride_words < kNativeTargetMeshPosDwords ||
        col_binding->stride_words < kNativeTargetMeshColorDwords ||
        uv_binding->stride_words < kNativeTargetMeshUvDwords) {
        return;
    }

    const auto pos_fetch = regs.GetVertexFetch(pos_binding->fetch_constant);
    const auto col_fetch = regs.GetVertexFetch(col_binding->fetch_constant);
    const auto uv_fetch = regs.GetVertexFetch(uv_binding->fetch_constant);
    if (!IsVertexFetchActive(pos_fetch) ||
        !IsVertexFetchActive(col_fetch) ||
        !IsVertexFetchActive(uv_fetch)) {
        return;
    }

    sample.pos_fetch_constant = pos_binding->fetch_constant;
    sample.col_fetch_constant = col_binding->fetch_constant;
    sample.uv_fetch_constant = uv_binding->fetch_constant;
    sample.pos_base_phys = uint32_t(pos_fetch.address) << 2;
    sample.col_base_phys = uint32_t(col_fetch.address) << 2;
    sample.uv_base_phys = uint32_t(uv_fetch.address) << 2;
    sample.pos_size_bytes = uint32_t(pos_fetch.size) << 2;
    sample.col_size_bytes = uint32_t(col_fetch.size) << 2;
    sample.uv_size_bytes = uint32_t(uv_fetch.size) << 2;
    sample.pos_stride_words = pos_binding->stride_words;
    sample.col_stride_words = col_binding->stride_words;
    sample.uv_stride_words = uv_binding->stride_words;
    sample.pos_endian = static_cast<uint32_t>(pos_fetch.endian);
    sample.col_endian = static_cast<uint32_t>(col_fetch.endian);
    sample.uv_endian = static_cast<uint32_t>(uv_fetch.endian);

    for (uint32_t row = 0; row < 4; ++row) {
        const auto c = ReadVsFloatConstant(regs, 72 + row);
        std::memcpy(sample.c72_c75[row], c.data(), sizeof(float) * 4);
    }

    sample.indexed = index_buffer_info && index_buffer_info->guest_base &&
                     index_buffer_info->length &&
                     (index_buffer_info->format == uint32_t(xenos::IndexFormat::kInt16) ||
                      index_buffer_info->format == uint32_t(xenos::IndexFormat::kInt32));
    if (sample.indexed) {
        sample.index_format = index_buffer_info->format;
        sample.index_endianness = index_buffer_info->endianness;
        sample.index_guest_base = index_buffer_info->guest_base;
        sample.index_length = index_buffer_info->length;
    }

    const uint32_t capture_vertices =
        sig.index_count < kNativeTargetMeshSampleVertices
            ? sig.index_count
            : kNativeTargetMeshSampleVertices;
    const uint32_t index_size =
        sample.index_format == uint32_t(xenos::IndexFormat::kInt16) ? 2u : 4u;
    const uint32_t index_capture =
        sig.index_count < kNativeTargetMeshIndexSampleCount
            ? sig.index_count
            : kNativeTargetMeshIndexSampleCount;
    if (sample.indexed) {
        const auto fmt = static_cast<xenos::IndexFormat>(sample.index_format);
        const auto endian = static_cast<xenos::Endian>(sample.index_endianness);
        for (uint32_t i = 0; i < index_capture; ++i) {
            sample.decoded_indices[i] =
                ReadPhysicalIndex(cp, sample.index_guest_base + i * index_size, fmt, endian);
        }
        sample.decoded_index_count = index_capture;
    }

    for (uint32_t i = 0; i < capture_vertices; ++i) {
        uint32_t vi = sample.index_offset + i;
        if (sample.indexed && i < sample.decoded_index_count) {
            vi = sample.index_offset + sample.decoded_indices[i];
        }
        const uint32_t pos_phys = sample.pos_base_phys + vi * sample.pos_stride_words * 4u;
        const uint32_t col_phys = sample.col_base_phys + vi * sample.col_stride_words * 4u;
        const uint32_t uv_phys = sample.uv_base_phys + vi * sample.uv_stride_words * 4u;
        for (uint32_t w = 0; w < kNativeTargetMeshPosDwords; ++w) {
            sample.pos_rows[i][w] = ReadPhysU32(cp, pos_phys + w * 4u);
        }
        for (uint32_t w = 0; w < kNativeTargetMeshColorDwords; ++w) {
            sample.col_rows[i][w] = ReadPhysU32(cp, col_phys + w * 4u);
        }
        for (uint32_t w = 0; w < kNativeTargetMeshUvDwords; ++w) {
            sample.uv_rows[i][w] = ReadPhysU32(cp, uv_phys + w * 4u);
        }
    }

    g_native_mesh_samples[g_native_mesh_sample_count++] = sample;
    REXLOG_ERROR("DaytonaRenderer: captured Mesh target sample #{} draw={} prim={} idx={} tex0={:08X} vfetch={}/{}/{} strides={}/{}/{}",
                 g_native_mesh_sample_count, sample.draw_number,
                 PrimName(sig.prim_type), sig.index_count, sample.texture0_base,
                 sample.pos_fetch_constant, sample.col_fetch_constant,
                 sample.uv_fetch_constant,
                 sample.pos_stride_words, sample.col_stride_words,
                 sample.uv_stride_words);
    WriteRecommendedMeshSamples();
}


// ── Full draw record population ───────────────────────────────────────────────

void DaytonaRenderer::PopulateRecord(DrawRecord& rec,
                                      const rex::graphics::RegisterFile& regs,
                                      rex::graphics::Shader* vs,
                                      rex::graphics::Shader* ps,
                                      uint32_t index_count,
                                      const rex::graphics::CommandProcessor::DaytonaIndexBufferInfo* index_buffer_info,
                                      bool major_mode_explicit) {
    rec.first_index_count = index_count;
    rec.major_mode_explicit = major_mode_explicit ? 1u : 0u;
    CaptureShaderInfo(rec.vs, vs);
    CaptureShaderInfo(rec.ps, ps);
    if (index_buffer_info) {
        rec.index.valid = true;
        rec.index.format = index_buffer_info->format;
        rec.index.endianness = index_buffer_info->endianness;
        rec.index.count = index_buffer_info->count;
        rec.index.guest_base = index_buffer_info->guest_base;
        rec.index.length = index_buffer_info->length;
    }

    // Texture fetch details.
    for (uint32_t i = 0; i < 32; ++i) {
        const auto f = regs.GetTextureFetch(i);
        rec.tex[i].active = IsTextureFetchActive(f);
        if (!rec.tex[i].active) continue;
        rec.tex[i].base_phys  = uint32_t(f.base_address) << 12;
        rec.tex[i].mip_phys   = uint32_t(f.mip_address)  << 12;
        rec.tex[i].format     = static_cast<uint8_t>(f.format);
        rec.tex[i].dimension  = static_cast<uint8_t>(f.dimension);
        rec.tex[i].tiled      = f.tiled;
        rec.tex[i].endian     = static_cast<uint8_t>(f.endianness);
        // Width/height are stored with 1 subtracted.
        if (f.dimension == xenos::DataDimension::k2DOrStacked ||
            f.dimension == xenos::DataDimension::kCube) {
            rec.tex[i].width  = static_cast<uint16_t>(f.size_2d.width  + 1u);
            rec.tex[i].height = static_cast<uint16_t>(f.size_2d.height + 1u);
        } else {
            rec.tex[i].width  = static_cast<uint16_t>(f.size_1d.width + 1u);
            rec.tex[i].height = 1;
        }
    }

    // Vertex fetch details.
    for (uint32_t i = 0; i < 96; ++i) {
        const auto f = regs.GetVertexFetch(i);
        rec.vtx[i].active = IsVertexFetchActive(f);
        if (!rec.vtx[i].active) continue;
        rec.vtx[i].base_phys  = uint32_t(f.address) << 2;  // address >> 2 in register
        rec.vtx[i].size_bytes = f.size << 2;                     // size in dwords → bytes
        rec.vtx[i].endian     = static_cast<uint8_t>(f.endian);
    }

    // Render target info.
    {
        const auto color = regs.Get<reg::RB_COLOR_INFO>();
        const auto surf  = regs.Get<reg::RB_SURFACE_INFO>();
        const auto depth = regs.Get<reg::RB_DEPTH_INFO>();
        rec.rt_edram_base   = color.color_base | (color.color_base_bit_11 << 11);
        rec.rt_surface_pitch = surf.surface_pitch;
        rec.depth_edram_base = depth.depth_base | (depth.depth_base_bit_11 << 11);
    }

    for (uint32_t i = 0; i < DrawRecord::kRawRegCount; ++i) {
        rec.raw_regs[i] = regs[kRawRegs[i].index];
    }
}

// ── JSON catalog writer ───────────────────────────────────────────────────────

namespace {
// Return name string for known TextureFormat values (keeps JSON human-readable).
const char* TexFmtName(uint8_t fmt) {
    switch (static_cast<xenos::TextureFormat>(fmt)) {
    case xenos::TextureFormat::k_8_8_8_8:        return "RGBA8";
    case xenos::TextureFormat::k_DXT1:            return "BC1";
    case xenos::TextureFormat::k_DXT2_3:          return "BC2";
    case xenos::TextureFormat::k_DXT4_5:          return "BC3";
    case xenos::TextureFormat::k_16_16:           return "RG16";
    case xenos::TextureFormat::k_8_8:             return "RG8";
    case xenos::TextureFormat::k_8:               return "R8";
    case xenos::TextureFormat::k_16_16_FLOAT:     return "RG16F";
    case xenos::TextureFormat::k_32_FLOAT:        return "R32F";
    case xenos::TextureFormat::k_32_32_FLOAT:     return "RG32F";
    case xenos::TextureFormat::k_32_32_32_32_FLOAT: return "RGBA32F";
    case xenos::TextureFormat::k_16_16_16_16_FLOAT: return "RGBA16F";
    case xenos::TextureFormat::k_2_10_10_10:      return "R10G10B10A2";
    case xenos::TextureFormat::k_10_11_11:        return "R11G11B10";
    case xenos::TextureFormat::k_11_11_10:        return "B10G11R11";
    default:                                      return "?";
    }
}

const char* PrimName(uint8_t p) {
    switch (static_cast<xenos::PrimitiveType>(p)) {
    case xenos::PrimitiveType::kNone:             return "None";
    case xenos::PrimitiveType::kPointList:        return "PointList";
    case xenos::PrimitiveType::kLineList:         return "LineList";
    case xenos::PrimitiveType::kLineStrip:        return "LineStrip";
    case xenos::PrimitiveType::kTriangleList:     return "TriangleList";
    case xenos::PrimitiveType::kTriangleFan:      return "TriangleFan";
    case xenos::PrimitiveType::kTriangleStrip:    return "TriangleStrip";
    case xenos::PrimitiveType::kRectangleList:    return "RectangleList";
    case xenos::PrimitiveType::kQuadList:         return "QuadList";
    default:                                      return "?";
    }
}
}  // namespace

void DaytonaRenderer::WriteCatalog() {
    (void)daytona_mkdir_compat("logs");
    std::FILE* f = std::fopen("logs/daytona_draw_catalog.json", "w");
    if (!f) return;

    std::fprintf(f, "{\n");
    std::fprintf(f, "  \"total_draws\": %llu,\n",
                 (unsigned long long)g_total_draws);
    std::fprintf(f, "  \"unique_sigs\": %u,\n", g_record_count);
    std::fprintf(f, "  \"max_sigs\": %u,\n", kMaxRecords);
    std::fprintf(f, "  \"overflow_draws\": %llu,\n",
                 (unsigned long long)g_overflow_draws);
    std::fprintf(f, "  \"capture_truncated\": %s,\n",
                 g_overflow_draws ? "true" : "false");
    std::fprintf(f, "  \"draws\": [\n");

    for (uint32_t ri = 0; ri < g_record_count; ++ri) {
        const DrawRecord& r = g_records[ri];
        std::fprintf(f, "    {\n");
        std::fprintf(f, "      \"vs_hash\": \"%016llx\",\n", (unsigned long long)r.sig.vs_hash);
        std::fprintf(f, "      \"ps_hash\": \"%016llx\",\n", (unsigned long long)r.sig.ps_hash);
        std::fprintf(f, "      \"prim\": \"%s\",\n", PrimName(r.sig.prim_type));
        std::fprintf(f, "      \"tex_mask\": \"0x%08X\",\n", r.sig.tex_mask);
        std::fprintf(f, "      \"vtx_mask\": \"0x%08X\",\n", r.sig.vtx_mask);
        std::fprintf(f, "      \"vtx_mask_mid\": \"0x%08X\",\n", r.sig.vtx_mask_mid);
        std::fprintf(f, "      \"vtx_mask_hi\": \"0x%08X\",\n", r.sig.vtx_mask_hi);
        std::fprintf(f, "      \"tex_desc_hash\": \"%016llx\",\n", (unsigned long long)r.sig.tex_desc_hash);
        std::fprintf(f, "      \"vtx_desc_hash\": \"%016llx\",\n", (unsigned long long)r.sig.vtx_desc_hash);
        std::fprintf(f, "      \"rt_color_fmt\": %u,\n", (unsigned)r.sig.rt_color_format);
        std::fprintf(f, "      \"rt_depth_fmt\": %u,\n", (unsigned)r.sig.rt_depth_format);
        std::fprintf(f, "      \"rt_edram_base\": %u,\n", r.rt_edram_base);
        std::fprintf(f, "      \"rt_pitch\": %u,\n", r.rt_surface_pitch);
        std::fprintf(f, "      \"draw_count\": %llu,\n", (unsigned long long)r.draw_count);
        std::fprintf(f, "      \"index_count\": %u,\n", r.first_index_count);

        // Active textures.
        std::fprintf(f, "      \"textures\": [");
        bool first = true;
        for (uint32_t i = 0; i < 32; ++i) {
            const auto& t = r.tex[i];
            if (!t.active) continue;
            if (!first) std::fprintf(f, ", ");
            first = false;
            std::fprintf(f, "{\"slot\":%u,\"base\":\"0x%08X\",\"mip\":\"0x%08X\","
                            "\"w\":%u,\"h\":%u,\"fmt\":\"%s\",\"dim\":%u,"
                            "\"tiled\":%u,\"endian\":%u}",
                         i, t.base_phys, t.mip_phys,
                         (unsigned)t.width, (unsigned)t.height,
                         TexFmtName(t.format), (unsigned)t.dimension,
                         (unsigned)t.tiled, (unsigned)t.endian);
        }
        std::fprintf(f, "],\n");

        // Active vertex fetches.
        std::fprintf(f, "      \"vertex_fetches\": [");
        first = true;
        for (uint32_t i = 0; i < 96; ++i) {
            const auto& v = r.vtx[i];
            if (!v.active) continue;
            if (!first) std::fprintf(f, ", ");
            first = false;
            std::fprintf(f, "{\"slot\":%u,\"base\":\"0x%08X\",\"bytes\":%u,\"endian\":%u}",
                         i, v.base_phys, v.size_bytes, (unsigned)v.endian);
        }
        std::fprintf(f, "]\n    }");
        if (ri + 1 < g_record_count) std::fprintf(f, ",");
        std::fprintf(f, "\n");
    }

    std::fprintf(f, "  ]\n}\n");
    std::fclose(f);

    REXLOG_ERROR("DaytonaRenderer: catalog written — {} unique sigs, {} total draws, {} overflow draws",
                g_record_count, g_total_draws, g_overflow_draws);
}

void DaytonaRenderer::WriteObjectInventory() {
    (void)daytona_mkdir_compat("logs");
    std::FILE* f = std::fopen("logs/daytona_renderer_object_inventory.json", "w");
    if (!f) return;

    std::fprintf(f, "{\n");
    std::fprintf(f, "  \"phase\": \"1.3\",\n");
    std::fprintf(f, "  \"note\": \"Representative raw IssueDraw state; hook remains passive and ReXGlue still renders.\",\n");
    std::fprintf(f, "  \"total_draws\": %llu,\n", (unsigned long long)g_total_draws);
    std::fprintf(f, "  \"unique_sigs\": %u,\n", g_record_count);
    std::fprintf(f, "  \"max_sigs\": %u,\n", kMaxRecords);
    std::fprintf(f, "  \"overflow_draws\": %llu,\n", (unsigned long long)g_overflow_draws);

    std::fprintf(f, "  \"raw_registers\": [");
    for (uint32_t i = 0; i < DrawRecord::kRawRegCount; ++i) {
        if (i) std::fprintf(f, ", ");
        std::fprintf(f, "{\"name\":\"%s\",\"index\":\"0x%04X\"}",
                     kRawRegs[i].name, kRawRegs[i].index);
    }
    std::fprintf(f, "],\n");

    struct ShaderPairAgg {
        uint64_t vs = 0;
        uint64_t ps = 0;
        uint64_t draw_count = 0;
        uint32_t sig_count = 0;
        uint32_t representative = 0;
        bool used = false;
    };
    ShaderPairAgg pairs[64]{};
    uint32_t pair_count = 0;
    for (uint32_t ri = 0; ri < g_record_count; ++ri) {
        const DrawRecord& r = g_records[ri];
        ShaderPairAgg* pair = nullptr;
        for (uint32_t pi = 0; pi < pair_count; ++pi) {
            if (pairs[pi].vs == r.sig.vs_hash && pairs[pi].ps == r.sig.ps_hash) {
                pair = &pairs[pi];
                break;
            }
        }
        if (!pair && pair_count < 64) {
            pair = &pairs[pair_count++];
            pair->vs = r.sig.vs_hash;
            pair->ps = r.sig.ps_hash;
            pair->representative = ri;
            pair->used = true;
        }
        if (!pair) continue;
        ++pair->sig_count;
        pair->draw_count += r.draw_count;
        if (r.draw_count > g_records[pair->representative].draw_count)
            pair->representative = ri;
    }

    std::fprintf(f, "  \"shader_pairs\": [\n");
    for (uint32_t pi = 0; pi < pair_count; ++pi) {
        const ShaderPairAgg& p = pairs[pi];
        const DrawRecord& rep = g_records[p.representative];
        std::fprintf(f, "    {\"vs_hash\":\"%016llx\",\"ps_hash\":\"%016llx\","
                        "\"draw_count\":%llu,\"signature_count\":%u,"
                        "\"representative_signature\":%u,\"representative_prim\":\"%s\","
                        "\"representative_index_count\":%u,\"representative_textures\":%u,"
                        "\"representative_vertex_fetches\":%u}",
                     (unsigned long long)p.vs, (unsigned long long)p.ps,
                     (unsigned long long)p.draw_count, p.sig_count,
                     p.representative, PrimName(rep.sig.prim_type), rep.first_index_count,
                     __builtin_popcount(rep.sig.tex_mask),
                     __builtin_popcount(rep.sig.vtx_mask) +
                     __builtin_popcount(rep.sig.vtx_mask_mid) +
                     __builtin_popcount(rep.sig.vtx_mask_hi));
        if (pi + 1 < pair_count) std::fprintf(f, ",");
        std::fprintf(f, "\n");
    }
    std::fprintf(f, "  ],\n");

    std::fprintf(f, "  \"draw_signatures\": [\n");
    for (uint32_t ri = 0; ri < g_record_count; ++ri) {
        const DrawRecord& r = g_records[ri];
        std::fprintf(f, "    {\n");
        std::fprintf(f, "      \"signature_index\": %u,\n", ri);
        std::fprintf(f, "      \"draw_count\": %llu,\n", (unsigned long long)r.draw_count);
        std::fprintf(f, "      \"vs_hash\": \"%016llx\",\n", (unsigned long long)r.sig.vs_hash);
        std::fprintf(f, "      \"ps_hash\": \"%016llx\",\n", (unsigned long long)r.sig.ps_hash);
        std::fprintf(f, "      \"prim\": \"%s\",\n", PrimName(r.sig.prim_type));
        std::fprintf(f, "      \"index_count\": %u,\n", r.first_index_count);
        std::fprintf(f, "      \"shaders\": {\"vs\":{\"valid\":%s,\"hash\":\"%016llx\","
                        "\"type\":%u,\"ucode_dwords\":%u,\"analyzed\":%u,"
                        "\"texture_bindings\":%u,\"vertex_bindings\":%u,"
                        "\"memexport_written_mask\":%u,\"writes_color_targets\":%u,"
                        "\"writes_interpolators\":%u,\"writes_depth\":%u,\"kills_pixels\":%u,"
                        "\"ucode_path\":\"%s\",\"disasm_path\":\"%s\"},"
                        "\"ps\":{\"valid\":%s,\"hash\":\"%016llx\","
                        "\"type\":%u,\"ucode_dwords\":%u,\"analyzed\":%u,"
                        "\"texture_bindings\":%u,\"vertex_bindings\":%u,"
                        "\"memexport_written_mask\":%u,\"writes_color_targets\":%u,"
                        "\"writes_interpolators\":%u,\"writes_depth\":%u,\"kills_pixels\":%u,"
                        "\"ucode_path\":\"%s\",\"disasm_path\":\"%s\"}},\n",
                     r.vs.valid ? "true" : "false", (unsigned long long)r.vs.hash,
                     r.vs.type, r.vs.ucode_dwords, r.vs.analyzed,
                     r.vs.texture_binding_count, r.vs.vertex_binding_count,
                     r.vs.memexport_written_mask, r.vs.writes_color_targets,
                     r.vs.writes_interpolators, r.vs.writes_depth, r.vs.kills_pixels,
                     r.vs.ucode_path, r.vs.disasm_path,
                     r.ps.valid ? "true" : "false", (unsigned long long)r.ps.hash,
                     r.ps.type, r.ps.ucode_dwords, r.ps.analyzed,
                     r.ps.texture_binding_count, r.ps.vertex_binding_count,
                     r.ps.memexport_written_mask, r.ps.writes_color_targets,
                     r.ps.writes_interpolators, r.ps.writes_depth, r.ps.kills_pixels,
                     r.ps.ucode_path, r.ps.disasm_path);
        std::fprintf(f, "      \"major_mode_explicit\": %s,\n",
                     r.major_mode_explicit ? "true" : "false");
        std::fprintf(f, "      \"tex_desc_hash\": \"%016llx\",\n", (unsigned long long)r.sig.tex_desc_hash);
        std::fprintf(f, "      \"vtx_desc_hash\": \"%016llx\",\n", (unsigned long long)r.sig.vtx_desc_hash);
        std::fprintf(f, "      \"vtx_mask\": \"0x%08X\",\n", r.sig.vtx_mask);
        std::fprintf(f, "      \"vtx_mask_mid\": \"0x%08X\",\n", r.sig.vtx_mask_mid);
        std::fprintf(f, "      \"vtx_mask_hi\": \"0x%08X\",\n", r.sig.vtx_mask_hi);
        std::fprintf(f, "      \"rt\": {\"color_fmt\":%u,\"depth_fmt\":%u,\"edram_base\":%u,"
                        "\"depth_base\":%u,\"pitch\":%u},\n",
                     (unsigned)r.sig.rt_color_format, (unsigned)r.sig.rt_depth_format,
                     r.rt_edram_base, r.depth_edram_base, r.rt_surface_pitch);
        if (r.index.valid) {
            std::fprintf(f, "      \"index_buffer\": {\"valid\":true,\"format\":%u,"
                            "\"endianness\":%u,\"count\":%u,\"guest_base\":\"0x%08X\","
                            "\"length\":%llu},\n",
                         r.index.format, r.index.endianness, r.index.count,
                         r.index.guest_base, (unsigned long long)r.index.length);
        } else {
            std::fprintf(f, "      \"index_buffer\": {\"valid\":false},\n");
        }

        std::fprintf(f, "      \"textures\": [");
        bool first = true;
        for (uint32_t i = 0; i < 32; ++i) {
            const auto& t = r.tex[i];
            if (!t.active) continue;
            if (!first) std::fprintf(f, ", ");
            first = false;
            std::fprintf(f, "{\"slot\":%u,\"base\":\"0x%08X\",\"mip\":\"0x%08X\","
                            "\"w\":%u,\"h\":%u,\"fmt\":\"%s\",\"dim\":%u,"
                            "\"tiled\":%u,\"endian\":%u}",
                         i, t.base_phys, t.mip_phys,
                         (unsigned)t.width, (unsigned)t.height,
                         TexFmtName(t.format), (unsigned)t.dimension,
                         (unsigned)t.tiled, (unsigned)t.endian);
        }
        std::fprintf(f, "],\n");

        std::fprintf(f, "      \"vertex_fetches\": [");
        first = true;
        for (uint32_t i = 0; i < 96; ++i) {
            const auto& v = r.vtx[i];
            if (!v.active) continue;
            if (!first) std::fprintf(f, ", ");
            first = false;
            std::fprintf(f, "{\"slot\":%u,\"base\":\"0x%08X\",\"bytes\":%u,\"endian\":%u}",
                         i, v.base_phys, v.size_bytes, (unsigned)v.endian);
        }
        std::fprintf(f, "],\n");

        std::fprintf(f, "      \"raw_state\": {");
        for (uint32_t i = 0; i < DrawRecord::kRawRegCount; ++i) {
            if (i) std::fprintf(f, ", ");
            std::fprintf(f, "\"%s\":\"0x%08X\"", kRawRegs[i].name, r.raw_regs[i]);
        }
        std::fprintf(f, "}\n");
        std::fprintf(f, "    }");
        if (ri + 1 < g_record_count) std::fprintf(f, ",");
        std::fprintf(f, "\n");
    }
    std::fprintf(f, "  ]\n}\n");
    std::fclose(f);

    REXLOG_ERROR("DaytonaRenderer: object inventory written — {} signatures, {} shader pairs",
                 g_record_count, pair_count);
}

void DaytonaRenderer::WriteNativeTargetPlan() {
    (void)daytona_mkdir_compat("logs");
    std::FILE* f = std::fopen("logs/daytona_native_targets.json", "w");
    if (!f) return;

    struct TargetClass {
        uint64_t vs = 0;
        uint64_t ps = 0;
        uint8_t prim = 0;
        uint32_t index_count = 0;
        uint32_t tex_mask = 0;
        uint32_t vtx_mask = 0;
        uint32_t vtx_mask_mid = 0;
        uint32_t vtx_mask_hi = 0;
        uint8_t rt_color_format = 0;
        uint8_t rt_depth_format = 0;
        uint32_t rt_surface_pitch = 0;
        uint64_t draw_count = 0;
        uint32_t signature_count = 0;
        uint32_t representative = 0;
    };

    std::vector<TargetClass> targets;
    targets.reserve(g_record_count);
    for (uint32_t ri = 0; ri < g_record_count; ++ri) {
        const DrawRecord& r = g_records[ri];
        TargetClass* target = nullptr;
        for (auto& t : targets) {
            if (t.vs == r.sig.vs_hash &&
                t.ps == r.sig.ps_hash &&
                t.prim == r.sig.prim_type &&
                t.index_count == r.sig.index_count &&
                t.tex_mask == r.sig.tex_mask &&
                t.vtx_mask == r.sig.vtx_mask &&
                t.vtx_mask_mid == r.sig.vtx_mask_mid &&
                t.vtx_mask_hi == r.sig.vtx_mask_hi &&
                t.rt_color_format == r.sig.rt_color_format &&
                t.rt_depth_format == r.sig.rt_depth_format &&
                t.rt_surface_pitch == r.sig.rt_surface_pitch) {
                target = &t;
                break;
            }
        }
        if (!target) {
            targets.push_back({});
            target = &targets.back();
            target->vs = r.sig.vs_hash;
            target->ps = r.sig.ps_hash;
            target->prim = r.sig.prim_type;
            target->index_count = r.sig.index_count;
            target->tex_mask = r.sig.tex_mask;
            target->vtx_mask = r.sig.vtx_mask;
            target->vtx_mask_mid = r.sig.vtx_mask_mid;
            target->vtx_mask_hi = r.sig.vtx_mask_hi;
            target->rt_color_format = r.sig.rt_color_format;
            target->rt_depth_format = r.sig.rt_depth_format;
            target->rt_surface_pitch = r.sig.rt_surface_pitch;
            target->representative = ri;
        }
        ++target->signature_count;
        target->draw_count += r.draw_count;
        if (r.draw_count > g_records[target->representative].draw_count)
            target->representative = ri;
    }

    std::sort(targets.begin(), targets.end(),
              [](const TargetClass& a, const TargetClass& b) {
                  return a.draw_count > b.draw_count;
              });

    uint32_t first_quad_target = UINT32_MAX;
    uint32_t first_point_target = UINT32_MAX;
    for (uint32_t i = 0; i < targets.size(); ++i) {
        if (static_cast<xenos::PrimitiveType>(targets[i].prim) ==
            xenos::PrimitiveType::kQuadList && first_quad_target == UINT32_MAX) {
            first_quad_target = i;
        }
        if (static_cast<xenos::PrimitiveType>(targets[i].prim) ==
            xenos::PrimitiveType::kPointList &&
            targets[i].vs == kNativeTargetPointListVs &&
            targets[i].ps == kNativeTargetPointListPs &&
            first_point_target == UINT32_MAX) {
            first_point_target = i;
        }
    }

    std::fprintf(f, "{\n");
    std::fprintf(f, "  \"phase\": \"1.4-target-selection\",\n");
    std::fprintf(f, "  \"note\": \"Ranked native-render takeover candidates derived from Phase 1.3 object inventory. ReXGlue still renders.\",\n");
    std::fprintf(f, "  \"total_draws\": %llu,\n", (unsigned long long)g_total_draws);
    std::fprintf(f, "  \"unique_signatures\": %u,\n", g_record_count);
    std::fprintf(f, "  \"target_classes\": %zu,\n", targets.size());
    if (!targets.empty()) {
        const TargetClass& top = targets.front();
        std::fprintf(f, "  \"recommended_highest_volume_target\": {\"rank\":0,"
                        "\"reason\":\"highest observed draw volume; replacement requires matching primitive expansion and shader input generation\","
                        "\"vs_hash\":\"%016llx\",\"ps_hash\":\"%016llx\","
                        "\"draw_count\":%llu,\"signature_count\":%u,"
                        "\"prim\":\"%s\",\"index_count\":%u,\"tex_mask\":\"0x%08X\","
                        "\"vtx_mask\":\"0x%08X\",\"vtx_mask_mid\":\"0x%08X\","
                        "\"vtx_mask_hi\":\"0x%08X\",\"representative_signature\":%u},\n",
                     (unsigned long long)top.vs, (unsigned long long)top.ps,
                     (unsigned long long)top.draw_count, top.signature_count,
                     PrimName(top.prim), top.index_count, top.tex_mask, top.vtx_mask,
                     top.vtx_mask_mid, top.vtx_mask_hi, top.representative);
    } else {
        std::fprintf(f, "  \"recommended_highest_volume_target\": null,\n");
    }
    if (first_point_target != UINT32_MAX) {
        const TargetClass& p = targets[first_point_target];
        std::fprintf(f, "  \"recommended_pointlist_target\": {\"rank\":%u,"
                        "\"reason\":\"highest-volume remaining class; pass-through point shader but ReXGlue expands PointList as triangle strips\","
                        "\"vs_hash\":\"%016llx\",\"ps_hash\":\"%016llx\","
                        "\"draw_count\":%llu,\"signature_count\":%u,"
                        "\"prim\":\"%s\",\"index_count\":%u,\"tex_mask\":\"0x%08X\","
                        "\"vtx_mask\":\"0x%08X\",\"vtx_mask_mid\":\"0x%08X\","
                        "\"vtx_mask_hi\":\"0x%08X\",\"representative_signature\":%u},\n",
                     first_point_target,
                     (unsigned long long)p.vs, (unsigned long long)p.ps,
                     (unsigned long long)p.draw_count, p.signature_count,
                     PrimName(p.prim), p.index_count, p.tex_mask, p.vtx_mask,
                     p.vtx_mask_mid, p.vtx_mask_hi,
                     p.representative);
    } else {
        std::fprintf(f, "  \"recommended_pointlist_target\": null,\n");
    }
    if (first_quad_target != UINT32_MAX) {
        const TargetClass& q = targets[first_quad_target];
        std::fprintf(f, "  \"recommended_quadlist_target\": {\"rank\":%u,"
                        "\"reason\":\"first implemented native takeover path; stable batched QuadList replacement with texture ownership bridge\","
                        "\"vs_hash\":\"%016llx\",\"ps_hash\":\"%016llx\","
                        "\"draw_count\":%llu,\"signature_count\":%u,"
                        "\"prim\":\"%s\",\"index_count\":%u,\"tex_mask\":\"0x%08X\","
                        "\"vtx_mask\":\"0x%08X\",\"vtx_mask_mid\":\"0x%08X\","
                        "\"vtx_mask_hi\":\"0x%08X\",\"representative_signature\":%u},\n",
                     first_quad_target,
                     (unsigned long long)q.vs, (unsigned long long)q.ps,
                     (unsigned long long)q.draw_count, q.signature_count,
                     PrimName(q.prim), q.index_count, q.tex_mask, q.vtx_mask,
                     q.vtx_mask_mid, q.vtx_mask_hi,
                     q.representative);
    } else {
        std::fprintf(f, "  \"recommended_quadlist_target\": null,\n");
    }
    const uint64_t total_native = g_native_quadlist_takeover_count +
                                  g_native_pointlist_takeover_count +
                                  g_native_mesh_takeover_count;
    const double native_pct = g_total_draws > 0
        ? 100.0 * double(total_native) / double(g_total_draws) : 0.0;
    const bool native_all_enabled = EnvEnabled("DAYTONA_NATIVE_ALL");
    const bool native_suppression_unlocked = NativeSuppressionUnlocked();
    const bool quadlist_takeover_enabled   = native_suppression_unlocked;
    const bool pointlist_takeover_enabled  = native_suppression_unlocked;
    const bool mesh_takeover_enabled      = native_suppression_unlocked;
    const bool mesh_2d_takeover_enabled   = native_suppression_unlocked;
    const bool mesh_fan_takeover_enabled  =
        native_suppression_unlocked && EnvEnabled("DAYTONA_NATIVE_MESH_FAN");
    std::fprintf(f, "  \"readiness\": {\n");
    std::fprintf(f, "    \"issue_draw_hook\": true,\n");
    std::fprintf(f, "    \"native_shader_translation\": true,\n");
    std::fprintf(f, "    \"native_vertex_decode\": true,\n");
    std::fprintf(f, "    \"native_rt_resolve\": false,\n");
    std::fprintf(f, "    \"native_takeover_default\": false,\n");
    std::fprintf(f, "    \"native_all_env\": %s,\n",
                 native_all_enabled ? "true" : "false");
    std::fprintf(f, "    \"native_suppression_unlocked\": %s,\n",
                 native_suppression_unlocked ? "true" : "false");
    std::fprintf(f, "    \"shadow_classifiers_active\": true,\n");
    std::fprintf(f, "    \"quadlist_active\": %s,\n",
                 quadlist_takeover_enabled ? "true" : "false");
    std::fprintf(f, "    \"pointlist_active\": %s,\n",
                 pointlist_takeover_enabled ? "true" : "false");
    std::fprintf(f, "    \"mesh_active\": %s,\n",
                 mesh_takeover_enabled ? "true" : "false");
    std::fprintf(f, "    \"mesh_2d_active\": %s,\n",
                 mesh_2d_takeover_enabled ? "true" : "false");
    std::fprintf(f, "    \"mesh_fan_active\": %s,\n",
                 mesh_fan_takeover_enabled ? "true" : "false");
    std::fprintf(f, "    \"quadlist_classifier_hits\": %llu,\n",
                 (unsigned long long)g_native_shadow_quadlist_hits);
    std::fprintf(f, "    \"quadlist_takeover_count\": %llu,\n",
                 (unsigned long long)g_native_quadlist_takeover_count);
    std::fprintf(f, "    \"quadlist_fallthrough_count\": %llu,\n",
                 (unsigned long long)g_native_shadow_quadlist_enabled_hits);
    std::fprintf(f, "    \"quadlist_zero_sample_skips\": %llu,\n",
                 (unsigned long long)g_native_quadlist_zero_sample_skips);
    std::fprintf(f, "    \"quadlist_sample_valid\": %s,\n",
                 g_native_quadlist_sample_valid ? "true" : "false");
    std::fprintf(f, "    \"pointlist_classifier_hits\": %llu,\n",
                 (unsigned long long)g_native_shadow_pointlist_hits);
    std::fprintf(f, "    \"pointlist_takeover_count\": %llu,\n",
                 (unsigned long long)g_native_pointlist_takeover_count);
    std::fprintf(f, "    \"pointlist_fallthrough_count\": %llu,\n",
                 (unsigned long long)g_native_shadow_pointlist_enabled_hits);
    std::fprintf(f, "    \"pointlist_zero_sample_skips\": %llu,\n",
                 (unsigned long long)g_native_pointlist_zero_sample_skips);
    std::fprintf(f, "    \"pointlist_sample_valid\": %s,\n",
                 g_native_pointlist_sample_valid ? "true" : "false");
    std::fprintf(f, "    \"mesh_classifier_hits\": %llu,\n",
                 (unsigned long long)g_native_shadow_mesh_hits);
    std::fprintf(f, "    \"mesh_takeover_count\": %llu,\n",
                 (unsigned long long)g_native_mesh_takeover_count);
    std::fprintf(f, "    \"mesh_fallthrough_count\": %llu,\n",
                 (unsigned long long)g_native_shadow_mesh_enabled_hits);
    std::fprintf(f, "    \"total_native_draws\": %llu,\n",
                 (unsigned long long)total_native);
    const unsigned long long pct_num =
        g_total_draws ? (total_native * 10000ull / g_total_draws) : 0ull;
    std::fprintf(f, "    \"native_coverage_pct\": %llu.%02llu,\n",
                 pct_num / 100ull, pct_num % 100ull);
    std::fprintf(f, "    \"shader_microcode_dump\": true,\n");
    std::fprintf(f, "    \"texture_descriptors_captured\": true,\n");
    std::fprintf(f, "    \"vertex_fetches_captured\": true,\n");
    std::fprintf(f, "    \"index_buffers_captured\": true\n");
    std::fprintf(f, "  },\n");
    std::fprintf(f, "  \"targets\": [\n");
    const uint32_t limit = targets.size() < 128 ? static_cast<uint32_t>(targets.size()) : 128u;
    for (uint32_t i = 0; i < limit; ++i) {
        const TargetClass& t = targets[i];
        const DrawRecord& rep = g_records[t.representative];
        std::fprintf(f, "    {\"rank\":%u,\"draw_count\":%llu,\"signature_count\":%u,"
                        "\"vs_hash\":\"%016llx\",\"ps_hash\":\"%016llx\","
                        "\"prim\":\"%s\",\"index_count\":%u,"
                        "\"tex_mask\":\"0x%08X\",\"texture_count\":%u,"
                        "\"vtx_mask\":\"0x%08X\",\"vtx_mask_mid\":\"0x%08X\","
                        "\"vtx_mask_hi\":\"0x%08X\",\"vertex_fetch_count\":%u,"
                        "\"rt_color_fmt\":%u,\"rt_depth_fmt\":%u,\"rt_pitch\":%u,"
                        "\"representative_signature\":%u,"
                        "\"representative_texture_bases\":[",
                     i, (unsigned long long)t.draw_count, t.signature_count,
                     (unsigned long long)t.vs, (unsigned long long)t.ps,
                     PrimName(t.prim), t.index_count,
                     t.tex_mask, __builtin_popcount(t.tex_mask),
                     t.vtx_mask, t.vtx_mask_mid, t.vtx_mask_hi,
                     __builtin_popcount(t.vtx_mask) +
                     __builtin_popcount(t.vtx_mask_mid) +
                     __builtin_popcount(t.vtx_mask_hi),
                     (unsigned)t.rt_color_format, (unsigned)t.rt_depth_format,
                     t.rt_surface_pitch, t.representative);
        bool first = true;
        for (uint32_t slot = 0; slot < 32; ++slot) {
            if (!rep.tex[slot].active) continue;
            if (!first) std::fprintf(f, ",");
            first = false;
            std::fprintf(f, "\"0x%08X\"", rep.tex[slot].base_phys);
        }
        std::fprintf(f, "]}");
        if (i + 1 < limit) std::fprintf(f, ",");
        std::fprintf(f, "\n");
    }
    std::fprintf(f, "  ]\n}\n");
    std::fclose(f);

    REXLOG_ERROR("DaytonaRenderer: native target plan written — {} target classes, recommended QuadList rank {}",
                 targets.size(),
                 first_quad_target == UINT32_MAX ? -1 : static_cast<int>(first_quad_target));
}

void DaytonaRenderer::WriteRecommendedQuadListSample() {
    (void)daytona_mkdir_compat("logs");
    std::FILE* f = std::fopen("logs/daytona_quadlist_target_sample.json", "w");
    if (!f) return;

    std::fprintf(f, "{\n");
    std::fprintf(f, "  \"phase\": \"1.4-quadlist-shadow-decode\",\n");
    std::fprintf(f, "  \"valid\": %s,\n", g_native_quadlist_sample_valid ? "true" : "false");
    std::fprintf(f, "  \"target\": {\"vs_hash\":\"%016llx\",\"ps_hash\":\"%016llx\","
                    "\"prim\":\"QuadList\",\"index_count\":4,\"tex_mask\":\"0x%08X\","
                    "\"vtx_mask\":\"0x%08X\",\"vtx_mask_mid\":\"0x%08X\","
                    "\"vtx_mask_hi\":\"0x%08X\",\"rt_pitch\":%u},\n",
                 (unsigned long long)kNativeTargetQuadListVs,
                 (unsigned long long)kNativeTargetQuadListPs,
                 kNativeTargetQuadListTexMask,
                 kNativeTargetQuadListVtxMaskLo,
                 kNativeTargetQuadListVtxMaskMid,
                 kNativeTargetQuadListVtxMaskHi,
                 kNativeTargetQuadListRtPitch);
    std::fprintf(f, "  \"shader_vertex_layout\": {\n");
    std::fprintf(f, "    \"source\": \"shader_39076F2082E5AE16.ucode.vert\",\n");
    std::fprintf(f, "    \"vfetch_constant\": \"vf0\",\n");
    std::fprintf(f, "    \"stride_dwords\": %u,\n", kNativeTargetQuadListShaderStrideDwords);
    std::fprintf(f, "    \"fields\": [\n");
    std::fprintf(f, "      {\"name\":\"position_xyz\",\"offset_dword\":0,\"format\":\"FMT_32_32_32_FLOAT\"},\n");
    std::fprintf(f, "      {\"name\":\"packed_color_or_params\",\"offset_dword\":3,\"format\":\"FMT_8_8_8_8\"},\n");
    std::fprintf(f, "      {\"name\":\"scalar_x\",\"offset_dword\":4,\"format\":\"FMT_32_FLOAT\"},\n");
    std::fprintf(f, "      {\"name\":\"uv0_xy\",\"offset_dword\":5,\"format\":\"FMT_32_32_FLOAT\"},\n");
    std::fprintf(f, "      {\"name\":\"uv1_xy\",\"offset_dword\":7,\"format\":\"FMT_32_32_FLOAT\"}\n");
    std::fprintf(f, "    ]\n");
    std::fprintf(f, "  },\n");

    if (!g_native_quadlist_sample_valid) {
        std::fprintf(f, "  \"sample\": null\n");
        std::fprintf(f, "}\n");
        std::fclose(f);
        return;
    }

    const NativeQuadListSample& s = g_native_quadlist_sample;
    std::fprintf(f, "  \"sample\": {\n");
    std::fprintf(f, "    \"draw_number\": %llu,\n", (unsigned long long)s.draw_number);
    std::fprintf(f, "    \"zero_sample_skips_before_capture\": %llu,\n",
                 (unsigned long long)g_native_quadlist_zero_sample_skips);
    std::fprintf(f, "    \"vgt_indx_offset\": %u,\n", s.index_offset);
    std::fprintf(f, "    \"vgt_draw_initiator\": \"0x%08X\",\n", s.draw_initiator);
    std::fprintf(f, "    \"shader_vertex_bindings\": [");
    for (uint32_t bi = 0; bi < s.shader_binding_count; ++bi) {
        if (bi) std::fprintf(f, ",");
        std::fprintf(f, "{\"fetch_constant\":%u,\"stride_words\":%u,"
                        "\"attribute_count\":%u,\"base_phys\":\"0x%08X\","
                        "\"size_bytes\":%u}",
                     s.shader_binding_fetch_constants[bi],
                     s.shader_binding_stride_words[bi],
                     s.shader_binding_attribute_counts[bi],
                     s.shader_binding_base_phys[bi],
                     s.shader_binding_size_bytes[bi]);
    }
    std::fprintf(f, "],\n");
    std::fprintf(f, "    \"shader_binding_streams\": [");
    for (uint32_t bi = 0; bi < s.shader_binding_count; ++bi) {
        if (bi) std::fprintf(f, ",");
        std::fprintf(f, "{\"fetch_constant\":%u,\"stride_words\":%u,"
                        "\"base_phys\":\"0x%08X\",\"rows\":[",
                     s.shader_binding_fetch_constants[bi],
                     s.shader_binding_stride_words[bi],
                     s.shader_binding_base_phys[bi]);
        for (uint32_t vi = 0; vi < kNativeTargetQuadListSampleVertices; ++vi) {
            if (vi) std::fprintf(f, ",");
            std::fprintf(f, "[");
            for (uint32_t wi = 0; wi < kNativeTargetQuadListSampleDwords; ++wi) {
                if (wi) std::fprintf(f, ",");
                std::fprintf(f, "\"0x%08X\"", s.shader_binding_rows[bi][vi][wi]);
            }
            std::fprintf(f, "]");
        }
        std::fprintf(f, "],\"decoded_vertices\":[");
        for (uint32_t vi = 0; vi < kNativeTargetQuadListSampleVertices; ++vi) {
            if (vi) std::fprintf(f, ",");
            WriteDecodedQuadListVertexJson(f, s.shader_binding_rows[bi][vi]);
        }
        std::fprintf(f, "]}");
    }
    std::fprintf(f, "],\n");
    std::fprintf(f, "    \"active_vfetch_slots\": [\n");
    for (uint32_t si = 0; si < 3; ++si) {
        const uint32_t indexed_phys =
            s.raw_vfetch_bases[si] + s.index_offset * kNativeTargetQuadListShaderStrideDwords * 4u;
        std::fprintf(f, "      {\"slot\":%u,\"address_dwords\":\"0x%08X\","
                        "\"base_phys\":\"0x%08X\",\"size_bytes\":%u,"
                        "\"assumed_stride_dwords\":%u,\"indexed_start_phys\":\"0x%08X\","
                        "\"base_rows\":[",
                     s.raw_vfetch_slots[si], s.raw_vfetch_address_dwords[si],
                     s.raw_vfetch_bases[si], s.raw_vfetch_sizes[si],
                     kNativeTargetQuadListShaderStrideDwords, indexed_phys);
        for (uint32_t vi = 0; vi < kNativeTargetQuadListSampleVertices; ++vi) {
            if (vi) std::fprintf(f, ",");
            std::fprintf(f, "[");
            for (uint32_t wi = 0; wi < kNativeTargetQuadListSampleDwords; ++wi) {
                if (wi) std::fprintf(f, ",");
                std::fprintf(f, "\"0x%08X\"", s.base_rows[si][vi][wi]);
            }
            std::fprintf(f, "]");
        }
        std::fprintf(f, "],\"indexed_rows\":[");
        for (uint32_t vi = 0; vi < kNativeTargetQuadListSampleVertices; ++vi) {
            if (vi) std::fprintf(f, ",");
            std::fprintf(f, "[");
            for (uint32_t wi = 0; wi < kNativeTargetQuadListSampleDwords; ++wi) {
                if (wi) std::fprintf(f, ",");
                std::fprintf(f, "\"0x%08X\"", s.indexed_rows[si][vi][wi]);
            }
            std::fprintf(f, "]");
        }
        std::fprintf(f, "],\"unshifted_address_rows\":[");
        for (uint32_t vi = 0; vi < kNativeTargetQuadListSampleVertices; ++vi) {
            if (vi) std::fprintf(f, ",");
            std::fprintf(f, "[");
            for (uint32_t wi = 0; wi < kNativeTargetQuadListSampleDwords; ++wi) {
                if (wi) std::fprintf(f, ",");
                std::fprintf(f, "\"0x%08X\"", s.unshifted_rows[si][vi][wi]);
            }
            std::fprintf(f, "]");
        }
        std::fprintf(f, "]}");
        if (si + 1 < 3) std::fprintf(f, ",");
        std::fprintf(f, "\n");
    }
    std::fprintf(f, "    ],\n");
    std::fprintf(f, "    \"nonzero_active_vfetch_scan\": [");
    for (uint32_t si = 0; si < s.scan_count; ++si) {
        if (si) std::fprintf(f, ",");
        std::fprintf(f, "{\"slot\":%u,\"address_dwords\":\"0x%08X\","
                        "\"base_phys\":\"0x%08X\",\"size_bytes\":%u,\"first_dwords\":[",
                     s.scan_slots[si], s.scan_address_dwords[si],
                     s.scan_base_phys[si], s.scan_size_bytes[si]);
        for (uint32_t wi = 0; wi < 4; ++wi) {
            if (wi) std::fprintf(f, ",");
            std::fprintf(f, "\"0x%08X\"", s.scan_first_dwords[si][wi]);
        }
        std::fprintf(f, "],\"rows\":[");
        for (uint32_t vi = 0; vi < kNativeTargetQuadListSampleVertices; ++vi) {
            if (vi) std::fprintf(f, ",");
            std::fprintf(f, "[");
            for (uint32_t wi = 0; wi < kNativeTargetQuadListSampleDwords; ++wi) {
                if (wi) std::fprintf(f, ",");
                std::fprintf(f, "\"0x%08X\"", s.scan_rows[si][vi][wi]);
            }
            std::fprintf(f, "]");
        }
        std::fprintf(f, "],\"decoded_vertices\":[");
        for (uint32_t vi = 0; vi < kNativeTargetQuadListSampleVertices; ++vi) {
            if (vi) std::fprintf(f, ",");
            WriteDecodedQuadListVertexJson(f, s.scan_rows[si][vi]);
        }
        std::fprintf(f, "]}");
    }
    std::fprintf(f, "]\n");
    std::fprintf(f, "  }\n");
    std::fprintf(f, "}\n");
    std::fclose(f);

    REXLOG_ERROR("DaytonaRenderer: QuadList target sample written — valid={}",
                 g_native_quadlist_sample_valid);
}

void DaytonaRenderer::WriteRecommendedQuadListSamples() {
    (void)daytona_mkdir_compat("logs");
    std::FILE* f = std::fopen("logs/daytona_quadlist_target_samples.json", "w");
    if (!f) return;

    std::fprintf(f, "{\n");
    std::fprintf(f, "  \"phase\": \"quadlist-shadow-multisample\",\n");
    std::fprintf(f, "  \"sample_count\": %u,\n", g_native_quadlist_sample_count);
    std::fprintf(f, "  \"samples\": [\n");
    for (uint32_t si = 0; si < g_native_quadlist_sample_count; ++si) {
        const NativeQuadListSample& s = g_native_quadlist_samples[si];
        const DrawSig& sig = s.sig;
        if (si) std::fprintf(f, ",\n");
        std::fprintf(f, "    {\n");
        std::fprintf(f, "      \"sample_index\": %u,\n", si);
        std::fprintf(f, "      \"draw_number\": %llu,\n",
                     (unsigned long long)s.draw_number);
        std::fprintf(f, "      \"signature\": {\"vs_hash\":\"%016llx\","
                        "\"ps_hash\":\"%016llx\",\"prim\":\"%s\","
                        "\"index_count\":%u,\"tex_mask\":\"0x%08X\","
                        "\"vtx_mask\":\"0x%08X\",\"vtx_mask_mid\":\"0x%08X\","
                        "\"vtx_mask_hi\":\"0x%08X\",\"rt_pitch\":%u,"
                        "\"rt_color_format\":%u,\"rt_depth_format\":%u},\n",
                     (unsigned long long)sig.vs_hash,
                     (unsigned long long)sig.ps_hash,
                     PrimName(sig.prim_type),
                     sig.index_count, sig.tex_mask,
                     sig.vtx_mask, sig.vtx_mask_mid, sig.vtx_mask_hi,
                     sig.rt_surface_pitch, sig.rt_color_format,
                     sig.rt_depth_format);
        std::fprintf(f, "      \"vgt_indx_offset\": %u,\n", s.index_offset);
        std::fprintf(f, "      \"vgt_draw_initiator\": \"0x%08X\",\n",
                     s.draw_initiator);
        std::fprintf(f, "      \"raw_state\": {");
        for (uint32_t ri = 0; ri < DrawRecord::kRawRegCount; ++ri) {
            if (ri) std::fprintf(f, ",");
            std::fprintf(f, "\"%s\":\"0x%08X\"", kRawRegs[ri].name, s.raw_regs[ri]);
        }
        std::fprintf(f, "},\n");
        std::fprintf(f, "      \"textures\": [");
        for (uint32_t ti = 0; ti < 2; ++ti) {
            if (ti) std::fprintf(f, ",");
            std::fprintf(f, "{\"slot\":%u,\"base\":\"0x%08X\",\"mip\":\"0x%08X\","
                            "\"w\":%u,\"h\":%u,\"fmt\":%u,\"tiled\":%u,\"endian\":%u}",
                         ti, s.texture_base[ti], s.texture_mip[ti],
                         s.texture_width[ti], s.texture_height[ti],
                         s.texture_format[ti], s.texture_tiled[ti],
                         s.texture_endian[ti]);
        }
        std::fprintf(f, "],\n");
        std::fprintf(f, "      \"c72_c75\": [");
        for (uint32_t row = 0; row < 4; ++row) {
            if (row) std::fprintf(f, ",");
            std::fprintf(f, "[");
            for (uint32_t col = 0; col < 4; ++col) {
                if (col) std::fprintf(f, ",");
                WriteJsonFloat(f, s.c72_c75[row][col]);
            }
            std::fprintf(f, "]");
        }
        std::fprintf(f, "],\n");
        std::fprintf(f, "      \"c0\": [");
        for (uint32_t i = 0; i < 4; ++i) {
            if (i) std::fprintf(f, ",");
            WriteJsonFloat(f, s.c0[i]);
        }
        std::fprintf(f, "],\n");
        std::fprintf(f, "      \"c255\": [");
        for (uint32_t i = 0; i < 4; ++i) {
            if (i) std::fprintf(f, ",");
            WriteJsonFloat(f, s.c255[i]);
        }
        std::fprintf(f, "],\n");
        std::fprintf(f, "      \"shader_vertex_bindings\": [");
        for (uint32_t bi = 0; bi < s.shader_binding_count; ++bi) {
            if (bi) std::fprintf(f, ",");
            std::fprintf(f, "{\"fetch_constant\":%u,\"stride_words\":%u,"
                            "\"attribute_count\":%u,\"base_phys\":\"0x%08X\","
                            "\"size_bytes\":%u,\"rows\":[",
                         s.shader_binding_fetch_constants[bi],
                         s.shader_binding_stride_words[bi],
                         s.shader_binding_attribute_counts[bi],
                         s.shader_binding_base_phys[bi],
                         s.shader_binding_size_bytes[bi]);
            for (uint32_t vi = 0; vi < kNativeTargetQuadListSampleVertices; ++vi) {
                if (vi) std::fprintf(f, ",");
                std::fprintf(f, "[");
                for (uint32_t wi = 0; wi < kNativeTargetQuadListSampleDwords; ++wi) {
                    if (wi) std::fprintf(f, ",");
                    std::fprintf(f, "\"0x%08X\"", s.shader_binding_rows[bi][vi][wi]);
                }
                std::fprintf(f, "]");
            }
            std::fprintf(f, "],\"decoded_vertices\":[");
            for (uint32_t vi = 0; vi < kNativeTargetQuadListSampleVertices; ++vi) {
                if (vi) std::fprintf(f, ",");
                WriteDecodedQuadListVertexJson(f, s.shader_binding_rows[bi][vi]);
            }
            std::fprintf(f, "]}");
        }
        std::fprintf(f, "]\n");
        std::fprintf(f, "    }");
    }
    std::fprintf(f, "\n  ]\n");
    std::fprintf(f, "}\n");
    std::fclose(f);
}

void DaytonaRenderer::WriteRecommendedPointListSample() {
    (void)daytona_mkdir_compat("logs");
    std::FILE* f = std::fopen("logs/daytona_pointlist_target_sample.json", "w");
    if (!f) return;

    std::fprintf(f, "{\n");
    std::fprintf(f, "  \"phase\": \"pointlist-shadow-decode\",\n");
    std::fprintf(f, "  \"valid\": %s,\n", g_native_pointlist_sample_valid ? "true" : "false");
    std::fprintf(f, "  \"target\": {\"vs_hash\":\"%016llx\",\"ps_hash\":\"%016llx\","
                    "\"prim\":\"PointList\",\"index_count\":1,\"rt_pitch\":%u,"
                    "\"observed_vtx_masks\":[\"0x00000002\",\"0x00000011\",\"0x00000012\"]},\n",
                 (unsigned long long)kNativeTargetPointListVs,
                 (unsigned long long)kNativeTargetPointListPs,
                 kNativeTargetQuadListRtPitch);
    std::fprintf(f, "  \"shader_semantics\": {\"vertex\":\"o0=r0, oPos=r1\","
                    "\"pixel\":\"oC0=r0\","
                    "\"note\":\"shader has no analyzed vertex bindings; stream rows below are raw inference input\"},\n");

    if (!g_native_pointlist_sample_valid) {
        std::fprintf(f, "  \"sample\": null\n");
        std::fprintf(f, "}\n");
        std::fclose(f);
        return;
    }

    const NativePointListSample& s = g_native_pointlist_sample;
    std::fprintf(f, "  \"sample\": {\n");
    std::fprintf(f, "    \"draw_number\": %llu,\n", (unsigned long long)s.draw_number);
    std::fprintf(f, "    \"zero_sample_skips_before_capture\": %llu,\n",
                 (unsigned long long)g_native_pointlist_zero_sample_skips);
    std::fprintf(f, "    \"vgt_indx_offset\": %u,\n", s.index_offset);
    std::fprintf(f, "    \"vgt_draw_initiator\": \"0x%08X\",\n", s.draw_initiator);
    std::fprintf(f, "    \"index_count\": %u,\n", s.index_count);
    std::fprintf(f, "    \"tex_mask\": \"0x%08X\",\n", s.tex_mask);
    std::fprintf(f, "    \"vtx_mask\": \"0x%08X\",\n", s.vtx_mask);
    std::fprintf(f, "    \"vtx_mask_mid\": \"0x%08X\",\n", s.vtx_mask_mid);
    std::fprintf(f, "    \"vtx_mask_hi\": \"0x%08X\",\n", s.vtx_mask_hi);
    std::fprintf(f, "    \"shader_vertex_bindings\": [");
    for (uint32_t bi = 0; bi < s.shader_binding_count; ++bi) {
        if (bi) std::fprintf(f, ",");
        std::fprintf(f, "{\"fetch_constant\":%u,\"stride_words\":%u,\"attribute_count\":%u}",
                     s.shader_binding_fetch_constants[bi],
                     s.shader_binding_stride_words[bi],
                     s.shader_binding_attribute_counts[bi]);
    }
    std::fprintf(f, "],\n");
    std::fprintf(f, "    \"active_vfetch_scan\": [\n");
    for (uint32_t si = 0; si < s.scan_count; ++si) {
        std::fprintf(f, "      {\"slot\":%u,\"address_dwords\":\"0x%08X\","
                        "\"base_phys\":\"0x%08X\",\"size_bytes\":%u,\"endian\":%u,"
                        "\"assumed_stride_dwords\":%u,\"indexed_start_phys\":\"0x%08X\","
                        "\"base_rows\":[",
                     s.scan_slots[si], s.scan_address_dwords[si],
                     s.scan_base_phys[si], s.scan_size_bytes[si],
                     s.scan_endian[si], kNativeTargetPointListSampleDwords,
                     s.scan_base_phys[si] +
                         s.index_offset * kNativeTargetPointListSampleDwords * 4u);
        for (uint32_t vi = 0; vi < kNativeTargetPointListSampleVertices; ++vi) {
            if (vi) std::fprintf(f, ",");
            std::fprintf(f, "[");
            for (uint32_t wi = 0; wi < kNativeTargetPointListSampleDwords; ++wi) {
                if (wi) std::fprintf(f, ",");
                std::fprintf(f, "\"0x%08X\"", s.base_rows[si][vi][wi]);
            }
            std::fprintf(f, "]");
        }
        std::fprintf(f, "],\"indexed_rows\":[");
        for (uint32_t vi = 0; vi < kNativeTargetPointListSampleVertices; ++vi) {
            if (vi) std::fprintf(f, ",");
            std::fprintf(f, "[");
            for (uint32_t wi = 0; wi < kNativeTargetPointListSampleDwords; ++wi) {
                if (wi) std::fprintf(f, ",");
                std::fprintf(f, "\"0x%08X\"", s.indexed_rows[si][vi][wi]);
            }
            std::fprintf(f, "]");
        }
        std::fprintf(f, "]}");
        if (si + 1 < s.scan_count) std::fprintf(f, ",");
        std::fprintf(f, "\n");
    }
    std::fprintf(f, "    ]\n");
    std::fprintf(f, "  }\n");
    std::fprintf(f, "}\n");
    std::fclose(f);

    REXLOG_ERROR("DaytonaRenderer: PointList target sample written — valid={}",
                 g_native_pointlist_sample_valid);
}

void DaytonaRenderer::WriteRecommendedMeshSamples() {
    (void)daytona_mkdir_compat("logs");
    std::FILE* f = std::fopen("logs/daytona_mesh_target_samples.json", "w");
    if (!f) return;

    std::fprintf(f, "{\n");
    std::fprintf(f, "  \"phase\": \"mesh-shadow-decode\",\n");
    std::fprintf(f, "  \"note\": \"Shadow-only Mesh samples. ReXGlue rendered these draws; native suppression remains gated while classifiers are validated.\",\n");
    std::fprintf(f, "  \"sample_count\": %u,\n", g_native_mesh_sample_count);
    std::fprintf(f, "  \"samples\": [\n");
    for (uint32_t si = 0; si < g_native_mesh_sample_count; ++si) {
        const NativeMeshSample& s = g_native_mesh_samples[si];
        const uint32_t vertex_count =
            s.sig.index_count < kNativeTargetMeshSampleVertices
                ? s.sig.index_count
                : kNativeTargetMeshSampleVertices;
        std::fprintf(f, "    {\n");
        std::fprintf(f, "      \"sample_index\": %u,\n", si);
        std::fprintf(f, "      \"draw_number\": %llu,\n",
                     (unsigned long long)s.draw_number);
        std::fprintf(f, "      \"signature\": {\"vs_hash\":\"%016llx\",\"ps_hash\":\"%016llx\","
                        "\"prim\":\"%s\",\"index_count\":%u,\"tex_mask\":\"0x%08X\","
                        "\"vtx_mask\":\"0x%08X\",\"vtx_mask_mid\":\"0x%08X\","
                        "\"vtx_mask_hi\":\"0x%08X\",\"rt_pitch\":%u,\"rt_color_format\":%u},\n",
                     (unsigned long long)s.sig.vs_hash,
                     (unsigned long long)s.sig.ps_hash,
                     PrimName(s.sig.prim_type), s.sig.index_count,
                     s.sig.tex_mask, s.sig.vtx_mask, s.sig.vtx_mask_mid,
                     s.sig.vtx_mask_hi, s.sig.rt_surface_pitch,
                     s.sig.rt_color_format);
        std::fprintf(f, "      \"raw_state\": {");
        for (uint32_t i = 0; i < DrawRecord::kRawRegCount; ++i) {
            if (i) std::fprintf(f, ",");
            std::fprintf(f, "\"%s\":\"0x%08X\"", kRawRegs[i].name, s.raw_regs[i]);
        }
        std::fprintf(f, "},\n");
        std::fprintf(f, "      \"texture0\": {\"base\":\"0x%08X\",\"mip\":\"0x%08X\","
                        "\"w\":%u,\"h\":%u,\"fmt\":%u,\"tiled\":%u,\"endian\":%u},\n",
                     s.texture0_base, s.texture0_mip, s.texture0_width,
                     s.texture0_height, s.texture0_format, s.texture0_tiled,
                     s.texture0_endian);
        std::fprintf(f, "      \"index_buffer\": {\"indexed\":%s,\"format\":%u,"
                        "\"endianness\":%u,\"guest_base\":\"0x%08X\",\"length\":%llu,"
                        "\"decoded_indices\":[",
                     s.indexed ? "true" : "false",
                     s.index_format, s.index_endianness, s.index_guest_base,
                     (unsigned long long)s.index_length);
        for (uint32_t i = 0; i < s.decoded_index_count; ++i) {
            if (i) std::fprintf(f, ",");
            std::fprintf(f, "%u", s.decoded_indices[i]);
        }
        std::fprintf(f, "]},\n");
        std::fprintf(f, "      \"shader_vertex_bindings\": [");
        for (uint32_t bi = 0; bi < s.shader_binding_count; ++bi) {
            if (bi) std::fprintf(f, ",");
            std::fprintf(f, "{\"fetch_constant\":%u,\"stride_words\":%u,\"attribute_count\":%u}",
                         s.shader_binding_fetch_constants[bi],
                         s.shader_binding_stride_words[bi],
                         s.shader_binding_attribute_counts[bi]);
        }
        std::fprintf(f, "],\n");
        std::fprintf(f, "      \"streams\": {\"position\":{\"fetch_constant\":%u,"
                        "\"base_phys\":\"0x%08X\",\"size_bytes\":%u,\"stride_words\":%u,"
                        "\"endian\":%u},\"color\":{\"fetch_constant\":%u,"
                        "\"base_phys\":\"0x%08X\",\"size_bytes\":%u,\"stride_words\":%u,"
                        "\"endian\":%u},\"uv\":{\"fetch_constant\":%u,"
                        "\"base_phys\":\"0x%08X\",\"size_bytes\":%u,\"stride_words\":%u,"
                        "\"endian\":%u}},\n",
                     s.pos_fetch_constant, s.pos_base_phys, s.pos_size_bytes,
                     s.pos_stride_words, s.pos_endian,
                     s.col_fetch_constant, s.col_base_phys, s.col_size_bytes,
                     s.col_stride_words, s.col_endian,
                     s.uv_fetch_constant, s.uv_base_phys, s.uv_size_bytes,
                     s.uv_stride_words, s.uv_endian);
        std::fprintf(f, "      \"c72_c75\": [");
        for (uint32_t row = 0; row < 4; ++row) {
            if (row) std::fprintf(f, ",");
            std::fprintf(f, "[");
            for (uint32_t col = 0; col < 4; ++col) {
                if (col) std::fprintf(f, ",");
                WriteJsonFloat(f, FloatFromU32(s.c72_c75[row][col]));
            }
            std::fprintf(f, "]");
        }
        std::fprintf(f, "],\n");
        std::fprintf(f, "      \"vertices\": [");
        for (uint32_t vi = 0; vi < vertex_count; ++vi) {
            if (vi) std::fprintf(f, ",");
            WriteDecodedMeshVertexJson(f, s.pos_rows[vi], s.col_rows[vi],
                                       s.uv_rows[vi], s.c72_c75);
        }
        std::fprintf(f, "]\n");
        std::fprintf(f, "    }");
        if (si + 1 < g_native_mesh_sample_count) std::fprintf(f, ",");
        std::fprintf(f, "\n");
    }
    std::fprintf(f, "  ]\n");
    std::fprintf(f, "}\n");
    std::fclose(f);
}

// ── IssueDraw hook ────────────────────────────────────────────────────────────

bool DaytonaRenderer::OnIssueDraw(rex::graphics::CommandProcessor* cp,
                                   xenos::PrimitiveType prim_type,
                                   uint32_t index_count,
                                   const rex::graphics::CommandProcessor::DaytonaIndexBufferInfo* index_buffer_info,
                                   bool major_mode_explicit) {
    const rex::graphics::RegisterFile* rf = cp->GetRegisterFile();
    if (!rf) return false;  // pass through to Xenia

    rex::graphics::Shader* vs = cp->active_vertex_shader();
    rex::graphics::Shader* ps = cp->active_pixel_shader();

    const DrawSig sig = BuildSig(*rf, vs, ps, prim_type, index_count);
    const bool diagnostics_enabled = RendererDiagnosticsEnabled();

    if (diagnostics_enabled) {
        ++g_total_draws;

        // Record every draw signature before any native suppression decision.
        // This catalog is diagnostics-only; keep it off in normal gameplay
        // because it grows over a race and periodically rewrites large JSON
        // files from the render thread.
        DrawRecord* rec = nullptr;
        for (uint32_t i = 0; i < g_record_count; ++i) {
            if (g_records[i].sig == sig) {
                rec = &g_records[i];
                break;
            }
        }

        bool new_record = false;
        if (!rec) {
            if (g_record_count >= kMaxRecords) {
                ++g_overflow_draws;
                if ((g_overflow_draws & ((1ull << 20) - 1ull)) == 1ull) {
                    REXLOG_ERROR("DaytonaRenderer: signature table full at {} records; overflow draws={}",
                                g_record_count, g_overflow_draws);
                }
            } else {
                rec = &g_records[g_record_count++];
                rec->sig = sig;
                rec->draw_count = 0;
                PopulateRecord(*rec, *rf, vs, ps, index_count, index_buffer_info, major_mode_explicit);
                new_record = true;
            }
        }

        if (rec) {
            ++rec->draw_count;
        }

        if (new_record) {
            if (g_record_count <= 64 || (g_record_count % kFlushEveryNewRecords) == 0) {
                REXLOG_ERROR("DaytonaRenderer: new DrawSig #{} vs={:016x} ps={:016x} "
                            "prim={} tex={:08x} vtx={:08x} rt_cfmt={} draws_total={}",
                            g_record_count,
                            sig.vs_hash, sig.ps_hash,
                            PrimName(sig.prim_type),
                            sig.tex_mask, sig.vtx_mask,
                            (unsigned)sig.rt_color_format,
                            g_total_draws);
            }

            if ((g_record_count % kFlushEveryNewRecords) == 0) {
                WriteCatalog();
                WriteNativeTargetPlan();
                WriteRecommendedQuadListSample();
                WriteRecommendedQuadListSamples();
                WriteRecommendedPointListSample();
                WriteRecommendedMeshSamples();
            }
        }
    }

    // Aspect-ratio correction (Hor+) for 3D geometry ONLY. 2D bitmaps (UI, logos,
    // menus, screen-space sprites) are deliberately left untouched — they are
    // orthographic and will simply be stretched by the present-fill, which is the
    // accepted behaviour for 2D content on non-16:9 displays.
    //
    // The MVP is stored column-major in c72..c75: clip.i = dot(column_i, vertex)
    // where column_i = (c72[i], c73[i], c74[i], c75[i]).
    //   * column 0 → clip.x   (X scale + rotation + translation)
    //   * column 3 → clip.w
    //
    // Perspective vs orthographic is decided by column 3 (clip.w):
    //   * orthographic  → column3 ≈ (0,0,0,1)  (clip.w == vertex.w == 1)
    //   * perspective   → anything else        (clip.w depends on position)
    // Confirmed from captured matrices: QuadList-3D col3=(0,0,1,0),
    // Mesh-3D col3=(0,0,-1,~2000), Fan/UI col3=(0,0,0,1).
    //
    // Hor+ then scales the ENTIRE clip.x output — column 0 — by g_ar_scale. This
    // touches clip.x only (Y/Z/W untouched), so rotated/translated objects such as
    // billboard sprites stay positioned correctly; only the horizontal FOV widens.
    //
    // Patched on the live register file before dispatch so both the native takeover
    // paths and the ReXGlue fallback use the corrected value. The game re-uploads
    // fresh constants before each draw, so the patch never leaks to the next draw.
    //
    // CRITICAL: only the QuadList and Mesh shaders are known to store their MVP in
    // c72..c75. Other shaders (2D UI, menus, logos) use those constant slots for
    // unrelated data, so the patch is restricted to those two VS hashes. This
    // guarantees 2D bitmaps are never touched (they simply stretch with the
    // present-fill), and the perspective test below further excludes the 2D draws
    // that share the QuadList/Mesh shaders (UI quads, Mesh-2D, the Fan present).
    // ── TEMP DIAGNOSTIC (DAYTONA_AR_DIAG=1): log distinct shader projection
    // classification so we can see which VS the menus/billboards use and how
    // their c72..c75 column 3 looks. One line per distinct (vs_hash, prim).
    if (EnvEnabled("DAYTONA_AR_DIAG")) {
        static uint64_t s_seen[512];
        static uint32_t s_seen_count = 0;
        const auto vsc = rf->Get<reg::SQ_VS_CONST>(rex::graphics::XE_GPU_REG_SQ_VS_CONST);
        const uint32_t a72 = vsc.base + 72;
        float c[16] = {0};
        if (a72 + 3 < 512) {
            const uint32_t r0 = rex::graphics::XE_GPU_REG_SHADER_CONSTANT_000_X + 4 * a72;
            for (uint32_t i = 0; i < 16; ++i)
                std::memcpy(&c[i], &rf->values[r0 + i], sizeof(float));
        }
        // Column-major: col3 = (c[3],c[7],c[11],c[15]); col0.x=c[0]; Yscale=c[5].
        const float c75w = c[15];
        const uint32_t depthctl = rf->values[0x2200];  // RB_DEPTHCONTROL
        const bool z_enable = (depthctl & 0x2u) != 0;     // bit1 = Z_ENABLE
        const bool z_write  = (depthctl & 0x4u) != 0;     // bit2 = Z_WRITE_ENABLE
        const uint64_t ortho_bucket = (std::abs(c75w - 1.0f) < 0.01f) ? 1ull : 0ull;
        // Dedup richer: vs, prim, ortho, depth-state, rt_pitch — so 3D vs 2D
        // variants of the SAME shader each surface separately.
        const uint64_t key = sig.vs_hash
                           ^ (uint64_t(sig.prim_type) << 56)
                           ^ (ortho_bucket << 55)
                           ^ (uint64_t(z_enable) << 54)
                           ^ (uint64_t(z_write) << 53)
                           ^ (uint64_t(sig.rt_surface_pitch) << 20);
        bool seen = false;
        for (uint32_t i = 0; i < s_seen_count; ++i)
            if (s_seen[i] == key) { seen = true; break; }
        if (!seen && s_seen_count < 512) {
            s_seen[s_seen_count++] = key;
            const bool ortho = std::abs(c[3]) < 0.01f && std::abs(c[7]) < 0.01f &&
                               std::abs(c[11]) < 0.01f && std::abs(c[15] - 1.0f) < 0.01f;
            const bool is_qm = (sig.vs_hash == kNativeTargetQuadListVs) ||
                               (sig.vs_hash == kNativeTargetMeshVs) ||
                               (sig.vs_hash == kNativeTargetWorldStripVs);
            REXLOG_ERROR("AR_DIAG vs={:016x} prim={} rtpitch={} dfmt={} zEn={} zWr={} "
                         "col0x={:.3f} Yscale={:.3f} col3=({:.2f},{:.2f},{:.2f},{:.2f}) "
                         "ortho={} known_mvp={} PATCH={}",
                         sig.vs_hash, (unsigned)sig.prim_type, sig.rt_surface_pitch,
                         (unsigned)sig.rt_depth_format, z_enable?1:0, z_write?1:0,
                         c[0], c[5], c[3], c[7], c[11], c[15],
                         ortho?1:0, is_qm?1:0, (is_qm && !ortho)?1:0);
        }
    }

    // Bisection toggle: DAYTONA_AR_NOPATCH=1 disables ALL register patching while
    // leaving the ultrawide present (letterbox=false + window) intact. Used to tell
    // whether disappearing 2D/billboards are caused by the projection patch or by
    // the present-fill itself.
    static const bool s_ar_nopatch = EnvEnabled("DAYTONA_AR_NOPATCH");
    const bool uses_mvp_in_c72 =
        (sig.vs_hash == kNativeTargetQuadListVs) ||
        (sig.vs_hash == kNativeTargetMeshVs) ||
        (sig.vs_hash == kNativeTargetWorldStripVs);
    if (!s_ar_nopatch && (g_ar_scale < 0.999f || g_ar_scale > 1.001f) && uses_mvp_in_c72) {
        const auto vs_const = rf->Get<reg::SQ_VS_CONST>(
            rex::graphics::XE_GPU_REG_SQ_VS_CONST);
        const uint32_t abs72 = vs_const.base + 72;
        if (abs72 + 3 < 512) {
            const uint32_t reg0 =
                rex::graphics::XE_GPU_REG_SHADER_CONSTANT_000_X + 4 * abs72;
            auto load = [&](uint32_t off) {
                float v = 0.0f;
                std::memcpy(&v, &rf->values[reg0 + off], sizeof(float));
                return v;
            };
            // Column 3 = (c72[3]=+3, c73[3]=+7, c74[3]=+11, c75[3]=+15).
            const float c72_3 = load(3);
            const float c73_3 = load(7);
            const float c74_3 = load(11);
            const float c75_3 = load(15);
            const bool is_ortho =
                std::abs(c72_3) < 0.01f && std::abs(c73_3) < 0.01f &&
                std::abs(c74_3) < 0.01f && std::abs(c75_3 - 1.0f) < 0.01f;
            if (!is_ortho) {
                // Scale column 0 = (c72[0]=+0, c73[0]=+4, c74[0]=+8, c75[0]=+12).
                //
                // IDEMPOTENT WRITE: the game does NOT re-upload c72..c75 before
                // every draw — some draws (e.g. the billboard strips) reuse the
                // constants left in the register file. A naive `reg *= scale`
                // therefore compounds frame-over-frame (scale^n → 0), collapsing
                // that geometry to a point. To avoid this we remember the last
                // values we wrote and the original they came from: if the current
                // register still holds exactly what we wrote (i.e. the game did
                // not re-upload), we re-derive from the stored original instead of
                // multiplying our own output again. This makes the patch stable
                // regardless of whether the game re-uploads.
                static float s_last_written[4] = {0, 0, 0, 0};
                static float s_last_origin[4]  = {0, 0, 0, 0};
                static bool  s_have_last = false;
                const uint32_t col0_off[4] = {0u, 4u, 8u, 12u};

                float cur[4];
                for (int i = 0; i < 4; ++i) cur[i] = load(col0_off[i]);

                bool is_our_leftover = s_have_last;
                for (int i = 0; i < 4 && is_our_leftover; ++i) {
                    const float tol = 1e-6f * (1.0f + std::abs(s_last_written[i]));
                    if (std::abs(cur[i] - s_last_written[i]) > tol)
                        is_our_leftover = false;
                }

                auto* mrf = const_cast<rex::graphics::RegisterFile*>(rf);
                for (int i = 0; i < 4; ++i) {
                    const float origin = is_our_leftover ? s_last_origin[i] : cur[i];
                    const float v = origin * g_ar_scale;
                    std::memcpy(&mrf->values[reg0 + col0_off[i]], &v, sizeof(float));
                    s_last_origin[i]  = origin;
                    s_last_written[i] = v;
                }
                s_have_last = true;
            }
        }
    }

    const bool recommended_quadlist_target  = IsRecommendedNativeQuadListTarget(sig);
    const bool recommended_pointlist_target = IsRecommendedNativePointListTarget(sig);
    const bool recommended_mesh_target      = IsRecommendedNativeMeshTarget(sig);
    const bool recommended_mesh_fan_target  = IsRecommendedNativeMeshFanTarget(sig);
    const bool allow_native_suppression = NativeSuppressionUnlocked();
    // Lightweight project-owned context: the base CommandProcessor pointer.
    // Virtual dispatch routes to VulkanCommandProcessor's *Impl overrides.
    const daytona::NativeCtx native_ctx { cp };
    // Convert DaytonaIndexBufferInfo → project IBI (trivially layout-compatible).
    daytona::IBI proj_ibi{};
    const daytona::IBI* proj_ibi_ptr = nullptr;
    if (index_buffer_info) {
        proj_ibi.format     = index_buffer_info->format;
        proj_ibi.endianness = index_buffer_info->endianness;
        proj_ibi.count      = index_buffer_info->count;
        proj_ibi.guest_base = index_buffer_info->guest_base;
        proj_ibi.length     = index_buffer_info->length;
        proj_ibi_ptr        = &proj_ibi;
    }

    if (recommended_quadlist_target) {
        ++g_native_shadow_quadlist_hits;
        if (diagnostics_enabled) {
            CaptureRecommendedQuadListSample(cp, *rf, vs, sig);
        }
        // Phase 2+: lazily initialize native pipeline; Phase 3 returns true to suppress ReXGlue.
        if (allow_native_suppression &&
            daytona::NativeIssueDraw(native_ctx, prim_type, index_count, proj_ibi_ptr)) {
            ++g_native_quadlist_takeover_count;
            return true;
        }
        const bool shadow_enabled = EnvEnabled("DAYTONA_NATIVE_SHADOW_QUADLIST");
        if (shadow_enabled) {
            ++g_native_shadow_quadlist_enabled_hits;
            if (g_native_shadow_quadlist_enabled_hits <= 16 ||
                IsPowerOfTwo(g_native_shadow_quadlist_enabled_hits)) {
                REXLOG_ERROR("DaytonaRenderer: native shadow QuadList hit #{} total={} "
                             "idx={} tex={:08X} vtx={:08X}/{:08X}/{:08X} rt_pitch={} "
                             "vs={:016x} ps={:016x}; passthrough=true",
                             g_native_shadow_quadlist_enabled_hits,
                             g_native_shadow_quadlist_hits,
                             sig.index_count, sig.tex_mask, sig.vtx_mask,
                             sig.vtx_mask_mid, sig.vtx_mask_hi,
                             sig.rt_surface_pitch, sig.vs_hash, sig.ps_hash);
            }
        }
    }
    if (recommended_pointlist_target) {
        ++g_native_shadow_pointlist_hits;
        if (diagnostics_enabled) {
            CaptureRecommendedPointListSample(cp, *rf, vs, sig);
        }
        if (allow_native_suppression &&
            daytona::NativeIssuePointList(native_ctx, index_count, proj_ibi_ptr)) {
            ++g_native_pointlist_takeover_count;
            return true;
        }
        const bool shadow_enabled = EnvEnabled("DAYTONA_NATIVE_SHADOW_POINTLIST");
        if (shadow_enabled) {
            ++g_native_shadow_pointlist_enabled_hits;
            if (g_native_shadow_pointlist_enabled_hits <= 16 ||
                IsPowerOfTwo(g_native_shadow_pointlist_enabled_hits)) {
                REXLOG_ERROR("DaytonaRenderer: native shadow PointList hit #{} total={} "
                             "idx={} tex={:08X} vtx={:08X}/{:08X}/{:08X} rt_pitch={} "
                             "vs={:016x} ps={:016x}; passthrough=true",
                             g_native_shadow_pointlist_enabled_hits,
                             g_native_shadow_pointlist_hits,
                             sig.index_count, sig.tex_mask, sig.vtx_mask,
                             sig.vtx_mask_mid, sig.vtx_mask_hi,
                             sig.rt_surface_pitch, sig.vs_hash, sig.ps_hash);
            }
        }
    }
    if (recommended_mesh_target || recommended_mesh_fan_target) {
        ++g_native_shadow_mesh_hits;
        if (diagnostics_enabled) {
            CaptureRecommendedMeshSample(cp, *rf, vs, sig, index_buffer_info);
        }
        if (allow_native_suppression &&
            daytona::NativeIssueMesh(native_ctx, prim_type, index_count, proj_ibi_ptr)) {
            ++g_native_mesh_takeover_count;
            return true;
        }
        const bool shadow_enabled = EnvEnabled("DAYTONA_NATIVE_SHADOW_MESH");
        if (shadow_enabled) {
            ++g_native_shadow_mesh_enabled_hits;
            if (g_native_shadow_mesh_enabled_hits <= 16 ||
                IsPowerOfTwo(g_native_shadow_mesh_enabled_hits)) {
                REXLOG_ERROR("DaytonaRenderer: native shadow Mesh hit #{} total={} "
                             "idx={} tex={:08X} vtx={:08X}/{:08X}/{:08X} rt_pitch={} "
                             "vs={:016x} ps={:016x}; passthrough=true",
                             g_native_shadow_mesh_enabled_hits,
                             g_native_shadow_mesh_hits,
                             sig.index_count, sig.tex_mask, sig.vtx_mask,
                             sig.vtx_mask_mid, sig.vtx_mask_hi,
                             sig.rt_surface_pitch, sig.vs_hash, sig.ps_hash);
            }
        }
    }

    // Phase 1.4: even shadow-target hits return false.  The first true takeover
    // requires native shader translation, vertex decode, texture binding, and
    // render-target output to be validated for this class.
    return false;
}

// ── Public interface ──────────────────────────────────────────────────────────

void DaytonaRenderer::Install() {
    rex::graphics::CommandProcessor::SetDaytonaDrawHook(&DaytonaRenderer::OnIssueDraw);

    // Aspect-ratio correction for ANY display ratio (4:3, 16:10, 16:9, ultrawide).
    // Computes gameAR/windowAR from the configured window size and disables
    // letterboxing so the present fills the window; the per-draw c72 patch in
    // OnIssueDraw keeps geometry proportionally correct with no bars/stretching.
    // Set window_width/window_height in daytona.ini to your display resolution.
    {
        const std::string ws = rex::cvar::GetFlagByName("window_width");
        const std::string hs = rex::cvar::GetFlagByName("window_height");
        int w = 0, h = 0;
        std::from_chars(ws.data(), ws.data() + ws.size(), w);
        std::from_chars(hs.data(), hs.data() + hs.size(), h);
        if (w > 0 && h > 0) {
            const float window_ar = static_cast<float>(w) / static_cast<float>(h);
            constexpr float kGameAr = 16.0f / 9.0f;
            g_ar_scale = kGameAr / window_ar;
            // Only act when the window AR meaningfully differs from 16:9.
            if (g_ar_scale < 0.99f || g_ar_scale > 1.01f) {
                rex::cvar::SetFlagByName("present_letterbox", "false");
                const char* shape =
                    window_ar > kGameAr ? "wider-than-16:9 (Hor+)"
                                        : "narrower-than-16:9";
                REXLOG_ERROR("DaytonaRenderer: aspect correction {}x{} ar={:.3f} "
                             "({}) c72 scale={:.4f} letterbox=off",
                             w, h, window_ar, shape, g_ar_scale);
            } else {
                g_ar_scale = 1.0f;  // exactly 16:9 — no correction needed
            }
        }
    }

    const bool notex    = EnvEnabled("DAYTONA_NATIVE_QUADLIST_NOTEXTURED");
    const bool mesh_fan = EnvEnabled("DAYTONA_NATIVE_MESH_FAN");
    const bool quad_3d  = EnvEnabled("DAYTONA_NATIVE_QUADLIST_3D");
    REXLOG_ERROR("DaytonaRenderer: installed IssueDraw hook — "
                 "native paths active: pointlist=on mesh=on mesh_2d=on quadlist=on "
                 "quadlist_textured={} quadlist_3d={} mesh_fan={} aspect_correct={}",
                 notex ? "no(debug)" : "yes",
                 quad_3d ? "on" : "off",
                 mesh_fan ? "on" : "off",
                 (g_ar_scale < 0.999f || g_ar_scale > 1.001f) ? "on" : "off");
}

void DaytonaRenderer::Flush() {
    if (!RendererDiagnosticsEnabled()) {
        return;
    }
    WriteCatalog();
    WriteObjectInventory();
    WriteNativeTargetPlan();
    WriteRecommendedQuadListSample();
    WriteRecommendedQuadListSamples();
    WriteRecommendedPointListSample();
    WriteRecommendedMeshSamples();
}

}  // namespace daytona
