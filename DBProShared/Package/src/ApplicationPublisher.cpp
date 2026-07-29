#include "dbp/package/ApplicationPublisher.h"
#include "dbp/package/ExecutableKeyResource.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace dbp::package {

namespace {

PackageError PublicationError(std::string message) {
    return {
        PackageErrorCode::PublicationFailed,
        std::move(message),
        std::nullopt,
    };
}

PackageError WithApplicationPublicationContext(
    PackageError error,
    const ApplicationPublicationPhase phase,
    const bool tupleCommitted = false) {
    error.applicationPublicationPhase = phase;
    error.applicationTupleCommitted = tupleCommitted;
    return error;
}

PackageResult<ApplicationPublishResult> FailureAt(
    PackageError error,
    const ApplicationPublicationPhase phase,
    const bool tupleCommitted = false) {
    return PackageResult<ApplicationPublishResult>::Failure(
        WithApplicationPublicationContext(
            std::move(error),
            phase,
            tupleCommitted));
}

PackageResult<ApplicationPublishResult> PublicationFailure(
    std::string message,
    const ApplicationPublicationPhase phase =
        ApplicationPublicationPhase::Executable,
    const bool tupleCommitted = false) {
    return FailureAt(
        {
            PackageErrorCode::PublicationFailed,
            std::move(message),
            std::nullopt,
        },
        phase,
        tupleCommitted);
}

PackageResult<std::filesystem::path> AbsoluteNormalized(
    const std::filesystem::path& path) {
    std::error_code error;
    auto result = std::filesystem::absolute(path, error);
    if (error) {
        return PackageResult<std::filesystem::path>::Failure({
            PackageErrorCode::PublicationFailed,
            "Resolving an application publication path failed.",
            std::nullopt,
        });
    }
    return PackageResult<std::filesystem::path>::Success(
        result.lexically_normal());
}

class FileHandle {
public:
    FileHandle() noexcept = default;

    explicit FileHandle(const HANDLE handle) noexcept
        : handle_(handle) {}

    ~FileHandle() {
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
    }

    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;

    FileHandle(FileHandle&& other) noexcept
        : handle_(std::exchange(
              other.handle_,
              INVALID_HANDLE_VALUE)) {}

    FileHandle& operator=(FileHandle&& other) noexcept {
        if (this != &other) {
            if (handle_ != INVALID_HANDLE_VALUE) {
                CloseHandle(handle_);
            }
            handle_ = std::exchange(
                other.handle_,
                INVALID_HANDLE_VALUE);
        }
        return *this;
    }

    HANDLE get() const noexcept {
        return handle_;
    }

    void Reset() noexcept {
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
        }
    }

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
};

struct FileIdentity {
    DWORD volume = 0;
    DWORD indexHigh = 0;
    DWORD indexLow = 0;

    bool operator==(const FileIdentity& other) const noexcept {
        return volume == other.volume &&
            indexHigh == other.indexHigh &&
            indexLow == other.indexLow;
    }
};

PackageResult<FileIdentity> ReadFileIdentity(
    const FileHandle& handle,
    const char* const message) {
    BY_HANDLE_FILE_INFORMATION information{};
    if (!GetFileInformationByHandle(
            handle.get(),
            &information) ||
        (information.dwFileAttributes &
            (FILE_ATTRIBUTE_DIRECTORY |
             FILE_ATTRIBUTE_REPARSE_POINT)) != 0) {
        return PackageResult<FileIdentity>::Failure(
            PublicationError(message));
    }
    return PackageResult<FileIdentity>::Success({
        information.dwVolumeSerialNumber,
        information.nFileIndexHigh,
        information.nFileIndexLow,
    });
}

class PublicationLock {
public:
    explicit PublicationLock(const HANDLE handle) noexcept
        : handle_(handle) {}

    ~PublicationLock() {
        if (handle_ != nullptr) {
            ReleaseMutex(handle_);
            CloseHandle(handle_);
        }
    }

    PublicationLock(const PublicationLock&) = delete;
    PublicationLock& operator=(const PublicationLock&) = delete;

private:
    HANDLE handle_ = nullptr;
};

class ScopedPathCleanup {
public:
    explicit ScopedPathCleanup(std::filesystem::path path)
        : path_(std::move(path)) {}

    ~ScopedPathCleanup() {
        if (!path_.empty()) {
            std::error_code ignored;
            std::filesystem::remove(path_, ignored);
        }
    }

    ScopedPathCleanup(const ScopedPathCleanup&) = delete;
    ScopedPathCleanup& operator=(const ScopedPathCleanup&) = delete;

    void Release() noexcept {
        path_.clear();
    }

private:
    std::filesystem::path path_;
};

std::string Hex(const std::vector<std::uint8_t>& bytes) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 2U);
    for (const auto byte : bytes) {
        result.push_back(digits[(byte >> 4U) & 0x0FU]);
        result.push_back(digits[byte & 0x0FU]);
    }
    return result;
}

