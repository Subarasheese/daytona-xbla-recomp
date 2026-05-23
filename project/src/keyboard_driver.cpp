// Daytona USA -- Keyboard input driver
// Uses direct Win32 GetAsyncKeyState polling for reliable key detection.

#include "keyboard_driver.h"

#include <rex/input/input.h>
#include <rex/ui/virtual_key.h>
#include <rex/ui/window.h>

#include <cstring>

#ifdef _WIN32
#include <windows.h>

// XInput loaded dynamically to avoid DLL conflicts with SDK's SDL
struct XINPUT_GAMEPAD_S {
    WORD wButtons;
    BYTE bLeftTrigger;
    BYTE bRightTrigger;
    SHORT sThumbLX;
    SHORT sThumbLY;
    SHORT sThumbRX;
    SHORT sThumbRY;
};
struct XINPUT_STATE_S {
    DWORD dwPacketNumber;
    XINPUT_GAMEPAD_S Gamepad;
};
typedef DWORD (WINAPI *XInputGetState_t)(DWORD, XINPUT_STATE_S*);

static XInputGetState_t GetXInputGetState() {
    static XInputGetState_t fn = nullptr;
    static bool tried = false;
    if (!tried) {
        tried = true;
        HMODULE h = LoadLibraryA("xinput1_4.dll");
        if (!h) h = LoadLibraryA("xinput1_3.dll");
        if (!h) h = LoadLibraryA("xinput9_1_0.dll");
        if (h) fn = (XInputGetState_t)GetProcAddress(h, "XInputGetState");
    }
    return fn;
}
#endif

using namespace rex::ui;
using namespace rex::input;

KeyboardInputDriver::KeyboardInputDriver(rex::ui::Window* window)
    : InputDriver(window, 0) {
}

KeyboardInputDriver::~KeyboardInputDriver() {
}

X_STATUS KeyboardInputDriver::Setup() {
    return X_STATUS_SUCCESS;
}

uint16_t KeyboardInputDriver::MapKeyToButton(VirtualKey key) { return 0; }
void KeyboardInputDriver::OnKeyDown(KeyEvent& e) {}
void KeyboardInputDriver::OnKeyUp(KeyEvent& e) {}

X_RESULT KeyboardInputDriver::GetCapabilities(uint32_t user_index,
                                               uint32_t flags,
                                               X_INPUT_CAPABILITIES* out_caps) {
    if (user_index != 0) return X_ERROR_DEVICE_NOT_CONNECTED;
    if (out_caps) {
        std::memset(out_caps, 0, sizeof(*out_caps));
        out_caps->type = 0x01;
        out_caps->sub_type = 0x01;
        out_caps->gamepad.buttons = 0xFFFF;
        out_caps->gamepad.left_trigger = 0xFF;
        out_caps->gamepad.right_trigger = 0xFF;
        out_caps->gamepad.thumb_lx = static_cast<int16_t>(0x7FFF);
        out_caps->gamepad.thumb_ly = static_cast<int16_t>(0x7FFF);
        out_caps->gamepad.thumb_rx = static_cast<int16_t>(0x7FFF);
        out_caps->gamepad.thumb_ry = static_cast<int16_t>(0x7FFF);
    }
    return X_ERROR_SUCCESS;
}

