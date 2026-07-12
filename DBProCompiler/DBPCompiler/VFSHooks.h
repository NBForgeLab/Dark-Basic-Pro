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