PackageResult<FileHandle> OpenPinnedDirectory(
    const std::filesystem::path& path) {
    FileHandle handle(CreateFileW(
        path.c_str(),
        FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS |
            FILE_FLAG_OPEN_REPARSE_POINT,
        nullptr));
    if (handle.get() == INVALID_HANDLE_VALUE) {
        return PackageResult<FileHandle>::Failure(
            PublicationError(
                "Opening the application output directory failed."));
    }
    BY_HANDLE_FILE_INFORMATION information{};
    if (!GetFileInformationByHandle(
            handle.get(),
            &information) ||
        (information.dwFileAttributes &
            FILE_ATTRIBUTE_DIRECTORY) == 0 ||
        (information.dwFileAttributes &
            FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        return PackageResult<FileHandle>::Failure(
            PublicationError(
                "The application output directory is unsafe."));
    }
    return PackageResult<FileHandle>::Success(
        std::move(handle));
}

PackageResult<FileHandle> OpenPinnedHost(
    const std::filesystem::path& path) {
    FileHandle handle(CreateFileW(
        path.c_str(),
        GENERIC_READ | FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT |
            FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr));
    if (handle.get() == INVALID_HANDLE_VALUE) {
        return PackageResult<FileHandle>::Failure(
            PublicationError(
                "Opening the host executable failed."));
    }
    BY_HANDLE_FILE_INFORMATION information{};
    if (!GetFileInformationByHandle(
            handle.get(),
            &information) ||
        (information.dwFileAttributes &
            FILE_ATTRIBUTE_DIRECTORY) != 0 ||
        (information.dwFileAttributes &
            FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        return PackageResult<FileHandle>::Failure(
            PublicationError(
                "The host executable is unsafe."));
    }
    return PackageResult<FileHandle>::Success(
        std::move(handle));
}

PackageResult<std::wstring> InvariantLowercase(
    const std::wstring& value) {
    const auto required = LCMapStringEx(
        LOCALE_NAME_INVARIANT,
        LCMAP_LOWERCASE,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr,
        0);
    if (required == 0) {
        return PackageResult<std::wstring>::Failure(
            PublicationError(
                "Canonicalizing the publication lock identity failed."));
    }
    std::wstring result(
        static_cast<std::size_t>(required),
        L'\0');
    const auto written = LCMapStringEx(
        LOCALE_NAME_INVARIANT,
        LCMAP_LOWERCASE,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        required,
        nullptr,
        nullptr,
        0);
    if (written != required) {
        return PackageResult<std::wstring>::Failure(
            PublicationError(
                "Canonicalizing the publication lock identity failed."));
    }
    return PackageResult<std::wstring>::Success(
        std::move(result));
}

PackageResult<std::wstring> FinalPathByHandle(
    const FileHandle& handle,
    const char* const message) {
    const auto required = GetFinalPathNameByHandleW(
        handle.get(),
        nullptr,
        0,
        FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (required == 0) {
        return PackageResult<std::wstring>::Failure(
            PublicationError(message));
    }
    std::wstring path(required, L'\0');
    const auto written = GetFinalPathNameByHandleW(
        handle.get(),
        path.data(),
        required,
        FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (written == 0 || written >= required) {
        return PackageResult<std::wstring>::Failure(
            PublicationError(message));
    }
    path.resize(written);
    return PackageResult<std::wstring>::Success(
        std::move(path));
}

PackageResult<bool> ValidateCanonicalWindowsFileName(
    const std::filesystem::path& fileName) {
    const auto value = fileName.native();
    if (value.empty() ||
        value == L"." ||
        value == L".." ||
        value.back() == L'.' ||
        value.back() == L' ' ||
        value.find(L':') != std::wstring::npos ||
        value.find(L'/') != std::wstring::npos ||
        value.find(L'\\') != std::wstring::npos) {
        return PackageResult<bool>::Failure(
            PublicationError(
                "The output executable name is not canonical on Windows."));
    }
    const auto separator = value.find(L'.');
    const auto base = InvariantLowercase(
        value.substr(0, separator));
    if (!base) {
        return PackageResult<bool>::Failure(base.error());
    }
    const auto& name = base.value();
    const auto reserved =
        name == L"con" ||
        name == L"prn" ||
        name == L"aux" ||
        name == L"nul" ||
        (name.size() == 4U &&
         (name.compare(0, 3, L"com") == 0 ||
          name.compare(0, 3, L"lpt") == 0) &&
         name[3] >= L'1' &&
         name[3] <= L'9');
    if (reserved) {
        return PackageResult<bool>::Failure(
            PublicationError(
                "The output executable name is reserved on Windows."));
    }
    return PackageResult<bool>::Success(true);
}

PackageResult<std::wstring> CanonicalOutputFileName(
    const std::filesystem::path& output) {
    FileHandle handle(CreateFileW(
        output.c_str(),
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT,
        nullptr));
    if (handle.get() == INVALID_HANDLE_VALUE) {
        const auto error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND ||
            error == ERROR_PATH_NOT_FOUND) {
            return InvariantLowercase(
                output.filename().native());
        }
        return PackageResult<std::wstring>::Failure(
            PublicationError(
                "Resolving the output executable identity failed."));
    }
    BY_HANDLE_FILE_INFORMATION information{};
    if (!GetFileInformationByHandle(
            handle.get(),
            &information) ||
        (information.dwFileAttributes &
            (FILE_ATTRIBUTE_DIRECTORY |
             FILE_ATTRIBUTE_REPARSE_POINT)) != 0) {
        return PackageResult<std::wstring>::Failure(
            PublicationError(
                "The output executable identity is unsafe."));
    }
    const auto required = GetFinalPathNameByHandleW(
        handle.get(),
        nullptr,
        0,
        FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (required == 0) {
        return PackageResult<std::wstring>::Failure(
            PublicationError(
                "Resolving the output executable identity failed."));
    }
    std::wstring finalPath(required, L'\0');
    const auto written = GetFinalPathNameByHandleW(
        handle.get(),
        finalPath.data(),
        required,
        FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (written == 0 || written >= required) {
        return PackageResult<std::wstring>::Failure(
            PublicationError(
                "Resolving the output executable identity failed."));
    }
    finalPath.resize(written);
    return InvariantLowercase(
        std::filesystem::path(finalPath).filename().native());
}

PackageResult<std::unique_ptr<PublicationLock>>
AcquirePublicationLock(
    const FileHandle& outputDirectory,
    const std::filesystem::path& output,
    const CryptoProvider& crypto) {
    BY_HANDLE_FILE_INFORMATION directory{};
    if (!GetFileInformationByHandle(
            outputDirectory.get(),
            &directory)) {
        return PackageResult<
            std::unique_ptr<PublicationLock>>::Failure(
                PublicationError(
                    "Resolving the output directory identity failed."));
    }
    const auto canonicalFileName =
        CanonicalOutputFileName(output);
    if (!canonicalFileName) {
        return PackageResult<
            std::unique_ptr<PublicationLock>>::Failure(
                canonicalFileName.error());
    }
    std::wstring identity =
        std::to_wstring(directory.dwVolumeSerialNumber) +
        L":" +
        std::to_wstring(directory.nFileIndexHigh) +
        L":" +
        std::to_wstring(directory.nFileIndexLow) +
        L":" +
        canonicalFileName.value();
    const auto* const begin =
        reinterpret_cast<const std::uint8_t*>(
            identity.data());
    std::vector<std::uint8_t> bytes(
        begin,
        begin +
            identity.size() * sizeof(wchar_t));
    const auto digest = crypto.Sha256(bytes);
    if (!digest) {
        return PackageResult<
            std::unique_ptr<PublicationLock>>::Failure(
                digest.error());
    }
    const std::vector<std::uint8_t> digestBytes(
        digest.value().begin(),
        digest.value().end());
    const auto name =
        L"Global\\DBP.ApplicationPublisher." +
        std::filesystem::path(Hex(digestBytes)).wstring();
    const auto mutex = CreateMutexW(
        nullptr,
        FALSE,
        name.c_str());
    if (mutex == nullptr) {
        return PackageResult<
            std::unique_ptr<PublicationLock>>::Failure(
                PublicationError(
                    "Creating the application publication lock failed."));
    }
    const auto wait = WaitForSingleObject(mutex, INFINITE);
    if (wait != WAIT_OBJECT_0 &&
        wait != WAIT_ABANDONED) {
        CloseHandle(mutex);
        return PackageResult<
            std::unique_ptr<PublicationLock>>::Failure(
                PublicationError(
                    "Acquiring the application publication lock failed."));
    }
    return PackageResult<
        std::unique_ptr<PublicationLock>>::Success(
            std::make_unique<PublicationLock>(mutex));
}

PackageResult<FileHandle> CopyPinnedFileToStage(
    const FileHandle& host,
    const std::filesystem::path& stage) {
    LARGE_INTEGER beginning{};
    if (!SetFilePointerEx(
            host.get(),
            beginning,
            nullptr,
            FILE_BEGIN)) {
        return PackageResult<FileHandle>::Failure(
            PublicationError(
                "Seeking the pinned host executable failed."));
    }
    FileHandle output(CreateFileW(
        stage.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL |
            FILE_FLAG_WRITE_THROUGH,
        nullptr));
    if (output.get() == INVALID_HANDLE_VALUE) {
        return PackageResult<FileHandle>::Failure(
            PublicationError(
                "Creating the private executable stage failed."));
    }
    std::vector<std::uint8_t> buffer(1024U * 1024U);
    while (true) {
        DWORD bytesRead = 0;
        if (!ReadFile(
                host.get(),
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                &bytesRead,
                nullptr)) {
            return PackageResult<FileHandle>::Failure(
                PublicationError(
                    "Reading the pinned host executable failed."));
        }
        if (bytesRead == 0) {
            break;
        }
        DWORD offset = 0;
        while (offset < bytesRead) {
            DWORD bytesWritten = 0;
            if (!WriteFile(
                    output.get(),
                    buffer.data() + offset,
                    bytesRead - offset,
                    &bytesWritten,
                    nullptr) ||
                bytesWritten == 0) {
                return PackageResult<FileHandle>::Failure(
                    PublicationError(
                        "Writing the private executable stage failed."));
            }
            offset += bytesWritten;
        }
    }
    if (!FlushFileBuffers(output.get())) {
        return PackageResult<FileHandle>::Failure(
            PublicationError(
                "Flushing the copied host executable failed."));
    }
    const auto identity = ReadFileIdentity(
        output,
        "The copied transaction file is unsafe.");
    if (!identity) {
        return PackageResult<FileHandle>::Failure(
            identity.error());
    }
    output.Reset();
    FileHandle monitor(CreateFileW(
        stage.c_str(),
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT,
        nullptr));
    if (monitor.get() == INVALID_HANDLE_VALUE) {
        return PackageResult<FileHandle>::Failure(
            PublicationError(
                "Monitoring the copied transaction file failed."));
    }
    const auto monitoredIdentity = ReadFileIdentity(
        monitor,
        "The copied transaction file became unsafe.");
    if (!monitoredIdentity ||
        !(monitoredIdentity.value() == identity.value())) {
        return PackageResult<FileHandle>::Failure(
            monitoredIdentity
                ? PublicationError(
                    "The copied transaction file identity changed.")
                : monitoredIdentity.error());
    }
    return PackageResult<FileHandle>::Success(
        std::move(monitor));
}

PackageResult<FileHandle> WriteBytesToStage(
    const std::filesystem::path& stage,
    const std::vector<std::uint8_t>& bytes) {
    FileHandle output(CreateFileW(
        stage.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL |
            FILE_FLAG_WRITE_THROUGH,
        nullptr));
    if (output.get() == INVALID_HANDLE_VALUE) {
        return PackageResult<FileHandle>::Failure(
            PublicationError(
                "Creating the private descriptor stage failed."));
    }
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto remaining = std::min<std::size_t>(
            bytes.size() - offset,
            static_cast<std::size_t>(MAXDWORD));
        DWORD written = 0;
        if (!WriteFile(
                output.get(),
                bytes.data() + offset,
                static_cast<DWORD>(remaining),
                &written,
                nullptr) ||
            written == 0) {
            return PackageResult<FileHandle>::Failure(
                PublicationError(
                    "Writing the private descriptor stage failed."));
        }
        offset += written;
    }
    if (!FlushFileBuffers(output.get())) {
        return PackageResult<FileHandle>::Failure(
            PublicationError(
                "Flushing the private descriptor stage failed."));
    }
    return PackageResult<FileHandle>::Success(
        std::move(output));
}

PackageResult<FileHandle> ReopenForHandleCommit(
    const std::filesystem::path& path,
    const FileIdentity& expectedIdentity) {
    FileHandle handle(CreateFileW(
        path.c_str(),
        DELETE | GENERIC_READ | GENERIC_WRITE |
            FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT |
            FILE_FLAG_WRITE_THROUGH,
        nullptr));
    if (handle.get() == INVALID_HANDLE_VALUE) {
        return PackageResult<FileHandle>::Failure(
            PublicationError(
                "Pinning a private transaction file for commit failed."));
    }
    const auto identity = ReadFileIdentity(
        handle,
        "A private transaction file became unsafe.");
    if (!identity) {
        return PackageResult<FileHandle>::Failure(
            identity.error());
    }
    if (!(identity.value() == expectedIdentity)) {
        return PackageResult<FileHandle>::Failure(
            PublicationError(
                "A private transaction file identity changed."));
    }
    if (!FlushFileBuffers(handle.get())) {
        return PackageResult<FileHandle>::Failure(
            PublicationError(
                "Flushing a private transaction file failed."));
    }
    return PackageResult<FileHandle>::Success(
        std::move(handle));
}

PackageResult<FileHandle> OpenForHandleCommit(
    const std::filesystem::path& path) {
    FileHandle handle(CreateFileW(
        path.c_str(),
        DELETE | GENERIC_READ | GENERIC_WRITE |
            FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT |
            FILE_FLAG_WRITE_THROUGH,
        nullptr));
    if (handle.get() == INVALID_HANDLE_VALUE) {
        return PackageResult<FileHandle>::Failure(
            PublicationError(
                "Pinning a private transaction file for commit failed."));
    }
    const auto identity = ReadFileIdentity(
        handle,
        "A private transaction file became unsafe.");
    if (!identity) {
        return PackageResult<FileHandle>::Failure(
            identity.error());
    }
    if (!FlushFileBuffers(handle.get())) {
        return PackageResult<FileHandle>::Failure(
            PublicationError(
                "Flushing a private transaction file failed."));
    }
    return PackageResult<FileHandle>::Success(
        std::move(handle));
}

PackageResult<bool> VerifyPathIdentity(
    const std::filesystem::path& path,
    const FileIdentity& expectedIdentity) {
    FileHandle handle(CreateFileW(
        path.c_str(),
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT,
        nullptr));
    if (handle.get() == INVALID_HANDLE_VALUE) {
        return PackageResult<bool>::Failure(
            PublicationError(
                "Reopening a private transaction file failed."));
    }
    const auto identity = ReadFileIdentity(
        handle,
        "A private transaction file became unsafe.");
    if (!identity) {
        return PackageResult<bool>::Failure(
            identity.error());
    }
    if (!(identity.value() == expectedIdentity)) {
        return PackageResult<bool>::Failure(
            PublicationError(
                "A private transaction file identity changed."));
    }
    return PackageResult<bool>::Success(true);
}

PackageResult<std::optional<FileHandle>> OpenPinnedExistingFile(
    const std::filesystem::path& path) {
    FileHandle handle(CreateFileW(
        path.c_str(),
        GENERIC_READ | FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT |
            FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr));
    if (handle.get() == INVALID_HANDLE_VALUE) {
        const auto error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND ||
            error == ERROR_PATH_NOT_FOUND) {
            return PackageResult<
                std::optional<FileHandle>>::Success(
                    std::nullopt);
        }
        return PackageResult<
            std::optional<FileHandle>>::Failure(
                PublicationError(
                    "Pinning an existing publication file failed."));
    }
    const auto identity = ReadFileIdentity(
        handle,
        "An existing publication file is unsafe.");
    if (!identity) {
        return PackageResult<
            std::optional<FileHandle>>::Failure(
                identity.error());
    }
    std::optional<FileHandle> result;
    result.emplace(std::move(handle));
    return PackageResult<
        std::optional<FileHandle>>::Success(
            std::move(result));
}

PackageResult<FileHandle> UpgradeToCommitHandle(
    FileHandle& protectiveHandle,
    const std::filesystem::path& path) {
    const auto identity = ReadFileIdentity(
        protectiveHandle,
        "A private transaction file is unsafe.");
    if (!identity) {
        return PackageResult<FileHandle>::Failure(
            identity.error());
    }
    protectiveHandle.Reset();
    return ReopenForHandleCommit(
        path,
        identity.value());
}

PackageResult<FileHandle> CopyToPinnedBackup(
    const FileHandle& source,
    const std::filesystem::path& backupPath) {
    auto protective =
        CopyPinnedFileToStage(source, backupPath);
    if (!protective) {
        return PackageResult<FileHandle>::Failure(
            protective.error());
    }
    return UpgradeToCommitHandle(
        protective.value(),
        backupPath);
}

PackageResult<bool> RenameByHandle(
    const FileHandle& source,
    const FileHandle& outputDirectory,
    const std::filesystem::path& destinationFileName,
    const bool replaceExisting) {
    const auto directoryPath = FinalPathByHandle(
        outputDirectory,
        "Resolving the pinned output directory failed.");
    if (!directoryPath) {
        return PackageResult<bool>::Failure(
            directoryPath.error());
    }
    auto fileName = directoryPath.value();
    if (fileName.rfind(L"\\\\?\\", 0) == 0) {
        fileName.replace(0, 4, L"\\??\\");
    }
    if (!fileName.empty() &&
        fileName.back() != L'\\') {
        fileName.push_back(L'\\');
    }
    fileName += destinationFileName.native();
    const auto fileNameBytes =
        fileName.size() * sizeof(wchar_t);
    const auto bufferSize =
        offsetof(FILE_RENAME_INFO, FileName) +
        fileNameBytes +
        sizeof(wchar_t);
    std::vector<std::uint8_t> storage(bufferSize, 0);
    auto* const rename =
        reinterpret_cast<FILE_RENAME_INFO*>(
            storage.data());
    rename->ReplaceIfExists =
        replaceExisting ? TRUE : FALSE;
    rename->RootDirectory = nullptr;
    rename->FileNameLength =
        static_cast<DWORD>(fileNameBytes);
    std::memcpy(
        rename->FileName,
        fileName.data(),
        fileNameBytes);
    if (!SetFileInformationByHandle(
            source.get(),
            FileRenameInfo,
            rename,
            static_cast<DWORD>(storage.size()))) {
        const auto error = GetLastError();
        return PackageResult<bool>::Failure(
            PublicationError(
                "Atomically committing a transaction file failed "
                "(Win32 " + std::to_string(error) + ")."));
    }
    return PackageResult<bool>::Success(true);
}

PackageResult<bool> DeleteByHandle(
    const FileHandle& file) {
    FILE_DISPOSITION_INFO disposition{};
    disposition.DeleteFile = TRUE;
    if (!SetFileInformationByHandle(
            file.get(),
            FileDispositionInfo,
            &disposition,
            sizeof(disposition))) {
        return PackageResult<bool>::Failure(
            PublicationError(
                "Deleting an interrupted transaction file failed."));
    }
    return PackageResult<bool>::Success(true);
}

bool PathsEqualOrdinalIgnoreCase(
    const std::filesystem::path& left,
    const std::filesystem::path& right) {
    const auto leftString = left.native();
    const auto rightString = right.native();
    if (leftString.size() >
            static_cast<std::size_t>(MAXLONG) ||
        rightString.size() >
            static_cast<std::size_t>(MAXLONG)) {
        return false;
    }
    return CompareStringOrdinal(
               leftString.data(),
               static_cast<int>(leftString.size()),
               rightString.data(),
               static_cast<int>(rightString.size()),
               TRUE) == CSTR_EQUAL;
}

PackageResult<bool> IsSameExistingFile(
    const std::filesystem::path& left,
    const std::filesystem::path& right) {
    FileHandle leftHandle(CreateFileW(
        left.c_str(),
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr));
    if (leftHandle.get() == INVALID_HANDLE_VALUE) {
        return PackageResult<bool>::Failure(PublicationError(
            "Opening the host executable for identity validation failed."));
    }
    FileHandle rightHandle(CreateFileW(
        right.c_str(),
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr));
    if (rightHandle.get() == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND ||
            GetLastError() == ERROR_PATH_NOT_FOUND) {
            return PackageResult<bool>::Success(false);
        }
        return PackageResult<bool>::Failure(PublicationError(
            "Opening the output executable for identity validation failed."));
    }

    BY_HANDLE_FILE_INFORMATION leftInfo{};
    BY_HANDLE_FILE_INFORMATION rightInfo{};
    if (!GetFileInformationByHandle(leftHandle.get(), &leftInfo) ||
        !GetFileInformationByHandle(rightHandle.get(), &rightInfo)) {
        return PackageResult<bool>::Failure(PublicationError(
            "Reading executable file identity failed."));
    }
    return PackageResult<bool>::Success(
        leftInfo.dwVolumeSerialNumber ==
            rightInfo.dwVolumeSerialNumber &&
        leftInfo.nFileIndexHigh == rightInfo.nFileIndexHigh &&
        leftInfo.nFileIndexLow == rightInfo.nFileIndexLow);
}

PackageResult<bool> ValidateRegularNonReparseFile(
    const std::filesystem::path& path,
    const char* const missingMessage) {
    const auto attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        return PackageResult<bool>::Failure(
            PublicationError(missingMessage));
    }
    return PackageResult<bool>::Success(true);
}

PackageResult<bool> ValidateOutputDirectory(
    const std::filesystem::path& path) {
    const auto attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        return PackageResult<bool>::Failure(PublicationError(
            "The application output directory must be an existing "
            "non-reparse directory."));
    }
    return PackageResult<bool>::Success(true);
}

