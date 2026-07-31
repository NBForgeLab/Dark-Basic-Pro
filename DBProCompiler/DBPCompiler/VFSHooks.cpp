#include "VFSHooks.h"

#include "MemoryPE.h"
#include "SafeDLLLoading.h"
#include "TextConvert.h"
#include "dbp/package/PackagePath.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using dbp::package::PackageErrorCode;
using dbp::package::PackageResult;

template <typename T>
PackageResult<T> VFSError(
    const PackageErrorCode code,
    std::string message) {
    return PackageResult<T>::Failure({
        code,
        std::move(message),
        std::nullopt,
    });
}

std::string AsciiLower(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](const unsigned char character) {
            if (character >= 'A' && character <= 'Z') {
                return static_cast<char>(
                    character + ('a' - 'A'));
            }
            return static_cast<char>(character);
        });
    return value;
}

PackageResult<std::string> RegistryKey(
    const std::string_view filename) {
    const auto normalized =
        dbp::package::NormalizePackageInputPath(filename);
    if (!normalized) {
        return PackageResult<std::string>::Failure(
            normalized.error());
    }
    return PackageResult<std::string>::Success(
        AsciiLower(normalized.value()));
}

class MemoryReadStream final : public IVFSReadStream {
public:
    explicit MemoryReadStream(
        std::shared_ptr<const std::vector<std::uint8_t>> bytes)
        : bytes_(std::move(bytes)) {}

    std::uint64_t Size() const noexcept override {
        return bytes_->size();
    }

    PackageResult<std::size_t> Read(
        void* const destination,
        const std::size_t size) override {
        if (size != 0 && destination == nullptr) {
            return VFSError<std::size_t>(
                PackageErrorCode::IoFailed,
                "The VFS read destination is null.");
        }
        std::lock_guard lock(mutex_);
        const auto available = offset_ < bytes_->size()
            ? bytes_->size() - static_cast<std::size_t>(offset_)
            : 0U;
        const auto count = (std::min)(size, available);
        if (count != 0) {
            std::memcpy(
                destination,
                bytes_->data() + static_cast<std::size_t>(offset_),
                count);
        }
        offset_ += count;
        return PackageResult<std::size_t>::Success(count);
    }

    PackageResult<std::uint64_t> Seek(
        const std::int64_t distance,
        const VFSSeekOrigin origin) override {
        std::lock_guard lock(mutex_);
        std::uint64_t base = 0;
        switch (origin) {
            case VFSSeekOrigin::Begin:
                base = 0;
                break;
            case VFSSeekOrigin::Current:
                base = offset_;
                break;
            case VFSSeekOrigin::End:
                base = bytes_->size();
                break;
            default:
                return VFSError<std::uint64_t>(
                    PackageErrorCode::IoFailed,
                    "The VFS seek origin is invalid.");
        }

        std::uint64_t next = 0;
        if (distance < 0) {
            const auto magnitude =
                static_cast<std::uint64_t>(-(distance + 1)) + 1U;
            if (magnitude > base) {
                return VFSError<std::uint64_t>(
                    PackageErrorCode::IoFailed,
                    "The VFS seek would move before the start.");
            }
            next = base - magnitude;
        } else {
            const auto magnitude =
                static_cast<std::uint64_t>(distance);
            if (magnitude >
                (std::numeric_limits<std::uint64_t>::max)() - base) {
                return VFSError<std::uint64_t>(
                    PackageErrorCode::ArithmeticOverflow,
                    "The VFS seek offset overflowed.");
            }
            next = base + magnitude;
        }
        if (next >
            static_cast<std::uint64_t>(
                (std::numeric_limits<std::int64_t>::max)())) {
            return VFSError<std::uint64_t>(
                PackageErrorCode::LimitExceeded,
                "The VFS seek exceeds the signed 64-bit file range.");
        }
        offset_ = next;
        return PackageResult<std::uint64_t>::Success(offset_);
    }

private:
    std::shared_ptr<const std::vector<std::uint8_t>> bytes_;
    std::mutex mutex_;
    std::uint64_t offset_ = 0;
};

std::unordered_map<
    std::string,
    std::shared_ptr<const IVFSDataSource>> registry;
