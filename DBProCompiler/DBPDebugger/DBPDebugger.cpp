#include "DBPDebugger.h"
#include <iostream>

namespace DBPDebugger {

DebuggerApp::~DebuggerApp() {
    if (m_hWnd) {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
}

bool DebuggerApp::Initialize(HINSTANCE hInstance) {
    m_hInstance = hInstance;

    const wchar_t CLASS_NAME[] = L"DBProDebuggerHeadlessClass";

    WNDCLASSW wc = {};
    wc.lpfnWndProc   = DebuggerApp::WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClassW(&wc);

    // Register invisible message-only window titled "DBProDebugger" for IPC compatibility
    m_hWnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"DBProDebugger",
        0,
        0, 0, 0, 0,
        HWND_MESSAGE,
        nullptr,
        hInstance,
        this
    );

    if (!m_hWnd) {
        // Fallback: Create background invisible window
        m_hWnd = CreateWindowExW(
            0,
            CLASS_NAME,
            L"DBProDebugger",
            WS_POPUP,
            0, 0, 0, 0,
            nullptr,
            nullptr,
            hInstance,
            this
        );
    }

    if (!m_hWnd) {
        std::cerr << "[HEADLESS DEBUGGER] Failed to register IPC message listener window.\n";
        return false;
    }

    std::cout << "[HEADLESS DEBUGGER] Process started in Headless CLI mode. Listening for events...\n";
    return true;
}

int DebuggerApp::Run() {
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK DebuggerApp::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    DebuggerApp* pApp = nullptr;

    if (uMsg == WM_NCCREATE) {
        CREATESTRUCTW* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pApp = reinterpret_cast<DebuggerApp*>(pCreate->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pApp));
    } else {
        pApp = reinterpret_cast<DebuggerApp*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    if (pApp) {
        if (uMsg == (WM_USER + 10)) {
            return pApp->HandleUserMessage(wParam, lParam);
        }

        if (uMsg == WM_DESTROY) {
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

LRESULT DebuggerApp::HandleUserMessage(WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    int iType = static_cast<int>(wParam);

    HANDLE hFileMap = OpenFileMappingW(FILE_MAP_READ, FALSE, L"DBPRODEBUGGERMESSAGE");
    if (!hFileMap) {
        hFileMap = OpenFileMappingW(FILE_MAP_READ, FALSE, L"DBPROEDITORMESSAGE");
    }

    if (hFileMap) {
        LPVOID lpVoid = MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 0);
        if (lpVoid) {
            MEMORY_BASIC_INFORMATION mbi;
            if (VirtualQuery(lpVoid, &mbi, sizeof(mbi))) {
                size_t size = mbi.RegionSize;
                auto evtOpt = m_parser.ParseMessage(iType, static_cast<const uint8_t*>(lpVoid), size);
                if (evtOpt) {
                    HandleDebugEvent(*evtOpt);
                }
            }
            UnmapViewOfFile(lpVoid);
        }
        CloseHandle(hFileMap);
    }

    return 1;
}

void DebuggerApp::HandleDebugEvent(const DebugEvent& evt) {
    std::string formatted = FormatEventForCLI(evt);
    std::cout << formatted << std::endl;
}

} // namespace DBPDebugger

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    HINSTANCE hInstance = GetModuleHandleW(nullptr);
    DBPDebugger::DebuggerApp app;

    if (!app.Initialize(hInstance)) {
        return -1;
    }

    return app.Run();
}
