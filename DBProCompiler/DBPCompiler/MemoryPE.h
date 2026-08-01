#pragma once
#include <windows.h>
#include <optional>
#include <string>

struct MemoryPEAddressInfo {
    std::string moduleName;
    std::string sectionName;
    DWORD relativeVirtualAddress;
    DWORD sectionCharacteristics;
};

class MemoryPE {
public:
    static HMODULE LoadFromVFS(const std::string& filename);
    static HMODULE LoadFromMemory(const char* data, size_t size, const std::string& name = "");
    static bool IsMemoryModule(HMODULE hModule);
    static FARPROC GetProcAddress(HMODULE hModule, LPCSTR lpProcName);
    [[nodiscard]] static std::optional<MemoryPEAddressInfo> InspectAddress(
        const void* address);
    static void UnloadModule(HMODULE hModule);
    static void FreeAll();
};
