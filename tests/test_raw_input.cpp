#include <gtest/gtest.h>
#include "RawInputManager.h"
#include "CInputC.h"
#include <xinput.h>

// Mock functions to satisfy CInputC.cpp linker requirements in test build
void GlobExpandChecklist([[maybe_unused]] unsigned long index, [[maybe_unused]] unsigned long size) {}

TEST(RawInputTest, KeyboardInputAndMapping) {
    auto& manager = RawInputManager::GetInstance();
    manager.Clear();

    // 1. Test standard key press (VK_ESCAPE -> DIK_ESCAPE which is 0x01)
    manager.SimulateKeyboardInput(VK_ESCAPE, 0x01, false, false); // Down
    EXPECT_EQ(manager.GetKeyBuffer()[0x01], 0x80);

    // Test standard key release
    manager.SimulateKeyboardInput(VK_ESCAPE, 0x01, false, true); // Up
    EXPECT_EQ(manager.GetKeyBuffer()[0x01], 0x00);

    // 2. Test extended arrow keys (VK_UP -> DIK_UP which is 0xC8)
    manager.SimulateKeyboardInput(VK_UP, 0x48, true, false); // Down with E0 flag
    EXPECT_EQ(manager.GetKeyBuffer()[0xC8], 0x80);
    manager.SimulateKeyboardInput(VK_UP, 0x48, true, true); // Up
    EXPECT_EQ(manager.GetKeyBuffer()[0xC8], 0x00);

    // 3. Test left shift (VK_SHIFT with makeCode 0x2A -> DIK_LSHIFT)
    manager.SimulateKeyboardInput(VK_SHIFT, 0x2A, false, false); // Down
    EXPECT_EQ(manager.GetKeyBuffer()[0x2A], 0x80);

    // 4. Test right shift (VK_SHIFT with makeCode 0x36 -> DIK_RSHIFT)
    manager.SimulateKeyboardInput(VK_SHIFT, 0x36, false, false); // Down
    EXPECT_EQ(manager.GetKeyBuffer()[0x36], 0x80);
}

TEST(RawInputTest, MouseInputAccumulation) {
    auto& manager = RawInputManager::GetInstance();
    manager.Clear();

    int dx = 0, dy = 0, dz = 0;
    BYTE buttons[4] = {0};

    // Simulate mouse movements
    manager.SimulateMouseInput(10, -5, 0, 0);
    manager.GetMouseState(dx, dy, dz, buttons);
    EXPECT_EQ(dx, 10);
    EXPECT_EQ(dy, -5);
    EXPECT_EQ(dz, 0);

    // Accumulate movements
    manager.SimulateMouseInput(-3, 8, 0, 0);
    manager.GetMouseState(dx, dy, dz, buttons);
    EXPECT_EQ(dx, 7);
    EXPECT_EQ(dy, 3);

    // Test mouse button press (Left button)
    manager.SimulateMouseInput(0, 0, RI_MOUSE_LEFT_BUTTON_DOWN, 0);
    manager.GetMouseState(dx, dy, dz, buttons);
    EXPECT_EQ(buttons[0], 0x80);

    // Test mouse button release (Left button)
    manager.SimulateMouseInput(0, 0, RI_MOUSE_LEFT_BUTTON_UP, 0);
    manager.GetMouseState(dx, dy, dz, buttons);
    EXPECT_EQ(buttons[0], 0x00);

    // Reset deltas
    manager.ResetMouseDeltas();
    manager.GetMouseState(dx, dy, dz, buttons);
    EXPECT_EQ(dx, 0);
    EXPECT_EQ(dy, 0);
}

TEST(XInputTest, GamepadMapping) {
    XINPUT_STATE state;
    ZeroMemory(&state, sizeof(XINPUT_STATE));
    DIJOYSTATE2 joyState;
    ZeroMemory(&joyState, sizeof(DIJOYSTATE2));

    // 1. Test Thumbsticks
    state.Gamepad.sThumbLX = 15000;
    state.Gamepad.sThumbLY = -12000;
    state.Gamepad.sThumbRX = -18000;
    state.Gamepad.sThumbRY = 20000;

    MapXInputToDIJoyState(state, joyState);
    EXPECT_EQ(joyState.lX, 15000);
    EXPECT_EQ(joyState.lY, 12000); // Inverted Y
    EXPECT_EQ(joyState.lRx, -18000);
    EXPECT_EQ(joyState.lRy, -20000); // Inverted Y

    // 2. Test Triggers
    state.Gamepad.bLeftTrigger = 200;
    state.Gamepad.bRightTrigger = 50;

    MapXInputToDIJoyState(state, joyState);
    EXPECT_EQ(joyState.lZ, 19200); // (200 - 50) * 128

    // 3. Test Buttons
    state.Gamepad.wButtons = XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_RIGHT_SHOULDER | XINPUT_GAMEPAD_START;

    MapXInputToDIJoyState(state, joyState);
    EXPECT_EQ(joyState.rgbButtons[0], 0x80); // A
    EXPECT_EQ(joyState.rgbButtons[1], 0x00); // B (not pressed)
    EXPECT_EQ(joyState.rgbButtons[5], 0x80); // Right Shoulder
    EXPECT_EQ(joyState.rgbButtons[7], 0x80); // Start

    // 4. Test D-pad POV Mapping
    // UP only -> 0 degrees (0)
    state.Gamepad.wButtons = XINPUT_GAMEPAD_DPAD_UP;
    MapXInputToDIJoyState(state, joyState);
    EXPECT_EQ(joyState.rgdwPOV[0], 0);

    // DOWN + LEFT -> 225 degrees (22500)
    state.Gamepad.wButtons = XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT;
    MapXInputToDIJoyState(state, joyState);
    EXPECT_EQ(joyState.rgdwPOV[0], 22500);

    // Released -> -1
    state.Gamepad.wButtons = 0;
    MapXInputToDIJoyState(state, joyState);
    EXPECT_EQ(joyState.rgdwPOV[0], 0xFFFFFFFF);
}
