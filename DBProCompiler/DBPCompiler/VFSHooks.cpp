#include "VFSHooks.h"
#include "MemoryPE.h"
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
static DWORD g_nextHandleId = 0x80000000;
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
    return ((DWORD)hFile & 0x80000000) != 0;
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

// Inline Detour Structure
struct HookState {
    void* targetAddr = nullptr;
    BYTE originalBytes[5];
    BYTE hookBytes[5];
    bool active = false;
    
    void Hook(void* target, void* hook) {
        targetAddr = target;
        memcpy(originalBytes, target, 5);
        
        hookBytes[0] = 0xE9; // Relative JMP
        DWORD relativeOffset = (DWORD)hook - (DWORD)target - 5;
        memcpy(&hookBytes[1], &relativeOffset, 4);
        
        Apply();
    }
    
    void Apply() {
        if (active || !targetAddr) return;
        DWORD oldProtect;
        VirtualProtect(targetAddr, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy(targetAddr, hookBytes, 5);
        VirtualProtect(targetAddr, 5, oldProtect, &oldProtect);
        active = true;
    }
    
    void Remove() {
        if (!active || !targetAddr) return;
        DWORD oldProtect;
        VirtualProtect(targetAddr, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy(targetAddr, originalBytes, 5);
        VirtualProtect(targetAddr, 5, oldProtect, &oldProtect);
        active = false;
    }
};

static HookState hook_CreateFileW;
static HookState hook_CreateFileA;
static HookState hook_ReadFile;
static HookState hook_GetFileSize;
static HookState hook_SetFilePointer;
static HookState hook_SetFilePointerEx;
static HookState hook_CloseHandle;
static HookState hook_GetProcAddress;

// Detour API Hook Functions
HANDLE WINAPI Hook_CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, 
                               LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, 
                               DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    std::wstring ws(lpFileName);
    std::string s(ws.begin(), ws.end());
    std::string name = get_filename_only(s);
    
    if (VFSRegistry::Exists(name)) {
        const VFSFile* f = VFSRegistry::Get(name);
        VFSStream* stream = new VFSStream{f->dataPtr, f->size, 0};
        HANDLE hVFS = (HANDLE)g_nextHandleId++;
        g_activeStreams[hVFS] = stream;
        return hVFS;
    }
    
    hook_CreateFileW.Remove();
    HANDLE res = CreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, 
                             dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    hook_CreateFileW.Apply();
    return res;
}

HANDLE WINAPI Hook_CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, 
                               LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, 
                               DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    std::string s(lpFileName);
    std::string name = get_filename_only(s);
    
    if (VFSRegistry::Exists(name)) {
        const VFSFile* f = VFSRegistry::Get(name);
        VFSStream* stream = new VFSStream{f->dataPtr, f->size, 0};
        HANDLE hVFS = (HANDLE)g_nextHandleId++;
        g_activeStreams[hVFS] = stream;
        return hVFS;
    }
    
    hook_CreateFileA.Remove();
    HANDLE res = CreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, 
                             dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    hook_CreateFileA.Apply();
    return res;
}

BOOL WINAPI Hook_ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, 
                          LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped) {
    if (IsVFSHandle(hFile)) {
        auto it = g_activeStreams.find(hFile);
        if (it != g_activeStreams.end()) {
            VFSStream* stream = it->second;
            if (stream->offset >= stream->size) {
                if (lpNumberOfBytesRead) *lpNumberOfBytesRead = 0;
                return TRUE;
            }
            size_t toRead = nNumberOfBytesToRead;
            if (stream->offset + toRead > stream->size) {
                toRead = stream->size - stream->offset;
            }
            memcpy(lpBuffer, stream->dataPtr + stream->offset, toRead);
            stream->offset += toRead;
            if (lpNumberOfBytesRead) *lpNumberOfBytesRead = toRead;
            return TRUE;
        }
    }
    
    hook_ReadFile.Remove();
    BOOL res = ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
    hook_ReadFile.Apply();
    return res;
}

