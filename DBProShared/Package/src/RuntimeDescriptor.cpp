#include "dbp/package/RuntimeDescriptor.h"

#include "dbp/package/ByteCodec.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace dbp::package {

namespace {

constexpr std::array<std::uint8_t, 8> descriptorMagic{
    'D', 'B', 'P', 'R', 'E', 'F', '2', 0,
};
constexpr std::uint16_t descriptorMajorVersion = 2;
constexpr std::uint16_t descriptorMinorVersion = 0;
constexpr std::size_t descriptorFileNameFieldSize = 44U;

PackageError MetadataError(
    const PackageErrorCode code,
    std::string message) {
    return {
        code,
        std::move(message),
        std::nullopt,
    };
}

template <typename T>
PackageResult<T> Failure(
    const PackageErrorCode code,
    std::string message) {
    return PackageResult<T>::Failure(
        MetadataError(code, std::move(message)));
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

class TemporaryPath {
public:
    explicit TemporaryPath(std::filesystem::path path)
        : path_(std::move(path)) {}

    ~TemporaryPath() {
        if (!published_) {
            std::error_code ignored;
            std::filesystem::remove(path_, ignored);
        }
    }

    TemporaryPath(const TemporaryPath&) = delete;
    TemporaryPath& operator=(const TemporaryPath&) = delete;

    const std::filesystem::path& path() const noexcept {
        return path_;
    }

    void MarkPublished() noexcept {
        published_ = true;
    }

private:
    std::filesystem::path path_;
    bool published_ = false;
};

PackageResult<std::filesystem::path> CreateTemporaryDescriptor(
    const std::filesystem::path& descriptorPath,
    const std::vector<std::uint8_t>& bytes) {
    static std::atomic<std::uint64_t> sequence{0};
    for (std::size_t attempt = 0; attempt < 32U; ++attempt) {
        const auto suffix =
            (static_cast<std::uint64_t>(GetCurrentProcessId()) << 32U) ^
            sequence.fetch_add(1, std::memory_order_relaxed);
        const auto temporaryPath =
            descriptorPath.parent_path() /
            (L"." + descriptorPath.filename().wstring() +
             L".tmp-" + std::to_wstring(suffix));
        bool writeFailed = false;
        {
            FileHandle output(CreateFileW(
                temporaryPath.c_str(),
                GENERIC_WRITE,
                0,
                nullptr,
                CREATE_NEW,
                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
                nullptr));
            if (output.get() == INVALID_HANDLE_VALUE) {
                if (GetLastError() == ERROR_FILE_EXISTS) {
                    continue;
                }
                return Failure<std::filesystem::path>(
                    PackageErrorCode::IoFailed,
                    "Creating the private runtime descriptor failed.");
            }

            DWORD written = 0;
            writeFailed =
                !WriteFile(
                    output.get(),
                    bytes.data(),
                    static_cast<DWORD>(bytes.size()),
                    &written,
                    nullptr) ||
                written != bytes.size() ||
                !FlushFileBuffers(output.get());
        }
        if (writeFailed) {
            std::error_code ignored;
            std::filesystem::remove(temporaryPath, ignored);
            return Failure<std::filesystem::path>(
                PackageErrorCode::IoFailed,
                "Writing the runtime descriptor failed.");
        }
        return PackageResult<std::filesystem::path>::Success(
            temporaryPath);
    }
    return Failure<std::filesystem::path>(
        PackageErrorCode::IoFailed,
        "Creating a unique runtime descriptor staging file failed.");
}

bool IsKnownMode(const RuntimeMode mode) noexcept {
    return mode == RuntimeMode::Application ||
        mode == RuntimeMode::Installer;
}

} // namespace

std::string ExpectedPackageFileName(const PackageId& packageId) {
    std::ostringstream output;
    output << "data-" << std::hex << std::setfill('0');
    for (const auto byte : packageId) {
        output << std::setw(2) << static_cast<unsigned>(byte);
    }
    output << ".dbpak";
    return output.str();
}

PackageResult<std::vector<std::uint8_t>> SerializeRuntimeDescriptor(
    const RuntimeDescriptor& descriptor) {
    if (!IsKnownMode(descriptor.mode)) {
        return Failure<std::vector<std::uint8_t>>(
            PackageErrorCode::InvalidFormat,
            "The runtime descriptor mode is unknown.");
    }
    const auto expectedName =
        ExpectedPackageFileName(descriptor.packageId);
    if (descriptor.packageFileName != expectedName) {
        return Failure<std::vector<std::uint8_t>>(
            PackageErrorCode::InvalidFormat,
            "The runtime descriptor package filename does not match "
            "its package identifier.");
    }

    ByteWriter writer;
    writer.WriteBytes(descriptorMagic.data(), descriptorMagic.size());
    writer.WriteUInt16(descriptorMajorVersion);
    writer.WriteUInt16(descriptorMinorVersion);
    writer.WriteUInt32(static_cast<std::uint32_t>(descriptor.mode));
    writer.WriteBytes(
        descriptor.packageId.data(),
        descriptor.packageId.size());
    writer.WriteBytes(descriptor.keyId.data(), descriptor.keyId.size());
    writer.WriteUInt32(
        static_cast<std::uint32_t>(descriptor.packageFileName.size()));
    writer.WriteBytes(
        reinterpret_cast<const std::uint8_t*>(
            descriptor.packageFileName.data()),
        descriptor.packageFileName.size());
    const std::array<std::uint8_t, descriptorFileNameFieldSize> zeros{};
    writer.WriteBytes(
        zeros.data(),
        descriptorFileNameFieldSize - descriptor.packageFileName.size());
    if (writer.Bytes().size() != kRuntimeDescriptorSize) {
        return Failure<std::vector<std::uint8_t>>(
            PackageErrorCode::InvalidFormat,
            "The runtime descriptor has a non-canonical size.");
    }
    return PackageResult<std::vector<std::uint8_t>>::Success(
        writer.Bytes());
}

PackageResult<RuntimeDescriptor> ParseRuntimeDescriptor(
    const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() != kRuntimeDescriptorSize) {
        return Failure<RuntimeDescriptor>(
            PackageErrorCode::InvalidFormat,
            "The runtime descriptor size is invalid.");
    }
    ByteReader reader(bytes);
    const auto magic = reader.ReadBytes(descriptorMagic.size());
    const auto major = reader.ReadUInt16();
    const auto minor = reader.ReadUInt16();
    const auto mode = reader.ReadUInt32();
    const auto packageIdBytes = reader.ReadBytes(PackageId{}.size());
    const auto keyIdBytes = reader.ReadBytes(KeyId{}.size());
    const auto fileNameSize = reader.ReadUInt32();
    const auto fileNameField =
        reader.ReadBytes(descriptorFileNameFieldSize);
    if (!magic || !major || !minor || !mode || !packageIdBytes ||
        !keyIdBytes || !fileNameSize || !fileNameField) {
        return Failure<RuntimeDescriptor>(
            PackageErrorCode::InvalidFormat,
            "The runtime descriptor is truncated.");
    }
    if (!std::equal(
            magic.value().begin(),
            magic.value().end(),
            descriptorMagic.begin())) {
        return Failure<RuntimeDescriptor>(
            PackageErrorCode::InvalidFormat,
            "The runtime descriptor magic is invalid.");
    }
    if (major.value() != descriptorMajorVersion ||
        minor.value() != descriptorMinorVersion) {
        return Failure<RuntimeDescriptor>(
            PackageErrorCode::UnsupportedVersion,
            "The runtime descriptor version is unsupported.");
    }
    if (mode.value() >
        static_cast<std::uint32_t>(RuntimeMode::Installer)) {
        return Failure<RuntimeDescriptor>(
            PackageErrorCode::InvalidFormat,
            "The runtime descriptor mode is unknown.");
    }
    if (fileNameSize.value() > descriptorFileNameFieldSize) {
        return Failure<RuntimeDescriptor>(
            PackageErrorCode::InvalidFormat,
            "The runtime descriptor filename length is invalid.");
    }
    if (std::find(
            fileNameField.value().begin(),
            fileNameField.value().begin() + fileNameSize.value(),
            std::uint8_t{0}) !=
        fileNameField.value().begin() + fileNameSize.value()) {
        return Failure<RuntimeDescriptor>(
            PackageErrorCode::InvalidFormat,
            "The runtime descriptor filename contains an embedded NUL.");
    }
    if (!std::all_of(
            fileNameField.value().begin() + fileNameSize.value(),
            fileNameField.value().end(),
            [](const std::uint8_t byte) { return byte == 0; })) {
        return Failure<RuntimeDescriptor>(
            PackageErrorCode::InvalidFormat,
            "The runtime descriptor reserved padding is nonzero.");
    }

