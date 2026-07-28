#include "dbp/package/PackageWriter.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <filesystem>
#include <string>

namespace dbp::package {

namespace {

PackageError PublicationError(std::string message) {
    return {
        PackageErrorCode::PublicationFailed,
        std::move(message),
        std::nullopt,
    };
}

class FileHandle {
public:
    explicit FileHandle(const HANDLE value) noexcept
        : value_(value) {}

    ~FileHandle() {
        if (value_ != INVALID_HANDLE_VALUE) {
            CloseHandle(value_);
        }
    }

    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;

    HANDLE get() const noexcept {
        return value_;
    }

private:
    HANDLE value_;
};

} // namespace

PackageResult<bool> Win32AtomicFilePublisher::Publish(
    const std::filesystem::path& temporaryPath,
    const std::filesystem::path& finalPath) const {
    std::error_code existsError;
    if (std::filesystem::exists(finalPath, existsError) ||
        existsError) {
        return PackageResult<bool>::Failure(
            PublicationError(
                "The immutable package destination already exists or "
                "cannot be inspected."));
    }

    {
        FileHandle temporary(CreateFileW(
            temporaryPath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
            nullptr));
        if (temporary.get() == INVALID_HANDLE_VALUE ||
            !FlushFileBuffers(temporary.get())) {
            return PackageResult<bool>::Failure(
                PublicationError(
                    "Flushing the completed package to stable storage failed."));
        }
    }

    if (!MoveFileExW(
            temporaryPath.c_str(),
            finalPath.c_str(),
            MOVEFILE_WRITE_THROUGH)) {
        return PackageResult<bool>::Failure(
            PublicationError(
                "Atomically publishing the immutable package failed."));
    }
    return PackageResult<bool>::Success(true);
}

} // namespace dbp::package
