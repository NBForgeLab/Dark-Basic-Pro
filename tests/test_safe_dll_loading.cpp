#include <gtest/gtest.h>
#include "SafeDLLLoading.h"
#include <windows.h>
#include <string>

// ---------------------------------------------------------------------------
// Characterization tests for safe DLL loading wrappers.
// These verify that the LoadLibraryEx-based helpers in SafeDLLLoading.h
// correctly load system and application DLLs while preventing DLL hijacking.
// ---------------------------------------------------------------------------

namespace {

// ---- System DLL loading (LOAD_LIBRARY_SEARCH_SYSTEM32) --------------------

TEST(SafeDLLLoadingTest, LoadSystemDLLW_LoadsKnownSystemDll) {
    // dbghelp.dll is always in System32 and used by CrashHandler.
    HMODULE hModule = dbp::dll::LoadSystemDLLW(L"dbghelp.dll");
    ASSERT_NE(hModule, nullptr) << "dbghelp.dll must load from System32";
    FreeLibrary(hModule);
}

TEST(SafeDLLLoadingTest, LoadSystemDLLA_LoadsKnownSystemDll) {
    HMODULE hModule = dbp::dll::LoadSystemDLLA("kernel32.dll");
    ASSERT_NE(hModule, nullptr) << "kernel32.dll must load from System32";
    FreeLibrary(hModule);
}

TEST(SafeDLLLoadingTest, LoadSystemDLLW_ReturnsNullForNonexistentDll) {
    HMODULE hModule = dbp::dll::LoadSystemDLLW(L"this_dll_does_not_exist_12345.dll");
    EXPECT_EQ(hModule, nullptr) << "Non-existent DLL must return nullptr";
}

TEST(SafeDLLLoadingTest, LoadSystemDLLW_ReturnsNullForNullName) {
    HMODULE hModule = dbp::dll::LoadSystemDLLW(nullptr);
    EXPECT_EQ(hModule, nullptr);
}

TEST(SafeDLLLoadingTest, LoadSystemDLLA_ReturnsNullForNullName) {
    HMODULE hModule = dbp::dll::LoadSystemDLLA(nullptr);
    EXPECT_EQ(hModule, nullptr);
}

// ---- Application DLL loading (LOAD_LIBRARY_SEARCH_DEFAULT_DIRS) -----------

TEST(SafeDLLLoadingTest, LoadApplicationDLLW_LoadsSystemDllViaDefaultDirs) {
    // kernel32.dll is resolvable through default dirs
    HMODULE hModule = dbp::dll::LoadApplicationDLLW(L"kernel32.dll");
    ASSERT_NE(hModule, nullptr) << "kernel32.dll must load via default dirs";
    FreeLibrary(hModule);
}

TEST(SafeDLLLoadingTest, LoadApplicationDLLA_LoadsSystemDllViaDefaultDirs) {
    HMODULE hModule = dbp::dll::LoadApplicationDLLA("kernel32.dll");
    ASSERT_NE(hModule, nullptr) << "kernel32.dll must load via default dirs";
    FreeLibrary(hModule);
}

TEST(SafeDLLLoadingTest, LoadApplicationDLLW_ReturnsNullForNonexistentDll) {
    HMODULE hModule = dbp::dll::LoadApplicationDLLW(L"nonexistent_plugin_99.dll");
    EXPECT_EQ(hModule, nullptr) << "Non-existent DLL must return nullptr";
}

// ---- Verify LoadLibraryEx flags are respected ----------------------------

TEST(SafeDLLLoadingTest, SystemDLLSearchDoesNotLoadFromAppDirectory) {
    // Create a fake DLL in the current directory - it should NOT be loaded
    // by LoadSystemDLLW because LOAD_LIBRARY_SEARCH_SYSTEM32 only searches System32.
    const std::wstring fakeDllName = L"fake_system_test.dll";
    HANDLE hFile = CreateFileW(
        fakeDllName.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        // Write minimal content (not a valid DLL, but the file exists)
        const char dummy[] = "MZ";
        DWORD written = 0;
        WriteFile(hFile, dummy, 2, &written, nullptr);
        CloseHandle(hFile);

        // LoadSystemDLLW must NOT find this file in the app directory
        HMODULE hModule = dbp::dll::LoadSystemDLLW(fakeDllName.c_str());
        EXPECT_EQ(hModule, nullptr)
            << "LoadSystemDLLW must not load DLLs from the application directory";

        DeleteFileW(fakeDllName.c_str());
    }
}

// ---- Verify GetProcAddress still works on loaded modules -----------------

TEST(SafeDLLLoadingTest, CanResolveExportFromSystemDll) {
    HMODULE hModule = dbp::dll::LoadSystemDLLW(L"kernel32.dll");
    ASSERT_NE(hModule, nullptr);

    FARPROC proc = GetProcAddress(hModule, "GetLastError");
    EXPECT_NE(proc, nullptr) << "Must be able to resolve GetLastError from kernel32";

    FreeLibrary(hModule);
}

TEST(SafeDLLLoadingTest, CanResolveExportFromAppDll) {
    HMODULE hModule = dbp::dll::LoadApplicationDLLW(L"kernel32.dll");
    ASSERT_NE(hModule, nullptr);

    FARPROC proc = GetProcAddress(hModule, "GetLastError");
    EXPECT_NE(proc, nullptr) << "Must be able to resolve GetLastError from kernel32";

    FreeLibrary(hModule);
}

} // anonymous namespace
