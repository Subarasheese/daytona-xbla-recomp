// Daytona USA (XBLA) — Symbolic names for identified game functions
//
// Maps sub_XXXXXXXX addresses to human-readable identifiers based on the
// Xbox 360 kernel/XAM APIs each wrapper calls.
//
// Usage in stubs.cpp:
//   REX_HOOK_RAW(GpuPresent) { ... __imp__sub_82233C80(ctx, base); }
//
// Note: use REX_HOOK_RAW(SymName) with an explicit __imp__sub_XXXXXXXX call.
// The DT_TRACE_HOOK macro uses ##name for __imp__ concatenation, so it does
// not compose with these defines.

#pragma once

// ── Audio ─────────────────────────────────────────────────────────────────────
// XAudioGetVoiceCategoryVolume wrapper
#define AudioGetVoiceCategoryVolume  sub_8240F070
// XMACreateContext wrapper
#define XmaCreateContext             sub_82413890
// XMAReleaseContext wrapper
#define XmaReleaseContext            sub_824139A0
// XAudioSubmitRenderDriverFrame wrapper
#define AudioSubmitRenderFrame       sub_8242E038
// XAudioUnregisterRenderDriverClient wrapper
#define AudioUnregisterRenderDriver  sub_8242E140
// XAudioGetSpeakerConfig + XAudioRegisterRenderDriverClient — audio driver init
#define AudioInitDriver              sub_8242E438
// XAudioGetDuckerLevel wrapper
#define AudioGetDuckerLevel          sub_82460290

// ── Graphics / Video (Vd*) ────────────────────────────────────────────────────
// VdEnableDisableClockGating
#define GpuSetClockGating            sub_8222EDA8
// VdSwap + VdPersistDisplay + VdGetSystemCommandBuffer — main present call
#define GpuPresent                   sub_82233C80
// VdInitializeRingBuffer + VdEnableRingBufferRPtrWriteBack + VdSetSystemCommandBufferGpuIdentifierAddress
#define GpuInitRingBuffer            sub_82235EA0
// VdGetCurrentDisplayGamma
#define GpuGetDisplayGamma           sub_82236418
// VdShutdownEngines
#define GpuShutdownEngines           sub_82237930
// VdQueryVideoMode (primary)
#define GpuQueryVideoMode            sub_82237BC0
// VdGetCurrentDisplayInformation + VdSetDisplayMode + VdSetDisplayModeOverride
#define GpuSetDisplayMode            sub_82237F18
// VdInitializeEngines + VdSetGraphicsInterruptCallback
#define GpuInitEngines               sub_82238028
// VdIsHSIOTrainingSucceeded
#define GpuIsHsioTrained             sub_82238130
// VdSetGraphicsInterruptCallback + VdSetSystemCommandBufferGpuIdentifierAddress + VdShutdownEngines
#define GpuReinitOrShutdown          sub_82238468
// VdQueryVideoMode (variant 2)
#define GpuQueryVideoMode2           sub_8223B5E0
// VdQueryVideoFlags
#define GpuQueryVideoFlags           sub_8223D738
// VdCallGraphicsNotificationRoutines
#define GpuCallNotifications         sub_8223D8D8
// VdQueryVideoMode (variant 3)
#define GpuQueryVideoMode3           sub_8223ECE8
// VdInitializeScalerCommandBuffer
#define GpuInitScaler                sub_8223F278
// VdRetrainEDRAM + VdRetrainEDRAMWorker
#define GpuRetrainEdram              sub_8223F4F0
// VdGetCurrentDisplayInformation (primary)
#define GpuGetDisplayInfo            sub_82240348
// VdGetCurrentDisplayInformation (variant 2)
#define GpuGetDisplayInfo2           sub_82240798

// ── Input ─────────────────────────────────────────────────────────────────────
// XamInputGetCapabilities
#define InputGetCapabilities         sub_82210450
// XamInputGetState
#define InputGetState                sub_82210458
// XamInputGetCapabilities + XamInputSetState
#define InputSetState                sub_82210468
// XamInputGetCapabilitiesEx
#define InputGetCapabilitiesEx       sub_8245D348
// XamInputGetState + XamInputRawState
#define InputGetRawState             sub_8245D500

// ── File I/O ──────────────────────────────────────────────────────────────────
// NtOpenFile + NtSetInformationFile + NtClose
#define IoOpenFile                   sub_8220F858
// NtCreateEvent (standalone)
#define IoCreateEvent                sub_8220F978
// NtWriteFile (direct wrapper)
#define IoWriteFile                  sub_8220FC48
// NtCreateFile + NtClose
#define IoCreateFile                 sub_82210220
// NtCreateMutant
#define IoCreateMutant               sub_82210378
// XamContentCreateEx
#define ContentCreate                sub_822106A8
// XamContentClose
#define ContentClose                 sub_82210730
// XamContentSetThumbnail
#define ContentSetThumbnail          sub_82210738
// XamContentGetCreator
#define ContentGetCreator            sub_82210740
// XamContentGetLicenseMask
#define ContentGetLicenseMask        sub_82210748
// XamContentCreateEnumerator
#define ContentCreateEnumerator      sub_82210750
// XamContentGetDeviceState
#define ContentGetDeviceState        sub_82210758
// XamContentGetDeviceData
#define ContentGetDeviceData         sub_82210760
// NtReadFile
#define IoReadFile                   sub_824F1670
// NtWriteFile (variant 2)
#define IoWriteFile2                 sub_824F1708
// NtWriteFileGather
#define IoWriteFileGather            sub_824F17A0

