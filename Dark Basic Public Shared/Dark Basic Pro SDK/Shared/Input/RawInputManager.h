#pragma once
#include <windows.h>
#include <vector>
#include <cstdint>
#include <cstring>

class RawInputManager {
public:
    static RawInputManager& GetInstance() {
        static RawInputManager instance;
        return instance;
    }

    void Initialize(HWND hWnd) {
        m_hWnd = hWnd;
        RAWINPUTDEVICE rid[2];

        // Mouse
        rid[0].usUsagePage = 0x01;
        rid[0].usUsage = 0x02;
        rid[0].dwFlags = 0;
        rid[0].hwndTarget = hWnd;

        // Keyboard
        rid[1].usUsagePage = 0x01;
        rid[1].usUsage = 0x06;
        rid[1].dwFlags = 0;
        rid[1].hwndTarget = hWnd;

        RegisterRawInputDevices(rid, 2, sizeof(rid[0]));
        Clear();
    }

    void ProcessInput(HRAWINPUT hRawInput) {
        UINT dwSize = 0;
        GetRawInputData(hRawInput, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
        if (dwSize == 0) return;

        std::vector<BYTE> lpb(dwSize);
        if (GetRawInputData(hRawInput, RID_INPUT, lpb.data(), &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) {
            return;
        }

        RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(lpb.data());
        if (raw->header.dwType == RIM_TYPEKEYBOARD) {
            SimulateKeyboardInput(
                raw->data.keyboard.VKey,
                raw->data.keyboard.MakeCode,
                (raw->data.keyboard.Flags & RI_KEY_E0) != 0,
                (raw->data.keyboard.Flags & RI_KEY_BREAK) != 0
            );
        }
        else if (raw->header.dwType == RIM_TYPEMOUSE) {
            SimulateMouseInput(
                raw->data.mouse.lLastX,
                raw->data.mouse.lLastY,
                raw->data.mouse.usButtonFlags,
                (raw->data.mouse.usButtonFlags & RI_MOUSE_WHEEL) ? static_cast<short>(raw->data.mouse.usButtonData) : 0
            );
        }
    }

    void SimulateKeyboardInput(USHORT vkey, USHORT makeCode, bool isE0, bool isUp) {
        BYTE scanCode = MapVKeyToDIK(vkey, makeCode, isE0);
        if (scanCode < 256) {
            m_KeyBuffer[scanCode] = isUp ? 0x00 : 0x80;
        }
    }

    void SimulateMouseInput(int lastX, int lastY, USHORT buttonFlags, short wheelDelta) {
        m_MouseDeltaX += lastX;
        m_MouseDeltaY += lastY;
        m_MouseDeltaZ += wheelDelta;

        if (buttonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)   m_MouseButtons[0] = 0x80;
        if (buttonFlags & RI_MOUSE_LEFT_BUTTON_UP)     m_MouseButtons[0] = 0x00;
        if (buttonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)  m_MouseButtons[1] = 0x80;
        if (buttonFlags & RI_MOUSE_RIGHT_BUTTON_UP)    m_MouseButtons[1] = 0x00;
        if (buttonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN) m_MouseButtons[2] = 0x80;
        if (buttonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)   m_MouseButtons[2] = 0x00;
    }

    void Clear() {
        memset(m_KeyBuffer, 0, sizeof(m_KeyBuffer));
        memset(m_MouseButtons, 0, sizeof(m_MouseButtons));
        m_MouseDeltaX = 0;
        m_MouseDeltaY = 0;
        m_MouseDeltaZ = 0;
    }

    const BYTE* GetKeyBuffer() const { return m_KeyBuffer; }
    
    void GetMouseState(int& dx, int& dy, int& dz, BYTE buttons[4]) const {
        dx = m_MouseDeltaX;
        dy = m_MouseDeltaY;
        dz = m_MouseDeltaZ;
        memcpy(buttons, m_MouseButtons, 4);
    }

    void ResetMouseDeltas() {
        m_MouseDeltaX = 0;
        m_MouseDeltaY = 0;
        m_MouseDeltaZ = 0;
    }

private:
    RawInputManager() : m_hWnd(nullptr), m_MouseDeltaX(0), m_MouseDeltaY(0), m_MouseDeltaZ(0) {
        Clear();
    }

    BYTE MapVKeyToDIK(USHORT vkey, USHORT makeCode, bool isE0) {
        BYTE dik = static_cast<BYTE>(makeCode);
        if (isE0) {
            dik |= 0x80;
        }

        switch (vkey) {
            case VK_LEFT:   return 0xCB;
            case VK_RIGHT:  return 0xCD;
            case VK_UP:     return 0xC8;
            case VK_DOWN:   return 0xD0;
            case VK_ESCAPE: return 0x01;
            case VK_RETURN: return isE0 ? 0x9C : 0x1C;
            case VK_CONTROL: return isE0 ? 0x9D : 0x1D;
            case VK_MENU:    return isE0 ? 0xB8 : 0x38;
            case VK_SHIFT:
                if (makeCode == 0x36) return 0x36;
                return 0x2A;
        }
        return dik;
    }

    HWND m_hWnd;
    BYTE m_KeyBuffer[256];
    BYTE m_MouseButtons[4];
    int m_MouseDeltaX;
    int m_MouseDeltaY;
    int m_MouseDeltaZ;
};
