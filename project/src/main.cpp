// Daytona USA (XBLA) - ReXGlue Static Recompilation
// Title ID: 58410B1D

#include "daytona_init.h"
#include "daytona_debug.h"
#include "daytona_inspector.h"
#include "daytona_render.h"
#include "daytona_renderer.h"
#include "keyboard_driver.h"

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/graphics/flags.h>
#include <rex/logging/api.h>
#include <rex/rex_app.h>
#include <rex/input/input_system.h>
#include <rex/system/xthread.h>
#include <rex/ui/flags.h>
#include <imgui.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace {

std::string Trim(std::string_view value) {
    const auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!value.empty() && is_space(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }
    while (!value.empty() && is_space(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }
    return std::string(value);
}

std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string StripInlineComment(std::string_view value) {
    const size_t comment = value.find_first_of("#;");
    if (comment != std::string_view::npos) {
        value = value.substr(0, comment);
    }
    return Trim(value);
}

std::optional<int32_t> ParseInt(std::string_view value) {
    const std::string text = Trim(value);
    int32_t out = 0;
    auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), out);
    if (ec != std::errc() || ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return out;
}

std::optional<bool> ParseBool(std::string_view value) {
    const std::string lowered = Lower(Trim(value));
    if (lowered == "true" || lowered == "yes" || lowered == "on" || lowered == "1") {
        return true;
    }
    if (lowered == "false" || lowered == "no" || lowered == "off" || lowered == "0") {
        return false;
    }
    return std::nullopt;
}

int32_t ClampInt(int32_t value, int32_t min, int32_t max) {
    return std::clamp(value, min, max);
}

std::optional<double> ParseDouble(std::string_view value) {
    const std::string text = Trim(value);
    double out = 0.0;
    auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), out);
    if (ec != std::errc() || ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return out;
}

double ClampDouble(double value, double min, double max) {
    return std::clamp(value, min, max);
}

std::filesystem::path ResolveIniPath(const std::filesystem::path& exe_dir,
                                     std::string_view value) {
    std::filesystem::path path = Trim(value);
    if (path.empty()) {
        return {};
    }
    return path.is_absolute() ? path : exe_dir / path;
}

void SetCvar(std::string_view name, std::string_view value) {
    rex::cvar::SetFlagByName(name, value);
}

void SetCvarInt(std::string_view name, int32_t value) {
    SetCvar(name, std::to_string(value));
}

void SetCvarBool(std::string_view name, bool value) {
    SetCvar(name, value ? "true" : "false");
}

void SetCvarDouble(std::string_view name, double value) {
    SetCvar(name, std::to_string(value));
}

void WriteDefaultIni(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) {
        return;
    }

    std::ofstream file(path);
    if (!file) {
        return;
    }

    file
        << "# Daytona USA Recompiled PC settings\n"
        << "# Edit this file, then restart the game.\n"
        << "\n"
        << "[Display]\n"
        << "fullscreen = true\n"
        << "# Windowed mode size in pixels (0 = use a sensible default).\n"
        << "# For ultrawide (21:9 or wider): set both values to your display resolution.\n"
        << "# The 3D scene will automatically use a wider horizontal FOV (Hor+).\n"
        << "# Example for 2560x1080: window_width = 2560, window_height = 1080\n"
        << "# Example for 3440x1440: window_width = 3440, window_height = 1440\n"
        << "window_width = 0\n"
        << "window_height = 0\n"
        << "# Vertical sync. Disable for uncapped frame rate.\n"
        << "vsync = true\n"
        << "\n"
        << "[Renderer]\n"
        << "# Daytona is forced to native internal resolution for correctness.\n"
        << "# Keep this at 1; higher values are ignored until the renderer is accurate.\n"
        << "resolution_scale = 1\n"
        << "# Final output effect: bilinear, cas, fsr\n"
        << "#   bilinear - standard upscale to display resolution\n"
        << "#   cas      - AMD Contrast Adaptive Sharpening\n"
        << "#   fsr      - AMD FidelityFX Super Resolution 1.0 (sharpened upscale)\n"
        << "present_effect = bilinear\n"
        << "# CAS/FSR sharpness in [0.0, 1.0]. Only used when present_effect = cas or fsr.\n"
        << "sharpness = 0.0\n"
        << "\n"
        << "[Storage]\n"
        << "cache_path = cache\n"
        << "# Leave empty to use the platform save folder.\n"
        << "user_data_path =\n"
        << "\n"
        << "[Diagnostics]\n"
        << "debug_overlay = false\n"
        << "log_level = warn\n"
        << "log_max_files = 5\n"
        << "log_max_file_size_mb = 5\n";
}

