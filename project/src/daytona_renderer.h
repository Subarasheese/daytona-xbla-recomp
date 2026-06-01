// Daytona USA – native GPU renderer (replaces ReXGlue Xenos emulation).
// Phase 0+1: hook infrastructure + draw-call cataloger.
// Phase 2+: shader → SPIR-V pipeline, texture/vertex buffer ownership, draw submission.
#pragma once

#include <rex/graphics/command_processor.h>
#include <rex/graphics/register_file.h>
#include <rex/graphics/registers.h>
#include <rex/graphics/xenos.h>

namespace daytona {

namespace xenos = rex::graphics::xenos;

// One canonical description of a unique draw call type observed at runtime.
// Two draws sharing the same DrawSig are structurally identical from the
// renderer's perspective and can share the same pipeline object.
struct DrawSig {
    uint64_t vs_hash = 0;       // Shader::ucode_data_hash() for vertex shader
    uint64_t ps_hash = 0;       // Shader::ucode_data_hash() for pixel shader

    // Primitive type issued
    uint8_t prim_type = 0;

    // Index count is part of the native draw shape.  Without it, rectangle,
    // quad, and postprocess variants using the same shaders collapse together.
    uint32_t index_count = 0;

    // Descriptor hashes keep the catalog from merging unrelated texture identities
    // while keeping streamed vertex buffers grouped by layout-like state.
    // Texture hash includes resource identity. Vertex hash intentionally excludes
    // transient base addresses so dynamic VBO allocations don't explode the catalog.
    uint64_t tex_desc_hash = 0;
    uint64_t vtx_desc_hash = 0;

    // Render target identity in EDRAM terms.
    uint32_t rt_edram_base = 0;
    uint32_t rt_surface_pitch = 0;
    uint32_t depth_edram_base = 0;

    // Active texture fetch slots (bit i set → TFETCH i has a non-zero base addr)
    uint32_t tex_mask = 0;
    // Active vertex fetch slots 0..31 (legacy field kept for quick scans).
    uint32_t vtx_mask = 0;
    // Active vertex fetch slots 32..63 and 64..95. Daytona's main QuadList
    // target uses high VFETCH constants, so these are part of identity.
    uint32_t vtx_mask_mid = 0;
    uint32_t vtx_mask_hi = 0;

    // Render target 0 color format (ColorRenderTargetFormat)
    uint8_t rt_color_format = 0;
    // Depth format (DepthRenderTargetFormat), 0xFF = no depth
    uint8_t rt_depth_format = 0xFF;

    bool operator==(const DrawSig& o) const {
        return vs_hash == o.vs_hash && ps_hash == o.ps_hash &&
               prim_type == o.prim_type && index_count == o.index_count &&
               tex_mask == o.tex_mask && vtx_mask == o.vtx_mask &&
               vtx_mask_mid == o.vtx_mask_mid &&
               vtx_mask_hi == o.vtx_mask_hi &&
               tex_desc_hash == o.tex_desc_hash &&
               vtx_desc_hash == o.vtx_desc_hash &&
               rt_color_format == o.rt_color_format &&
               rt_depth_format == o.rt_depth_format &&
               rt_edram_base == o.rt_edram_base &&
               rt_surface_pitch == o.rt_surface_pitch &&
               depth_edram_base == o.depth_edram_base;
    }
};

// Full state snapshot for a DrawSig — the extra detail we log once per unique sig.
struct DrawRecord {
    DrawSig sig;
    uint32_t first_index_count = 0;
    uint32_t major_mode_explicit = 0;

    struct IndexInfo {
        bool valid = false;
        uint32_t format = 0;
        uint32_t endianness = 0;
        uint32_t count = 0;
        uint32_t guest_base = 0;
        uint64_t length = 0;
    } index;