// ── Network ───────────────────────────────────────────────────────────────────
// NetDll_XNetStartup (also calls XamGetSystemVersion)
#define NetXNetStartup               sub_8240DA60
// NetDll_XNetCleanup
#define NetXNetCleanup               sub_8240DB30
// NetDll_XNetRandom
#define NetXNetRandom                sub_8240DB38
// NetDll_XNetXnAddrToInAddr
#define NetAddrToInAddr              sub_8240DB48
// NetDll_XNetConnect
#define NetXNetConnect               sub_8240DB60
// NetDll_XNetGetConnectStatus
#define NetGetConnectStatus          sub_8240DB70
// NetDll_XNetQosListen
#define NetQosListen                 sub_8240DB80
// NetDll_XNetQosLookup
#define NetQosLookup                 sub_8240DBA0
// NetDll_XNetQosServiceLookup
#define NetQosServiceLookup          sub_8240DC00
// NetDll_XNetQosRelease
#define NetQosRelease                sub_8240DC18
// NetDll_XNetGetTitleXnAddr
#define NetGetTitleXnAddr            sub_8240DC28
// NetDll_WSAStartup + XamGetSystemVersion
#define NetWsaStartup                sub_8240DC38
// NetDll_WSACleanup
#define NetWsaCleanup                sub_8240DCF8
// NetDll_socket
#define NetSocket                    sub_8240DD00
// NetDll_closesocket
#define NetCloseSocket               sub_8240DD18
// NetDll_ioctlsocket
#define NetIoctlSocket               sub_8240DD28
// NetDll_setsockopt
#define NetSetSockOpt                sub_8240DD40
// NetDll_bind
#define NetBind                      sub_8240DD60
// NetDll_WSAGetOverlappedResult
#define NetGetOverlappedResult       sub_8240DD78
// NetDll_recvfrom
#define NetRecvFrom                  sub_8240DD98
// NetDll_WSARecvFrom
#define NetWsaRecvFrom               sub_8240DDB8
// NetDll_sendto
#define NetSendTo                    sub_8240DE10
// NetDll_WSASendTo
#define NetWsaSendTo                 sub_8240DE30
// NetDll_WSAGetLastError
#define NetGetLastError              sub_8240DE88

// ── XUI / Overlay ─────────────────────────────────────────────────────────────
// XamShowSigninUI
#define UiShowSignin                 sub_8220F1D0
// XamShowFriendsUI
#define UiShowFriends                sub_8220F1D8
// XamShowKeyboardUI
#define UiShowKeyboard               sub_8220F1E0
// XamShowQuickChatUI
#define UiShowQuickChat              sub_8220F1E8
// XamShowGamerCardUIForXUID
#define UiShowGamerCard              sub_8220F1F0
// XamShowAchievementsUI
#define UiShowAchievements           sub_8220F1F8
// XamShowPlayerReviewUI
#define UiShowPlayerReview           sub_8220F200
// XamShowMarketplaceUI
#define UiShowMarketplace            sub_8220F208
// XamShowDeviceSelectorUI
#define UiShowDeviceSelector         sub_8220F218
// XamShowMessageBoxUI
#define UiShowMessageBox             sub_8220F220
// XamShowGameInviteUI
#define UiShowGameInvite             sub_8220F248
// XamShowDirtyDiscErrorUI
#define UiShowDirtyDiscError         sub_8220F250
// XamShowMessageBoxUIEx + NtCreateEvent + NtClose
#define UiShowMessageBoxEx           sub_82210B78

// ── Session / XMessage ────────────────────────────────────────────────────────
// XMsgInProcessCall (in-process IPC variant A)
#define SessionIpcInProc             sub_82459C58
// XMsgInProcessCall (in-process IPC variant B)
#define SessionIpcInProc2            sub_82459E00
// XamAlloc + XamFree — session object allocator/free
#define SessionAllocFree             sub_82459EF8
// XamSessionCreateHandle + XamSessionRefObjByHandle + XMsgStartIORequest — create session
#define SessionCreate                sub_82459FD0
// XamSessionRefObjByHandle + XMsgStartIORequest (op variant 1)
#define SessionOp1                   sub_8245A180
// XamSessionRefObjByHandle + XMsgStartIORequest (op variant 2)
#define SessionOp2                   sub_8245A230
// XamSessionRefObjByHandle + XMsgStartIORequest (op variant 3)
#define SessionOp3                   sub_8245A2E0
// XamSessionRefObjByHandle + XMsgStartIORequest (op variant 4)
#define SessionOp4                   sub_8245A390
// XamSessionRefObjByHandle + XMsgStartIORequest (op variant 5)
#define SessionOp5                   sub_8245A438
// XamSessionRefObjByHandle + XMsgStartIORequest (op variant 6)
#define SessionOp6                   sub_8245A4E0
// XamSessionRefObjByHandle + XMsgStartIORequest (op variant 7)
#define SessionOp7                   sub_8245A5C0
// XamSessionRefObjByHandle + XMsgStartIORequest (op variant 8)
#define SessionOp8                   sub_8245A660
// XMsgStartIORequest (session-only, no ref, variant A)
#define SessionIo1                   sub_8245A710
// XMsgStartIORequest (session-only, no ref, variant B)
#define SessionIo2                   sub_8245A7F8
// XamSessionRefObjByHandle + XMsgStartIORequest (op variant 9)
#define SessionOp9                   sub_8245A8D8
// ObDereferenceObject only — release session object reference
#define SessionDeref                 sub_8245A958
// XMsgInProcessCall + XamAlloc + XamFree (variant A)
#define SessionAllocInProc           sub_8245A9C8
// XMsgInProcessCall + XamAlloc + XamFree (variant B)
#define SessionAllocInProc2          sub_8245AB28
// XMsgStartIORequest + XamAlloc + XamFree (variant A)
#define SessionAllocIo               sub_8245AC58
// XMsgStartIORequest + XamAlloc + XamFree (variant B)
#define SessionAllocIo2              sub_8245AF38
// XMsgInProcessCall — user profile IPC (variant A)
#define UserProfileIpc               sub_8245B3A8
// XMsgInProcessCall — user profile IPC (variant B)
#define UserProfileIpc2              sub_8245B440
// XMsgInProcessCall — user profile IPC (variant C)
#define UserProfileIpc3              sub_8245B540
// XMsgInProcessCall — user profile IPC (variant D)
#define UserProfileIpc4              sub_8245CDD0

