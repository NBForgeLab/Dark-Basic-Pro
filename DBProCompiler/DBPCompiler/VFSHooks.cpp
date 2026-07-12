#include "VFSHooks.h"
#include "MemoryPE.h"
#include "TextConvert.h"
#include <unordered_map>
#include <algorithm>
#include <iostream>

struct VFSStream {
    const char* dataPtr = nullptr;
    size_t size = 0;
    size_t offset = 0;
};

// Global Registry State
static std::unordered_map<std::string, VFSFile> g_vfsMap;
static std::unordered_map<HANDLE, VFSStream*> g_activeStreams;
static uintptr_t g_nextHandleId = 0x7F000000;
static bool g_hookActive = false;

static std::string to_lower(const std::string& str) {
    std::string res = str;
    std::transform(res.begin(), res.end(), res.begin(), ::tolower);
    return res;
}

static std::string get_filename_only(const std::string& path) {
    size_t pos = path.find_last_of("\\/");
    if (pos != std::string::npos) {
        return path.substr(pos + 1);
    }
    return path;
}

static bool IsVFSHandle(HANDLE hFile) {
    return g_activeStreams.find(hFile) != g_activeStreams.end();
}

void VFSRegistry::Register(const std::string& filename, const char* ptr, size_t size) {
    g_vfsMap[to_lower(filename)] = { ptr, size };
}

bool VFSRegistry::Exists(const std::string& filename) {
    return g_vfsMap.find(to_lower(filename)) != g_vfsMap.end();
}

const VFSFile* VFSRegistry::Get(const std::string& filename) {
    auto it = g_vfsMap.find(to_lower(filename));
    if (it != g_vfsMap.end()) return &it->second;
    return nullptr;
}

void VFSRegistry::Clear() {
    g_vfsMap.clear();
}

// Detour API Hook Functions (Resolved via IAT, no detouring overrides needed)
HANDLE WINAPI Hook_CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, 
                               LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, 
                               DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    if (!lpFileName) return INVALID_HANDLE_VALUE;

    // VFS is read-only; fall back to disk if write or creation is requested
    if ((dwDesiredAccess & (GENERIC_WRITE | FILE_WRITE_DATA | FILE_APPEND_DATA)) != 0 || 
        dwCreationDisposition != OPEN_EXISTING) {
        return CreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, 
                           dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    }

    std::wstring ws(lpFileName);
    std::string s = TextConvert::UTF16ToUTF8(ws);
    std::replace(s.begin(), s.end(), '\\', '/');
    std::string name = to_lower(s);
    std::string baseName = to_lower(get_filename_only(s));

    std::string matchedName = "";
    if (VFSRegistry::Exists(name)) {
        matchedName = name;
    } else if (VFSRegistry::Exists(baseName)) {
        matchedName = baseName;
    }

    if (!matchedName.empty()) {
        const VFSFile* f = VFSRegistry::Get(matchedName);
        VFSStream* stream = new VFSStream{f->dataPtr, f->size, 0};
        HANDLE hVFS = (HANDLE)(uintptr_t)g_nextHandleId++;
        g_activeStreams[hVFS] = stream;
        return hVFS;
    }
    
    return CreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, 
                       dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

HANDLE WINAPI Hook_CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, 
                               LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, 
                               DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    if (!lpFileName) return INVALID_HANDLE_VALUE;

    if ((dwDesiredAccess & (GENERIC_WRITE | FILE_WRITE_DATA | FILE_APPEND_DATA)) != 0 || 
        dwCreationDisposition != OPEN_EXISTING) {
        return CreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, 
                           dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    }

    std::string s(lpFileName);
    std::replace(s.begin(), s.end(), '\\', '/');
    std::string name = to_lower(s);
    std::string baseName = to_lower(get_filename_only(s));

    std::string matchedName = "";
    if (VFSRegistry::Exists(name)) {
        matchedName = name;
    } else if (VFSRegistry::Exists(baseName)) {
        matchedName = baseName;
    }

    if (!matchedName.empty()) {
        const VFSFile* f = VFSRegistry::Get(matchedName);
        VFSStream* stream = new VFSStream{f->dataPtr, f->size, 0};
        HANDLE hVFS = (HANDLE)(uintptr_t)g_nextHandleId++;
        g_activeStreams[hVFS] = stream;
        return hVFS;
    }
    
    return CreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, 
                       dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

BOOL WINAPI Hook_ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, 
                          LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped) {
    if (IsVFSHandle(hFile)) {
        auto it = g_activeStreams.find(hFile);
        if (it != g_activeStreams.end()) {
            VFSStream* stream = it->second;
            DWORD readSize = nNumberOfBytesToRead;
            if (stream->offset + readSize > stream->size) {
                readSize = (DWORD)(stream->size - stream->offset);
            }
            memcpy(lpBuffer, stream->dataPtr + stream->offset, readSize);
            stream->offset += readSize;
            if (lpNumberOfBytesRead) {
                *lpNumberOfBytesRead = readSize;
            }
            return TRUE;
        }
    }
    return ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
}