std::mutex registryMutex;

std::unordered_map<HANDLE, std::shared_ptr<IVFSReadStream>>
    activeStreams;
std::mutex activeStreamsMutex;
std::atomic<bool> hookActive{false};

std::shared_ptr<IVFSReadStream> ResolveActiveStream(
    const HANDLE handle) {
    std::lock_guard lock(activeStreamsMutex);
    const auto found = activeStreams.find(handle);
    return found == activeStreams.end()
        ? nullptr
        : found->second;
}

HANDLE OpenVirtual(
    const std::string_view filename,
    const DWORD desiredAccess,
    const DWORD creationDisposition,
    bool& wasMounted) {
    const auto source = VFSRegistry::Resolve(filename);
    wasMounted = source != nullptr;
    if (!source) {
        return INVALID_HANDLE_VALUE;
    }
    if ((desiredAccess &
         (GENERIC_WRITE | FILE_WRITE_DATA | FILE_APPEND_DATA)) != 0 ||
        creationDisposition != OPEN_EXISTING) {
        SetLastError(ERROR_ACCESS_DENIED);
        return INVALID_HANDLE_VALUE;
    }

    const auto opened = source->Open();
    if (!opened) {
        SetLastError(ERROR_INVALID_DATA);
        return INVALID_HANDLE_VALUE;
    }
    const auto handle = CreateEventW(
        nullptr,
        FALSE,
        FALSE,
        nullptr);
    if (handle == nullptr) {
        return INVALID_HANDLE_VALUE;
    }
    {
        std::lock_guard lock(activeStreamsMutex);
        const auto inserted =
            activeStreams.emplace(handle, opened.value());
        if (!inserted.second) {
            CloseHandle(handle);
            SetLastError(ERROR_ALREADY_EXISTS);
            return INVALID_HANDLE_VALUE;
        }
    }
    return handle;
}

VFSSeekOrigin SeekOrigin(const DWORD moveMethod, bool& valid) {
    valid = true;
    switch (moveMethod) {
        case FILE_BEGIN:
            return VFSSeekOrigin::Begin;
        case FILE_CURRENT:
            return VFSSeekOrigin::Current;
        case FILE_END:
            return VFSSeekOrigin::End;
        default:
            valid = false;
            return VFSSeekOrigin::Begin;
    }
}

} // namespace

OwnedMemoryVFSDataSource::OwnedMemoryVFSDataSource(
    std::vector<std::uint8_t> bytes)
    : bytes_(
          std::make_shared<const std::vector<std::uint8_t>>(
              std::move(bytes))) {}

OwnedMemoryVFSDataSource::OwnedMemoryVFSDataSource(
    std::shared_ptr<const std::vector<std::uint8_t>> bytes)
    : bytes_(std::move(bytes)) {
    if (!bytes_) {
        bytes_ =
            std::make_shared<const std::vector<std::uint8_t>>();
    }
}

PackageResult<std::shared_ptr<IVFSReadStream>>
OwnedMemoryVFSDataSource::Open() const {
    return PackageResult<std::shared_ptr<IVFSReadStream>>::Success(
        std::make_shared<MemoryReadStream>(bytes_));
}

bool VFSRegistry::Register(
    const std::string_view filename,
    std::shared_ptr<const IVFSDataSource> source) {
    if (!source) {
        return false;
    }
    const auto key = RegistryKey(filename);
    if (!key) {
        return false;
    }
    std::lock_guard lock(registryMutex);
    return registry.emplace(
        key.value(),
        std::move(source)).second;
}

bool VFSRegistry::RegisterOwned(
    const std::string_view filename,
    std::vector<std::uint8_t> bytes) {
    return Register(
        filename,
        std::make_shared<OwnedMemoryVFSDataSource>(
            std::move(bytes)));
}

bool VFSRegistry::RegisterAlias(
    const std::string_view alias,
    const std::string_view existingFilename) {
    const auto source = Resolve(existingFilename);
    return source && Register(alias, std::move(source));
}