// ── User / Profile / Voice ────────────────────────────────────────────────────
// XamUserGetSigninInfo
#define UserGetSigninInfo            sub_8220EC88
// XamUserGetName
#define UserGetName                  sub_8220EF28
// XamUserGetSigninState
#define UserGetSigninState           sub_8220EF30
// XamUserAreUsersFriends
#define UserAreUsersFriends          sub_8220EF38
// XamUserCheckPrivilege + XamUserGetSigninState + XamGetSystemVersion
#define UserCheckPrivilege           sub_8220EF40
// XamUserCreateStatsEnumerator (primary)
#define UserCreateStatsEnumerator    sub_8220F0A8
// XamUserCreateStatsEnumerator (variant 2)
#define UserCreateStatsEnumerator2   sub_8220F108
// XamUserGetXUID
#define UserGetXuid                  sub_8220F180
// XamEnumerate — achievement/stats enumeration pump
#define AchievementEnumerate         sub_82210828
// XamGetExecutionId
#define UserGetExecutionId           sub_82212250
// XNetLogonGetTitleID
#define UserGetTitleId               sub_8245B1C8
// XamUserReadProfileSettings (primary)
#define UserReadProfileSettings      sub_8245B4D8
// XamVoiceIsActiveProcess
#define VoiceIsActive                sub_824609E0
// XamUserGetDeviceContext
#define VoiceGetDeviceContext        sub_82461B98
// XamVoiceHeadsetPresent + XamVoiceIsActiveProcess + XamUserCheckPrivilege + XamUserReadProfileSettings
#define VoiceCheckHeadset            sub_82461C58
// XamVoiceClose (primary)
#define VoiceClose                   sub_82462298
// XamVoiceSubmitPacket (primary)
#define VoiceSubmitPacket            sub_82462628
// XamVoiceClose (variant 2)
#define VoiceClose2                  sub_82462988
// XamVoiceCreate + XamVoiceClose
#define VoiceCreateOrClose           sub_82462A78
// XamUserReadProfileSettings (variant 2)
#define UserReadProfileSettings2     sub_82462B38
// XamUserAreUsersFriends + XamVoiceIsActiveProcess
#define VoiceCheckFriendsActive      sub_824654A8
// XamVoiceSubmitPacket (variant 2)
#define VoiceSubmitPacket2           sub_824678D0

// ── Voice / Audio notification control ────────────────────────────────────────
// XamNotifyCreateListener + RtlInitializeCriticalSection — register voice event sink
#define VoiceCreateNotifyListener    sub_82461810
// KfAcquireSpinLock + KfReleaseSpinLock — voice spinlock wrapper
#define VoiceSpinLock                sub_82461FF8
// XNotifyGetNext + RtlCriticalSection — audio/voice notification poll
#define AudioNotifyPoll              sub_82460D48

// ── Network CritSec lifecycle (near NetDll block, 0x8240Exxx) ─────────────────
// RtlInitializeCriticalSection (net subsystem init A)
#define NetCritSecInit               sub_8240E600
// RtlEnterCriticalSection + RtlLeaveCriticalSection (net lock A)
#define NetLock                      sub_8240E698
// RtlEnterCriticalSection + RtlLeaveCriticalSection (net lock B)
#define NetLock2                     sub_8240E718
// RtlEnterCriticalSection + RtlLeaveCriticalSection (net lock C)
#define NetLock3                     sub_8240E7D0
// RtlInitializeCriticalSection (net subsystem init B)
#define NetCritSecInit2              sub_8240E828
// RtlInitializeCriticalSection (net subsystem init C)
#define NetCritSecInit3              sub_8240E850
// RtlEnterCriticalSection only
#define NetLock4                     sub_8240E8C0
// RtlTryEnterCriticalSection
#define NetTryLock                   sub_8240E8C8

// ── Audio driver lifecycle (0x8242Exxx) ───────────────────────────────────────
// ExRegisterTitleTerminateNotification + Ke*CriticalRegion + RtlCriticalSection (A)
#define AudioRegisterTerminateA      sub_8242E2D0
// ExRegisterTitleTerminateNotification + RtlCriticalSection (B)
#define AudioRegisterTerminateB      sub_8242E4E8
// ExRegisterTitleTerminateNotification + Ke*CriticalRegion + RtlCriticalSection (C)
#define AudioRegisterTerminateC      sub_8242E5B8