PackageResult<bool> ValidateDestinationIfPresent(
    const std::filesystem::path& path,
    const char* const message) {
    const auto attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const auto error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND ||
            error == ERROR_PATH_NOT_FOUND) {
            return PackageResult<bool>::Success(false);
        }
        return PackageResult<bool>::Failure(
            PublicationError(message));
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        return PackageResult<bool>::Failure(
            PublicationError(message));
    }
    return PackageResult<bool>::Success(true);
}

PackageResult<std::filesystem::path> UniqueSiblingPath(
    const std::filesystem::path& output,
    const char* const label,
    const CryptoProvider& crypto) {
    for (std::size_t attempt = 0; attempt < 8U; ++attempt) {
        const auto random = crypto.RandomBytes(16U);
        if (!random) {
            return PackageResult<std::filesystem::path>::Failure(
                random.error());
        }
        const auto candidate =
            output.parent_path() /
            ("." + output.filename().string() + label +
             Hex(random.value()));
        std::error_code existsError;
        const auto exists =
            std::filesystem::exists(candidate, existsError);
        if (!exists && !existsError) {
            return PackageResult<std::filesystem::path>::Success(
                candidate);
        }
    }
    return PackageResult<std::filesystem::path>::Failure(
        PublicationError(
            "Allocating a private application transaction path failed."));
}

