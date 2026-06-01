#include "daytona_inspector.h"
#include "daytona_render.h"

#include <imgui.h>
#include <algorithm>

namespace {

const char* EventKindName(daytona_render::EventKind k) {
    using K = daytona_render::EventKind;
    switch (k) {
        case K::kFrameBegin:      return "FRAME_BEGIN";
        case K::kFrameEnd:        return "FRAME_END";
        case K::kRenderTargetBind:return "RT_BIND";
        case K::kClear:           return "CLEAR";
        case K::kDrawPrecheck:    return "DRAW_PRE";
        case K::kDrawExecute:     return "DRAW_EXEC";
        case K::kTextureProcess:  return "TEX_PROC";
        case K::kShaderBuildEnter:return "SHDR_ENT";
        case K::kShaderBuildExit: return "SHDR_EXT";
        case K::kRenderPassBegin: return "PASS_BGN";
        case K::kRenderPassEnd:   return "PASS_END";
        case K::kStateChange:     return "STATE";
        case K::kCommandSubmit:   return "CMD_SUB";
    }
    return "?";
}

ImVec4 EventKindColor(daytona_render::EventKind k) {
    using K = daytona_render::EventKind;
    switch (k) {
        case K::kDrawExecute:     return {1.0f, 0.6f, 0.2f, 1.0f}; // orange
        case K::kDrawPrecheck:    return {0.9f, 0.5f, 0.1f, 1.0f};
        case K::kTextureProcess:  return {0.4f, 0.9f, 0.4f, 1.0f}; // green
        case K::kRenderPassBegin:
        case K::kRenderPassEnd:   return {0.4f, 0.7f, 1.0f, 1.0f}; // blue
        case K::kShaderBuildEnter:
        case K::kShaderBuildExit: return {1.0f, 0.4f, 0.8f, 1.0f}; // pink
        case K::kRenderTargetBind:return {0.9f, 0.9f, 0.3f, 1.0f}; // yellow
        default:                  return {0.7f, 0.7f, 0.7f, 1.0f};
    }
}

const char* kKindFilterLabels[] = {
    "All", "RT_BIND", "CLEAR", "DRAW_PRE", "DRAW_EXEC",
    "TEX_PROC", "SHDR", "PASS", "STATE", "CMD_SUB",
};

// ── PM4 prim-type names ──────────────────────────────────────────────────────
const char* PrimTypeName(uint8_t pt) {
    switch (pt) {
        case 0:  return "NONE";
        case 1:  return "POINT_LIST";
        case 2:  return "LINE_LIST";
        case 3:  return "LINE_STRIP";
        case 4:  return "TRI_LIST";
        case 5:  return "TRI_FAN";
        case 6:  return "TRI_STRIP";
        case 8:  return "RECT_LIST";
        case 9:  return "LINE_LOOP";
        case 10: return "QUAD_LIST";
        case 11: return "QUAD_STRIP";
        default: {
            thread_local char buf[8];
            snprintf(buf, sizeof(buf), "PT%02X", pt);
            return buf;
        }
    }
}

// ── PM4 opcode names ─────────────────────────────────────────────────────────
const char* PM4OpName(uint8_t op) {
    switch (op) {
        case 0x10: return "NOP";
        case 0x12: return "IM_LOAD";
        case 0x19: return "LOAD_ALU_K";
        case 0x1A: return "SET_CONST2";
        case 0x1B: return "EXEC_CS_RAM";
        case 0x21: return "REG_RMW";
        case 0x23: return "VIZ_QUERY";
        case 0x26: return "LOAD_ALU_K2";
        case 0x27: return "IM_LOAD2";
        case 0x2B: return "SET_CONST";
        case 0x2C: return "DRAW_INDX";
        case 0x2D: return "DRAW_INDX_BIN";
        case 0x2F: return "LOAD_CTX_REG";  // loads constants from phys mem, NOT a draw
        case 0x31: return "IM_LOAD_IMM";
        case 0x33: return "DRAW_INDX_BIN2";
        case 0x36: return "DRAW_INDX_2";
        case 0x39: return "SET_SHADER_K";
        case 0x3B: return "INVAL_STATE";
        case 0x3C: return "WAIT_REG_MEM";
        case 0x3D: return "MEM_WRITE";
        case 0x3E: return "REG_TO_MEM";
        case 0x3F: return "INDIRECT_BUF";
        case 0x43: return "DRAW_INDX_IMM";
        case 0x45: return "COND_WRITE";
        case 0x46: return "EVENT_WRITE";
        case 0x47: return "EVT_WR_EOP";
        case 0x58: return "EVT_WR_SHD";
        case 0x59: return "EVT_WR_ZPD";
        case 0x5A: return "EVT_WR_EXT";
        case 0x64: return "XE_SWAP";
        default: {
            thread_local char buf[8];
            snprintf(buf, sizeof(buf), "[%02X]", op);
            return buf;
        }
    }
}

const char* TextureFormatName(uint32_t fmt) {
    switch (fmt) {
        case 6:  return "8_8_8_8";
        case 10: return "8_8";
        case 20: return "DXT5";
        default: {
            thread_local char buf[12];
            snprintf(buf, sizeof(buf), "fmt%u", fmt);
            return buf;
        }
    }
}

const char* TextureHostFormatName(uint32_t fmt) {
    switch (fmt) {
        case 1: return "RGBA8";
        case 2: return "RG8";
        case 3: return "BC3";
        default: return "?";
    }
}

const char* TextureCacheStateName(uint32_t state) {
    switch (state) {
        case 1: return "meta";
        case 2: return "cand";
        case 3: return "linear";
        default: return "-";
    }
}

// Read one big-endian dword from a GPU physical address via the uncached alias.
inline uint32_t ReadPhysU32(uint32_t phys) {
    return daytona_render::GuestReadU32(0xA0000000u | (phys & 0x1FFFFFFFu));
}

bool MatchesFilter(daytona_render::EventKind k, int filter) {
    using K = daytona_render::EventKind;
    if (filter <= 0) return true;
    switch (filter) {
        case 1:  return k == K::kRenderTargetBind;
        case 2:  return k == K::kClear;
        case 3:  return k == K::kDrawPrecheck;
        case 4:  return k == K::kDrawExecute;
        case 5:  return k == K::kTextureProcess;
        case 6:  return k == K::kShaderBuildEnter || k == K::kShaderBuildExit;
        case 7:  return k == K::kRenderPassBegin || k == K::kRenderPassEnd;
        case 8:  return k == K::kStateChange;
        case 9:  return k == K::kCommandSubmit;
        default: return true;
    }
}

}  // namespace