DWORD WINAPI Hook_GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh) {
    if (IsVFSHandle(hFile)) {
        auto it = g_activeStreams.find(hFile);
        if (it != g_activeStreams.end()) {
            VFSStream* stream = it->second;
            if (lpFileSizeHigh) {
                *lpFileSizeHigh = (DWORD)(stream->size >> 32);
            }
            return (DWORD)stream->size;
        }
    }
    return GetFileSize(hFile, lpFileSizeHigh);
}

DWORD WINAPI Hook_SetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod) {
    if (IsVFSHandle(hFile)) {
        auto it = g_activeStreams.find(hFile);
        if (it != g_activeStreams.end()) {
            VFSStream* stream = it->second;
            LONGLONG distance = lDistanceToMove;
            if (lpDistanceToMoveHigh) {
                distance |= ((LONGLONG)*lpDistanceToMoveHigh << 32);
            }
            LONGLONG newOffset = stream->offset;
            switch (dwMoveMethod) {
                case FILE_BEGIN: newOffset = distance; break;
                case FILE_CURRENT: newOffset += distance; break;
                case FILE_END: newOffset = (LONGLONG)stream->size + distance; break;
            }
            if (newOffset < 0) newOffset = 0;
            if (newOffset > (LONGLONG)stream->size) newOffset = (LONGLONG)stream->size;
            stream->offset = (size_t)newOffset;
            if (lpDistanceToMoveHigh) {
                *lpDistanceToMoveHigh = (LONG)(newOffset >> 32);
            }
            return (DWORD)newOffset;
        }
    }
    return SetFilePointer(hFile, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);
}

BOOL WINAPI Hook_SetFilePointerEx(HANDLE hFile, LARGE_INTEGER liDistanceToMove, PLARGE_INTEGER lpNewFilePointer, DWORD dwMoveMethod) {
    if (IsVFSHandle(hFile)) {
        auto it = g_activeStreams.find(hFile);
        if (it != g_activeStreams.end()) {
            VFSStream* stream = it->second;
            LONGLONG distance = liDistanceToMove.QuadPart;
            LONGLONG newOffset = stream->offset;
            switch (dwMoveMethod) {
                case FILE_BEGIN: newOffset = distance; break;
                case FILE_CURRENT: newOffset += distance; break;
                case FILE_END: newOffset = (LONGLONG)stream->size + distance; break;
            }
            if (newOffset < 0) newOffset = 0;
            if (newOffset > (LONGLONG)stream->size) newOffset = (LONGLONG)stream->size;
            stream->offset = (size_t)newOffset;
            if (lpNewFilePointer) {
                lpNewFilePointer->QuadPart = newOffset;
            }
            return TRUE;
        }
    }
    return SetFilePointerEx(hFile, liDistanceToMove, lpNewFilePointer, dwMoveMethod);
}

BOOL WINAPI Hook_CloseHandle(HANDLE hObject) {
    if (IsVFSHandle(hObject)) {
        auto it = g_activeStreams.find(hObject);
        if (it != g_activeStreams.end()) {
            delete it->second;
            g_activeStreams.erase(it);
            return TRUE;
        }
    }
    return CloseHandle(hObject);
}

FARPROC WINAPI Hook_GetProcAddress(HMODULE hModule, LPCSTR lpProcName) {
    if (MemoryPE::IsMemoryModule(hModule)) {
        return MemoryPE::GetProcAddress(hModule, lpProcName);
    }
    return GetProcAddress(hModule, lpProcName);
}

HMODULE WINAPI Hook_LoadLibraryA(LPCSTR lpLibFileName) {
    if (!lpLibFileName) return nullptr;
    std::string s(lpLibFileName);
    std::replace(s.begin(), s.end(), '\\', '/');
    std::string name = to_lower(s);
    std::string baseName = to_lower(get_filename_only(s));
    
    std::string matchedName = "";
    if (VFSRegistry::Exists(name)) {
        matchedName = name;
    } else if (VFSRegistry::Exists(baseName)) {
        matchedName = baseName;
    }
    
    if (!matchedName.empty()) {
        HMODULE hMod = MemoryPE::LoadFromVFS(matchedName);
        if (hMod) return hMod;
    }
    return LoadLibraryA(lpLibFileName);
}

HMODULE WINAPI Hook_LoadLibraryW(LPCWSTR lpLibFileName) {
    if (!lpLibFileName) return nullptr;
    std::wstring ws(lpLibFileName);
    std::string s = TextConvert::UTF16ToUTF8(ws);
    std::replace(s.begin(), s.end(), '\\', '/');
    std::string name = to_lower(s);
    std::string baseName = to_lower(get_filename_only(s));
    
    std::string matchedName = "";
    if (VFSRegistry::Exists(name)) {
        matchedName = name;
    } else if (VFSRegistry::Exists(baseName)) {
        matchedName = baseName;
    }
    
    if (!matchedName.empty()) {
        HMODULE hMod = MemoryPE::LoadFromVFS(matchedName);
        if (hMod) return hMod;
    }
    return LoadLibraryW(lpLibFileName);
}

// Global Hook Management (Empty initialization as we now use IAT redirection)
bool VFSHooks::Initialize() {
    g_hookActive = true;
    return true;
}

void VFSHooks::Shutdown() {
    for (auto it : g_activeStreams) {
        delete it.second;
    }
    g_activeStreams.clear();
    g_hookActive = false;
}

bool VFSHooks::IsHookActive() {
    return g_hookActive;
}
