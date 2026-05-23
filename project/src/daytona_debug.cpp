#include "daytona_debug.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace daytona_debug {
namespace {

using Clock = std::chrono::steady_clock;

struct HookState {
    const char* name;
    std::atomic<uint32_t> count{0};
    std::atomic<uint32_t> lr{0};
    std::atomic<uint32_t> r3{0};
    std::atomic<uint32_t> r4{0};
    std::atomic<uint32_t> r5{0};
    std::atomic<uint32_t> thread_hash{0};
    std::atomic<uint64_t> last_seen_us{0};
};

struct EventState {
    std::atomic<uint64_t> sequence{0};
    std::atomic<uint32_t> hook_index{0};
    std::atomic<uint32_t> count{0};
    std::atomic<uint32_t> lr{0};
    std::atomic<uint32_t> r3{0};
    std::atomic<uint32_t> r4{0};
    std::atomic<uint32_t> r5{0};
    std::atomic<uint32_t> thread_hash{0};
    std::atomic<uint64_t> seen_us{0};
};

const auto kStartTime = Clock::now();
std::atomic<AppStage> g_stage{AppStage::kStarting};
std::atomic<uint64_t> g_event_sequence{0};
std::array<EventState, 24> g_events{};

std::array<HookState, static_cast<size_t>(HookId::kCount)> g_hooks{{
    {"GPU IRQ sub_822346A0"},
    {"GPU desc sub_82234FA8"},
    {"GPU/cache sub_824140B8"},
    {"boot present sub_82130D68"},
    {"boot present sub_82131088"},
    {"present worker sub_8223CF68"},
    {"frame reset sub_82233C78"},
    {"frame wait sub_822337C0"},
    {"cmd flush sub_82235B20"},
    {"cmd alloc sub_82235D58"},
    {"VdSwap caller sub_82233C80"},
    {"device/init sub_822297D8"},
    {"state A sub_8221E940"},
    {"state B sub_8221E950"},
    {"state C sub_8221E658"},
    {"boot poll sub_82130870"},
    {"backbuffer sub_822337E8"},
    {"scene setup sub_8222C4C8"},
    {"rt setup sub_8222EBC0"},
    {"begin scene sub_8212F028"},
    {"clear/draw sub_82223F28"},
    {"copy/hash sub_824DC8A8"},
    {"copy/hash sub_824EB2B8"},
    {"gate in sub_82242A30"},
    {"gate out sub_82242A30"},
    {"logo setup in sub_8212C7A8"},
    {"logo setup out sub_8212C7A8"},
    {"boot poll in sub_82130870"},
    {"boot poll out sub_82130870"},
    {"end scene sub_8212F000"},
    {"boot rt0 sub_8222AF38"},
    {"boot rt1 sub_8222AE18"},
    {"boot bind sub_822203B8"},
    {"boot set sub_822200E0"},
    {"boot shader path sub_8212E068"},
    {"boot alloc A sub_82220730"},
    {"boot clear A sub_82221780"},
    {"boot unlock A sub_822207F8"},
    {"boot draw sub_82223EA8"},
    {"boot alloc B sub_82220808"},
    {"boot clear B sub_822217D0"},
    {"boot unlock B sub_822208B8"},
    {"boot object sub_8212C408"},
    {"boot ui A sub_82129DE0"},
    {"boot ui B sub_82126280"},
    {"boot ui C sub_8212A850"},
    {"boot ui D sub_82128638"},
    {"boot ui E in sub_8212AD80"},
    {"boot ui E out sub_8212AD80"},
    {"boot ui F in sub_8212A928"},
    {"boot ui F out sub_8212A928"},
    {"boot ui G in sub_8212C7B8"},
    {"boot ui G out sub_8212C7B8"},
    {"ui tex size A sub_82241750"},
    {"ui tex load sub_822429C8"},
    {"ui tex finalize A sub_82225658"},
    {"ui tex size B sub_82241740"},
    {"ui tex finalize B sub_82225470"},
    {"main shader setup sub_8212F990"},
    // Audio domain
    {"audio init driver AudioInitDriver"},
    {"audio submit frame AudioSubmitRenderFrame"},
    // Input domain
    {"input raw state InputGetRawState"},
    {"input caps ex InputGetCapabilitiesEx"},
    // Video domain
    {"gpu set display GpuSetDisplayMode"},
    {"gpu init engines GpuInitEngines"},
    {"gpu init ringbuf GpuInitRingBuffer"},
    // Network domain
    {"net xnet startup NetXNetStartup"},
    {"net wsa startup NetWsaStartup"},
    // XUI domain
    {"ui msgbox ex UiShowMessageBoxEx"},
    {"ui signin UiShowSignin"},
    // User domain
    {"user signin info UserGetSigninInfo"},
    {"voice headset VoiceCheckHeadset"},
    // Session domain
    {"session create SessionCreate"},
    // Platform domain
    {"sys platform info SysPlatformInfo"},
    {"launch title LaunchTitle"},
    // Crypto
    {"crypto sha CryptoSha"},
    // Render pipeline orchestrators
    {"frame render sub_8212B828"},
    {"scene render sub_8212B3C8"},
    {"scene 3d render sub_8222CB88"},
}};

uint64_t UptimeMicros() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - kStartTime).count());
}

