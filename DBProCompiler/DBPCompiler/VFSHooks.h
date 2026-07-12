#pragma once
#include <string>
#include <vector>
#include <windows.h>

struct VFSFile {
    const char* dataPtr = nullptr;
    size_t size = 0;
};

class VFSRegistry {
public:
    static void Register(const std::string& filename, const char* ptr, size_t size);
    static bool Exists(const std::string& filename);
    static const VFSFile* Get(const std::string& filename);
    static void Clear();
};

class VFSHooks {
public:
    static bool Initialize();
    static void Shutdown();
    static bool IsHookActive();
};

extern "C" {
HANDLE WINAPI Hook_CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, 
                               LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, 
                               DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
HANDLE WINAPI Hook_CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, 
                               LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, 
                               DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
BOOL WINAPI Hook_ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, 
                          LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);
DWORD WINAPI Hook_GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh);
DWORD WINAPI Hook_SetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod);
BOOL WINAPI Hook_SetFilePointerEx(HANDLE hFile, LARGE_INTEGER liDistanceToMove, PLARGE_INTEGER lpNewFilePointer, DWORD dwMoveMethod);
BOOL WINAPI Hook_CloseHandle(HANDLE hObject);
FARPROC WINAPI Hook_GetProcAddress(HMODULE hModule, LPCSTR lpProcName);
HMODULE WINAPI Hook_LoadLibraryA(LPCSTR lpLibFileName);
HMODULE WINAPI Hook_LoadLibraryW(LPCWSTR lpLibFileName);
}