// ── Platform abstraction layer (0x8220Exxx) ───────────────────────────────────
// XamWriteGamerTile
#define UserWriteGamerTile           sub_8220F168
// XamNotifyCreateListener — register system notification listener
#define UserCreateNotifyListener     sub_8220F270
// NtResumeThread wrapper
#define ThreadResume                 sub_8220F2D0
// KeSetBasePriorityThread + ObDeref/Ref — set thread priority
#define ThreadSetPriority            sub_8220F310
// KeQueryBasePriorityThread + ObDeref/Ref — get thread priority
#define ThreadGetPriority            sub_8220F390
// NtSuspendThread wrapper
#define ThreadSuspend                sub_8220F410
// ObDereferenceObject + ObReferenceObjectByHandle
#define ObjectRef                    sub_8220F450
// KeSetAffinityThread + ObDeref/Ref — set thread affinity mask
#define ThreadSetAffinity            sub_8220F4E0
// XexUnloadImage
#define ModuleUnload                 sub_8220F638
// XexGetProcedureAddress (standalone wrapper)
#define ModuleGetProc                sub_8220F670
// XexLoadImage
#define ModuleLoad                   sub_8220F6E0
// XexGetModuleSection
#define ModuleGetSection             sub_8220F728
// XexGetModuleHandle (standalone wrapper)
#define ModuleGetHandle              sub_8220F760
// FscSetCacheElementCount — file system cache sizing
#define FsCacheSetCount              sub_8220F928

// ── System / thread / sync (0x82210xxx-0x82215xxx) ───────────────────────────
// RtlInitAnsiString (variant A)
#define AnsiStringInit               sub_82210190
// ExTerminateThread (via ExTerminateThread, variant A)
#define ThreadTerminateEx            sub_822102B0
// MmAllocatePhysicalMemoryEx
#define PhysMemAlloc                 sub_822102D8
// MmFreePhysicalMemory
#define PhysMemFree                  sub_82210360
// MmQueryAddressProtect
#define MemQueryProtect              sub_82210370
// NtReleaseMutant
#define MutantRelease                sub_82210410
// XMsgCancelIORequest — cancel pending IPC
#define IpcCancel                    sub_82210558
// KeQueryPerformanceFrequency
#define GetPerfFrequency             sub_822105A0
// XamLoaderLaunchTitle — launch another title / restart
#define LaunchTitle                  sub_822105D8
// ExGetXConfigSetting + XGetGameRegion — read hardware region
#define GetGameRegion                sub_82210948
// KeQuerySystemTime
#define GetSystemTime                sub_822109D8
// RtlNtStatusToDosError + RtlUnicodeToMultiByteN
#define UnicodeToMultibyte           sub_822109E0
// RtlMultiByteToUnicodeN + RtlNtStatusToDosError
#define MultibyteToUnicode           sub_82210AA0
// DbgPrint + ExGetXConfigSetting + XGetAVPack + XGetLanguage + XamLoaderTerminateTitle + XexCheckExecutablePrivilege
#define SysPlatformInfo              sub_82210CB0
// DbgPrint + XamLoaderTerminateTitle — fatal error with log message
#define FatalWithLog                 sub_82210EB4
// XamLoaderTerminateTitle (direct, variant A)
#define TerminateTitle               sub_82211040
// XamLoaderTerminateTitle (direct, variant B)
#define TerminateTitle2              sub_8221104C
// RtlNtStatusToDosError (variant A)
#define NtStatusToDosError           sub_822121F8
// NtWaitForSingleObjectEx (variant A)
#define WaitForObjectEx              sub_822122C8
// NtClose + NtOpenFile
#define FileOpen                     sub_82212330
// KeDelayExecutionThread — sleep current thread
#define ThreadSleep                  sub_822124B8
// ExCreateThread
#define ThreadCreate                 sub_82212550
// RtlNtStatusToDosError (variant B)
#define NtStatusToDosError2          sub_822125C0
// ExTerminateThread (variant B)
#define ThreadTerminate              sub_82212638
// NtWaitForSingleObjectEx + RtlCriticalSection (variant A)
#define WaitWithLock                 sub_82212790
// NtWaitForSingleObjectEx + RtlCriticalSection (variant B)
#define WaitWithLock2                sub_82212838
// MmLockAndMapSegmentArray + NtClearEvent + NtWaitForSingleObjectEx + RtlCriticalSection
#define MemLockAndWait               sub_82212930
// XexGetModuleHandle + XexGetProcedureAddress — dynamic import resolver
#define GetModuleProcAddr            sub_82212D20
// MmUnlockAndUnmapSegmentArray + NtQueueApcThread + NtSetEvent
#define MemUnlockAndSignal           sub_82213738
// NtClose (standalone handle close)
#define HandleClose                  sub_82213DC0
// NtSetEvent (standalone)
#define EventSet                     sub_82213ED0
// ExCreateThread + KeSetBasePriorityThread + NtResumeThread + ObDeref/Ref — full thread create
#define ThreadCreateWithPriority     sub_82213FB8
// NtClose + NtOpenFile + NtSetInformationFile + RtlInitAnsiString — open+configure file
#define FileOpenInfo                 sub_82214390
// RtlInitAnsiString (variant B)
#define AnsiStringInit2              sub_82214448
// RtlImageXexHeaderField — read XEX image header
#define XexHeaderGetField            sub_82214AC8
// HalReturnToFirmware — hard reboot to dashboard
#define RebootToFirmware             sub_82214BA0
// NtClearEvent
#define EventClear                   sub_82214CF0
// NtWaitForSingleObjectEx (variant B)
#define WaitForObjectEx2             sub_82214DC0
// NtClose + NtWaitForSingleObjectEx — wait then close
#define WaitAndClose                 sub_82214FC8

