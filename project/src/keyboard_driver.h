// Crazy Taxi -- Keyboard-to-gamepad input driver
// Maps keyboard keys to Xbox 360 controller inputs
//
// Default key mapping (racing/arcade layout):
//   WASD / Arrow keys  = D-pad (menu navigation)
//   W / Up             = D-pad Up (also accelerate in some modes)
//   Space / Z          = A (accelerate / confirm)
//   Shift / X          = B (brake / reverse / cancel)
//   C                  = X (Crazy Drift / gear shift)
//   V                  = Y
//   Enter              = Start
//   Backspace / Esc    = Back
//   Q / E              = LB / RB (camera)
//   1 / 3              = LT / RT (analog triggers)

#pragma once

#include <rex/input/input_driver.h>
#include <rex/ui/window_listener.h>

#include <atomic>
#include <cstdint>

using rex::X_STATUS;
using rex::X_RESULT;
using rex::input::X_INPUT_CAPABILITIES;
using rex::input::X_INPUT_STATE;
using rex::input::X_INPUT_VIBRATION;
using rex::input::X_INPUT_KEYSTROKE;

class KeyboardInputDriver final : public rex::input::InputDriver,
                                  public rex::ui::WindowInputListener {
public:
    explicit KeyboardInputDriver(rex::ui::Window* window);
    ~KeyboardInputDriver() override;

    X_STATUS Setup() override;

    X_RESULT GetCapabilities(uint32_t user_index, uint32_t flags,
                             X_INPUT_CAPABILITIES* out_caps) override;
    X_RESULT GetState(uint32_t user_index, X_INPUT_STATE* out_state) override;
    X_RESULT SetState(uint32_t user_index,
                      X_INPUT_VIBRATION* vibration) override;
    X_RESULT GetKeystroke(uint32_t user_index, uint32_t flags,
                          X_INPUT_KEYSTROKE* out_keystroke) override;

    // WindowInputListener overrides
    void OnKeyDown(rex::ui::KeyEvent& e) override;
    void OnKeyUp(rex::ui::KeyEvent& e) override;

private:
    uint16_t MapKeyToButton(rex::ui::VirtualKey key);

    std::atomic<uint16_t> buttons_{0};
    std::atomic<uint8_t> left_trigger_{0};
    std::atomic<uint8_t> right_trigger_{0};
    uint32_t packet_number_ = 0;
    uint16_t prev_buttons_ = 0;
};
