/**
 * Daytona native Vulkan renderer — project-owned dispatch.
 *
 * Uses only the public SDK base-class header (command_processor.h) which is
 * in the project's include path. The SDK's VulkanCommandProcessor overrides
 * the virtual *Impl methods so no Vulkan/glslang headers are needed here.
 */

#include "daytona_vulkan_renderer.h"

#include <rex/graphics/command_processor.h>  // CommandProcessor + DaytonaIndexBufferInfo
#include <rex/graphics/xenos.h>              // PrimitiveType (lightweight, in public include)

namespace daytona {

using SDKIBI = rex::graphics::CommandProcessor::DaytonaIndexBufferInfo;

static SDKIBI to_sdk(const IBI* src) {
    SDKIBI d{};
    if (src) {
        d.format      = src->format;
        d.endianness  = src->endianness;
        d.count       = src->count;
        d.guest_base  = src->guest_base;
        d.length      = src->length;
    }
    return d;
}

bool NativeIssueDraw(NativeCtx ctx,
                     rex::graphics::xenos::PrimitiveType prim_type,
                     uint32_t index_count,
                     const IBI* ibi) {
    SDKIBI d = to_sdk(ibi);
    return ctx.cp->DaytonaNativeIssueDrawImpl(prim_type, index_count, ibi ? &d : nullptr);
}

bool NativeIssuePointList(NativeCtx ctx,
                          uint32_t index_count,
                          const IBI* ibi) {
    SDKIBI d = to_sdk(ibi);
    return ctx.cp->DaytonaNativeIssuePointListImpl(index_count, ibi ? &d : nullptr);
}

bool NativeIssueMesh(NativeCtx ctx,
                     rex::graphics::xenos::PrimitiveType prim_type,
                     uint32_t index_count,
                     const IBI* ibi) {
    SDKIBI d = to_sdk(ibi);
    return ctx.cp->DaytonaNativeIssueMeshImpl(prim_type, index_count, ibi ? &d : nullptr);
}

} // namespace daytona