PackageResult<bool> ReachCheckpoint(
    const PublicationCheckpoint& checkpoint,
    const PublicationStage stage) {
    const auto reached = checkpoint.Reach(stage);
    if (!reached) {
        return reached;
    }
    if (!reached.value()) {
        return PackageResult<bool>::Failure(PublicationError(
            "An application publication checkpoint was cancelled."));
    }
    return PackageResult<bool>::Success(true);
}

PackageResult<bool> RestoreCommittedFile(
    FileHandle& published,
    std::optional<FileHandle>& backup,
    const bool hadPrevious,
    const FileHandle& outputDirectory,
    const std::filesystem::path& destinationFileName) {
    if (!hadPrevious) {
        const auto removed = DeleteByHandle(published);
        published.Reset();
        return removed;
    }
    published.Reset();
    if (!backup) {
        return PackageResult<bool>::Failure(
            PublicationError(
                "A required transaction backup is unavailable."));
    }
    const auto restored = RenameByHandle(
        *backup,
        outputDirectory,
        destinationFileName,
        true);
    if (restored) {
        backup->Reset();
    }
    return restored;
}

PackageResult<ApplicationPublishResult> RollbackHandleFailure(
    const PackageError& cause,
    const ApplicationPublicationPhase phase,
    const FileHandle& outputDirectory,
    const std::filesystem::path& executableFileName,
    FileHandle& publishedExecutable,
    std::optional<FileHandle>& executableBackup,
    const bool hadPreviousExecutable,
    const std::filesystem::path& descriptorFileName,
    std::optional<FileHandle>& publishedDescriptor,
    std::optional<FileHandle>& descriptorBackup,
    const bool descriptorWasPublished,
    const bool hadPreviousDescriptor,
    ScopedPathCleanup& executableBackupCleanup,
    ScopedPathCleanup& descriptorBackupCleanup) {
    std::optional<PackageError> descriptorRestoreError;
    if (descriptorWasPublished) {
        const auto restoredDescriptor = RestoreCommittedFile(
            *publishedDescriptor,
            descriptorBackup,
            hadPreviousDescriptor,
            outputDirectory,
            descriptorFileName);
        if (!restoredDescriptor) {
            descriptorRestoreError =
                restoredDescriptor.error();
            if (hadPreviousDescriptor) {
                descriptorBackupCleanup.Release();
            }
        }
    }
    const auto restoredExecutable = RestoreCommittedFile(
        publishedExecutable,
        executableBackup,
        hadPreviousExecutable,
        outputDirectory,
        executableFileName);
    std::optional<PackageError> executableRestoreError;
    if (!restoredExecutable) {
        executableRestoreError =
            restoredExecutable.error();
        if (hadPreviousExecutable) {
            executableBackupCleanup.Release();
        }
    }
    if (descriptorRestoreError ||
        executableRestoreError) {
        auto message = cause.message;
        if (descriptorRestoreError) {
            message += " " +
                descriptorRestoreError->message;
        }
        if (executableRestoreError) {
            message += " " +
                executableRestoreError->message;
        }
        return PublicationFailure(
            std::move(message),
            phase);
    }
    return FailureAt(cause, phase);
}

