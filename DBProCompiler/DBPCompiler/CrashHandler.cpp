#include "CrashHandler.h"
#include "SafeDLLLoading.h"
#include <windows.h>
#include <dbghelp.h>
#include <iostream>
#include <eh.h>
#include <crtdbg.h>
#include <sstream>
#include <vector>
#include <filesystem>

namespace db3 {

static void SETranslator(unsigned int code, _EXCEPTION_POINTERS*) {
    throw CSEHException(code);
}

// Generate Minidump
static void CreateMinidump(_EXCEPTION_POINTERS* apExceptionInfo) {
    HMODULE hDbgHelp = dbp::dll::LoadSystemDLLW(L"dbghelp.dll");
    if (!hDbgHelp) return;

    typedef BOOL (WINAPI *MINIDUMPWRITEDUMP)(
        HANDLE hProcess,
        DWORD ProcessId,
        HANDLE hFile,
        MINIDUMP_TYPE DumpType,
        PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
        PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
        PMINIDUMP_CALLBACK_INFORMATION CallbackParam
    );

    MINIDUMPWRITEDUMP pfnMiniDumpWriteDump = (MINIDUMPWRITEDUMP)GetProcAddress(hDbgHelp, "MiniDumpWriteDump");
    if (pfnMiniDumpWriteDump) {
        WCHAR dumpPath[MAX_PATH];
        GetModuleFileNameW(NULL, dumpPath, MAX_PATH);
        // Replace extension with _crash.dmp
        LPWSTR ext = wcsrchr(dumpPath, L'.');
        if (ext) {
            size_t remainingSize = MAX_PATH - (ext - dumpPath);
            wcscpy_s(ext, remainingSize, L"_crash.dmp");
        } else {
            wcscat_s(dumpPath, MAX_PATH, L"_crash.dmp");
        }

        HANDLE hFile = CreateFileW(
            dumpPath,
            GENERIC_WRITE,
            FILE_SHARE_READ,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (hFile != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION mei;
            mei.ThreadId = GetCurrentThreadId();
            mei.ExceptionPointers = apExceptionInfo;
            mei.ClientPointers = FALSE;

            pfnMiniDumpWriteDump(
                GetCurrentProcess(),
                GetCurrentProcessId(),
                hFile,
                MiniDumpNormal,
                &mei,
                NULL,
                NULL
            );

            CloseHandle(hFile);
            std::cerr << "[CRITICAL] Crash dump written to: " << std::filesystem::path(dumpPath).string() << std::endl;
        }
    }
    FreeLibrary(hDbgHelp);
}

// Unhandled Exception Filter
static LONG WINAPI UnhandledCrashFilter(_EXCEPTION_POINTERS* apExceptionInfo) {
    std::cerr << "[CRITICAL] Unhandled crash detected! Exception Code: 0x" 
              << std::hex << apExceptionInfo->ExceptionRecord->ExceptionCode << std::dec << std::endl;

    CreateMinidump(apExceptionInfo);

    // Terminate immediately with non-zero exit code to prevent silent hangs/dialogs
    TerminateProcess(GetCurrentProcess(), 1);
    return EXCEPTION_EXECUTE_HANDLER;
}

// CRT Assert Report Hook (prevents GUI dialogs in headless builds)
static int CrtReportHook(int reportType, char* message, int* returnValue) {
    std::cerr << "[CRITICAL] CRT Assertion Failed: " << (message ? message : "Unknown assert") << std::endl;
    if (returnValue) {
        *returnValue = 0;
    }
    // Exit immediately to prevent blocking
    TerminateProcess(GetCurrentProcess(), 3);
    return TRUE; // Hook handled it
}

void SetupDiagnosticHandlers() {
    // 1. Register Unhandled Exception Filter
    SetUnhandledExceptionFilter(UnhandledCrashFilter);

    // 2. Set SEH Translator for local thread SEH-to-C++ conversion
    _set_se_translator(SETranslator);

    // 3. Disable CRT Assert GUI Popups in Debug build and redirect to stderr
    _CrtSetReportHook(CrtReportHook);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);

    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
}

} // namespace db3