// ── Virtual memory management (0x82218xxx) ────────────────────────────────────
// NtAllocateVirtualMemory + NtFreeVirtualMemory
#define VMemAlloc                    sub_82218128
// NtFreeVirtualMemory (standalone)
#define VMemFree                     sub_82218368
// KeGetCurrentProcessType + NtAlloc/Free/QueryVirtualMemory (variant A)
#define VMemAllocQuery               sub_822186D0
// KeGetCurrentProcessType + NtAlloc/Free/QueryVirtualMemory (variant B)
#define VMemAllocQuery2              sub_82218738
// KeGetCurrentProcessType + NtAlloc/Free/QueryVirtualMemory (variant C)
#define VMemAllocQuery3              sub_82218758
// KeGetCurrentProcessType + NtAlloc/Free/QueryVirtualMemory (variant D)
#define VMemAllocQuery4              sub_82218768
// KeBugCheckEx + NtAllocateVirtualMemory + RtlCriticalSection — alloc or panic
#define VMemAllocOrPanic             sub_82218CA0
// NtAllocateVirtualMemory + RtlCriticalSection + RtlRaiseException — alloc or throw
#define VMemAllocOrThrow             sub_82218D38

// ── Thread-Local Storage (0x82270xxx) ─────────────────────────────────────────
// KeTlsGetValue (standalone get)
#define TlsGet                       sub_822700B0
// KeTlsAlloc + KeTlsGetValue + KeTlsSetValue — TLS slot alloc + init
#define TlsAlloc                     sub_822700D0
// KeTlsFree + KeTlsGetValue + KeTlsSetValue — TLS slot free
#define TlsFree                      sub_82270228

// ── Game engine notifications (0x821Fxxxx) ────────────────────────────────────
// XNotifyGetNext — game-logic notification poll
#define GameNotifyPoll               sub_821F9CC0
// XGetGameRegion + XNotifyPositionUI — set notification UI position
#define GameSetNotifyPos             sub_821FB698

// ── Boot video mode query (0x82131xxx) ────────────────────────────────────────
// XGetVideoMode — read current display mode during boot
#define BootGetVideoMode             sub_82131088
// XNotifyGetNext during boot polling
#define BootNotifyPoll               sub_821378D0

// ── Crypto + I/O device (0x824F0xxx) ─────────────────────────────────────────
// XeCryptSha — SHA hash computation
#define CryptoSha                    sub_824F0110
// IoCreateDevice + NtCreateEvent + NtQueryInformationFile — create I/O device object
#define IoDeviceCreate               sub_824F0598
// IoDeleteDevice + Ke*CriticalRegion + NtClose — destroy I/O device object
#define IoDeviceDelete               sub_824F08F8
// KeSetEvent + KeWaitForSingleObject + ObDereferenceObject — I/O event wait
#define IoEventWait                  sub_824F0980
// ExAllocatePoolTypeWithTag (I/O pool alloc)
#define IoPoolAlloc                  sub_824F09E8
// ExFreePool (I/O pool free)
#define IoPoolFree                   sub_824F0CA8
// RtlCompareStringN
#define StringCompareN               sub_824F0E60

// ── Kernel pool alloc (0x8247CDxx) ────────────────────────────────────────────
// ExAllocatePoolTypeWithTag (generic pool alloc)
#define PoolAlloc                    sub_8247CD48
// ExFreePool (generic pool free)
#define PoolFree                     sub_8247CD60

// ── Game boot video query (0x82131xxx) ────────────────────────────────────────
// XamNotifyCreateListener — register notify listener in platform init
#define PlatformCreateNotifyListener sub_8220F270

// ── Game globals / context accessors (0x82180Axxx) ───────────────────────────
// Loads the main game-context singleton pointer from global BSS
#define GetGameCtx                   sub_82180A58
// Loads field[40] of the game-context base struct (a secondary pointer)
#define GetGameCtxField40            sub_82180A68
// Sets a global bool flag (one-bit write to global BSS)
#define SetGameCtxFlag               sub_82180A78
// Translates a guest PPC virtual address to a host pointer (range-checked)
#define GuestAddrToHost              sub_82180A88

// ── Scene / batch render (0x821DBxxx) ────────────────────────────────────────
// Allocates / resizes the scene-object list array: (list_ptr, capacity)
#define SceneListAlloc               sub_821DB810
// Renders a single scene item by index; bounds-checks, then calls FrameEntry
#define SceneItemRender              sub_821DB888
// Batch-renders all items in a scene list; loops 0..count calling SceneItemRender
#define SceneBatchRender             sub_821DBB20

// ── Game simulation helpers (0x821D0Fxxx / 0x8215xxxx-0x8216xxxx) ────────────
// sprintf-like format string processor: (r3=dst, r4=end, r5=fmt, r6=va_list)
#define GameSprintf                  sub_821D0F00
// Per-car physics frame: calls PhysicsIntegrate→PhysicsBodyUpdate→CollisionDetect
#define CarPhysicsSimStep            sub_8215D6B8
// First-pass physics step (friction, tire forces) called before PhysicsBodyUpdate
#define PhysicsIntegrate             sub_8215B4E8
// Full physics body update (1563 lines): spring/suspension/contact forces
#define PhysicsBodyUpdate            sub_8215B790
// Full collision detection (2506 lines): BVH traversal + contact manifold
#define CollisionDetect              sub_8215C600
// Per-car update: reads global ctx, stores state-machine IDs, dispatches sound cues
#define CarUpdate                    sub_821699F8
// Per-AI-car update: reads global ctx, updates control state, vector math
#define AiCarUpdate                  sub_821728E8
// Initializes car race-start state: reads speed field, stores state ID, plays sound 448
#define CarRaceStart                 sub_8216E8E8
// Initializes car physics state struct: zeros vector triplets, loads spring constants
#define CarPhysicsInit               sub_821A3978
// Argument-shuffle trampoline forwarding to sub_8212B4D8 (one-hop, no logic)
#define FrameRenderForward           sub_8212B800

