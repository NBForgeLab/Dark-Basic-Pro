#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include "DBPDebuggerProtocol.h"

namespace DBPDebugger {

class DebuggerApp {
public:
    DebuggerApp() = default;
    ~DebuggerApp();

    bool Initialize(HINSTANCE hInstance);
    int Run();

private:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleUserMessage(WPARAM wParam, LPARAM lParam);
    void HandleDebugEvent(const DebugEvent& evt);

    HWND m_hWnd{nullptr};
    HINSTANCE m_hInstance{nullptr};
    ProtocolParser m_parser;
};

} // namespace DBPDebugger