DaytonaInspector::DaytonaInspector(rex::ui::ImGuiDrawer* drawer)
    : ImGuiDialog(drawer) {}

void DaytonaInspector::OnDraw(ImGuiIO& /*io*/) {
    const auto* cap = daytona_render::GetLastCapture();

    // Auto-reopen the window whenever a new capture lands.
    if (cap && cap->stats.frame_index != last_seen_frame_) {
        last_seen_frame_ = cap->stats.frame_index;
        selected_event_ = -1;
        open_ = true;
    }

    if (!open_) return;

    ImGui::SetNextWindowPos(ImVec2(340.0f, 10.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(800.0f, 620.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Daytona Frame Inspector##insp", &open_)) {
        ImGui::End();
        return;
    }

    // ── Frame header (always visible) ─────────────────────────────────────────
    if (cap) {
        const auto& s = cap->stats;
        ImGui::Text("Frame %-6llu  Events %u / %u  Overflow %u",
                    static_cast<unsigned long long>(s.frame_index),
                    cap->event_count,
                    daytona_render::FrameCapture::kMaxEvents,
                    cap->overflow_count);
        ImGui::Text("Draw %u  Pass %u  Tex %u  Shdr %u  State %u  RT %u  Cmd %u",
                    s.draw_execute_count, s.render_pass_count,
                    s.texture_process_count, s.shader_build_count,
                    s.state_change_count, s.rt_bind_count,
                    s.command_submit_count);
        ImGui::Text("DrawSnaps %u / %u   TexSnaps %u / %u   TexLoaded %u   Shaders %u",
                    cap->draw_snap_count, daytona_render::FrameCapture::kMaxDrawSnaps,
                    cap->tex_snap_count,  daytona_render::FrameCapture::kMaxTexSnaps,
                    daytona_render::GetTexEntryCount(),
                    daytona_render::GetShaderEntryCount());
    } else {
        ImGui::TextDisabled("No capture yet — press the Inspect button in the perf overlay.");
    }

    ImGui::Separator();

    // ── Tab bar ───────────────────────────────────────────────────────────────
    if (ImGui::BeginTabBar("##inspector_tabs")) {
        if (ImGui::BeginTabItem("Events")) {
            DrawEventsTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Textures")) {
            DrawTexturesTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Shaders")) {
            DrawShadersTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void DaytonaInspector::DrawEventsTab() {
    const auto* cap = daytona_render::GetLastCapture();
    if (!cap) { ImGui::TextDisabled("No capture."); return; }

    ImGui::SetNextItemWidth(110.0f);
    ImGui::Combo("Filter##kind", &filter_kind_,
                 kKindFilterLabels,
                 static_cast<int>(sizeof(kKindFilterLabels) / sizeof(kKindFilterLabels[0])));
    ImGui::SameLine();
    ImGui::Checkbox("Memory", &show_memory_);
    ImGui::SameLine();
    if (ImGui::Button("Deselect")) selected_event_ = -1;

    ImGui::Separator();
    DrawEventTable();
}

void DaytonaInspector::DrawEventTable() {
    const auto* cap = daytona_render::GetLastCapture();
    if (!cap) return;

    constexpr ImGuiTableFlags kFlags =
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit;

    const float row_height = ImGui::GetTextLineHeightWithSpacing();
    // Show the detail panel below the table when something is selected.
    float detail_h = 0.0f;
    if (selected_event_ >= 0 && show_memory_) {
        const auto& sev = cap->events[static_cast<uint32_t>(selected_event_)];
        detail_h = (sev.kind == daytona_render::EventKind::kCommandSubmit) ? 420.0f : 160.0f;
    }
    const float table_h = ImGui::GetContentRegionAvail().y - detail_h - 8.0f;

    if (!ImGui::BeginTable("##events", 9, kFlags, ImVec2(0.0f, table_h))) return;

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("#",    ImGuiTableColumnFlags_WidthFixed, 48.0f);
    ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Seq",  ImGuiTableColumnFlags_WidthFixed, 48.0f);
    ImGui::TableSetupColumn("r3",   ImGuiTableColumnFlags_WidthFixed, 84.0f);
    ImGui::TableSetupColumn("r4",   ImGuiTableColumnFlags_WidthFixed, 84.0f);
    ImGui::TableSetupColumn("r5",   ImGuiTableColumnFlags_WidthFixed, 84.0f);
    ImGui::TableSetupColumn("r6",   ImGuiTableColumnFlags_WidthFixed, 84.0f);
    ImGui::TableSetupColumn("r7",   ImGuiTableColumnFlags_WidthFixed, 84.0f);
    ImGui::TableSetupColumn("r8",   ImGuiTableColumnFlags_WidthFixed, 84.0f);
    ImGui::TableHeadersRow();

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(cap->event_count));

    using K = daytona_render::EventKind;

    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            const auto& ev = cap->events[static_cast<uint32_t>(i)];

            // Update ordinal counters for snaps (only when iterating from top).
            // Clipper may skip rows — we cannot use this for snap indexing.
            // Instead we rely on pre-computed snap counts.
            if (!MatchesFilter(ev.kind, filter_kind_)) continue;

            ImGui::TableNextRow();
            const bool selected = (selected_event_ == i);

            ImGui::TableSetColumnIndex(0);
            char label[16];
            snprintf(label, sizeof(label), "%d", i);
            if (ImGui::Selectable(label, selected,
                                  ImGuiSelectableFlags_SpanAllColumns,
                                  ImVec2(0, row_height))) {
                selected_event_ = (selected_event_ == i) ? -1 : i;
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(EventKindColor(ev.kind), "%s", EventKindName(ev.kind));

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%u", ev.seq);

            for (int c = 0; c < 6; ++c) {
                ImGui::TableSetColumnIndex(3 + c);
                if (ev.r[c])
                    ImGui::Text("%08X", ev.r[c]);
                else
                    ImGui::TextDisabled("--------");
            }

        }
    }
    clipper.End();
    ImGui::EndTable();

    // ── Detail panel for selected event ──────────────────────────────────────
    if (selected_event_ >= 0 && show_memory_ && detail_h > 0.0f) {
        const uint32_t idx = static_cast<uint32_t>(selected_event_);
        if (idx < cap->event_count) {
            const auto& ev = cap->events[idx];
            ImGui::Separator();

            // Scan for the snap index that matches this event's ordinal.
            // Count how many draw/tex events precede index idx.
            int d = 0, t = 0;
            for (uint32_t j = 0; j < idx; ++j) {
                if (cap->events[j].kind == K::kDrawExecute) ++d;
                if (cap->events[j].kind == K::kTextureProcess) ++t;
            }

            if (ev.kind == K::kDrawExecute && d < static_cast<int>(cap->draw_snap_count)) {
                DrawDrawCallDetail(d);
            } else if (ev.kind == K::kTextureProcess && t < static_cast<int>(cap->tex_snap_count)) {
                DrawTexDetail(t);
            } else if (ev.kind == K::kCommandSubmit) {
                // CMD_SUB: r3=device, r4=descriptor, r5=count,
                //          r[3]=wptr_before, r[4]=wptr_after (dword indices into ring buffer).
                const uint32_t rb_phys  = daytona_render::GetRingBufferPhys();
                const uint32_t wptr_bef = ev.r[3];
                const uint32_t wptr_aft = ev.r[4];
                const uint32_t pm4_dwords = (wptr_aft >= wptr_bef)
                                            ? (wptr_aft - wptr_bef)
                                            : (daytona_render::GetRingBufferSize() / 4 - wptr_bef + wptr_aft);
                ImGui::Text("device=%08X  desc=%08X  wptr=[%08X,%08X) dwords=%u",
                            ev.r[0], ev.r[1], wptr_bef, wptr_aft, pm4_dwords);

                // Show raw ring buffer dwords and detect IT_INDIRECT_BUFFER (0x3F).
                if (rb_phys && pm4_dwords >= 1) {
                    const uint32_t hdr = daytona_render::ReadRingBufferDword(wptr_bef);
                    const uint32_t w1  = pm4_dwords >= 2 ? daytona_render::ReadRingBufferDword(wptr_bef + 1) : 0;
                    const uint32_t w2  = pm4_dwords >= 3 ? daytona_render::ReadRingBufferDword(wptr_bef + 2) : 0;
                    const uint8_t  op  = (hdr >> 8) & 0xFF;
                    const uint32_t cnt = ((hdr >> 16) & 0x3FFF) + 1;
                    ImGui::TextDisabled("rb[%05X] %08X %08X %08X  T3 op=%02X(%s) cnt=%u",
                                        wptr_bef, hdr, w1, w2, op, PM4OpName(op), cnt);

                    // Follow IT_INDIRECT_BUFFER to decode the actual draw commands.
                    if ((hdr >> 30) == 3 && op == 0x3F && cnt == 2)
                        DrawIndirectBuffer(w1, w2);
                }
            } else {
                // Generic live-read for non-snapped events.
                ImGui::Text("r3=%08X  r4=%08X  r5=%08X",
                            ev.r[0], ev.r[1], ev.r[2]);
                if (ev.r[0] >= 0x70000000u) {
                    uint32_t words[8]{};
                    for (int w = 0; w < 8; ++w)
                        words[w] = daytona_render::GuestReadU32(ev.r[0] + w * 4);
                    DrawGuestMemRow(ev.r[0], words, 8, "r3@");
                }
            }
        }
    }
}

void DaytonaInspector::DrawDrawCallDetail(int snap_idx) {
    const auto* cap = daytona_render::GetLastCapture();
    if (!cap || snap_idx < 0 || static_cast<uint32_t>(snap_idx) >= cap->draw_snap_count) return;
    const auto& s = cap->draw_snaps[static_cast<uint32_t>(snap_idx)];

    ImGui::Text("DRAW #%d  indices=%u  prim=%u  r3=%08X r4=%08X r5=%08X r6=%08X r7=%08X r8=%08X",
                snap_idx, s.index_count, s.prim_type,
                s.r[0], s.r[1], s.r[2], s.r[3], s.r[4], s.r[5]);
    DrawGuestMemRow(s.r[0], s.r3_dump, 8, "r3@");
    if (s.r[1] >= 0x70000000u)
        DrawGuestMemRow(s.r[1], s.r4_dump, 4, "r4@");
}

void DaytonaInspector::DrawTexDetail(int snap_idx) {
    const auto* cap = daytona_render::GetLastCapture();
    if (!cap || snap_idx < 0 || static_cast<uint32_t>(snap_idx) >= cap->tex_snap_count) return;
    const auto& s = cap->tex_snaps[static_cast<uint32_t>(snap_idx)];

    ImGui::Text("TEX #%d  desc=%08X type=%u",
                snap_idx, s.r[0], s.r[1]);
    DrawGuestMemRow(s.r[0], s.desc_dump, 8, "desc@");
}

void DaytonaInspector::DrawGuestMemRow(uint32_t base_addr, const uint32_t* words,
                                        int count, const char* label) {
    ImGui::TextDisabled("%s%08X:", label, base_addr);
    for (int i = 0; i < count; ++i) {
        ImGui::SameLine();
        if (words[i])
            ImGui::Text("%08X", words[i]);
        else
            ImGui::TextDisabled("00000000");
    }
}

void DaytonaInspector::DrawIndirectBuffer(uint32_t ib_phys, uint32_t ib_dwords) {
    // ── Pass 1: walk all packets, collect draw calls and totals ──────────────
    struct DrawRecord {
        uint32_t off;
        uint8_t  opcode;
        uint32_t count;  // data dwords
        uint32_t d[4];
    };
    constexpr int kMaxDrawRec = 64;
    DrawRecord draws[kMaxDrawRec];
    int draw_count = 0;
    uint32_t total_pkts = 0;

    {
        uint32_t off = 0;
        while (off < ib_dwords) {
            const uint32_t hdr  = ReadPhysU32(ib_phys + off * 4);
            const uint32_t type = hdr >> 30;
            if (type == 2) { ++off; continue; }
            uint32_t count = ((hdr >> 16) & 0x3FFF) + 1;
            uint8_t  opcode = 0;
            if (type == 3) opcode = (hdr >> 8) & 0xFF;
            else if (type != 0) break;
            ++total_pkts;
            const bool is_draw = type == 3 &&
                (opcode == 0x2C || opcode == 0x2D || opcode == 0x33 ||
                 opcode == 0x36 || opcode == 0x43);
            if (is_draw && draw_count < kMaxDrawRec) {
                DrawRecord& r = draws[draw_count++];
                r.off    = off;
                r.opcode = opcode;
                r.count  = count;
                for (int i = 0; i < 4; ++i)
                    r.d[i] = (count > static_cast<uint32_t>(i))
                              ? ReadPhysU32(ib_phys + (off + 1 + i) * 4) : 0;
            }
            off += 1 + count;
        }
    }

    // ── Header line ───────────────────────────────────────────────────────────
    ImGui::TextColored({0.4f, 0.9f, 0.4f, 1.f},
                       "IB phys=%08X  dwords=%u (%.1f KB)  draws=%d  pkts=%u",
                       ib_phys, ib_dwords, ib_dwords * 4.0f / 1024.0f,
                       draw_count, total_pkts);

    // ── Pass 2: show first N packets in scroll area (non-draw preview) ───────
    constexpr int kMaxShow = 60;
    {
        int shown = 0; bool overflow = false;
        if (ImGui::BeginChild("##ib_scroll", ImVec2(0.0f, 120.0f), false,
                              ImGuiWindowFlags_HorizontalScrollbar)) {
            uint32_t off = 0;
            while (off < ib_dwords && shown < kMaxShow) {
                const uint32_t hdr  = ReadPhysU32(ib_phys + off * 4);
                const uint32_t type = hdr >> 30;
                if (type == 2) { ++off; continue; }
                uint32_t count = ((hdr >> 16) & 0x3FFF) + 1;
                uint8_t  opcode = 0;
                if (type == 3) opcode = (hdr >> 8) & 0xFF;
                else if (type != 0) break;
                const bool is_draw = type == 3 &&
                    (opcode == 0x2C || opcode == 0x2D || opcode == 0x33 ||
                     opcode == 0x36 || opcode == 0x43);
                ++shown;
                const uint32_t d0 = ReadPhysU32(ib_phys + (off + 1) * 4);
                const uint32_t d1 = count >= 2 ? ReadPhysU32(ib_phys + (off + 2) * 4) : 0;
                if (is_draw)
                    ImGui::TextColored({1.0f, 0.6f, 0.2f, 1.f},
                        "[%05X] %-12s cnt=%4u  %08X %08X", off, PM4OpName(opcode), count, d0, d1);
                else if (type == 3)
                    ImGui::TextDisabled(
                        "[%05X] %-12s cnt=%4u  %08X %08X", off, PM4OpName(opcode), count, d0, d1);
                else
                    ImGui::TextDisabled(
                        "[%05X] T0 b=%04X  cnt=%4u  %08X %08X", off, hdr & 0x7FFF, count, d0, d1);
                off += 1 + count;
            }
            if (shown == kMaxShow && total_pkts > static_cast<uint32_t>(kMaxShow))
                ImGui::TextDisabled("  ... first %d shown, %u total packets", kMaxShow, total_pkts);
        }
        (void)overflow;
        ImGui::EndChild();
    }

    // ── Decoded draw calls (always shown, regardless of position in IB) ──────
    if (draw_count == 0) {
        ImGui::TextDisabled("No draw calls found.");
        return;
    }
    ImGui::TextColored({1.0f, 0.6f, 0.2f, 1.f}, "Draw calls (%d):", draw_count);
    for (int i = 0; i < draw_count; ++i) {
        const DrawRecord& r = draws[i];
        if (r.opcode == 0x2C) {
            // DRAW_INDX: d[0]=viz_query, d[1]=DRAW_INITIATOR
            //   bits[5:0]=prim_type, bits[7:6]=src_sel(0=DMA,2=auto), bits[31:10]=index_count
            // d[2]=ib_phys (if src_sel==0), d[3]=ib_size_dwords
            const uint8_t  prim    = r.d[1] & 0x3F;
            const uint8_t  src_sel = (r.d[1] >> 6) & 0x3;
            const uint32_t idx_cnt = r.d[1] >> 10;
            if (src_sel == 0 && r.count >= 4) {
                ImGui::TextColored({1.0f, 0.6f, 0.2f, 1.f},
                    "  [%05X] DRAW_INDX     prim=%-10s  idx=%6u  src=DMA  ib=%08X  sz=%u",
                    r.off, PrimTypeName(prim), idx_cnt, r.d[2], r.d[3]);
            } else {
                ImGui::TextColored({1.0f, 0.6f, 0.2f, 1.f},
                    "  [%05X] DRAW_INDX     prim=%-10s  idx=%6u  src=%s  raw=%08X %08X %08X %08X",
                    r.off, PrimTypeName(prim), idx_cnt,
                    src_sel == 2 ? "AUTO" : "?",
                    r.d[0], r.d[1], r.d[2], r.d[3]);
            }
        } else if (r.opcode == 0x36) {
            // DRAW_INDX_2: no viz_query slot — d[0] IS the DRAW_INITIATOR,
            // followed by inline index data (cnt-1 dwords of indices).
            const uint8_t  prim    = r.d[0] & 0x3F;
            const uint8_t  src_sel = (r.d[0] >> 6) & 0x3;
            const uint32_t idx_cnt = r.d[0] >> 10;
            ImGui::TextColored({1.0f, 0.6f, 0.2f, 1.f},
                "  [%05X] DRAW_INDX_2  prim=%-10s  idx=%6u  src=%s  raw=%08X %08X %08X %08X",
                r.off, PrimTypeName(prim), idx_cnt,
                src_sel == 2 ? "AUTO" : src_sel == 0 ? "DMA" : "?",
                r.d[0], r.d[1], r.d[2], r.d[3]);
        } else if (r.opcode == 0x2D) {
            // DRAW_INDX_BIN: format partially known; try both d[0] and d[1] as initiator
            ImGui::TextColored({1.0f, 0.6f, 0.2f, 1.f},
                "  [%05X] DRAW_BIN     cnt=%u  d0-init:prim=%-8s idx=%u | d1-init:prim=%-8s idx=%u  raw=%08X %08X %08X %08X",
                r.off, r.count,
                PrimTypeName(r.d[0] & 0x3F), r.d[0] >> 10,
                PrimTypeName(r.d[1] & 0x3F), r.d[1] >> 10,
                r.d[0], r.d[1], r.d[2], r.d[3]);
        } else {
            // Other draw variant — show raw
            ImGui::TextColored({1.0f, 0.6f, 0.2f, 1.f},
                "  [%05X] %-12s  cnt=%u  %08X %08X %08X %08X",
                r.off, PM4OpName(r.opcode), r.count,
                r.d[0], r.d[1], r.d[2], r.d[3]);
        }
    }
}

// Like DrawGuestMemRow but labels by dword offset (for ring buffer display).
void DaytonaInspector::DrawRingBufRow(uint32_t dword_offset, const uint32_t* words,
                                       int count) {
    ImGui::TextDisabled("rb[%05X]:", dword_offset);
    for (int i = 0; i < count; ++i) {
        ImGui::SameLine();
        if ((words[i] >> 24) == 0xC0)
            ImGui::TextColored({0.4f, 0.9f, 0.4f, 1.f}, "%08X", words[i]);
        else if (words[i])
            ImGui::Text("%08X", words[i]);
        else
            ImGui::TextDisabled("00000000");
    }
}

// ── Textures tab ─────────────────────────────────────────────────────────────
void DaytonaInspector::DrawTexturesTab() {
    const uint32_t guest_tex_count = daytona_render::GetGuestTextureCount();
    const daytona_render::GuestTextureEntry* guest_tex = daytona_render::GetGuestTextures();

    ImGui::Text("Guest textures from live TFETCH state: %u / %u",
                guest_tex_count, daytona_render::kMaxGuestTextures);
    ImGui::TextDisabled("Identity key: base/mip/format/dimensions/pitch/tile. Draw uses come from PM4 draw correlation.");
    ImGui::Separator();

    if (guest_tex_count > 0) {
        constexpr ImGuiTableFlags kGuestFlags =
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
            ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit;

        uint32_t order[daytona_render::kMaxGuestTextures]{};
        const uint32_t order_count = std::min(guest_tex_count, daytona_render::kMaxGuestTextures);
        for (uint32_t i = 0; i < order_count; ++i) order[i] = i;
        std::sort(order, order + order_count, [guest_tex](uint32_t a, uint32_t b) {
            const auto& ta = guest_tex[a];
            const auto& tb = guest_tex[b];
            if (ta.draw_use_count != tb.draw_use_count)
                return ta.draw_use_count > tb.draw_use_count;
            if (ta.tfetch_count != tb.tfetch_count)
                return ta.tfetch_count > tb.tfetch_count;
            return ta.base_phys < tb.base_phys;
        });

        const float table_h = std::min(280.0f,
            28.0f + static_cast<float>(order_count) * ImGui::GetTextLineHeightWithSpacing());
        if (ImGui::BeginTable("##guest_textures", 14, kGuestFlags, ImVec2(0.0f, table_h))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Uses", ImGuiTableColumnFlags_WidthFixed, 44.0f);
            ImGui::TableSetupColumn("TFetch", ImGuiTableColumnFlags_WidthFixed, 48.0f);
            ImGui::TableSetupColumn("Cache", ImGuiTableColumnFlags_WidthFixed, 48.0f);
            ImGui::TableSetupColumn("Base", ImGuiTableColumnFlags_WidthFixed, 84.0f);
            ImGui::TableSetupColumn("Mip", ImGuiTableColumnFlags_WidthFixed, 84.0f);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 82.0f);
            ImGui::TableSetupColumn("Fmt", ImGuiTableColumnFlags_WidthFixed, 72.0f);
            ImGui::TableSetupColumn("Host", ImGuiTableColumnFlags_WidthFixed, 58.0f);
            ImGui::TableSetupColumn("KB", ImGuiTableColumnFlags_WidthFixed, 44.0f);
            ImGui::TableSetupColumn("Pitch", ImGuiTableColumnFlags_WidthFixed, 44.0f);
            ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_WidthFixed, 42.0f);
            ImGui::TableSetupColumn("2D", ImGuiTableColumnFlags_WidthFixed, 38.0f);
            ImGui::TableSetupColumn("36", ImGuiTableColumnFlags_WidthFixed, 38.0f);
            ImGui::TableSetupColumn("Linear path", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (uint32_t oi = 0; oi < order_count; ++oi) {
                const auto& e = guest_tex[order[oi]];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("%u", e.draw_use_count);
                ImGui::TableSetColumnIndex(1); ImGui::Text("%u", e.tfetch_count);
                ImGui::TableSetColumnIndex(2); ImGui::Text("#%u %s", e.host_cache_id, TextureCacheStateName(e.cache_state));
                ImGui::TableSetColumnIndex(3); ImGui::Text("%08X", e.base_phys);
                ImGui::TableSetColumnIndex(4);
                if (e.mip_phys) ImGui::Text("%08X", e.mip_phys);
                else ImGui::TextDisabled("--------");
                ImGui::TableSetColumnIndex(5); ImGui::Text("%ux%u", e.width, e.height);
                ImGui::TableSetColumnIndex(6); ImGui::Text("%s", TextureFormatName(e.format));
                ImGui::TableSetColumnIndex(7); ImGui::Text("%s", TextureHostFormatName(e.host_format));
                ImGui::TableSetColumnIndex(8); ImGui::Text("%u", (e.estimated_bytes + 1023u) / 1024u);
                ImGui::TableSetColumnIndex(9); ImGui::Text("%u", e.pitch);
                ImGui::TableSetColumnIndex(10); ImGui::Text("%u", e.last_slot);
                ImGui::TableSetColumnIndex(11); ImGui::Text("%u", e.op_2d_count);
                ImGui::TableSetColumnIndex(12); ImGui::Text("%u", e.op_36_count);
                ImGui::TableSetColumnIndex(13);
                ImGui::TextDisabled("-");
            }
            ImGui::EndTable();
        }
    } else {
        ImGui::TextDisabled("No TFETCH textures decoded yet. Enter a render path that submits PM4 IBs.");
    }

    ImGui::Separator();
    ImGui::TextDisabled("Legacy descriptor path below is retained for comparison only.");

    // ── Texture descriptor snaps from last capture ────────────────────────────
    const auto* cap = daytona_render::GetLastCapture();
    if (!cap) { ImGui::TextDisabled("No capture yet."); return; }

    ImGui::Text("Tex snaps from last capture: %u", cap->tex_snap_count);
    if (cap->tex_snap_count == 0) {
        ImGui::TextDisabled("No snaps — GpuTextureProcess is not firing on the");
        ImGui::TextDisabled("captured render path (same root cause as Draw=0).");
        return;
    }

    constexpr ImGuiTableFlags kFlags =
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
        ImGuiTableFlags_SizingFixedFit;
    if (ImGui::BeginTable("##tex_snaps", 4, kFlags, ImVec2(0.0f, 0.0f))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("#",          ImGuiTableColumnFlags_WidthFixed, 36.0f);
        ImGui::TableSetupColumn("DescPtr",    ImGuiTableColumnFlags_WidthFixed, 84.0f);
        ImGui::TableSetupColumn("Type",       ImGuiTableColumnFlags_WidthFixed, 48.0f);
        ImGui::TableSetupColumn("Dump[0..7]", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        for (uint32_t i = 0; i < cap->tex_snap_count; ++i) {
            const auto& s = cap->tex_snaps[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%u", i);
            ImGui::TableSetColumnIndex(1); ImGui::Text("%08X", s.r[0]);
            ImGui::TableSetColumnIndex(2); ImGui::Text("%u", s.r[1]);
            ImGui::TableSetColumnIndex(3);
            char buf[128]; int pos = 0;
            for (int w = 0; w < 8 && pos < 120; ++w)
                pos += snprintf(buf + pos, sizeof(buf) - pos, "%08X ", s.desc_dump[w]);
            ImGui::TextDisabled("%s", buf);
        }
        ImGui::EndTable();
    }
}

// ── Shaders tab ───────────────────────────────────────────────────────────────
void DaytonaInspector::DrawShadersTab() {
    const uint32_t shdr_count = daytona_render::GetShaderEntryCount();
    const daytona_render::ShaderEntry* entries = daytona_render::GetShaderEntries();

    ImGui::Text("Session shader compiles: %u", shdr_count);
    if (shdr_count == 0) {
        ImGui::TextDisabled("No shaders captured yet.");
        ImGui::TextDisabled("ShaderCompile / ShaderSourceCompile hooks are active.");
        ImGui::TextDisabled("Enter a race to trigger shader builds.");
        return;
    }
    ImGui::Separator();

    constexpr ImGuiTableFlags kFlags =
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
        ImGuiTableFlags_SizingFixedFit;
    if (ImGui::BeginTable("##shader_reg", 4, kFlags, ImVec2(0.0f, 0.0f))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Frame",  ImGuiTableColumnFlags_WidthFixed, 52.0f);
        ImGui::TableSetupColumn("GuestPtr", ImGuiTableColumnFlags_WidthFixed, 84.0f);
        ImGui::TableSetupColumn("Bytes", ImGuiTableColumnFlags_WidthFixed, 52.0f);
        ImGui::TableSetupColumn("Source preview", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        for (uint32_t i = 0; i < shdr_count; ++i) {
            const auto& e = entries[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%llu", static_cast<unsigned long long>(e.frame_index));
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%08X", e.guest_shader_ptr);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%u", e.source_len);
            ImGui::TableSetColumnIndex(3);
            ImGui::TextDisabled("%s", e.preview[0] ? e.preview : "(no source captured)");
        }
        ImGui::EndTable();
    }
}
