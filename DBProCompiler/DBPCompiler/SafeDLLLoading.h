#pragma once
// ---------------------------------------------------------------------------
// SafeDLLLoading.h - DLL hijacking prevention wrappers
// ---------------------------------------------------------------------------
// All LoadLibrary calls in the compiler should go through these helpers to
// ensure LOAD_LIBRARY_SEARCH_* flags are applied, preventing DLL search-order
// hijacking attacks.
// ---------------------------------------------------------------------------

#include <windows.h>

namespace dbp {
namespace dll {

// Load a well-known system DLL (e.g. "dbghelp.dll", "d3d9.dll").
// Searches ONLY the System32 directory.
inline HMODULE LoadSystemDLLW(LPCWSTR name) {
    return LoadLibraryExW(name, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
}

inline HMODULE LoadSystemDLLA(LPCSTR name) {
    return LoadLibraryExA(name, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
}

// Load an application / plugin DLL with safe search behavior.
// For paths (relative or absolute), uses LOAD_WITH_ALTERED_SEARCH_PATH to resolve adjacent dependencies.
// For bare module names, searches default secure application directories.
inline HMODULE LoadApplicationDLLW(LPCWSTR name) {
    if (name == nullptr) return nullptr;
    if (wcschr(name, L'\\') != nullptr || wcschr(name, L'/') != nullptr) {
        HMODULE h = LoadLibraryExW(name, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (h) return h;
        return LoadLibraryW(name);
    }
    return LoadLibraryExW(name, nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
}

inline HMODULE LoadApplicationDLLA(LPCSTR name) {
    if (name == nullptr) return nullptr;
    if (strchr(name, '\\') != nullptr || strchr(name, '/') != nullptr) {
        HMODULE h = LoadLibraryExA(name, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (h) return h;
        return LoadLibraryA(name);
    }
    return LoadLibraryExA(name, nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
}

} // namespace dll
} // namespace dbp