bool VFSRegistry::Unregister(
    const std::string_view filename,
    const std::shared_ptr<const IVFSDataSource>& expectedSource) {
    const auto key = RegistryKey(filename);
    if (!key) {
        return false;
    }
    std::lock_guard lock(registryMutex);
    const auto found = registry.find(key.value());
    if (found == registry.end() ||
        found->second != expectedSource) {
        return false;
    }
    registry.erase(found);
    return true;
}

bool VFSRegistry::Exists(const std::string_view filename) {
    return Resolve(filename) != nullptr;
}

std::shared_ptr<const IVFSDataSource> VFSRegistry::Resolve(
    const std::string_view filename) {
    const auto key = RegistryKey(filename);
    if (!key) {
        return nullptr;
    }
    std::lock_guard lock(registryMutex);
    const auto found = registry.find(key.value());
    return found == registry.end()
        ? nullptr
        : found->second;
}

PackageResult<std::shared_ptr<IVFSReadStream>> VFSRegistry::Open(
    const std::string_view filename) {
    const auto source = Resolve(filename);
    if (!source) {
        return VFSError<std::shared_ptr<IVFSReadStream>>(
            PackageErrorCode::IoFailed,
            "The requested VFS path is not mounted.");
    }
    return source->Open();
}

void VFSRegistry::Clear() {
    std::lock_guard lock(registryMutex);
    registry.clear();
}

HANDLE WINAPI Hook_CreateFileW(
    const LPCWSTR fileName,
    const DWORD desiredAccess,
    const DWORD shareMode,
    const LPSECURITY_ATTRIBUTES securityAttributes,
    const DWORD creationDisposition,
    const DWORD flagsAndAttributes,
    const HANDLE templateFile) {
    if (fileName == nullptr) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }
    const auto utf8 =
        TextConvert::UTF16ToUTF8(std::wstring(fileName));
    bool wasMounted = false;
    const auto virtualHandle = OpenVirtual(
        utf8,
        desiredAccess,
        creationDisposition,
        wasMounted);
    if (wasMounted) {
        return virtualHandle;
    }
    return CreateFileW(
        fileName,
        desiredAccess,
        shareMode,
        securityAttributes,
        creationDisposition,
        flagsAndAttributes,
        templateFile);
}

HANDLE WINAPI Hook_CreateFileA(
    const LPCSTR fileName,
    const DWORD desiredAccess,
    const DWORD shareMode,
    const LPSECURITY_ATTRIBUTES securityAttributes,
    const DWORD creationDisposition,
    const DWORD flagsAndAttributes,
    const HANDLE templateFile) {
    if (fileName == nullptr) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }
    bool wasMounted = false;
    const auto virtualHandle = OpenVirtual(
        fileName,
        desiredAccess,
        creationDisposition,
        wasMounted);
    if (wasMounted) {
        return virtualHandle;
    }
    return CreateFileA(
        fileName,
        desiredAccess,
        shareMode,
        securityAttributes,
        creationDisposition,
        flagsAndAttributes,
        templateFile);
}

BOOL WINAPI Hook_ReadFile(
    const HANDLE file,
    const LPVOID buffer,
    const DWORD bytesToRead,
    const LPDWORD bytesRead,
    const LPOVERLAPPED overlapped) {
    const auto stream = ResolveActiveStream(file);
    if (!stream) {
        return ReadFile(
            file,
            buffer,
            bytesToRead,
            bytesRead,
            overlapped);
    }
    if (bytesRead != nullptr) {
        *bytesRead = 0;
    }
    if (overlapped != nullptr) {
        SetLastError(ERROR_NOT_SUPPORTED);
        return FALSE;
    }
    const auto read = stream->Read(buffer, bytesToRead);
    if (!read) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (bytesRead != nullptr) {
        *bytesRead = static_cast<DWORD>(read.value());
    }
    return TRUE;
}

DWORD WINAPI Hook_GetFileSize(
    const HANDLE file,
    const LPDWORD fileSizeHigh) {
    const auto stream = ResolveActiveStream(file);
    if (!stream) {
        return GetFileSize(file, fileSizeHigh);
    }
    const auto size = stream->Size();
    if (fileSizeHigh != nullptr) {
        *fileSizeHigh = static_cast<DWORD>(size >> 32U);
    }
    SetLastError(ERROR_SUCCESS);
    return static_cast<DWORD>(size);
}

