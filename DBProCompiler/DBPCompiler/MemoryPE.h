#pragma once
#include <windows.h>
#include <string>

class MemoryPE {
public:
    static HMODULE LoadFromVFS(const std::string& filename);
    static HMODULE LoadFromMemory(const char* data, size_t size, const std::string& name = "");
    static bool IsMemoryModule(HMODULE hModule);
    static FARPROC GetProcAddress(HMODULE hModule, LPCSTR lpProcName);
    static void UnloadModule(HMODULE hModule);
    static void FreeAll();
};