std::unordered_map<std::string, std::string> ReadIni(const std::filesystem::path& path) {
    std::unordered_map<std::string, std::string> values;
    std::ifstream file(path);
    if (!file) {
        return values;
    }

    std::string section;
    std::string line;
    while (std::getline(file, line)) {
        line = StripInlineComment(line);
        if (line.empty()) {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            section = Lower(Trim(std::string_view(line).substr(1, line.size() - 2)));
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string key = Lower(Trim(line.substr(0, eq)));
        std::string value = Trim(line.substr(eq + 1));
        if (!section.empty()) {
            key = section + "." + key;
        }
        values[std::move(key)] = std::move(value);
    }
    return values;
}

struct DaytonaIniSettings {
    bool debug_overlay = false;
};

DaytonaIniSettings LoadDaytonaIni(const std::filesystem::path& exe_dir,
                                  rex::PathConfig& paths) {
    const std::filesystem::path ini_path = exe_dir / "daytona.ini";
    WriteDefaultIni(ini_path);
    const auto values = ReadIni(ini_path);

    DaytonaIniSettings settings;

    if (auto it = values.find("display.fullscreen"); it != values.end()) {
        if (auto value = ParseBool(it->second)) SetCvarBool("fullscreen", *value);
    }
    int32_t win_w = 0;
    int32_t win_h = 0;
    if (auto it = values.find("display.window_width"); it != values.end()) {
        if (auto value = ParseInt(it->second)) {
            win_w = ClampInt(*value, 0, 8192);
            SetCvarInt("window_width", win_w);
        }
    }
    if (auto it = values.find("display.window_height"); it != values.end()) {
        if (auto value = ParseInt(it->second)) {
            win_h = ClampInt(*value, 0, 8192);
            SetCvarInt("window_height", win_h);
        }
    }

    // Cap the GUEST video mode (= the resolution Daytona renders its 3D scene
    // into) at 720 lines, independent of the on-screen window. Without this the
    // SDK reports window_width/height (e.g. 3440x1440) as the video mode, so the
    // game allocates its render targets at the full display size — 4x the fill
    // rate of 720p, which tanks the in-race frame rate. The present still upscales
    // this 720-line buffer to the full window. The video_mode_* cvars take
    // precedence over window_* in GetConfiguredVideoMode{Width,Height}().
    // Aspect correction (g_ar_scale = gameAR/windowAR) is independent of the
    // internal resolution, so widescreen geometry stays correct.
    if (win_w > 0 && win_h > 720) {
        const int32_t internal_h = 720;
        // Match the window's aspect ratio at 720 lines so the present upscales
        // uniformly (e.g. 3440x1440 -> 1720x720; 1920x1080 -> 1280x720).
        int32_t internal_w = static_cast<int32_t>(
            std::lround(720.0 * static_cast<double>(win_w) / static_cast<double>(win_h)));
        internal_w = ClampInt(internal_w & ~1, 640, 4095);  // even width, in range
        SetCvarInt("video_mode_width", internal_w);
        SetCvarInt("video_mode_height", internal_h);
        REXLOG_ERROR("Daytona: capping internal video mode to {}x{} (window {}x{}) "
                     "to keep in-race fill rate at 720p", internal_w, internal_h, win_w, win_h);
    }

    if (auto it = values.find("display.vsync"); it != values.end()) {
        if (auto value = ParseBool(it->second)) SetCvarBool("vsync", *value);
    }

    // Do not apply renderer.resolution_scale from the INI yet. Daytona's
    // texture/resolve path is sensitive to internal upscaling, so correctness
    // takes priority until the native backend is validated end to end.
    if (auto it = values.find("renderer.present_effect"); it != values.end()) {
        const std::string effect = Lower(Trim(it->second));
        if (effect == "bilinear" || effect == "cas" || effect == "fsr") {
            SetCvar("present_effect", effect);
        }
    }
    if (auto it = values.find("renderer.sharpness"); it != values.end()) {
        if (auto value = ParseDouble(it->second)) {
            SetCvarDouble("present_cas_additional_sharpness", ClampDouble(*value, 0.0, 1.0));
        }
    }

    if (auto it = values.find("storage.cache_path"); it != values.end()) {
        if (auto path = ResolveIniPath(exe_dir, it->second); !path.empty()) {
            paths.cache_root = path;
        }
    }
    if (auto it = values.find("storage.user_data_path"); it != values.end()) {
        if (auto path = ResolveIniPath(exe_dir, it->second); !path.empty()) {
            paths.user_data_root = path;
        }
    }

    if (auto it = values.find("diagnostics.debug_overlay"); it != values.end()) {
        if (auto value = ParseBool(it->second)) settings.debug_overlay = *value;
    }
    if (auto it = values.find("diagnostics.log_level"); it != values.end()) {
        SetCvar("log_level", Lower(Trim(it->second)));
    }
    if (auto it = values.find("diagnostics.log_max_files"); it != values.end()) {
        if (auto value = ParseInt(it->second)) SetCvarInt("log_max_files", ClampInt(*value, 1, 50));
    }
    if (auto it = values.find("diagnostics.log_max_file_size_mb"); it != values.end()) {
        if (auto value = ParseInt(it->second)) {
            SetCvarInt("log_max_file_size_mb", ClampInt(*value, 1, 256));
        }
    }

    return settings;
}

void ApplyDaytonaBackendCorrectnessCvars() {
    // Match emulator workarounds for Daytona: keep the guest-visible draw
    // resolution native. ReXGlue's primitive processor cache is CPU-side
    // index-conversion reuse, not the unsafe guest vertex cache seen in other
    // emulators, so leave it enabled for performance.
    SetCvarInt("resolution_scale", 1);
    SetCvarInt("draw_resolution_scale_x", 1);
    SetCvarInt("draw_resolution_scale_y", 1);
    SetCvarInt("primitive_processor_cache_min_indices", 0);

    // Daytona has shown visible texture instability when backend overrides move
    // away from the guest's native assumptions. Keep the generic path close to
    // defaults while native renderer work is still being validated.
    SetCvarInt("anisotropic_override", -1);
    SetCvarBool("async_shader_compilation", false);
    SetCvarBool("vulkan_force_dxt45_rgba8_decode", false);
    SetCvarBool("vulkan_readback_memexport", false);
    SetCvarBool("vulkan_readback_resolve", false);
    SetCvarBool("gpu_allow_invalid_fetch_constants", true);
    SetCvar("readback_resolve", "none");

    REXLOG_ERROR("Daytona backend correctness: native 1x draw resolution, "
                 "primitive processor cache enabled, anisotropic override disabled, "
                 "async shaders disabled, Vulkan DXT/readback overrides disabled, "
                 "invalid fetch constants allowed");

    // Confirm the guest video mode cap from LoadDaytonaIni took effect (that log
    // line runs before the log file opens, so echo it here).
    REXLOG_ERROR("Daytona guest video mode: {}x{} (window {}x{})",
                 rex::cvar::GetFlagByName("video_mode_width"),
                 rex::cvar::GetFlagByName("video_mode_height"),
                 rex::cvar::GetFlagByName("window_width"),
                 rex::cvar::GetFlagByName("window_height"));
}

class DaytonaDebugDialog : public rex::ui::ImGuiDialog {
public:
    explicit DaytonaDebugDialog(rex::ui::ImGuiDrawer* drawer) : ImGuiDialog(drawer) {}

protected:
    void OnDraw(ImGuiIO& io) override {
        auto snapshot = daytona_debug::GetSnapshot();
        UpdateVdSwapRate(snapshot, io.DeltaTime);

        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(310.0f, 170.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.55f);
        if (!ImGui::Begin("Daytona##perf", nullptr, ImGuiWindowFlags_NoResize)) {
            ImGui::End();
            return;
        }

        const float fps = io.Framerate;
        const float ms  = fps > 0.0f ? 1000.0f / fps : 0.0f;
        ImGui::Text("Host  %.1f fps  %.2f ms", fps, ms);
        ImGui::Text("Swap  %.1f fps  stage: %s",
                    vd_swap_rate_,
                    daytona_debug::StageName(snapshot.stage));

        ImGui::Separator();

        const auto rs = daytona_render::GetLastFrameStats();
        ImGui::Text("Frame %-6llu  Events %u",
                    static_cast<unsigned long long>(rs.frame_index),
                    rs.draw_execute_count + rs.render_pass_count +
                        rs.texture_process_count + rs.shader_build_count);
        ImGui::Text("Draw %-4u  Pass %-3u  Tex %-4u  Shdr %-3u",
                    rs.draw_execute_count,
                    rs.render_pass_count,
                    rs.texture_process_count,
                    rs.shader_build_count);
        ImGui::Text("State %-4u  RT %-3u  Cmd %u",
                    rs.state_change_count,
                    rs.rt_bind_count,
                    rs.command_submit_count);

        ImGui::Separator();

        if (ImGui::Button("Capture Frame")) {
            daytona_render::RequestCapture();
        }
        const auto* cap = daytona_render::GetLastCapture();
        if (cap) {
            ImGui::SameLine();
            ImGui::Text("%u evts%s",
                        cap->event_count,
                        cap->overflow_count ? " (overflow!)" : "");
        }

        ImGui::End();
    }

private:
    void UpdateVdSwapRate(const daytona_debug::Snapshot& snapshot, float delta_time) {
        rate_accumulator_ += std::max(delta_time, 0.0f);
        if (rate_accumulator_ < 0.25f) return;

        const uint32_t count = snapshot.hooks[static_cast<size_t>(daytona_debug::HookId::kVdSwapCaller)].count;
        vd_swap_rate_ = static_cast<float>(count - prev_vd_swap_count_) / rate_accumulator_;
        prev_vd_swap_count_ = count;
        rate_accumulator_ = 0.0f;
    }

    uint32_t prev_vd_swap_count_ = 0;
    float vd_swap_rate_ = 0.0f;
    float rate_accumulator_ = 0.0f;
};

}  // namespace