PackageResult<bool> RemoveBackupByHandle(
    std::optional<FileHandle>& backup) {
    if (!backup) {
        return PackageResult<bool>::Success(true);
    }
    const auto deleted = DeleteByHandle(*backup);
    if (deleted) {
        backup->Reset();
    }
    return deleted;
}

} // namespace

ApplicationPublisher::ApplicationPublisher(
    const CryptoProvider& crypto,
    const ZstdCompressionCodec& compression,
    const AtomicFilePublisher& filePublisher,
    const PublicationCheckpoint& checkpoint) noexcept
    : crypto_(crypto),
      compression_(compression),
      filePublisher_(filePublisher),
      checkpoint_(checkpoint) {}

PackageResult<ApplicationPublishResult>
ApplicationPublisher::Publish(
    const ApplicationPublishRequest& request,
    const KeyProvider& keys) const {
    const auto canonicalFileName =
        ValidateCanonicalWindowsFileName(
            request.outputExecutable.filename());
    if (!canonicalFileName) {
        return FailureAt(
            canonicalFileName.error(),
            ApplicationPublicationPhase::Executable);
    }
    const auto host = AbsoluteNormalized(request.hostExecutable);
    const auto output = AbsoluteNormalized(request.outputExecutable);
    if (!host || !output) {
        return PublicationFailure(
            "Resolving application publication paths failed.");
    }
    if (host.value().filename().empty() ||
        output.value().filename().empty()) {
        return PublicationFailure(
            "Application publication paths must name files.");
    }
    if (request.mode != RuntimeMode::Application &&
        request.mode != RuntimeMode::Installer) {
        return PublicationFailure(
            "The application runtime mode is invalid.");
    }
    if (PathsEqualOrdinalIgnoreCase(
            host.value(),
            output.value())) {
        return PublicationFailure(
            "The host executable and output executable must differ.");
    }
    const auto hostValid = ValidateRegularNonReparseFile(
        host.value(),
        "The host executable must be an existing non-reparse file.");
    if (!hostValid) {
        return FailureAt(
            hostValid.error(),
            ApplicationPublicationPhase::Executable);
    }
    const auto outputDirectory =
        output.value().parent_path();
    auto descriptorPath = output.value();
    descriptorPath.replace_extension(L".dbpakref");
    if (PathsEqualOrdinalIgnoreCase(
            output.value(),
            descriptorPath) ||
        PathsEqualOrdinalIgnoreCase(
            host.value(),
            descriptorPath)) {
        return PublicationFailure(
            "Application publication paths must not alias.");
    }
    const auto directoryValid =
        ValidateOutputDirectory(outputDirectory);
    if (!directoryValid) {
        return FailureAt(
            directoryValid.error(),
            ApplicationPublicationPhase::Executable);
    }
    auto outputDirectoryHandle =
        OpenPinnedDirectory(outputDirectory);
    if (!outputDirectoryHandle) {
        return FailureAt(
            outputDirectoryHandle.error(),
            ApplicationPublicationPhase::Executable);
    }
    auto publicationLock = AcquirePublicationLock(
        outputDirectoryHandle.value(),
        output.value(),
        crypto_);
    if (!publicationLock) {
        return FailureAt(
            publicationLock.error(),
            ApplicationPublicationPhase::Executable);
    }
    auto hostHandle = OpenPinnedHost(host.value());
    if (!hostHandle) {
        return FailureAt(
            hostHandle.error(),
            ApplicationPublicationPhase::Executable);
    }
    const auto outputPresent = ValidateDestinationIfPresent(
        output.value(),
        "The output executable destination is unsafe.");
    if (!outputPresent) {
        return FailureAt(
            outputPresent.error(),
            ApplicationPublicationPhase::Executable);
    }
    const auto descriptorPresent = ValidateDestinationIfPresent(
        descriptorPath,
        "The runtime descriptor destination is unsafe.");
    if (!descriptorPresent) {
        return FailureAt(
            descriptorPresent.error(),
            ApplicationPublicationPhase::Executable);
    }
    const auto sameFile =
        IsSameExistingFile(host.value(), output.value());
    if (!sameFile) {
        return FailureAt(
            sameFile.error(),
            ApplicationPublicationPhase::Executable);
    }
    if (sameFile.value()) {
        return PublicationFailure(
            "The host executable and output executable must differ.");
    }
    const auto hostDescriptorAlias =
        IsSameExistingFile(host.value(), descriptorPath);
    if (!hostDescriptorAlias) {
        return FailureAt(
            hostDescriptorAlias.error(),
            ApplicationPublicationPhase::Executable);
    }
    if (hostDescriptorAlias.value()) {
        return PublicationFailure(
            "Application publication paths must not alias.");
    }
    if (outputPresent.value() && descriptorPresent.value()) {
        const auto outputDescriptorAlias =
            IsSameExistingFile(output.value(), descriptorPath);
        if (!outputDescriptorAlias) {
            return FailureAt(
                outputDescriptorAlias.error(),
                ApplicationPublicationPhase::Executable);
        }
        if (outputDescriptorAlias.value()) {
            return PublicationFailure(
                "Application publication paths must not alias.");
        }
    }

    const auto masterKey = keys.Resolve(request.keyId);
    if (!masterKey) {
        return FailureAt(
            masterKey.error(),
            ApplicationPublicationPhase::Package);
    }
    MemoryKeyProvider packageKeys(
        request.keyId,
        SecureBuffer::FromBytes(
            masterKey.value().CopyBytes()));

    const auto stagePath = UniqueSiblingPath(
        output.value(),
        ".dbp-stage-",
        crypto_);
    const auto executableBackupPath = UniqueSiblingPath(
        output.value(),
        ".dbp-backup-",
        crypto_);
    const auto descriptorBackupPath = UniqueSiblingPath(
        descriptorPath,
        ".dbp-backup-",
        crypto_);
    const auto descriptorStagePath = UniqueSiblingPath(
        descriptorPath,
        ".dbp-stage-",
        crypto_);
    if (!stagePath ||
        !executableBackupPath ||
        !descriptorBackupPath ||
        !descriptorStagePath) {
        return PublicationFailure(
            "Allocating private application transaction paths failed.");
    }
    ScopedPathCleanup stageCleanup(stagePath.value());
    ScopedPathCleanup executableBackupCleanup(
        executableBackupPath.value());
    ScopedPathCleanup descriptorBackupCleanup(
        descriptorBackupPath.value());
    ScopedPathCleanup descriptorStageCleanup(
        descriptorStagePath.value());

    auto copiedHost = CopyPinnedFileToStage(
        hostHandle.value(),
        stagePath.value());
    if (!copiedHost) {
        return FailureAt(
            copiedHost.error(),
            ApplicationPublicationPhase::Executable);
    }
    const auto stagedValid = ValidateRegularNonReparseFile(
        stagePath.value(),
        "The staged host executable is not a regular file.");
    if (!stagedValid) {
        return FailureAt(
            stagedValid.error(),
            ApplicationPublicationPhase::Executable);
    }
    const auto originalStageIdentity = ReadFileIdentity(
        copiedHost.value(),
        "The staged executable is unsafe.");
    if (!originalStageIdentity) {
        return FailureAt(
            originalStageIdentity.error(),
            ApplicationPublicationPhase::Executable);
    }

    PackageWriter writer(
        crypto_,
        compression_,
        filePublisher_);
    const auto package = writer.Write(
        {
            outputDirectory,
            request.keyId,
            request.entries,
            request.limits,
        },
        packageKeys);
    if (!package) {
        return FailureAt(
            package.error(),
            ApplicationPublicationPhase::Package);
    }
    const auto packageCheckpoint = ReachCheckpoint(
        checkpoint_,
        PublicationStage::PackagePublished);
    if (!packageCheckpoint) {
        return FailureAt(
            packageCheckpoint.error(),
            ApplicationPublicationPhase::Package);
    }
    const auto stageUnchanged = VerifyPathIdentity(
        stagePath.value(),
        originalStageIdentity.value());
    if (!stageUnchanged) {
        return FailureAt(
            stageUnchanged.error(),
            ApplicationPublicationPhase::Executable);
    }

    auto previousExecutableResult =
        OpenPinnedExistingFile(output.value());
    auto previousDescriptorResult =
        OpenPinnedExistingFile(descriptorPath);
    if (!previousExecutableResult ||
        !previousDescriptorResult) {
        return FailureAt(
            !previousExecutableResult
                ? previousExecutableResult.error()
                : previousDescriptorResult.error(),
            ApplicationPublicationPhase::Executable);
    }
    auto previousExecutable =
        std::move(previousExecutableResult.value());
    auto previousDescriptor =
        std::move(previousDescriptorResult.value());
    const auto hadPreviousExecutable =
        previousExecutable.has_value();
    const auto hadPreviousDescriptor =
        previousDescriptor.has_value();
    if (hadPreviousExecutable != outputPresent.value() ||
        hadPreviousDescriptor != descriptorPresent.value()) {
        return PublicationFailure(
            "A publication destination changed during packaging.");
    }

    std::optional<ExecutablePackageKey> fallbackKey;
    if (hadPreviousExecutable && hadPreviousDescriptor) {
        const auto previousMetadata =
            ReadRuntimeDescriptor(descriptorPath);
        if (!previousMetadata) {
            return FailureAt(
                previousMetadata.error(),
                ApplicationPublicationPhase::Executable);
        }
        auto previousKey = ReadExecutablePackageKey(
            output.value(),
            previousMetadata.value().keyId);
        if (!previousKey) {
            return FailureAt(
                previousKey.error(),
                ApplicationPublicationPhase::Executable);
        }
        fallbackKey.emplace(std::move(previousKey.value()));
    }

    std::optional<FileHandle> executableBackup;
    if (hadPreviousExecutable) {
        auto backup = CopyToPinnedBackup(
            *previousExecutable,
            executableBackupPath.value());
        if (!backup) {
            return FailureAt(
                backup.error(),
                ApplicationPublicationPhase::Executable);
        }
        executableBackup.emplace(
            std::move(backup.value()));
    }
    std::optional<FileHandle> descriptorBackup;
    if (hadPreviousDescriptor) {
        auto backup = CopyToPinnedBackup(
            *previousDescriptor,
            descriptorBackupPath.value());
        if (!backup) {
            return FailureAt(
                backup.error(),
                ApplicationPublicationPhase::Executable);
        }
        descriptorBackup.emplace(
            std::move(backup.value()));
    }

    copiedHost.value().Reset();
    const auto injected = InjectExecutablePackageKeys(
        stagePath.value(),
        request.keyId,
        masterKey.value(),
        fallbackKey ? &*fallbackKey : nullptr);
    if (!injected) {
        return FailureAt(
            injected.error(),
            ApplicationPublicationPhase::Executable);
    }
    auto executableCommitHandle = OpenForHandleCommit(
        stagePath.value());
    if (!executableCommitHandle) {
        return FailureAt(
            executableCommitHandle.error(),
            ApplicationPublicationPhase::Executable);
    }

    if (previousExecutable) {
        previousExecutable->Reset();
    }
    if (previousDescriptor) {
        previousDescriptor->Reset();
    }
    const auto executablePublished = RenameByHandle(
        executableCommitHandle.value(),
        outputDirectoryHandle.value(),
        output.value().filename(),
        hadPreviousExecutable);
    if (!executablePublished) {
        return FailureAt(
            executablePublished.error(),
            ApplicationPublicationPhase::Executable);
    }
    stageCleanup.Release();
    FileHandle publishedExecutable(
        std::move(executableCommitHandle.value()));
    std::optional<FileHandle> publishedDescriptor;

    const auto executableCheckpoint = ReachCheckpoint(
        checkpoint_,
        PublicationStage::ExecutablePublished);
    if (!executableCheckpoint) {
        return RollbackHandleFailure(
            executableCheckpoint.error(),
            ApplicationPublicationPhase::Executable,
            outputDirectoryHandle.value(),
            output.value().filename(),
            publishedExecutable,
            executableBackup,
            hadPreviousExecutable,
            descriptorPath.filename(),
            publishedDescriptor,
            descriptorBackup,
            false,
            hadPreviousDescriptor,
            executableBackupCleanup,
            descriptorBackupCleanup);
    }

    RuntimeDescriptor descriptor;
    descriptor.mode = request.mode;
    descriptor.packageId = package.value().packageId;
    descriptor.keyId = request.keyId;
    descriptor.packageFileName =
        package.value().packagePath.filename().string();
    const auto serializedDescriptor =
        SerializeRuntimeDescriptor(descriptor);
    if (!serializedDescriptor) {
        return RollbackHandleFailure(
            serializedDescriptor.error(),
            ApplicationPublicationPhase::Descriptor,
            outputDirectoryHandle.value(),
            output.value().filename(),
            publishedExecutable,
            executableBackup,
            hadPreviousExecutable,
            descriptorPath.filename(),
            publishedDescriptor,
            descriptorBackup,
            false,
            hadPreviousDescriptor,
            executableBackupCleanup,
            descriptorBackupCleanup);
    }
    auto descriptorProtectiveHandle = WriteBytesToStage(
        descriptorStagePath.value(),
        serializedDescriptor.value());
    if (!descriptorProtectiveHandle) {
        return RollbackHandleFailure(
            descriptorProtectiveHandle.error(),
            ApplicationPublicationPhase::Descriptor,
            outputDirectoryHandle.value(),
            output.value().filename(),
            publishedExecutable,
            executableBackup,
            hadPreviousExecutable,
            descriptorPath.filename(),
            publishedDescriptor,
            descriptorBackup,
            false,
            hadPreviousDescriptor,
            executableBackupCleanup,
            descriptorBackupCleanup);
    }
    auto descriptorCommitHandle = UpgradeToCommitHandle(
        descriptorProtectiveHandle.value(),
        descriptorStagePath.value());
    if (!descriptorCommitHandle) {
        return RollbackHandleFailure(
            descriptorCommitHandle.error(),
            ApplicationPublicationPhase::Descriptor,
            outputDirectoryHandle.value(),
            output.value().filename(),
            publishedExecutable,
            executableBackup,
            hadPreviousExecutable,
            descriptorPath.filename(),
            publishedDescriptor,
            descriptorBackup,
            false,
            hadPreviousDescriptor,
            executableBackupCleanup,
            descriptorBackupCleanup);
    }
    const auto descriptorPublished = RenameByHandle(
        descriptorCommitHandle.value(),
        outputDirectoryHandle.value(),
        descriptorPath.filename(),
        hadPreviousDescriptor);
    if (!descriptorPublished) {
        return RollbackHandleFailure(
            descriptorPublished.error(),
            ApplicationPublicationPhase::Descriptor,
            outputDirectoryHandle.value(),
            output.value().filename(),
            publishedExecutable,
            executableBackup,
            hadPreviousExecutable,
            descriptorPath.filename(),
            publishedDescriptor,
            descriptorBackup,
            false,
            hadPreviousDescriptor,
            executableBackupCleanup,
            descriptorBackupCleanup);
    }
    descriptorStageCleanup.Release();
    publishedDescriptor.emplace(
        std::move(descriptorCommitHandle.value()));

    const auto descriptorCheckpoint = ReachCheckpoint(
        checkpoint_,
        PublicationStage::DescriptorPublished);
    if (!descriptorCheckpoint) {
        return RollbackHandleFailure(
            descriptorCheckpoint.error(),
            ApplicationPublicationPhase::Descriptor,
            outputDirectoryHandle.value(),
            output.value().filename(),
            publishedExecutable,
            executableBackup,
            hadPreviousExecutable,
            descriptorPath.filename(),
            publishedDescriptor,
            descriptorBackup,
            true,
            hadPreviousDescriptor,
            executableBackupCleanup,
            descriptorBackupCleanup);
    }

    const auto cleanupCheckpoint = ReachCheckpoint(
        checkpoint_,
        PublicationStage::CleanupStarted);
    if (!cleanupCheckpoint) {
        executableBackupCleanup.Release();
        descriptorBackupCleanup.Release();
        return FailureAt(
            cleanupCheckpoint.error(),
            ApplicationPublicationPhase::Cleanup,
            true);
    }

    const auto executableBackupRemoved =
        RemoveBackupByHandle(executableBackup);
    const auto descriptorBackupRemoved =
        RemoveBackupByHandle(descriptorBackup);
    if (!executableBackupRemoved ||
        !descriptorBackupRemoved) {
        if (!executableBackupRemoved &&
            executableBackup) {
            executableBackupCleanup.Release();
        }
        if (!descriptorBackupRemoved &&
            descriptorBackup) {
            descriptorBackupCleanup.Release();
        }
        return PublicationFailure(
            !executableBackupRemoved
                ? executableBackupRemoved.error().message
                : descriptorBackupRemoved.error().message,
            ApplicationPublicationPhase::Cleanup,
            true);
    }
    executableBackupCleanup.Release();
    descriptorBackupCleanup.Release();
    return PackageResult<ApplicationPublishResult>::Success({
        output.value(),
        descriptorPath,
        package.value(),
    });
}

} // namespace dbp::package