    struct ShaderInfo {
        bool valid = false;
        uint64_t hash = 0;
        uint32_t type = 0;
        uint32_t ucode_dwords = 0;
        uint32_t ucode_storage_index = 0xFFFFFFFFu;
        uint32_t analyzed = 0;
        uint32_t texture_binding_count = 0;
        uint32_t vertex_binding_count = 0;
        uint32_t memexport_written_mask = 0;
        uint32_t writes_color_targets = 0;
        uint32_t writes_interpolators = 0;
        uint32_t writes_depth = 0;
        uint32_t kills_pixels = 0;
        char ucode_path[160] = {};
        char disasm_path[160] = {};
    };
    ShaderInfo vs;
    ShaderInfo ps;

    // Per-texture slot detail (indexed by slot number 0..31)
    struct TexDetail {
        bool active = false;
        uint32_t base_phys = 0;
        uint32_t mip_phys = 0;
        uint16_t width = 0;
        uint16_t height = 0;
        uint8_t  format = 0;      // xenos::TextureFormat
        uint8_t  dimension = 0;   // xenos::DataDimension
        uint8_t  tiled = 0;
        uint8_t  endian = 0;
    } tex[32];

    // Per-vertex fetch slot detail (indexed by slot 0..95)
    struct VtxDetail {
        bool active = false;
        uint32_t base_phys = 0;
        uint32_t size_bytes = 0;
        uint8_t  endian = 0;
    } vtx[96];

    // Render target
    uint32_t rt_edram_base = 0;
    uint32_t rt_surface_pitch = 0;
    uint32_t depth_edram_base = 0;

    // Representative raw state needed for native pipeline construction.
    static constexpr uint32_t kRawRegCount = 32;
    uint32_t raw_regs[kRawRegCount] = {};

    uint64_t draw_count = 0;  // how many times this sig was seen
};

class DaytonaRenderer {
public:
    // Register the IssueDraw hook. Call once at startup before the emulator runs.
    static void Install();

    // Flush catalog to disk. Call at shutdown.
    static void Flush();

private:
    static bool OnIssueDraw(rex::graphics::CommandProcessor* cp,
                            xenos::PrimitiveType prim_type,
                            uint32_t index_count,
                            const rex::graphics::CommandProcessor::DaytonaIndexBufferInfo* index_buffer_info,
                            bool major_mode_explicit);

    static DrawSig BuildSig(const rex::graphics::RegisterFile& regs,
                            rex::graphics::Shader* vs,
                            rex::graphics::Shader* ps,
                            xenos::PrimitiveType prim_type,
                            uint32_t index_count);
    static bool IsRecommendedNativeQuadListTarget(const DrawSig& sig);
    static bool IsRecommendedNativePointListTarget(const DrawSig& sig);
    static bool IsRecommendedNativeMeshTarget(const DrawSig& sig);
    static bool IsRecommendedNativeMeshFanTarget(const DrawSig& sig);
    static void CaptureRecommendedQuadListSample(
        rex::graphics::CommandProcessor* cp, const rex::graphics::RegisterFile& regs,
        rex::graphics::Shader* vs, const DrawSig& sig);
    static void CaptureRecommendedPointListSample(
        rex::graphics::CommandProcessor* cp, const rex::graphics::RegisterFile& regs,
        rex::graphics::Shader* vs, const DrawSig& sig);
    static void CaptureRecommendedMeshSample(
        rex::graphics::CommandProcessor* cp, const rex::graphics::RegisterFile& regs,
        rex::graphics::Shader* vs, const DrawSig& sig,
        const rex::graphics::CommandProcessor::DaytonaIndexBufferInfo* index_buffer_info);

    static void PopulateRecord(DrawRecord& rec,
                               const rex::graphics::RegisterFile& regs,
                               rex::graphics::Shader* vs,
                               rex::graphics::Shader* ps,
                               uint32_t index_count,
                               const rex::graphics::CommandProcessor::DaytonaIndexBufferInfo* index_buffer_info,
                               bool major_mode_explicit);

    static void WriteCatalog();
    static void WriteObjectInventory();
    static void WriteNativeTargetPlan();
    static void WriteRecommendedQuadListSample();
    static void WriteRecommendedQuadListSamples();
    static void WriteRecommendedPointListSample();
    static void WriteRecommendedMeshSamples();
};

}  // namespace daytona
