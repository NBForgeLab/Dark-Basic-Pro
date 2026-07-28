#pragma once

#include "dbp/package/PackageError.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

enum class VFSSeekOrigin {
    Begin,
    Current,
    End,
};

class IVFSReadStream {
public:
    virtual ~IVFSReadStream() = default;

    virtual std::uint64_t Size() const noexcept = 0;
    virtual dbp::package::PackageResult<std::size_t> Read(
        void* destination,
        std::size_t size) = 0;
    virtual dbp::package::PackageResult<std::uint64_t> Seek(
        std::int64_t distance,
        VFSSeekOrigin origin) = 0;
};

class IVFSDataSource {
public:
    virtual ~IVFSDataSource() = default;

    virtual dbp::package::PackageResult<
        std::shared_ptr<IVFSReadStream>> Open() const = 0;
};

class OwnedMemoryVFSDataSource final : public IVFSDataSource {
public:
    explicit OwnedMemoryVFSDataSource(
        std::vector<std::uint8_t> bytes);
    explicit OwnedMemoryVFSDataSource(
        std::shared_ptr<const std::vector<std::uint8_t>> bytes);

    dbp::package::PackageResult<
        std::shared_ptr<IVFSReadStream>> Open() const override;

private:
    std::shared_ptr<const std::vector<std::uint8_t>> bytes_;
};

class VFSRegistry {
public:
    static bool Register(
        std::string_view filename,
        std::shared_ptr<const IVFSDataSource> source);
    static bool RegisterOwned(
        std::string_view filename,
        std::vector<std::uint8_t> bytes);
    static bool RegisterAlias(
        std::string_view alias,
        std::string_view existingFilename);
    static bool Unregister(
        std::string_view filename,
        const std::shared_ptr<const IVFSDataSource>& expectedSource);

    static bool Exists(std::string_view filename);
    static std::shared_ptr<const IVFSDataSource> Resolve(
        std::string_view filename);
    static dbp::package::PackageResult<
        std::shared_ptr<IVFSReadStream>> Open(
        std::string_view filename);
    static void Clear();
};

class VFSHooks {
public:
    static bool Initialize();
    static void Shutdown();
    static bool IsHookActive();
};

extern "C" {
HANDLE WINAPI Hook_CreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile);
HANDLE WINAPI Hook_CreateFileA(
    LPCSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile);
BOOL WINAPI Hook_ReadFile(
    HANDLE hFile,
    LPVOID lpBuffer,
    DWORD nNumberOfBytesToRead,
    LPDWORD lpNumberOfBytesRead,
    LPOVERLAPPED lpOverlapped);
DWORD WINAPI Hook_GetFileSize(
    HANDLE hFile,
    LPDWORD lpFileSizeHigh);
DWORD WINAPI Hook_SetFilePointer(
    HANDLE hFile,
    LONG lDistanceToMove,
    PLONG lpDistanceToMoveHigh,
    DWORD dwMoveMethod);
BOOL WINAPI Hook_SetFilePointerEx(
    HANDLE hFile,
    LARGE_INTEGER liDistanceToMove,
    PLARGE_INTEGER lpNewFilePointer,
    DWORD dwMoveMethod);
BOOL WINAPI Hook_CloseHandle(HANDLE hObject);
FARPROC WINAPI Hook_GetProcAddress(
    HMODULE hModule,
    LPCSTR lpProcName);
HMODULE WINAPI Hook_LoadLibraryA(LPCSTR lpLibFileName);
HMODULE WINAPI Hook_LoadLibraryW(LPCWSTR lpLibFileName);
}
