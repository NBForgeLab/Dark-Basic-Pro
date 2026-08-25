#pragma once
#include <windows.h>
#include <optional>
#include <string>
#include <string_view>

struct MemoryPEAddressInfo {
    std::string moduleName;
    std::string sectionName;
    DWORD relativeVirtualAddress;
    DWORD sectionCharacteristics;
};

class MemoryPE {
public:
    static HMODULE LoadFromVFS(std::string_view filename);
    static HMODULE LoadFromMemory(const char* data, size_t size, std::string_view name = "");
    static bool IsMemoryModule(HMODULE hModule);
    static FARPROC GetProcAddress(HMODULE hModule, LPCSTR lpProcName);
    [[nodiscard]] static std::optional<MemoryPEAddressInfo> InspectAddress(
        const void* address);
    static void UnloadModule(HMODULE hModule);
    static void FreeAll();
};
