#pragma once

#include <rex/ui/imgui_dialog.h>

namespace rex::ui { class ImGuiDrawer; }

// Frame Inspector — ImGui dialog that decodes and displays a captured frame.
// Show/hide with the "Inspect" button in the perf overlay. Reads memory
// snapshots taken at capture time (stable) and can also live-peek guest
// memory via daytona_render::GuestReadU32 for addresses that were not snapped.
class DaytonaInspector : public rex::ui::ImGuiDialog {
public:
    explicit DaytonaInspector(rex::ui::ImGuiDrawer* drawer);

protected:
    void OnDraw(ImGuiIO& io) override;

private:
    void DrawEventsTab();
    void DrawTexturesTab();
    void DrawShadersTab();
    void DrawEventTable();
    void DrawDrawCallDetail(int snap_idx);
    void DrawTexDetail(int snap_idx);
    void DrawGuestMemRow(uint32_t base_addr, const uint32_t* words, int count,
                         const char* label);
    void DrawRingBufRow(uint32_t dword_offset, const uint32_t* words, int count);
    void DrawIndirectBuffer(uint32_t ib_phys, uint32_t ib_dwords);

    int filter_kind_ = -1;         // -1 = all event kinds
    int selected_event_ = -1;      // index into last_capture->events
    bool show_memory_ = true;
    bool open_ = true;
    uint64_t last_seen_frame_ = 0; // tracks when to auto-reopen
};
