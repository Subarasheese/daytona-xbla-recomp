#ifdef _WIN32
#include <windows.h>
#include <timeapi.h>  // timeBeginPeriod / timeEndPeriod (winmm)

extern "C" int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPWSTR lpCmdLine,
    int nShowCmd
);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR, int nShowCmd) {
    // Raise the process timer resolution to 1ms for the whole run.
    //
    // The Xbox-360 scheduler shim implements guest thread delays (KeDelay
    // ExecutionThread / Sleep / wait-with-timeout) on top of Win32 ::Sleep(ms)
    // (see core/threading_win.cpp). With the default Windows timer quantum
    // (~15.6ms), every such sub-quantum sleep on a per-frame guest thread rounds
    // *up* to ~15.6ms, capping the frame loop at ~18-30 fps. The Linux build is
    // immune because its Sleep() is nanosleep() (true sub-ms granularity), which
    // is why the same scene runs at a flat 120 fps there. The SDK also disables
    // SDL's own 1ms raise (SDL_HINT_TIMER_RESOLUTION="0"), so nothing else
    // restores high resolution -- we own it explicitly here.
    //
    // timeBeginPeriod takes a process-wide, reference-counted hold, so this is
    // authoritative regardless of SDL, and it's the documented mechanism games/
    // media apps use (not a hack). Windows-only: the Linux build never compiles
    // this translation unit, so it is wholly unaffected.
    const bool timer_raised = (timeBeginPeriod(1) == TIMERR_NOERROR);

    const int rc = wWinMain(hInstance, hPrevInstance, GetCommandLineW(), nShowCmd);

    if (timer_raised) {
        timeEndPeriod(1);
    }
    return rc;
}
#endif