    RuntimeDescriptor descriptor;
    descriptor.mode = static_cast<RuntimeMode>(mode.value());
    std::copy(
        packageIdBytes.value().begin(),
        packageIdBytes.value().end(),
        descriptor.packageId.begin());
    std::copy(
        keyIdBytes.value().begin(),
        keyIdBytes.value().end(),
        descriptor.keyId.begin());
    descriptor.packageFileName.assign(
        fileNameField.value().begin(),
        fileNameField.value().begin() + fileNameSize.value());
    const auto canonical = SerializeRuntimeDescriptor(descriptor);
    if (!canonical || canonical.value() != bytes) {
        return Failure<RuntimeDescriptor>(
            PackageErrorCode::InvalidFormat,
            "The runtime descriptor is not canonical.");
    }
    return PackageResult<RuntimeDescriptor>::Success(
        std::move(descriptor));
}

PackageResult<bool> WriteRuntimeDescriptorAtomically(
    const std::filesystem::path& descriptorPath,
    const RuntimeDescriptor& descriptor) {
    const auto bytes = SerializeRuntimeDescriptor(descriptor);
    if (!bytes) {
        return PackageResult<bool>::Failure(bytes.error());
    }
    if (descriptorPath.empty() ||
        descriptorPath.filename().empty()) {
        return Failure<bool>(
            PackageErrorCode::IoFailed,
            "The runtime descriptor destination is invalid.");
    }
    std::error_code statusError;
    const auto status =
        std::filesystem::symlink_status(descriptorPath, statusError);
    if (!statusError && std::filesystem::is_symlink(status)) {
        return Failure<bool>(
            PackageErrorCode::PublicationFailed,
            "A runtime descriptor symlink is not replaced.");
    }
    if (statusError &&
        statusError != std::errc::no_such_file_or_directory) {
        return Failure<bool>(
            PackageErrorCode::IoFailed,
            "Inspecting the runtime descriptor destination failed.");
    }

    auto temporary =
        CreateTemporaryDescriptor(descriptorPath, bytes.value());
    if (!temporary) {
        return PackageResult<bool>::Failure(temporary.error());
    }
    TemporaryPath cleanup(temporary.value());
    if (!MoveFileExW(
            cleanup.path().c_str(),
            descriptorPath.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return Failure<bool>(
            PackageErrorCode::PublicationFailed,
            "Atomically publishing the runtime descriptor failed.");
    }
    cleanup.MarkPublished();
    return PackageResult<bool>::Success(true);
}

PackageResult<RuntimeDescriptor> ReadRuntimeDescriptor(
    const std::filesystem::path& descriptorPath) {
    std::error_code statusError;
    const auto status =
        std::filesystem::symlink_status(descriptorPath, statusError);
    if (statusError || !std::filesystem::is_regular_file(status)) {
        return Failure<RuntimeDescriptor>(
            PackageErrorCode::IoFailed,
            "The runtime descriptor is not a regular file.");
    }
    const auto size =
        std::filesystem::file_size(descriptorPath, statusError);
    if (statusError || size != kRuntimeDescriptorSize) {
        return Failure<RuntimeDescriptor>(
            PackageErrorCode::InvalidFormat,
            "The runtime descriptor size is invalid.");
    }
    std::ifstream input(descriptorPath, std::ios::binary);
    std::vector<std::uint8_t> bytes(kRuntimeDescriptorSize);
    input.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    if (input.gcount() !=
        static_cast<std::streamsize>(bytes.size())) {
        return Failure<RuntimeDescriptor>(
            PackageErrorCode::IoFailed,
            "Reading the runtime descriptor failed.");
    }
    return ParseRuntimeDescriptor(bytes);
}

} // namespace dbp::package