uint32_t CurrentThreadHash() {
    return static_cast<uint32_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
}

}  // namespace

void SetStage(AppStage stage) {
    g_stage.store(stage, std::memory_order_relaxed);
}

uint32_t RecordHook(HookId id, const PPCContext& ctx) {
    const uint32_t hook_index = static_cast<uint32_t>(id);
    auto& hook = g_hooks[static_cast<size_t>(id)];
    const uint32_t thread_hash = CurrentThreadHash();
    const uint64_t now_us = UptimeMicros();
    hook.lr.store(static_cast<uint32_t>(ctx.lr), std::memory_order_relaxed);
    hook.r3.store(ctx.r3.u32, std::memory_order_relaxed);
    hook.r4.store(ctx.r4.u32, std::memory_order_relaxed);
    hook.r5.store(ctx.r5.u32, std::memory_order_relaxed);
    hook.thread_hash.store(thread_hash, std::memory_order_relaxed);
    hook.last_seen_us.store(now_us, std::memory_order_relaxed);
    const uint32_t count = hook.count.fetch_add(1, std::memory_order_relaxed) + 1;

    const uint64_t sequence = g_event_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
    auto& event = g_events[sequence % g_events.size()];
    event.hook_index.store(hook_index, std::memory_order_relaxed);
    event.count.store(count, std::memory_order_relaxed);
    event.lr.store(static_cast<uint32_t>(ctx.lr), std::memory_order_relaxed);
    event.r3.store(ctx.r3.u32, std::memory_order_relaxed);
    event.r4.store(ctx.r4.u32, std::memory_order_relaxed);
    event.r5.store(ctx.r5.u32, std::memory_order_relaxed);
    event.thread_hash.store(thread_hash, std::memory_order_relaxed);
    event.seen_us.store(now_us, std::memory_order_relaxed);
    event.sequence.store(sequence, std::memory_order_release);

    return count;
}

Snapshot GetSnapshot() {
    Snapshot snapshot;
    const auto now_us = UptimeMicros();
    snapshot.stage = g_stage.load(std::memory_order_relaxed);
    snapshot.uptime_seconds = static_cast<double>(now_us) / 1000000.0;

    for (size_t i = 0; i < snapshot.hooks.size(); ++i) {
        const auto& src = g_hooks[i];
        auto& dst = snapshot.hooks[i];
        dst.name = src.name;
        dst.count = src.count.load(std::memory_order_relaxed);
        dst.lr = src.lr.load(std::memory_order_relaxed);
        dst.r3 = src.r3.load(std::memory_order_relaxed);
        dst.r4 = src.r4.load(std::memory_order_relaxed);
        dst.r5 = src.r5.load(std::memory_order_relaxed);
        dst.thread_hash = src.thread_hash.load(std::memory_order_relaxed);
        const auto last_seen_us = src.last_seen_us.load(std::memory_order_relaxed);
        dst.last_seen_seconds =
            last_seen_us ? static_cast<double>(now_us - last_seen_us) / 1000000.0 : -1.0;
    }

    for (size_t i = 0; i < snapshot.events.size(); ++i) {
        const auto& src = g_events[i];
        auto& dst = snapshot.events[i];
        dst.sequence = src.sequence.load(std::memory_order_acquire);
        const uint32_t hook_index = src.hook_index.load(std::memory_order_relaxed);
        dst.name = hook_index < g_hooks.size() ? g_hooks[hook_index].name : "unknown";
        dst.count = src.count.load(std::memory_order_relaxed);
        dst.lr = src.lr.load(std::memory_order_relaxed);
        dst.r3 = src.r3.load(std::memory_order_relaxed);
        dst.r4 = src.r4.load(std::memory_order_relaxed);
        dst.r5 = src.r5.load(std::memory_order_relaxed);
        dst.thread_hash = src.thread_hash.load(std::memory_order_relaxed);
        const auto seen_us = src.seen_us.load(std::memory_order_relaxed);
        dst.age_seconds = seen_us ? static_cast<double>(now_us - seen_us) / 1000000.0 : -1.0;
    }

    return snapshot;
}

const char* StageName(AppStage stage) {
    switch (stage) {
        case AppStage::kStarting:
            return "Starting";
        case AppStage::kPreSetup:
            return "PreSetup";
        case AppStage::kPostSetup:
            return "PostSetup";
        case AppStage::kPreLaunchModule:
            return "PreLaunchModule";
        case AppStage::kPostLaunchModule:
            return "PostLaunchModule";
        case AppStage::kGuestThreadExit:
            return "GuestThreadExit";
    }
    return "Unknown";
}

}  // namespace daytona_debug