class DaytonaApp : public rex::ReXApp {
public:
    using rex::ReXApp::ReXApp;

    static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& ctx) {
        return std::unique_ptr<DaytonaApp>(new DaytonaApp(ctx, "daytona", PPCImageConfig));
    }

protected:
    void OnConfigurePaths(rex::PathConfig& paths) override {
        const std::filesystem::path exe_dir = rex::filesystem::GetExecutableFolder();
        paths.config_path = exe_dir / "daytona.toml";
        paths.cache_root = exe_dir / "cache";
        ini_settings_ = LoadDaytonaIni(exe_dir, paths);
    }

    void OnPreSetup(rex::RuntimeConfig& config) override {
        (void)config;
        daytona_debug::SetStage(daytona_debug::AppStage::kPreSetup);
        daytona::DaytonaRenderer::Install();
        ApplyDaytonaBackendCorrectnessCvars();
    }

    void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
        if (ini_settings_.debug_overlay) {
            debug_dialog_ = std::make_unique<DaytonaDebugDialog>(drawer);
            inspector_ = std::make_unique<DaytonaInspector>(drawer);
        }
    }

    void OnPostSetup() override {
        daytona_debug::SetStage(daytona_debug::AppStage::kPostSetup);
        if (!window() || !runtime() || !runtime()->input_system()) return;
        auto* input_sys = static_cast<rex::input::InputSystem*>(runtime()->input_system());
        auto kbd = std::make_unique<KeyboardInputDriver>(window());
        kbd->Setup();
        input_sys->AddDriver(std::move(kbd));
    }

    void OnPreLaunchModule() override {
        daytona_debug::SetStage(daytona_debug::AppStage::kPreLaunchModule);
    }

    void OnPostLaunchModule(rex::system::XThread* thread) override {
        (void)thread;
        daytona_debug::SetStage(daytona_debug::AppStage::kPostLaunchModule);
    }

    void OnGuestThreadExit(rex::system::XThread* thread) override {
        (void)thread;
        daytona_debug::SetStage(daytona_debug::AppStage::kGuestThreadExit);
    }

    void OnShutdown() override {
        daytona::DaytonaRenderer::Flush();
        debug_dialog_.reset();
        inspector_.reset();
    }

private:
    DaytonaIniSettings ini_settings_;
    std::unique_ptr<DaytonaDebugDialog> debug_dialog_;
    std::unique_ptr<DaytonaInspector> inspector_;
};

REX_DEFINE_APP(daytona, DaytonaApp::Create)