BOOL WINAPI Hook_SetFilePointerEx(
    const HANDLE file,
    const LARGE_INTEGER distance,
    const PLARGE_INTEGER newFilePointer,
    const DWORD moveMethod) {
    const auto stream = ResolveActiveStream(file);
    if (!stream) {
        return SetFilePointerEx(
            file,
            distance,
            newFilePointer,
            moveMethod);
    }
    bool validOrigin = false;
    const auto origin = SeekOrigin(moveMethod, validOrigin);
    if (!validOrigin) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    const auto sought =
        stream->Seek(distance.QuadPart, origin);
    if (!sought ||
        sought.value() >
            static_cast<std::uint64_t>(
                (std::numeric_limits<LONGLONG>::max)())) {
        SetLastError(ERROR_NEGATIVE_SEEK);
        return FALSE;
    }
    if (newFilePointer != nullptr) {
        newFilePointer->QuadPart =
            static_cast<LONGLONG>(sought.value());
    }
    return TRUE;
}

DWORD WINAPI Hook_SetFilePointer(
    const HANDLE file,
    const LONG distanceLow,
    const PLONG distanceHigh,
    const DWORD moveMethod) {
    if (!ResolveActiveStream(file)) {
        return SetFilePointer(
            file,
            distanceLow,
            distanceHigh,
            moveMethod);
    }
    LARGE_INTEGER distance{};
    if (distanceHigh == nullptr) {
        distance.QuadPart = distanceLow;
    } else {
        distance.LowPart = static_cast<DWORD>(distanceLow);
        distance.HighPart = *distanceHigh;
    }
    LARGE_INTEGER result{};
    if (!Hook_SetFilePointerEx(
            file,
            distance,
            &result,
            moveMethod)) {
        return INVALID_SET_FILE_POINTER;
    }
    if (distanceHigh != nullptr) {
        *distanceHigh = result.HighPart;
    }
    SetLastError(ERROR_SUCCESS);
    return result.LowPart;
}

BOOL WINAPI Hook_CloseHandle(const HANDLE object) {
    std::shared_ptr<IVFSReadStream> stream;
    {
        std::lock_guard lock(activeStreamsMutex);
        const auto found = activeStreams.find(object);
        if (found != activeStreams.end()) {
            stream = std::move(found->second);
            activeStreams.erase(found);
        }
    }
    if (!stream) {
        return CloseHandle(object);
    }
    return CloseHandle(object);
}

FARPROC WINAPI Hook_GetProcAddress(
    const HMODULE module,
    const LPCSTR procedureName) {
    if (MemoryPE::IsMemoryModule(module)) {
        return MemoryPE::GetProcAddress(module, procedureName);
    }
    return GetProcAddress(module, procedureName);
}

HMODULE WINAPI Hook_LoadLibraryA(const LPCSTR fileName) {
    if (fileName == nullptr) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return nullptr;
    }
    if (VFSRegistry::Exists(fileName)) {
        return MemoryPE::LoadFromVFS(fileName);
    }
    return dbp::dll::LoadApplicationDLLA(fileName);
}

HMODULE WINAPI Hook_LoadLibraryW(const LPCWSTR fileName) {
    if (fileName == nullptr) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return nullptr;
    }
    const auto utf8 =
        TextConvert::UTF16ToUTF8(std::wstring(fileName));
    if (VFSRegistry::Exists(utf8)) {
        return MemoryPE::LoadFromVFS(utf8);
    }
    return dbp::dll::LoadApplicationDLLW(fileName);
}

bool VFSHooks::Initialize() {
    hookActive.store(true, std::memory_order_release);
    return true;
}

void VFSHooks::Shutdown() {
    std::vector<HANDLE> handles;
    {
        std::lock_guard lock(activeStreamsMutex);
        handles.reserve(activeStreams.size());
        for (const auto& entry : activeStreams) {
            handles.push_back(entry.first);
        }
        activeStreams.clear();
    }
    for (const auto handle : handles) {
        CloseHandle(handle);
    }
    hookActive.store(false, std::memory_order_release);
}

bool VFSHooks::IsHookActive() {
    return hookActive.load(std::memory_order_acquire);
}