#ifdef _WIN32
// Keyboard layout for Crazy Taxi:
//   WASD / Arrows = D-pad (menu nav + gear selection)
//   Space         = Gas (RT trigger) + A button (for menu confirm)
//   Shift         = Brake (LT trigger)
//   Z             = A button (confirm / Crazy Boost)
//   X             = B button (cancel)
//   C             = X button (Crazy Drift)
//   V             = Y button
//   Enter         = Start
//   Esc / Bksp    = Back
//   Q / E         = LB / RB
static void PollKeyboard(uint16_t& buttons, uint8_t& lt, uint8_t& rt) {
    buttons = 0;
    lt = 0;
    rt = 0;

    // D-pad: WASD + Arrow keys
    if (GetAsyncKeyState('W') & 0x8000 || GetAsyncKeyState(VK_UP) & 0x8000)
        buttons |= X_INPUT_GAMEPAD_DPAD_UP;
    if (GetAsyncKeyState('S') & 0x8000 || GetAsyncKeyState(VK_DOWN) & 0x8000)
        buttons |= X_INPUT_GAMEPAD_DPAD_DOWN;
    if (GetAsyncKeyState('A') & 0x8000 || GetAsyncKeyState(VK_LEFT) & 0x8000)
        buttons |= X_INPUT_GAMEPAD_DPAD_LEFT;
    if (GetAsyncKeyState('D') & 0x8000 || GetAsyncKeyState(VK_RIGHT) & 0x8000)
        buttons |= X_INPUT_GAMEPAD_DPAD_RIGHT;

    // Face buttons
    if (GetAsyncKeyState(VK_SPACE) & 0x8000 || GetAsyncKeyState('Z') & 0x8000)
        buttons |= X_INPUT_GAMEPAD_A;
    if (GetAsyncKeyState(VK_LSHIFT) & 0x8000 || GetAsyncKeyState('X') & 0x8000)
        buttons |= X_INPUT_GAMEPAD_B;
    if (GetAsyncKeyState('C') & 0x8000)
        buttons |= X_INPUT_GAMEPAD_X;
    if (GetAsyncKeyState('V') & 0x8000)
        buttons |= X_INPUT_GAMEPAD_Y;

    // System
    if (GetAsyncKeyState(VK_RETURN) & 0x8000)
        buttons |= X_INPUT_GAMEPAD_START;
    if (GetAsyncKeyState(VK_BACK) & 0x8000 || GetAsyncKeyState(VK_ESCAPE) & 0x8000)
        buttons |= X_INPUT_GAMEPAD_BACK;

    // Shoulders
    if (GetAsyncKeyState('Q') & 0x8000)
        buttons |= X_INPUT_GAMEPAD_LEFT_SHOULDER;
    if (GetAsyncKeyState('E') & 0x8000)
        buttons |= X_INPUT_GAMEPAD_RIGHT_SHOULDER;

    // Triggers: 1 = LT, 3 = RT (number keys)
    if (GetAsyncKeyState('1') & 0x8000)
        lt = 255;
    if (GetAsyncKeyState('3') & 0x8000)
        rt = 255;
}
#endif

X_RESULT KeyboardInputDriver::GetState(uint32_t user_index,
                                        X_INPUT_STATE* out_state) {
    if (user_index != 0) return X_ERROR_DEVICE_NOT_CONNECTED;
    if (out_state) {
        uint16_t current = 0;
        uint8_t lt = 0, rt = 0;
        int16_t lx = 0, ly = 0, rx = 0, ry = 0;

#ifdef _WIN32
        PollKeyboard(current, lt, rt);

        // Merge Xbox controller input via dynamically-loaded XInput
        if (auto xig = GetXInputGetState()) {
            XINPUT_STATE_S xi;
            if (xig(0, &xi) == ERROR_SUCCESS) {
                current |= xi.Gamepad.wButtons;
                if (xi.Gamepad.bLeftTrigger > lt) lt = xi.Gamepad.bLeftTrigger;
                if (xi.Gamepad.bRightTrigger > rt) rt = xi.Gamepad.bRightTrigger;
                auto dz = [](int16_t v, int16_t d) -> int16_t {
                    return (v > d || v < -d) ? v : 0;
                };
                lx = dz(xi.Gamepad.sThumbLX, 7849);
                ly = dz(xi.Gamepad.sThumbLY, 7849);
                rx = dz(xi.Gamepad.sThumbRX, 8689);
                ry = dz(xi.Gamepad.sThumbRY, 8689);
            }
        }
#endif

        if (current != prev_buttons_) {
            packet_number_++;
            prev_buttons_ = current;
        }
        out_state->packet_number = packet_number_;
        out_state->gamepad.buttons = current;
        out_state->gamepad.left_trigger = lt;
        out_state->gamepad.right_trigger = rt;
        out_state->gamepad.thumb_lx = lx;
        out_state->gamepad.thumb_ly = ly;
        out_state->gamepad.thumb_rx = rx;
        out_state->gamepad.thumb_ry = ry;
    }
    return X_ERROR_SUCCESS;
}

X_RESULT KeyboardInputDriver::SetState(uint32_t user_index,
                                        X_INPUT_VIBRATION* vibration) {
    if (user_index != 0) return X_ERROR_DEVICE_NOT_CONNECTED;
    return X_ERROR_SUCCESS;
}

X_RESULT KeyboardInputDriver::GetKeystroke(uint32_t user_index, uint32_t flags,
                                            X_INPUT_KEYSTROKE* out_keystroke) {
    if (user_index != 0) return X_ERROR_DEVICE_NOT_CONNECTED;
    return X_ERROR_EMPTY;
}
