#include <rex/ppc.h>
#include <cstdint>

#ifdef _WIN32
extern "C" {

void __imp__DbgBreakPoint(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _DbgBreakPoint(PPCContext& ctx, std::uint8_t* base) {
  __imp__DbgBreakPoint(ctx, base);
}

void __imp__ExAllocatePoolTypeWithTag(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _ExAllocatePoolTypeWithTag(PPCContext& ctx, std::uint8_t* base) {
  __imp__ExAllocatePoolTypeWithTag(ctx, base);
}

void __imp__ExCreateThread(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _ExCreateThread(PPCContext& ctx, std::uint8_t* base) {
  __imp__ExCreateThread(ctx, base);
}

void __imp__ExFreePool(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _ExFreePool(PPCContext& ctx, std::uint8_t* base) {
  __imp__ExFreePool(ctx, base);
}

void __imp__ExGetXConfigSetting(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _ExGetXConfigSetting(PPCContext& ctx, std::uint8_t* base) {
  __imp__ExGetXConfigSetting(ctx, base);
}

void __imp__ExRegisterTitleTerminateNotification(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _ExRegisterTitleTerminateNotification(PPCContext& ctx, std::uint8_t* base) {
  __imp__ExRegisterTitleTerminateNotification(ctx, base);
}

void __imp__ExTerminateThread(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _ExTerminateThread(PPCContext& ctx, std::uint8_t* base) {
  __imp__ExTerminateThread(ctx, base);
}

void __imp__FscSetCacheElementCount(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _FscSetCacheElementCount(PPCContext& ctx, std::uint8_t* base) {
  __imp__FscSetCacheElementCount(ctx, base);
}

void __imp__HalReturnToFirmware(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _HalReturnToFirmware(PPCContext& ctx, std::uint8_t* base) {
  __imp__HalReturnToFirmware(ctx, base);
}

void __imp__IoCheckShareAccess(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _IoCheckShareAccess(PPCContext& ctx, std::uint8_t* base) {
  __imp__IoCheckShareAccess(ctx, base);
}

void __imp__IoCompleteRequest(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _IoCompleteRequest(PPCContext& ctx, std::uint8_t* base) {
  __imp__IoCompleteRequest(ctx, base);
}

void __imp__IoCreateDevice(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _IoCreateDevice(PPCContext& ctx, std::uint8_t* base) {
  __imp__IoCreateDevice(ctx, base);
}

void __imp__IoDeleteDevice(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _IoDeleteDevice(PPCContext& ctx, std::uint8_t* base) {
  __imp__IoDeleteDevice(ctx, base);
}

void __imp__IoInvalidDeviceRequest(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _IoInvalidDeviceRequest(PPCContext& ctx, std::uint8_t* base) {
  __imp__IoInvalidDeviceRequest(ctx, base);
}

void __imp__IoRemoveShareAccess(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _IoRemoveShareAccess(PPCContext& ctx, std::uint8_t* base) {
  __imp__IoRemoveShareAccess(ctx, base);
}

void __imp__IoSetShareAccess(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _IoSetShareAccess(PPCContext& ctx, std::uint8_t* base) {
  __imp__IoSetShareAccess(ctx, base);
}

void __imp__KeAcquireSpinLockAtRaisedIrql(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KeAcquireSpinLockAtRaisedIrql(PPCContext& ctx, std::uint8_t* base) {
  __imp__KeAcquireSpinLockAtRaisedIrql(ctx, base);
}

void __imp__KeBugCheck(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KeBugCheck(PPCContext& ctx, std::uint8_t* base) {
  __imp__KeBugCheck(ctx, base);
}

void __imp__KeBugCheckEx(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KeBugCheckEx(PPCContext& ctx, std::uint8_t* base) {
  __imp__KeBugCheckEx(ctx, base);
}

void __imp__KeDelayExecutionThread(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KeDelayExecutionThread(PPCContext& ctx, std::uint8_t* base) {
  __imp__KeDelayExecutionThread(ctx, base);
}

void __imp__KeEnableFpuExceptions(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KeEnableFpuExceptions(PPCContext& ctx, std::uint8_t* base) {
  __imp__KeEnableFpuExceptions(ctx, base);
}

void __imp__KeEnterCriticalRegion(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KeEnterCriticalRegion(PPCContext& ctx, std::uint8_t* base) {
  __imp__KeEnterCriticalRegion(ctx, base);
}

void __imp__KeGetCurrentProcessType(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KeGetCurrentProcessType(PPCContext& ctx, std::uint8_t* base) {
  __imp__KeGetCurrentProcessType(ctx, base);
}

void __imp__KeInitializeDpc(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KeInitializeDpc(PPCContext& ctx, std::uint8_t* base) {
  __imp__KeInitializeDpc(ctx, base);
}

void __imp__KeInsertQueueDpc(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KeInsertQueueDpc(PPCContext& ctx, std::uint8_t* base) {
  __imp__KeInsertQueueDpc(ctx, base);
}

void __imp__KeLeaveCriticalRegion(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KeLeaveCriticalRegion(PPCContext& ctx, std::uint8_t* base) {
  __imp__KeLeaveCriticalRegion(ctx, base);
}

void __imp__KeLockL2(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KeLockL2(PPCContext& ctx, std::uint8_t* base) {
  __imp__KeLockL2(ctx, base);
}

void __imp__KeQueryBasePriorityThread(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KeQueryBasePriorityThread(PPCContext& ctx, std::uint8_t* base) {
  __imp__KeQueryBasePriorityThread(ctx, base);
}

void __imp__KeQueryPerformanceFrequency(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KeQueryPerformanceFrequency(PPCContext& ctx, std::uint8_t* base) {
  __imp__KeQueryPerformanceFrequency(ctx, base);
}

void __imp__KeQuerySystemTime(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KeQuerySystemTime(PPCContext& ctx, std::uint8_t* base) {
  __imp__KeQuerySystemTime(ctx, base);
}

void __imp__KeReleaseSpinLockFromRaisedIrql(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KeReleaseSpinLockFromRaisedIrql(PPCContext& ctx, std::uint8_t* base) {
  __imp__KeReleaseSpinLockFromRaisedIrql(ctx, base);
}

void __imp__KeResetEvent(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KeResetEvent(PPCContext& ctx, std::uint8_t* base) {
  __imp__KeResetEvent(ctx, base);
}

void __imp__KeSetAffinityThread(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KeSetAffinityThread(PPCContext& ctx, std::uint8_t* base) {
  __imp__KeSetAffinityThread(ctx, base);
}

void __imp__KeSetBasePriorityThread(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KeSetBasePriorityThread(PPCContext& ctx, std::uint8_t* base) {
  __imp__KeSetBasePriorityThread(ctx, base);
}

void __imp__KeSetCurrentProcessType(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KeSetCurrentProcessType(PPCContext& ctx, std::uint8_t* base) {
  __imp__KeSetCurrentProcessType(ctx, base);
}

void __imp__KeSetEvent(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KeSetEvent(PPCContext& ctx, std::uint8_t* base) {
  __imp__KeSetEvent(ctx, base);
}

void __imp__KeTlsAlloc(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KeTlsAlloc(PPCContext& ctx, std::uint8_t* base) {
  __imp__KeTlsAlloc(ctx, base);
}

void __imp__KeTlsFree(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KeTlsFree(PPCContext& ctx, std::uint8_t* base) {
  __imp__KeTlsFree(ctx, base);
}

void __imp__KeTlsGetValue(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KeTlsGetValue(PPCContext& ctx, std::uint8_t* base) {
  __imp__KeTlsGetValue(ctx, base);
}

void __imp__KeTlsSetValue(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KeTlsSetValue(PPCContext& ctx, std::uint8_t* base) {
  __imp__KeTlsSetValue(ctx, base);
}

void __imp__KeUnlockL2(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KeUnlockL2(PPCContext& ctx, std::uint8_t* base) {
  __imp__KeUnlockL2(ctx, base);
}

void __imp__KeWaitForMultipleObjects(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KeWaitForMultipleObjects(PPCContext& ctx, std::uint8_t* base) {
  __imp__KeWaitForMultipleObjects(ctx, base);
}

void __imp__KeWaitForSingleObject(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KeWaitForSingleObject(PPCContext& ctx, std::uint8_t* base) {
  __imp__KeWaitForSingleObject(ctx, base);
}

void __imp__KfAcquireSpinLock(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KfAcquireSpinLock(PPCContext& ctx, std::uint8_t* base) {
  __imp__KfAcquireSpinLock(ctx, base);
}

void __imp__KfReleaseSpinLock(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KfReleaseSpinLock(PPCContext& ctx, std::uint8_t* base) {
  __imp__KfReleaseSpinLock(ctx, base);
}

void __imp__KiApcNormalRoutineNop(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _KiApcNormalRoutineNop(PPCContext& ctx, std::uint8_t* base) {
  __imp__KiApcNormalRoutineNop(ctx, base);
}

void __imp__MmAllocatePhysicalMemoryEx(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _MmAllocatePhysicalMemoryEx(PPCContext& ctx, std::uint8_t* base) {
  __imp__MmAllocatePhysicalMemoryEx(ctx, base);
}

void __imp__MmFreePhysicalMemory(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _MmFreePhysicalMemory(PPCContext& ctx, std::uint8_t* base) {
  __imp__MmFreePhysicalMemory(ctx, base);
}

void __imp__MmGetPhysicalAddress(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _MmGetPhysicalAddress(PPCContext& ctx, std::uint8_t* base) {
  __imp__MmGetPhysicalAddress(ctx, base);
}

void __imp__MmLockAndMapSegmentArray(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _MmLockAndMapSegmentArray(PPCContext& ctx, std::uint8_t* base) {
  __imp__MmLockAndMapSegmentArray(ctx, base);
}

void __imp__MmMapIoSpace(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _MmMapIoSpace(PPCContext& ctx, std::uint8_t* base) {
  __imp__MmMapIoSpace(ctx, base);
}

void __imp__MmQueryAddressProtect(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _MmQueryAddressProtect(PPCContext& ctx, std::uint8_t* base) {
  __imp__MmQueryAddressProtect(ctx, base);
}

void __imp__MmUnlockAndUnmapSegmentArray(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _MmUnlockAndUnmapSegmentArray(PPCContext& ctx, std::uint8_t* base) {
  __imp__MmUnlockAndUnmapSegmentArray(ctx, base);
}

void __imp__NetDll_WSACleanup(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NetDll_WSACleanup(PPCContext& ctx, std::uint8_t* base) {
  __imp__NetDll_WSACleanup(ctx, base);
}

void __imp__NetDll_WSAGetLastError(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NetDll_WSAGetLastError(PPCContext& ctx, std::uint8_t* base) {
  __imp__NetDll_WSAGetLastError(ctx, base);
}

void __imp__NetDll_WSAGetOverlappedResult(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NetDll_WSAGetOverlappedResult(PPCContext& ctx, std::uint8_t* base) {
  __imp__NetDll_WSAGetOverlappedResult(ctx, base);
}

void __imp__NetDll_WSARecvFrom(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NetDll_WSARecvFrom(PPCContext& ctx, std::uint8_t* base) {
  __imp__NetDll_WSARecvFrom(ctx, base);
}

void __imp__NetDll_WSASendTo(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NetDll_WSASendTo(PPCContext& ctx, std::uint8_t* base) {
  __imp__NetDll_WSASendTo(ctx, base);
}

void __imp__NetDll_WSAStartup(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NetDll_WSAStartup(PPCContext& ctx, std::uint8_t* base) {
  __imp__NetDll_WSAStartup(ctx, base);
}

void __imp__NetDll_XNetCleanup(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NetDll_XNetCleanup(PPCContext& ctx, std::uint8_t* base) {
  __imp__NetDll_XNetCleanup(ctx, base);
}

void __imp__NetDll_XNetConnect(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NetDll_XNetConnect(PPCContext& ctx, std::uint8_t* base) {
  __imp__NetDll_XNetConnect(ctx, base);
}

void __imp__NetDll_XNetGetConnectStatus(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NetDll_XNetGetConnectStatus(PPCContext& ctx, std::uint8_t* base) {
  __imp__NetDll_XNetGetConnectStatus(ctx, base);
}

void __imp__NetDll_XNetGetTitleXnAddr(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NetDll_XNetGetTitleXnAddr(PPCContext& ctx, std::uint8_t* base) {
  __imp__NetDll_XNetGetTitleXnAddr(ctx, base);
}

void __imp__NetDll_XNetQosListen(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NetDll_XNetQosListen(PPCContext& ctx, std::uint8_t* base) {
  __imp__NetDll_XNetQosListen(ctx, base);
}

void __imp__NetDll_XNetQosLookup(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NetDll_XNetQosLookup(PPCContext& ctx, std::uint8_t* base) {
  __imp__NetDll_XNetQosLookup(ctx, base);
}

void __imp__NetDll_XNetQosRelease(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NetDll_XNetQosRelease(PPCContext& ctx, std::uint8_t* base) {
  __imp__NetDll_XNetQosRelease(ctx, base);
}

void __imp__NetDll_XNetQosServiceLookup(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NetDll_XNetQosServiceLookup(PPCContext& ctx, std::uint8_t* base) {
  __imp__NetDll_XNetQosServiceLookup(ctx, base);
}

void __imp__NetDll_XNetRandom(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NetDll_XNetRandom(PPCContext& ctx, std::uint8_t* base) {
  __imp__NetDll_XNetRandom(ctx, base);
}

void __imp__NetDll_XNetStartup(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NetDll_XNetStartup(PPCContext& ctx, std::uint8_t* base) {
  __imp__NetDll_XNetStartup(ctx, base);
}

void __imp__NetDll_XNetXnAddrToInAddr(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NetDll_XNetXnAddrToInAddr(PPCContext& ctx, std::uint8_t* base) {
  __imp__NetDll_XNetXnAddrToInAddr(ctx, base);
}

void __imp__NetDll_bind(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NetDll_bind(PPCContext& ctx, std::uint8_t* base) {
  __imp__NetDll_bind(ctx, base);
}

void __imp__NetDll_closesocket(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NetDll_closesocket(PPCContext& ctx, std::uint8_t* base) {
  __imp__NetDll_closesocket(ctx, base);
}

void __imp__NetDll_ioctlsocket(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NetDll_ioctlsocket(PPCContext& ctx, std::uint8_t* base) {
  __imp__NetDll_ioctlsocket(ctx, base);
}

void __imp__NetDll_recvfrom(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NetDll_recvfrom(PPCContext& ctx, std::uint8_t* base) {
  __imp__NetDll_recvfrom(ctx, base);
}

void __imp__NetDll_sendto(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NetDll_sendto(PPCContext& ctx, std::uint8_t* base) {
  __imp__NetDll_sendto(ctx, base);
}

void __imp__NetDll_setsockopt(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NetDll_setsockopt(PPCContext& ctx, std::uint8_t* base) {
  __imp__NetDll_setsockopt(ctx, base);
}

void __imp__NetDll_socket(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NetDll_socket(PPCContext& ctx, std::uint8_t* base) {
  __imp__NetDll_socket(ctx, base);
}

void __imp__NtAllocateVirtualMemory(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtAllocateVirtualMemory(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtAllocateVirtualMemory(ctx, base);
}

void __imp__NtCancelTimer(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtCancelTimer(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtCancelTimer(ctx, base);
}

void __imp__NtClearEvent(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtClearEvent(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtClearEvent(ctx, base);
}

void __imp__NtClose(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtClose(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtClose(ctx, base);
}

void __imp__NtCreateEvent(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtCreateEvent(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtCreateEvent(ctx, base);
}

void __imp__NtCreateFile(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtCreateFile(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtCreateFile(ctx, base);
}

void __imp__NtCreateMutant(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtCreateMutant(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtCreateMutant(ctx, base);
}

void __imp__NtCreateSemaphore(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtCreateSemaphore(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtCreateSemaphore(ctx, base);
}

void __imp__NtCreateTimer(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtCreateTimer(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtCreateTimer(ctx, base);
}

void __imp__NtDuplicateObject(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtDuplicateObject(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtDuplicateObject(ctx, base);
}

void __imp__NtFlushBuffersFile(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtFlushBuffersFile(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtFlushBuffersFile(ctx, base);
}

void __imp__NtFreeVirtualMemory(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtFreeVirtualMemory(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtFreeVirtualMemory(ctx, base);
}

void __imp__NtOpenFile(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtOpenFile(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtOpenFile(ctx, base);
}

void __imp__NtQueryDirectoryFile(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtQueryDirectoryFile(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtQueryDirectoryFile(ctx, base);
}

void __imp__NtQueryFullAttributesFile(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtQueryFullAttributesFile(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtQueryFullAttributesFile(ctx, base);
}

void __imp__NtQueryInformationFile(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtQueryInformationFile(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtQueryInformationFile(ctx, base);
}

void __imp__NtQueryVirtualMemory(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtQueryVirtualMemory(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtQueryVirtualMemory(ctx, base);
}

void __imp__NtQueryVolumeInformationFile(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtQueryVolumeInformationFile(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtQueryVolumeInformationFile(ctx, base);
}

void __imp__NtQueueApcThread(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtQueueApcThread(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtQueueApcThread(ctx, base);
}

void __imp__NtReadFile(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtReadFile(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtReadFile(ctx, base);
}

void __imp__NtReadFileScatter(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtReadFileScatter(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtReadFileScatter(ctx, base);
}

void __imp__NtReleaseMutant(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtReleaseMutant(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtReleaseMutant(ctx, base);
}

void __imp__NtReleaseSemaphore(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtReleaseSemaphore(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtReleaseSemaphore(ctx, base);
}

void __imp__NtResumeThread(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtResumeThread(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtResumeThread(ctx, base);
}

void __imp__NtSetEvent(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtSetEvent(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtSetEvent(ctx, base);
}

void __imp__NtSetInformationFile(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtSetInformationFile(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtSetInformationFile(ctx, base);
}

void __imp__NtSetTimerEx(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtSetTimerEx(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtSetTimerEx(ctx, base);
}

void __imp__NtSuspendThread(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtSuspendThread(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtSuspendThread(ctx, base);
}

void __imp__NtWaitForSingleObjectEx(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtWaitForSingleObjectEx(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtWaitForSingleObjectEx(ctx, base);
}

void __imp__NtWriteFile(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtWriteFile(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtWriteFile(ctx, base);
}

void __imp__NtWriteFileGather(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _NtWriteFileGather(PPCContext& ctx, std::uint8_t* base) {
  __imp__NtWriteFileGather(ctx, base);
}

void __imp__ObCreateSymbolicLink(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _ObCreateSymbolicLink(PPCContext& ctx, std::uint8_t* base) {
  __imp__ObCreateSymbolicLink(ctx, base);
}

void __imp__ObDeleteSymbolicLink(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _ObDeleteSymbolicLink(PPCContext& ctx, std::uint8_t* base) {
  __imp__ObDeleteSymbolicLink(ctx, base);
}

void __imp__ObDereferenceObject(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _ObDereferenceObject(PPCContext& ctx, std::uint8_t* base) {
  __imp__ObDereferenceObject(ctx, base);
}

void __imp__ObIsTitleObject(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _ObIsTitleObject(PPCContext& ctx, std::uint8_t* base) {
  __imp__ObIsTitleObject(ctx, base);
}

void __imp__ObReferenceObject(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _ObReferenceObject(PPCContext& ctx, std::uint8_t* base) {
  __imp__ObReferenceObject(ctx, base);
}

void __imp__ObReferenceObjectByHandle(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _ObReferenceObjectByHandle(PPCContext& ctx, std::uint8_t* base) {
  __imp__ObReferenceObjectByHandle(ctx, base);
}

void __imp__RtlCaptureContext(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _RtlCaptureContext(PPCContext& ctx, std::uint8_t* base) {
  __imp__RtlCaptureContext(ctx, base);
}

void __imp__RtlCompareMemoryUlong(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _RtlCompareMemoryUlong(PPCContext& ctx, std::uint8_t* base) {
  __imp__RtlCompareMemoryUlong(ctx, base);
}

void __imp__RtlCompareStringN(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _RtlCompareStringN(PPCContext& ctx, std::uint8_t* base) {
  __imp__RtlCompareStringN(ctx, base);
}

void __imp__RtlEnterCriticalSection(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _RtlEnterCriticalSection(PPCContext& ctx, std::uint8_t* base) {
  __imp__RtlEnterCriticalSection(ctx, base);
}

void __imp__RtlFillMemoryUlong(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _RtlFillMemoryUlong(PPCContext& ctx, std::uint8_t* base) {
  __imp__RtlFillMemoryUlong(ctx, base);
}

void __imp__RtlFreeAnsiString(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _RtlFreeAnsiString(PPCContext& ctx, std::uint8_t* base) {
  __imp__RtlFreeAnsiString(ctx, base);
}

void __imp__RtlImageXexHeaderField(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _RtlImageXexHeaderField(PPCContext& ctx, std::uint8_t* base) {
  __imp__RtlImageXexHeaderField(ctx, base);
}

void __imp__RtlInitAnsiString(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _RtlInitAnsiString(PPCContext& ctx, std::uint8_t* base) {
  __imp__RtlInitAnsiString(ctx, base);
}

void __imp__RtlInitUnicodeString(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _RtlInitUnicodeString(PPCContext& ctx, std::uint8_t* base) {
  __imp__RtlInitUnicodeString(ctx, base);
}

void __imp__RtlInitializeCriticalSection(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _RtlInitializeCriticalSection(PPCContext& ctx, std::uint8_t* base) {
  __imp__RtlInitializeCriticalSection(ctx, base);
}

void __imp__RtlInitializeCriticalSectionAndSpinCount(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _RtlInitializeCriticalSectionAndSpinCount(PPCContext& ctx, std::uint8_t* base) {
  __imp__RtlInitializeCriticalSectionAndSpinCount(ctx, base);
}

void __imp__RtlLeaveCriticalSection(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _RtlLeaveCriticalSection(PPCContext& ctx, std::uint8_t* base) {
  __imp__RtlLeaveCriticalSection(ctx, base);
}

void __imp__RtlMultiByteToUnicodeN(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _RtlMultiByteToUnicodeN(PPCContext& ctx, std::uint8_t* base) {
  __imp__RtlMultiByteToUnicodeN(ctx, base);
}

void __imp__RtlNtStatusToDosError(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _RtlNtStatusToDosError(PPCContext& ctx, std::uint8_t* base) {
  __imp__RtlNtStatusToDosError(ctx, base);
}

void __imp__RtlRaiseException(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _RtlRaiseException(PPCContext& ctx, std::uint8_t* base) {
  __imp__RtlRaiseException(ctx, base);
}

void __imp__RtlTimeFieldsToTime(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _RtlTimeFieldsToTime(PPCContext& ctx, std::uint8_t* base) {
  __imp__RtlTimeFieldsToTime(ctx, base);
}

void __imp__RtlTimeToTimeFields(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _RtlTimeToTimeFields(PPCContext& ctx, std::uint8_t* base) {
  __imp__RtlTimeToTimeFields(ctx, base);
}

void __imp__RtlTryEnterCriticalSection(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _RtlTryEnterCriticalSection(PPCContext& ctx, std::uint8_t* base) {
  __imp__RtlTryEnterCriticalSection(ctx, base);
}

void __imp__RtlUnicodeStringToAnsiString(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _RtlUnicodeStringToAnsiString(PPCContext& ctx, std::uint8_t* base) {
  __imp__RtlUnicodeStringToAnsiString(ctx, base);
}

void __imp__RtlUnicodeToMultiByteN(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _RtlUnicodeToMultiByteN(PPCContext& ctx, std::uint8_t* base) {
  __imp__RtlUnicodeToMultiByteN(ctx, base);
}

void __imp__RtlUnwind(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _RtlUnwind(PPCContext& ctx, std::uint8_t* base) {
  __imp__RtlUnwind(ctx, base);
}

void __imp__RtlUpcaseUnicodeChar(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _RtlUpcaseUnicodeChar(PPCContext& ctx, std::uint8_t* base) {
  __imp__RtlUpcaseUnicodeChar(ctx, base);
}

void __imp__StfsControlDevice(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _StfsControlDevice(PPCContext& ctx, std::uint8_t* base) {
  __imp__StfsControlDevice(ctx, base);
}

void __imp__StfsCreateDevice(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _StfsCreateDevice(PPCContext& ctx, std::uint8_t* base) {
  __imp__StfsCreateDevice(ctx, base);
}

void __imp__VdCallGraphicsNotificationRoutines(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _VdCallGraphicsNotificationRoutines(PPCContext& ctx, std::uint8_t* base) {
  __imp__VdCallGraphicsNotificationRoutines(ctx, base);
}

void __imp__VdEnableDisableClockGating(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _VdEnableDisableClockGating(PPCContext& ctx, std::uint8_t* base) {
  __imp__VdEnableDisableClockGating(ctx, base);
}

void __imp__VdEnableRingBufferRPtrWriteBack(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _VdEnableRingBufferRPtrWriteBack(PPCContext& ctx, std::uint8_t* base) {
  __imp__VdEnableRingBufferRPtrWriteBack(ctx, base);
}

void __imp__VdGetCurrentDisplayGamma(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _VdGetCurrentDisplayGamma(PPCContext& ctx, std::uint8_t* base) {
  __imp__VdGetCurrentDisplayGamma(ctx, base);
}

void __imp__VdGetCurrentDisplayInformation(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _VdGetCurrentDisplayInformation(PPCContext& ctx, std::uint8_t* base) {
  __imp__VdGetCurrentDisplayInformation(ctx, base);
}

void __imp__VdGetSystemCommandBuffer(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _VdGetSystemCommandBuffer(PPCContext& ctx, std::uint8_t* base) {
  __imp__VdGetSystemCommandBuffer(ctx, base);
}

void __imp__VdInitializeEngines(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _VdInitializeEngines(PPCContext& ctx, std::uint8_t* base) {
  __imp__VdInitializeEngines(ctx, base);
}

void __imp__VdInitializeRingBuffer(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _VdInitializeRingBuffer(PPCContext& ctx, std::uint8_t* base) {
  __imp__VdInitializeRingBuffer(ctx, base);
}

void __imp__VdInitializeScalerCommandBuffer(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _VdInitializeScalerCommandBuffer(PPCContext& ctx, std::uint8_t* base) {
  __imp__VdInitializeScalerCommandBuffer(ctx, base);
}

void __imp__VdIsHSIOTrainingSucceeded(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _VdIsHSIOTrainingSucceeded(PPCContext& ctx, std::uint8_t* base) {
  __imp__VdIsHSIOTrainingSucceeded(ctx, base);
}

void __imp__VdPersistDisplay(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _VdPersistDisplay(PPCContext& ctx, std::uint8_t* base) {
  __imp__VdPersistDisplay(ctx, base);
}

void __imp__VdQueryVideoFlags(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _VdQueryVideoFlags(PPCContext& ctx, std::uint8_t* base) {
  __imp__VdQueryVideoFlags(ctx, base);
}

void __imp__VdQueryVideoMode(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _VdQueryVideoMode(PPCContext& ctx, std::uint8_t* base) {
  __imp__VdQueryVideoMode(ctx, base);
}

void __imp__VdRetrainEDRAM(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _VdRetrainEDRAM(PPCContext& ctx, std::uint8_t* base) {
  __imp__VdRetrainEDRAM(ctx, base);
}

void __imp__VdRetrainEDRAMWorker(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _VdRetrainEDRAMWorker(PPCContext& ctx, std::uint8_t* base) {
  __imp__VdRetrainEDRAMWorker(ctx, base);
}

void __imp__VdSetDisplayMode(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _VdSetDisplayMode(PPCContext& ctx, std::uint8_t* base) {
  __imp__VdSetDisplayMode(ctx, base);
}

void __imp__VdSetDisplayModeOverride(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _VdSetDisplayModeOverride(PPCContext& ctx, std::uint8_t* base) {
  __imp__VdSetDisplayModeOverride(ctx, base);
}

void __imp__VdSetGraphicsInterruptCallback(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _VdSetGraphicsInterruptCallback(PPCContext& ctx, std::uint8_t* base) {
  __imp__VdSetGraphicsInterruptCallback(ctx, base);
}

void __imp__VdSetSystemCommandBufferGpuIdentifierAddress(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _VdSetSystemCommandBufferGpuIdentifierAddress(PPCContext& ctx, std::uint8_t* base) {
  __imp__VdSetSystemCommandBufferGpuIdentifierAddress(ctx, base);
}

void __imp__VdShutdownEngines(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _VdShutdownEngines(PPCContext& ctx, std::uint8_t* base) {
  __imp__VdShutdownEngines(ctx, base);
}

void __imp__VdSwap(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _VdSwap(PPCContext& ctx, std::uint8_t* base) {
  __imp__VdSwap(ctx, base);
}

void __imp__XAudioGetDuckerLevel(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XAudioGetDuckerLevel(PPCContext& ctx, std::uint8_t* base) {
  __imp__XAudioGetDuckerLevel(ctx, base);
}

void __imp__XAudioGetSpeakerConfig(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XAudioGetSpeakerConfig(PPCContext& ctx, std::uint8_t* base) {
  __imp__XAudioGetSpeakerConfig(ctx, base);
}

void __imp__XAudioGetVoiceCategoryVolume(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XAudioGetVoiceCategoryVolume(PPCContext& ctx, std::uint8_t* base) {
  __imp__XAudioGetVoiceCategoryVolume(ctx, base);
}

void __imp__XAudioRegisterRenderDriverClient(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XAudioRegisterRenderDriverClient(PPCContext& ctx, std::uint8_t* base) {
  __imp__XAudioRegisterRenderDriverClient(ctx, base);
}

void __imp__XAudioSubmitRenderDriverFrame(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XAudioSubmitRenderDriverFrame(PPCContext& ctx, std::uint8_t* base) {
  __imp__XAudioSubmitRenderDriverFrame(ctx, base);
}

void __imp__XAudioUnregisterRenderDriverClient(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XAudioUnregisterRenderDriverClient(PPCContext& ctx, std::uint8_t* base) {
  __imp__XAudioUnregisterRenderDriverClient(ctx, base);
}

void __imp__XGetAVPack(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XGetAVPack(PPCContext& ctx, std::uint8_t* base) {
  __imp__XGetAVPack(ctx, base);
}

void __imp__XGetGameRegion(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XGetGameRegion(PPCContext& ctx, std::uint8_t* base) {
  __imp__XGetGameRegion(ctx, base);
}

void __imp__XGetLanguage(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XGetLanguage(PPCContext& ctx, std::uint8_t* base) {
  __imp__XGetLanguage(ctx, base);
}

void __imp__XGetVideoMode(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XGetVideoMode(PPCContext& ctx, std::uint8_t* base) {
  __imp__XGetVideoMode(ctx, base);
}

void __imp__XMACreateContext(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XMACreateContext(PPCContext& ctx, std::uint8_t* base) {
  __imp__XMACreateContext(ctx, base);
}

void __imp__XMAReleaseContext(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XMAReleaseContext(PPCContext& ctx, std::uint8_t* base) {
  __imp__XMAReleaseContext(ctx, base);
}

void __imp__XMsgCancelIORequest(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XMsgCancelIORequest(PPCContext& ctx, std::uint8_t* base) {
  __imp__XMsgCancelIORequest(ctx, base);
}

void __imp__XMsgInProcessCall(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XMsgInProcessCall(PPCContext& ctx, std::uint8_t* base) {
  __imp__XMsgInProcessCall(ctx, base);
}

void __imp__XMsgStartIORequest(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XMsgStartIORequest(PPCContext& ctx, std::uint8_t* base) {
  __imp__XMsgStartIORequest(ctx, base);
}

void __imp__XNetLogonGetTitleID(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XNetLogonGetTitleID(PPCContext& ctx, std::uint8_t* base) {
  __imp__XNetLogonGetTitleID(ctx, base);
}

void __imp__XNotifyGetNext(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XNotifyGetNext(PPCContext& ctx, std::uint8_t* base) {
  __imp__XNotifyGetNext(ctx, base);
}

void __imp__XNotifyPositionUI(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XNotifyPositionUI(PPCContext& ctx, std::uint8_t* base) {
  __imp__XNotifyPositionUI(ctx, base);
}

void __imp__XamAlloc(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamAlloc(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamAlloc(ctx, base);
}

void __imp__XamContentClose(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamContentClose(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamContentClose(ctx, base);
}

void __imp__XamContentCreateEnumerator(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamContentCreateEnumerator(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamContentCreateEnumerator(ctx, base);
}

void __imp__XamContentCreateEx(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamContentCreateEx(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamContentCreateEx(ctx, base);
}

void __imp__XamContentGetCreator(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamContentGetCreator(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamContentGetCreator(ctx, base);
}

void __imp__XamContentGetDeviceData(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamContentGetDeviceData(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamContentGetDeviceData(ctx, base);
}

void __imp__XamContentGetDeviceState(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamContentGetDeviceState(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamContentGetDeviceState(ctx, base);
}

void __imp__XamContentGetLicenseMask(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamContentGetLicenseMask(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamContentGetLicenseMask(ctx, base);
}

void __imp__XamContentSetThumbnail(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamContentSetThumbnail(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamContentSetThumbnail(ctx, base);
}

void __imp__XamEnumerate(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamEnumerate(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamEnumerate(ctx, base);
}

void __imp__XamFree(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamFree(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamFree(ctx, base);
}

void __imp__XamGetExecutionId(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamGetExecutionId(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamGetExecutionId(ctx, base);
}

void __imp__XamGetSystemVersion(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamGetSystemVersion(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamGetSystemVersion(ctx, base);
}

void __imp__XamInputGetCapabilities(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamInputGetCapabilities(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamInputGetCapabilities(ctx, base);
}

void __imp__XamInputGetCapabilitiesEx(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamInputGetCapabilitiesEx(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamInputGetCapabilitiesEx(ctx, base);
}

void __imp__XamInputGetState(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamInputGetState(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamInputGetState(ctx, base);
}

void __imp__XamInputRawState(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamInputRawState(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamInputRawState(ctx, base);
}

void __imp__XamInputSetState(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamInputSetState(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamInputSetState(ctx, base);
}

void __imp__XamLoaderLaunchTitle(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamLoaderLaunchTitle(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamLoaderLaunchTitle(ctx, base);
}

void __imp__XamLoaderTerminateTitle(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamLoaderTerminateTitle(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamLoaderTerminateTitle(ctx, base);
}

void __imp__XamNotifyCreateListener(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamNotifyCreateListener(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamNotifyCreateListener(ctx, base);
}

void __imp__XamSessionCreateHandle(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamSessionCreateHandle(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamSessionCreateHandle(ctx, base);
}

void __imp__XamSessionRefObjByHandle(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamSessionRefObjByHandle(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamSessionRefObjByHandle(ctx, base);
}

void __imp__XamShowAchievementsUI(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamShowAchievementsUI(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamShowAchievementsUI(ctx, base);
}

void __imp__XamShowDeviceSelectorUI(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamShowDeviceSelectorUI(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamShowDeviceSelectorUI(ctx, base);
}

void __imp__XamShowDirtyDiscErrorUI(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamShowDirtyDiscErrorUI(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamShowDirtyDiscErrorUI(ctx, base);
}

void __imp__XamShowFriendsUI(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamShowFriendsUI(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamShowFriendsUI(ctx, base);
}

void __imp__XamShowGameInviteUI(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamShowGameInviteUI(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamShowGameInviteUI(ctx, base);
}

void __imp__XamShowGamerCardUIForXUID(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamShowGamerCardUIForXUID(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamShowGamerCardUIForXUID(ctx, base);
}

void __imp__XamShowKeyboardUI(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamShowKeyboardUI(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamShowKeyboardUI(ctx, base);
}

void __imp__XamShowMarketplaceUI(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamShowMarketplaceUI(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamShowMarketplaceUI(ctx, base);
}

void __imp__XamShowMessageBoxUI(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamShowMessageBoxUI(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamShowMessageBoxUI(ctx, base);
}

void __imp__XamShowMessageBoxUIEx(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamShowMessageBoxUIEx(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamShowMessageBoxUIEx(ctx, base);
}

void __imp__XamShowPlayerReviewUI(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamShowPlayerReviewUI(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamShowPlayerReviewUI(ctx, base);
}

void __imp__XamShowQuickChatUI(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamShowQuickChatUI(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamShowQuickChatUI(ctx, base);
}

void __imp__XamShowSigninUI(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamShowSigninUI(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamShowSigninUI(ctx, base);
}

void __imp__XamUserAreUsersFriends(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamUserAreUsersFriends(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamUserAreUsersFriends(ctx, base);
}

void __imp__XamUserCheckPrivilege(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamUserCheckPrivilege(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamUserCheckPrivilege(ctx, base);
}

void __imp__XamUserCreateStatsEnumerator(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamUserCreateStatsEnumerator(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamUserCreateStatsEnumerator(ctx, base);
}

void __imp__XamUserGetDeviceContext(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamUserGetDeviceContext(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamUserGetDeviceContext(ctx, base);
}

void __imp__XamUserGetName(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamUserGetName(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamUserGetName(ctx, base);
}

void __imp__XamUserGetSigninInfo(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamUserGetSigninInfo(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamUserGetSigninInfo(ctx, base);
}

void __imp__XamUserGetSigninState(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamUserGetSigninState(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamUserGetSigninState(ctx, base);
}

void __imp__XamUserGetXUID(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamUserGetXUID(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamUserGetXUID(ctx, base);
}

void __imp__XamUserReadProfileSettings(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamUserReadProfileSettings(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamUserReadProfileSettings(ctx, base);
}

void __imp__XamVoiceClose(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamVoiceClose(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamVoiceClose(ctx, base);
}

void __imp__XamVoiceCreate(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamVoiceCreate(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamVoiceCreate(ctx, base);
}

void __imp__XamVoiceHeadsetPresent(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamVoiceHeadsetPresent(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamVoiceHeadsetPresent(ctx, base);
}

void __imp__XamVoiceIsActiveProcess(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamVoiceIsActiveProcess(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamVoiceIsActiveProcess(ctx, base);
}

void __imp__XamVoiceSubmitPacket(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamVoiceSubmitPacket(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamVoiceSubmitPacket(ctx, base);
}

void __imp__XamWriteGamerTile(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XamWriteGamerTile(PPCContext& ctx, std::uint8_t* base) {
  __imp__XamWriteGamerTile(ctx, base);
}

void __imp__XeCryptSha(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XeCryptSha(PPCContext& ctx, std::uint8_t* base) {
  __imp__XeCryptSha(ctx, base);
}

void __imp__XexCheckExecutablePrivilege(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XexCheckExecutablePrivilege(PPCContext& ctx, std::uint8_t* base) {
  __imp__XexCheckExecutablePrivilege(ctx, base);
}

void __imp__XexGetModuleHandle(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XexGetModuleHandle(PPCContext& ctx, std::uint8_t* base) {
  __imp__XexGetModuleHandle(ctx, base);
}

void __imp__XexGetModuleSection(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XexGetModuleSection(PPCContext& ctx, std::uint8_t* base) {
  __imp__XexGetModuleSection(ctx, base);
}

void __imp__XexGetProcedureAddress(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XexGetProcedureAddress(PPCContext& ctx, std::uint8_t* base) {
  __imp__XexGetProcedureAddress(ctx, base);
}

void __imp__XexLoadImage(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XexLoadImage(PPCContext& ctx, std::uint8_t* base) {
  __imp__XexLoadImage(ctx, base);
}

void __imp__XexUnloadImage(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _XexUnloadImage(PPCContext& ctx, std::uint8_t* base) {
  __imp__XexUnloadImage(ctx, base);
}

void __imp____C_specific_handler(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void ___C_specific_handler(PPCContext& ctx, std::uint8_t* base) {
  __imp____C_specific_handler(ctx, base);
}

}
#endif


#ifdef _WIN32
extern "C" {

void __imp__DbgPrint(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _DbgPrint(PPCContext& ctx, std::uint8_t* base) {
  __imp__DbgPrint(ctx, base);
}

void __imp__sprintf(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void _sprintf(PPCContext& ctx, std::uint8_t* base) {
  __imp__sprintf(ctx, base);
}

void __imp___snprintf(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void __snprintf(PPCContext& ctx, std::uint8_t* base) {
  __imp___snprintf(ctx, base);
}

void __imp___vsnprintf(PPCContext& ctx, std::uint8_t* base);
__declspec(dllexport) void __vsnprintf(PPCContext& ctx, std::uint8_t* base) {
  __imp___vsnprintf(ctx, base);
}

}
#endif