// ── Physics / state init helpers (0x82135xxx) ────────────────────────────────
// Initializes a render-color state struct: stores RGBA byte pairs, zeros floats,
// sets material index 6 and draw mode 13; loads a color table entry as default
#define RenderColorInit              sub_82135A68
// Zeroes out a physics body state struct: {-1, 0, 0} header + zero float triplets
#define PhysBodyReset                sub_82135850

// ── Game object state predicates (0x82162Exxx) ────────────────────────────────
// Returns 1 if [r3+0] == 3 (object in "active" state); 0 otherwise
#define ObjIsActive                  sub_82162ED8
// Returns 1 if [r3+0] != 0 (object state non-zero); 0 otherwise
#define ObjIsEnabled                 sub_82162EF0

// ── Managed heap / pool allocator (0x8219Cxxx) ────────────────────────────────
// Allocates from one of 8 typed pools: (r3=pool_type 0-7, r4=size, r5=table, r6=flags)
#define ManagedPoolAlloc             sub_8219C230

// ── Endian-converting buffer helpers (0x821EE3xx) ─────────────────────────────
// Reads u32 big-endian from managed buffer at byte offset r4; stores to [r3+0]
#define BufReadU32BE                 sub_821EE3F8
// Byteswaps r4 and writes to managed buffer at byte offset r3
#define BufWriteU32BE                sub_821EE3D8
// Byteswaps low 16 bits of r4 and writes to managed buffer at byte offset r3
#define BufWriteU16BE                sub_821EE410

// ── Object type-dispatch family (0x821EDxxx) ─────────────────────────────────
// All functions below share the same structure: extract a "class tag" from the
// upper bits of the passed pointer, use it to index a per-type vtable at
// 0x82076FF8, and tail-call the entry at a fixed slot offset.
// Dispatch on r4, slot +8  in the per-type vtable entry (64 bytes each)
#define ObjDispatch_r4_8             sub_821ED488
// Dispatch on r4, slot +16
#define ObjDispatch_r4_16            sub_821ED4C0
// Dispatch on r4, slot +24 (pattern continues in same stride)
#define ObjDispatch_r4_24            sub_821ED4F8
// Dispatch on r3, slot +20
#define ObjDispatch_r3_20            sub_821ED610
// Dispatch on r3, slot +28
#define ObjDispatch_r3_28            sub_821ED648

// ── Sound / cue system (0x8213DCxx / 0x821E9Cxx) ─────────────────────────────
// Looks up sound definition by ID in table at image_base+11368; returns 1 if found
#define SoundLookup                  sub_8213DC50
// Writes one entry into the sound ring buffer; returns whether free slots remain
#define SoundQueueWrite              sub_8213DC90
// Reads remaining-count from the sound ring buffer (used after lookup)
#define SoundQueueRead               sub_8213DCE0
// Trigger a sound cue: lookup by (r3>>4 bits), queue if found, store to global
#define SoundCueTrigger              sub_821E9CC8
// Poll sound cue with a different lookup path (no explicit ID in r3)
#define SoundCuePoll                 sub_821E9D18
// Dequeue one entry from the sound ring buffer (ring-buffer pop)
#define SoundDequeue                 sub_821E9D60

// ── GPU draw-call dispatch layer (0x8225xxxx-0x8226xxxx) ─────────────────────
// Validates vertex/index buffer sizes before a draw; returns error if overflow
#define GpuDrawPrecheck              sub_82258F20
// Executes a draw call using the prepared buffers; returns error on failure
#define GpuDrawExecute               sub_8225A0B0
// Processes a type-14 texture descriptor (width×height mip chain) for rendering
#define GpuTextureProcess            sub_8225A8F0
// Maps a D3D texture format/type code to an internal GPU enum (switch on bits 8-11)
#define GpuFormatLookup              sub_8219BE00
// Checks ring-buffer space (sub_8219C370) then writes a D3D command packet
#define GpuSubmitCommand             sub_8219BF38

// ── Texture load pipeline (0x8224xxxx) ───────────────────────────────────────
// Return pointer to global texture state table A (BSS_BASE-32248 + 3024)
#define GetTexStateTableA            sub_82241750
// Return pointer to global texture state table B (BSS_BASE-32248 + 3192)
#define GetTexStateTableB            sub_82241740
// Texture load entry point: marshals args around BSS[4028] descriptor, calls sub_822426E8
#define TexLoad                      sub_822429C8
// Finalize texture B: check BSS[14084] hook, call GpuFormatLookup, set up GPU texture state
#define TexFinalizeB                 sub_82225470
// Finalize texture A: check BSS[14088] hook, process texture descriptor variant A
#define TexFinalizeA                 sub_82225658

// ── GPU state & cache flush (0x8241xxxx) ──────────────────────────────────────
// Set texture state entry's ID field (bits 12-19 of entry[0]); marks entry dirty at +48
#define TexStateSetId                sub_82414010
// Set texture state entry's size (bits 0-25 of entry[8]) and format (bits 5-7 of entry[4])
#define TexStateSetSizeFormat        sub_82414068
// GPU command-cache flush: RtlEnterCritSection, write HW trigger reg, iterate 320 entries
// (hooked in debug via kGpuCacheFlush in daytona_debug.h)
#define GpuCacheFlush                sub_824140B8
// GPU state flush: iterate dirty 96-byte state entries; dcbf cache, lvx128/stvx128 to HW regs
#define GpuStateFlush                sub_82414228

