#pragma once

#include <array>
#include <cstdint>

#include <rex/ppc/context.h>

namespace daytona_debug {

enum class AppStage : uint32_t {
    kStarting,
    kPreSetup,
    kPostSetup,
    kPreLaunchModule,
    kPostLaunchModule,
    kGuestThreadExit,
};

enum class HookId : size_t {
    kGpuInterrupt,
    kGpuDescriptorSubmit,
    kGpuCacheFlush,
    kBootPresentA,
    kBootPresentB,
    kRenderPresentWorker,
    kFrameFenceReset,
    kFrameFenceWait,
    kCommandFlush,
    kCommandAlloc,
    kVdSwapCaller,
    kCreateDeviceLike,
    kSetRenderStateA,
    kSetRenderStateB,
    kSetRenderStateC,
    kBootPoll,
    kBackbufferSetup,
    kSceneSetup,
    kRenderTargetSetup,
    kBeginSceneLike,
    kClearLike,
    kMemcpyLikeDc8,
    kMemcpyLikeEb2,
    kPostStateGateEnter,
    kPostStateGateExit,
    kBootLogoSetupEnter,
    kBootLogoSetupExit,
    kBootPollEnter,
    kBootPollExit,
    kEndSceneLike,
    kBootCreateTarget,
    kBootCreateTarget2,
    kBootBindTarget,
    kBootSetTarget,
    kBootShaderPath,
    kBootAllocA,
    kBootClearAllocA,
    kBootUnlockAllocA,
    kBootMakeDraw,
    kBootAllocB,
    kBootClearAllocB,
    kBootUnlockAllocB,
    kBootObjectSetup,
    kBootUiSetupA,
    kBootUiSetupB,
    kBootUiSetupC,
    kBootUiSetupD,
    kBootUiSetupE,
    kBootUiSetupEExit,
    kBootUiSetupF,
    kBootUiSetupFExit,
    kBootUiSetupG,
    kBootUiSetupGExit,
    kUiTextureSizeA,
    kUiTextureLoad,
    kUiTextureFinalizeA,
    kUiTextureSizeB,
    kUiTextureFinalizeB,
    kMainShaderSetup,

    // Audio domain
    kAudioInitDriver,
    kAudioSubmitRenderFrame,

    // Input domain
    kInputGetRawState,
    kInputGetCapabilitiesEx,

    // Video domain
    kGpuSetDisplayMode,
    kGpuInitEngines,
    kGpuInitRingBuffer,

    // Network domain
    kNetXNetStartup,
    kNetWsaStartup,

    // XUI / overlay domain
    kUiShowMessageBoxEx,
    kUiShowSignin,

    // User / profile domain
    kUserGetSigninInfo,
    kVoiceCheckHeadset,

    // Session domain
    kSessionCreate,

    // Platform / system domain
    kSysPlatformInfo,
    kLaunchTitle,

    // Crypto
    kCryptoSha,

    // Render pipeline orchestrators
    kFrameRender,
    kSceneRender,
    kScene3dRender,

    kCount,
};

struct HookStats {
    const char* name = "";
    uint32_t count = 0;
    uint32_t lr = 0;
    uint32_t r3 = 0;
    uint32_t r4 = 0;
    uint32_t r5 = 0;
    uint32_t thread_hash = 0;
    double last_seen_seconds = 0.0;
};

struct HookEvent {
    uint64_t sequence = 0;
    const char* name = "";
    uint32_t count = 0;
    uint32_t lr = 0;
    uint32_t r3 = 0;
    uint32_t r4 = 0;
    uint32_t r5 = 0;
    uint32_t thread_hash = 0;
    double age_seconds = 0.0;
};

struct Snapshot {
    AppStage stage = AppStage::kStarting;
    double uptime_seconds = 0.0;
    std::array<HookStats, static_cast<size_t>(HookId::kCount)> hooks{};
    std::array<HookEvent, 24> events{};
};

void SetStage(AppStage stage);
uint32_t RecordHook(HookId id, const PPCContext& ctx);
Snapshot GetSnapshot();
const char* StageName(AppStage stage);

}  // namespace daytona_debug
