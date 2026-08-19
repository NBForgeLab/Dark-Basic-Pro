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
        GetModuleFileNameW(nullptr, dumpPath, MAX_PATH);
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
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
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
                nullptr,
                nullptr
            );

            CloseHandle(hFile);
            std::cerr << "[CRITICAL] Crash dump written to: " << std::filesystem::path(dumpPath).string() << std::endl;
        }
    }
    FreeLibrary(hDbgHelp);
}

static void PrintStackTrace(PCONTEXT contextRecord) {
    if (!contextRecord) return;
    
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    SymInitialize(process, nullptr, TRUE);

    CONTEXT context = *contextRecord;
    STACKFRAME64 stackFrame = {};
#if defined(_M_X64) || defined(__x86_64__)
    // The SDK builds x64-only; stack unwinding uses the AMD64 context.
    DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
    stackFrame.AddrPC.Offset = context.Rip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context.Rbp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context.Rsp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
#else
#error "CrashHandler supports x64 targets only"
#endif

    std::cerr << "[CRITICAL] Stack Trace:" << std::endl;

    for (int frame = 0; frame < 32; ++frame) {
        if (!StackWalk64(machineType, process, thread, &stackFrame, &context,
                         nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) {
            break;
        }

        if (stackFrame.AddrPC.Offset == 0) break;

        DWORD64 displacement = 0;
        char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)] = {};
        PSYMBOL_INFO symbol = (PSYMBOL_INFO)buffer;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;

        IMAGEHLP_LINE64 line = {};
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        DWORD lineDisplacement = 0;

        std::cerr << "  [" << frame << "] 0x" << std::hex << stackFrame.AddrPC.Offset << std::dec << " ";
        if (SymFromAddr(process, stackFrame.AddrPC.Offset, &displacement, symbol)) {
            std::cerr << symbol->Name;
        } else {
            std::cerr << "???";
        }

        if (SymGetLineFromAddr64(process, stackFrame.AddrPC.Offset, &lineDisplacement, &line)) {
            std::cerr << " (" << line.FileName << ":" << line.LineNumber << ")";
        }
        std::cerr << std::endl;
    }

    SymCleanup(process);
}

// Unhandled Exception Filter
static LONG WINAPI UnhandledCrashFilter(_EXCEPTION_POINTERS* apExceptionInfo) {
    std::cerr << "[CRITICAL] Unhandled crash detected! Exception Code: 0x" 
              << std::hex << apExceptionInfo->ExceptionRecord->ExceptionCode << std::dec << std::endl;

    if (apExceptionInfo && apExceptionInfo->ContextRecord) {
        PrintStackTrace(apExceptionInfo->ContextRecord);
    }

    CreateMinidump(apExceptionInfo);

    // Terminate immediately with non-zero exit code to prevent silent hangs/dialogs
    TerminateProcess(GetCurrentProcess(), 1);
    return EXCEPTION_EXECUTE_HANDLER;
}

// CRT Assert Report Hook (prevents GUI dialogs in headless builds)
static int CrtReportHook([[maybe_unused]] int reportType, char* message, int* returnValue) {
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