// ── Render pipeline orchestrators ────────────────────────────────────────────
// Outermost per-frame entry — calls FrameRender
#define FrameEntry                   sub_8212BD68
// Full frame renderer (478 lines): BeginScene→SceneRender→EndScene
#define FrameRender                  sub_8212B828
// Scene orchestrator: checks dirty flags, dispatches to Scene3dRender
#define SceneRender                  sub_8212B3C8
// 3D scene renderer (678 lines): builds draw list, calls RenderTargetSetup
#define Scene3dRender                sub_8222CB88
// Render target binder before Scene3dRender: calls RenderTargetCore
#define RenderTargetSetup            sub_8222EBC0
// Render target EDRAM core (reads format/viewport, VMX ops)
#define RenderTargetCore             sub_8222DA70
// Per-object scene setup (camera/projection state)
#define SetupScene                   sub_8212AF18
// Render target attach helper (called by SceneRender on dirty flags)
#define SetupRenderTarget            sub_8212AC20
// Scaler render pass — calls RenderPass + GpuInitScaler
#define ScalerPass                   sub_8223AB80
// Full render pass (1160 lines): shader bind, draw, state flush
#define RenderPass                   sub_82239F30
// Present-side display update (Clear + IoWriteFile + display notify)
#define DisplayUpdate                sub_82240B00
// Present-side update pump: calls DisplayUpdate
#define PresentUpdate                sub_822413C0

// ── D3D-like device state setters (0x8221Exxx) ───────────────────────────────
// stw r4, 13872(r3) — single-word device state store (slot A)
#define DeviceSetStateA              sub_8221E940
// bitfield insert into device+12156, bits 2-8 (slot B, e.g. alpha-test ref)
#define DeviceSetStateB              sub_8221E950
// stw r4, device+10688; sets dirty bit 35 (slot C, e.g. backbuffer gamma)
#define DeviceSetStateC              sub_8221E658
// reads bit 0 of device+10688 — getter for slot C
#define DeviceGetStateC              sub_8221E680

// ── Shader source builder (0x8227xxxx) ────────────────────────────────────────
// Append a formatted string fragment to the shader source buffer at r3+1532[+1536]
// Called 320 times from recomp.11; r4=format-string ptr, r5-r10=args
#define ShaderSrcAppendStr           sub_8227CE08
// Append the same string N times (r3+1544 iterations), each calling ShaderSrcAppendStr
#define ShaderSrcAppendN             sub_8227CEE0
// Append a declaration block to shader source (reads r3+1816; takes r4-r7 args)
#define ShaderSrcAppendDecl          sub_8227CF38
// Validated sprintf helper: checks r4 <= 0x7FFFFFFF then calls sub_82241DE8
#define ShaderSrcSprintf             sub_82242960
// Core format-into-buffer: sprintf-like, writes to stack-buf pointed to by r6 chain
#define ShaderSrcFormat              sub_82241DE8

// ── Shader pipeline (0x822Fxxxx) ─────────────────────────────────────────────
// Drain GPU command linked list at r3+152: loops while bit0 clear and ptr non-null,
// submits each node via GpuSubmitCommand, writes 0 sentinel
#define ShaderCmdListDrain           sub_822F8720
// Flush shader command buffer: loops buf+132 linked list then calls ShaderCmdListDrain
#define ShaderCmdBufFlush            sub_822F8790
// Allocate 4096-byte GPU command buffer via GpuFormatLookup; stores self-ptr at buf[0]
#define ShaderBufAlloc               sub_822F8868
// Flush pending VS/PS state + two cmd bufs (at self+812 and self+972) then submit
#define ShaderStateFlush             sub_822F88C8
// Query shader program ID: reads [r3+4]+52 vs [r3+0]; writes 0x8000FFFF on match
#define ShaderIdGet                  sub_822F8D48
// Shader object constructor: zeroes arrays at +812 and +972, inits linked-list fields
#define ShaderObjInit                sub_822F8EB8
// Shader object finalizer: stores VS handle, pixel handle, bytecode ptr/size, flags
#define ShaderObjFinalize            sub_822F8F98
// Main shader compilation: sets compile flag, calls AFB08 + EE8 + 89B8
#define ShaderCompile                sub_822F9180
// Shader build inner loop: lookup → alloc (sub_824DFC40) → init → finalize
#define ShaderBuildLoop              sub_822F92D8
// Shader program lookup dispatcher: reads function ptr from 0x83434524 and calls it if non-null
// Hooked in stubs.cpp to always return 0, forcing ShaderBuildLoop to always alloc new programs
#define ShaderProgLookup             sub_824DFC40

// ── Texture cache management (0x824DCxxx / 0x824Exxx) ────────────────────────
// Acquire texture cache lock: fast path for internal slots, CriticalSection fallback
#define TexLock                      sub_824DC668
// Release texture cache lock (counterpart to TexLock)
#define TexUnlock                    sub_824DC628
// Acquire lock by slot index (< 20 uses hash slot lock, else CriticalSection at obj+32)
#define TexLockBySlot                sub_824DC6D8
// Find texture cache entry: maps texture descriptor → slot index, returns 72-byte entry ptr
#define TexCacheFind                 sub_824DC270
// Look up or allocate a texture slot (r4 must be power-of-2 size)
#define TexSlotLookup                sub_824DC420
// Traverse texture cache list A: lock all entries with flags & 0x83 ≠ 0
#define TexCacheTraverseA            sub_824DC8A8
// Traverse texture cache list B: same pattern as A, second GPU submission pass
#define TexCacheTraverseB            sub_824EB2B8
// Hash texture descriptor to a slot index (returns -1/-2 on miss)
#define TexHash                      sub_824E4990
// Acquire hash table entry lock (r3 = slot bucket index)
#define TexHashLock                  sub_824E30B0

// ── Session management (0x8245xxxx) ──────────────────────────────────────────
// SessionCreate is already defined above; additional session operations below:
// Session join (XamSessionRefObjByHandle + XMsgStartIORequest)
#define SessionJoin                  sub_8245A180
// Session leave
#define SessionLeave                 sub_8245A230
// Session delete
#define SessionDelete                sub_8245A2E0
// Session modify
#define SessionModify                sub_8245A4E0