DWORD WINAPI Hook_GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh) {
    if (IsVFSHandle(hFile)) {
        auto it = g_activeStreams.find(hFile);
        if (it != g_activeStreams.end()) {
            VFSStream* stream = it->second;
            if (lpFileSizeHigh) *lpFileSizeHigh = 0;
            return (DWORD)stream->size;
        }
    }
    
    hook_GetFileSize.Remove();
    DWORD res = GetFileSize(hFile, lpFileSizeHigh);
    hook_GetFileSize.Apply();
    return res;
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
            if (newOffset > (LONGLONG)stream->size) newOffset = stream->size;
            stream->offset = (size_t)newOffset;
            if (lpDistanceToMoveHigh) {
                *lpDistanceToMoveHigh = (LONG)(newOffset >> 32);
            }
            return (DWORD)newOffset;
        }
    }
    
    hook_SetFilePointer.Remove();
    DWORD res = SetFilePointer(hFile, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);
    hook_SetFilePointer.Apply();
    return res;
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
            if (newOffset > (LONGLONG)stream->size) newOffset = stream->size;
            stream->offset = (size_t)newOffset;
            if (lpNewFilePointer) {
                lpNewFilePointer->QuadPart = newOffset;
            }
            return TRUE;
        }
    }
    
    hook_SetFilePointerEx.Remove();
    BOOL res = SetFilePointerEx(hFile, liDistanceToMove, lpNewFilePointer, dwMoveMethod);
    hook_SetFilePointerEx.Apply();
    return res;
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
    
    hook_CloseHandle.Remove();
    BOOL res = CloseHandle(hObject);
    hook_CloseHandle.Apply();
    return res;
}

FARPROC WINAPI Hook_GetProcAddress(HMODULE hModule, LPCSTR lpProcName) {
    if (MemoryPE::IsMemoryModule(hModule)) {
        return MemoryPE::GetProcAddress(hModule, lpProcName);
    }
    hook_GetProcAddress.Remove();
    FARPROC res = GetProcAddress(hModule, lpProcName);
    hook_GetProcAddress.Apply();
    return res;
}

// Global Hook Management
bool VFSHooks::Initialize() {
    if (g_hookActive) return true;
    
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (!hKernel32) return false;
    
    void* pCreateFileW = (void*)GetProcAddress(hKernel32, "CreateFileW");
    void* pCreateFileA = (void*)GetProcAddress(hKernel32, "CreateFileA");
    void* pReadFile = (void*)GetProcAddress(hKernel32, "ReadFile");
    void* pGetFileSize = (void*)GetProcAddress(hKernel32, "GetFileSize");
    void* pSetFilePointer = (void*)GetProcAddress(hKernel32, "SetFilePointer");
    void* pSetFilePointerEx = (void*)GetProcAddress(hKernel32, "SetFilePointerEx");
    void* pCloseHandle = (void*)GetProcAddress(hKernel32, "CloseHandle");
    void* pGetProcAddress = (void*)GetProcAddress(hKernel32, "GetProcAddress");
    
    if (!pCreateFileW || !pCreateFileA || !pReadFile || !pGetFileSize || !pSetFilePointer || !pCloseHandle || !pGetProcAddress) {
        return false;
    }
    
    hook_CreateFileW.Hook(pCreateFileW, (void*)Hook_CreateFileW);
    hook_CreateFileA.Hook(pCreateFileA, (void*)Hook_CreateFileA);
    hook_ReadFile.Hook(pReadFile, (void*)Hook_ReadFile);
    hook_GetFileSize.Hook(pGetFileSize, (void*)Hook_GetFileSize);
    hook_SetFilePointer.Hook(pSetFilePointer, (void*)Hook_SetFilePointer);
    if (pSetFilePointerEx) {
        hook_SetFilePointerEx.Hook(pSetFilePointerEx, (void*)Hook_SetFilePointerEx);
    }
    hook_CloseHandle.Hook(pCloseHandle, (void*)Hook_CloseHandle);
    hook_GetProcAddress.Hook(pGetProcAddress, (void*)Hook_GetProcAddress);
    
    g_hookActive = true;
    return true;
}

void VFSHooks::Shutdown() {
    if (!g_hookActive) return;
    
    hook_CreateFileW.Remove();
    hook_CreateFileA.Remove();
    hook_ReadFile.Remove();
    hook_GetFileSize.Remove();
    hook_SetFilePointer.Remove();
    hook_SetFilePointerEx.Remove();
    hook_CloseHandle.Remove();
    hook_GetProcAddress.Remove();
    
    // Clear active streams
    for (auto it : g_activeStreams) {
        delete it.second;
    }
    g_activeStreams.clear();
    
    g_hookActive = false;
}

bool VFSHooks::IsHookActive() {
    return g_hookActive;
}
