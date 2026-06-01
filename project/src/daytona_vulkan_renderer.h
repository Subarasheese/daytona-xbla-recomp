#pragma once
#include <cstdint>
/**
 * Daytona native Vulkan renderer — project-owned interface.
 *
 * Uses only the base CommandProcessor (public SDK header) so this file compiles
 * without the SDK's private Vulkan/glslang/renderdoc include paths. The SDK's
 * VulkanCommandProcessor overrides the virtual *Impl methods called here.
 */

namespace rex::graphics {
  class CommandProcessor;
  namespace xenos { enum class PrimitiveType : uint32_t; }
}

namespace daytona {

// Mirrors CommandProcessor::DaytonaIndexBufferInfo without including the header.
struct IBI {
    uint32_t format;
    uint32_t endianness;
    uint32_t count;
    uint32_t guest_base;
    uint64_t length;
};

// Context: only the base-class pointer is needed — the virtual dispatch
// handles the rest without the project needing Vulkan SDK headers.
struct NativeCtx {
    rex::graphics::CommandProcessor* cp;
};

bool NativeIssueDraw(NativeCtx ctx,
                     rex::graphics::xenos::PrimitiveType prim_type,
                     uint32_t index_count,
                     const IBI* ibi);

bool NativeIssuePointList(NativeCtx ctx,
                          uint32_t index_count,
                          const IBI* ibi);

bool NativeIssueMesh(NativeCtx ctx,
                     rex::graphics::xenos::PrimitiveType prim_type,
                     uint32_t index_count,
                     const IBI* ibi);

} // namespace daytona