// ── Shader compilation infrastructure (0x823Cxxxx / 0x823Bxxxx / 0x823867xxxx) ─
// Shader assert/error reporter: r3=fatal, r4=line, r5=file, r6=msg, r7=code
#define ShaderAssert                 sub_822FE3C8
// Variadic shader log: saves r5-r10 as va_list, calls sub_822F8480 (Printf-style)
#define ShaderVaLog                  sub_822F8628
// Bump allocator for shader compilation arena; rounds to 4-byte, auto-grows
#define ShaderArenaAlloc             sub_823C2CC0
// Walk shader bytecode tokens and map register indices into r31+16800 table (6400-byte bitmap)
#define ShaderRegMap                 sub_823C1BB8
// Initialize shader compilation context: zero 23380 bytes, set up linked lists, call ShaderRegMap
#define ShaderCtxInit                sub_823C2638
// Reset per-draw shader compile state (2000-2700 range); alloc 8192-byte work buffer
#define ShaderStateReset             sub_823C3958
// Initialize shader program parameter block (fields 1376-1536)
#define ShaderProgParamInit          sub_823C3A08
// Allocate 24-byte shader job entry (vtable + object ref + VS/PS shader ctx)
#define ShaderJobAlloc               sub_823C3A80
// Allocate 24-byte shader job entry, simpler form (vtable + caller + shader ctx)
#define ShaderJobCreate              sub_823C42F8
// Full shader program init: zero 16-entry slot table, store semantics string table
#define ShaderProgInit               sub_823C3D30
// Main shader build/reuse orchestrator: StateReset → ParamInit → JobAlloc → ProgLookup
// If lookup returns 0 (cache miss) → alloc new slot, call ShaderProgInit, compile
#define ShaderBuildOrReuse           sub_823C4350
// Second shader build/reuse variant (uses ShaderJobCreate instead of ShaderJobAlloc)
#define ShaderBuildOrReuse2          sub_823C4920
// Shader source-to-bytecode compiler: calls vtable[12] on shader source, checks HLSL magic
#define ShaderSourceCompile          sub_823AFB08
// Shader pass list builder: count list nodes, allocate index array, store at obj+96
#define ShaderPassListBuild          sub_82391EE8
// Get allocator for a given size class (returns r31[(size+2)*4] slot)
#define ShaderSlotAlloc              sub_823B09A8

// ── Shader arena management (0x823Exxxx) ─────────────────────────────────────
// Grow shader compilation arena: alloc ≥12248-byte chunk via device vtable, chain to old block
#define ShaderArenaGrow              sub_823E3540
// Release shader arena: walks chain calling ShaderArenaFreeBlock (sub_823C2E48)
#define ShaderArenaRelease           sub_823E35B0
// Create 28-byte shader job with two callback pointers (VS and PS compile callbacks)
#define ShaderJobWith2Callbacks      sub_823E3650
// Dispatch shader job: calls vtable[7] on job record then checks pipeline progress
#define ShaderJobDispatch            sub_823E2908

// ── Task / work-item system (0x8247xxxx) ─────────────────────────────────────
// Set task status and notify work queue if bit 15 was clear (transition complete→notified)
#define TaskNotifyComplete           sub_8247BAC0
// Set task status with async flag; notify if bit 15 was clear (variant with cancel flag)
#define TaskNotifyAsync              sub_8247BB08
// Task object constructor: set up doubly-linked list, IPC handle, flags, queue entry
#define TaskInit                     sub_8247BCC8
// Task release: calls sub_824750E8(obj, 11) — destroy/decref
#define TaskRelease                  sub_8247BDB0
// Set packed queue-index field in task obj+24 (5-bit index in top bits of 64-bit word)
#define TaskSetQueueIndex            sub_8247BDC0
// Submit task to work queue: reads device at obj+32, checks flags, dispatches
#define TaskDispatch                 sub_8247BDF0
// Compute variable-length integer byte size (thresholds: 15/127/16383 = 1/2/3/4 bytes)
#define TaskVarIntSize               sub_8247BC50
// Wrap ExFreePool: frees r4 (pool tag already embedded in caller context)
#define PoolFree                     sub_8247CD60
// Initialize 3-word linked-list pool header: [0]=vtable, [4]=null, [8]=null
#define PoolListInit                 sub_8247CD68
// Atomic-increment global work queue counter using lwarx/stwcx CAS loop
#define PoolAtomicInc                sub_8247CD88

// ── Shader IR analysis (0x8238xxxx) ──────────────────────────────────────────
// Main shader IR liveness/dataflow analysis pass (15690-line function)
// Processes tagged-pointer linked-list of shader IR nodes, updates live-use sets
#define ShaderIrAnalysis             sub_823867E0

// ── XEX dynamic loader helpers (near input init) ─────────────────────────────
// XMsgStartIORequest (platform IPC, variant A)
#define PlatformIpc                  sub_8220ECE0
// XMsgStartIORequest (platform IPC, variant B)
#define PlatformIpc2                 sub_8220ED58
// XMsgStartIORequest (platform IPC, variant C)
#define PlatformIpc3                 sub_8220EDD0
// XMsgStartIORequest + XexCheckExecutablePrivilege — privilege-gated IPC
#define PlatformPrivilegeIpc         sub_8220EE68
// XMsgStartIORequest (platform IPC, variant D)
#define PlatformIpc4                 sub_8220EFD0
// NtWaitForSingleObjectEx (platform async wait)
#define PlatformAsyncWait            sub_8220FA20
