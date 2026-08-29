#include "RuntimePackageBootstrap.h"

#include "dbp/package/ExecutableKeyResource.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>
#include <windows.h>

namespace {

using namespace dbp::package;

template <typename T>
PackageResult<T> BootstrapError(
    const PackageErrorCode code,
    std::string message) {
    return PackageResult<T>::Failure({
        code,
        std::move(message),
        std::nullopt,
    });
}

} // namespace

namespace {

class RuntimeFileHandle {
public:
    explicit RuntimeFileHandle(HANDLE value) noexcept : value_(value) {}
    ~RuntimeFileHandle() {
        if (value_ != INVALID_HANDLE_VALUE) {
            CloseHandle(value_);
        }
    }
    RuntimeFileHandle(const RuntimeFileHandle&) = delete;
    RuntimeFileHandle& operator=(const RuntimeFileHandle&) = delete;
    RuntimeFileHandle(RuntimeFileHandle&& other) noexcept
        : value_(std::exchange(
              other.value_,
              INVALID_HANDLE_VALUE)) {}
    RuntimeFileHandle& operator=(RuntimeFileHandle&& other) noexcept {
        if (this != &other) {
            if (value_ != INVALID_HANDLE_VALUE) {
                CloseHandle(value_);
            }
            value_ = std::exchange(
                other.value_,
                INVALID_HANDLE_VALUE);
        }
        return *this;
    }
    HANDLE get() const noexcept { return value_; }
private:
    HANDLE value_;
};

PackageResult<std::wstring> SafeFinalPath(
    const HANDLE handle) {
    FILE_ATTRIBUTE_TAG_INFO tag{};
    if (!GetFileInformationByHandleEx(
            handle,
            FileAttributeTagInfo,
            &tag,
            sizeof(tag)) ||
        (tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        return BootstrapError<std::wstring>(
            PackageErrorCode::UnsafePath,
            "A materialization path traverses a reparse point.");
    }
    const auto required = GetFinalPathNameByHandleW(
        handle,
        nullptr,
        0,
        FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (required == 0) {
        return BootstrapError<std::wstring>(
            PackageErrorCode::IoFailed,
            "Resolving a materialization handle failed.");
    }
    std::wstring result(required, L'\0');
    const auto written = GetFinalPathNameByHandleW(
        handle,
        result.data(),
        required,
        FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (written == 0 || written >= required) {
        return BootstrapError<std::wstring>(
            PackageErrorCode::IoFailed,
            "Resolving a materialization handle failed.");
    }
    result.resize(written);
    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](const wchar_t value) {
            return static_cast<wchar_t>(std::towlower(value));
        });
    return PackageResult<std::wstring>::Success(
        std::move(result));
}

bool IsStrictlyBeneath(
    std::wstring root,
    const std::wstring& candidate) {
    if (!root.empty() &&
        root.back() != L'\\') {
        root.push_back(L'\\');
    }
    return candidate.size() > root.size() &&
        candidate.compare(0, root.size(), root) == 0;
}

struct SafeMaterializationRoot {
    RuntimeFileHandle handle;
    std::wstring finalPath;
};

PackageResult<SafeMaterializationRoot> OpenSafeRoot(
    const std::filesystem::path& root) {
    RuntimeFileHandle handle(CreateFileW(
        root.c_str(),
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS |
            FILE_FLAG_OPEN_REPARSE_POINT,
        nullptr));
    if (handle.get() == INVALID_HANDLE_VALUE) {
        return BootstrapError<SafeMaterializationRoot>(
            PackageErrorCode::IoFailed,
            "Opening the materialization root failed.");
    }
    auto finalPath = SafeFinalPath(handle.get());
    if (!finalPath) {
        return PackageResult<SafeMaterializationRoot>::Failure(
            finalPath.error());
    }
    return PackageResult<SafeMaterializationRoot>::Success({
        std::move(handle),
        std::move(finalPath.value()),
    });
}

PackageResult<RuntimeFileHandle> OpenDirectoryBeneath(
    const std::filesystem::path& directory,
    const std::wstring& rootFinal) {
    RuntimeFileHandle handle(CreateFileW(
        directory.c_str(),
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS |
            FILE_FLAG_OPEN_REPARSE_POINT,
        nullptr));
    if (handle.get() == INVALID_HANDLE_VALUE) {
        return BootstrapError<RuntimeFileHandle>(
            PackageErrorCode::IoFailed,
            "Opening a materialization directory failed.");
    }
    const auto finalPath = SafeFinalPath(handle.get());
    if (!finalPath ||
        !IsStrictlyBeneath(rootFinal, finalPath.value())) {
        return BootstrapError<RuntimeFileHandle>(
            PackageErrorCode::UnsafePath,
            "A materialization directory escaped its root.");
    }
    return PackageResult<RuntimeFileHandle>::Success(
        std::move(handle));
}

PackageResult<RuntimeFileHandle> OpenMaterializationOutput(
    const std::filesystem::path& root,
    const std::filesystem::path& relativePath) {
    if (relativePath.empty() ||
        relativePath.is_absolute()) {
        return BootstrapError<RuntimeFileHandle>(
            PackageErrorCode::UnsafePath,
            "A materialization destination is not relative.");
    }
    auto rootState = OpenSafeRoot(root);
    if (!rootState) {
        return PackageResult<RuntimeFileHandle>::Failure(
            rootState.error());
    }

    std::vector<RuntimeFileHandle> lockedDirectories;
    auto current = root;
    for (const auto& component :
         relativePath.parent_path()) {
        if (component == L"." ||
            component == L".." ||
            component.empty()) {
            return BootstrapError<RuntimeFileHandle>(
                PackageErrorCode::UnsafePath,
                "A materialization path component is unsafe.");
        }
        current /= component;
        if (!CreateDirectoryW(current.c_str(), nullptr) &&
            GetLastError() != ERROR_ALREADY_EXISTS) {
            return BootstrapError<RuntimeFileHandle>(
                PackageErrorCode::IoFailed,
                "Creating a materialization directory failed.");
        }
        auto directory = OpenDirectoryBeneath(
            current,
            rootState.value().finalPath);
        if (!directory) {
            return PackageResult<RuntimeFileHandle>::Failure(
                directory.error());
        }
        lockedDirectories.push_back(
            std::move(directory.value()));
    }

    const auto target = root / relativePath;
    RuntimeFileHandle output(CreateFileW(
        target.c_str(),
        GENERIC_WRITE | FILE_READ_ATTRIBUTES | DELETE,
        0,
        nullptr,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL |
            FILE_FLAG_OPEN_REPARSE_POINT |
            FILE_FLAG_WRITE_THROUGH,
        nullptr));
    if (output.get() == INVALID_HANDLE_VALUE) {
        return BootstrapError<RuntimeFileHandle>(
            PackageErrorCode::IoFailed,
            "Creating a materialization destination failed.");
    }
    const auto targetFinal = SafeFinalPath(output.get());
    if (!targetFinal ||
        !IsStrictlyBeneath(
            rootState.value().finalPath,
            targetFinal.value())) {
        FILE_DISPOSITION_INFO disposition{TRUE};
        SetFileInformationByHandle(
            output.get(),
            FileDispositionInfo,
            &disposition,
            sizeof(disposition));
        return BootstrapError<RuntimeFileHandle>(
            PackageErrorCode::UnsafePath,
            "A materialization destination escaped its root.");
    }
    return PackageResult<RuntimeFileHandle>::Success(
        std::move(output));
}

PackageResult<bool> FlushMaterializationOutput(
    const HANDLE output) {
    if (!FlushFileBuffers(output)) {
        return BootstrapError<bool>(
            PackageErrorCode::IoFailed,
            "Flushing a materialization destination failed.");
    }
    return PackageResult<bool>::Success(true);
}

} // namespace

PackageResult<bool> WriteRuntimeMaterializedFileSafely(
    const std::filesystem::path& root,
    const std::filesystem::path& relativePath,
    const std::vector<std::uint8_t>& bytes) {
    auto output = OpenMaterializationOutput(root, relativePath);
    if (!output) {
        return PackageResult<bool>::Failure(output.error());
    }
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto remaining = bytes.size() - offset;
        const auto chunk = static_cast<DWORD>(
            std::min<std::size_t>(remaining, 1024U * 1024U));
        DWORD written = 0;
        if (!WriteFile(
                output.value().get(),
                bytes.data() + offset,
                chunk,
                &written,
                nullptr) ||
            written != chunk) {
            return BootstrapError<bool>(
                PackageErrorCode::IoFailed,
                "Writing a materialization destination failed.");
        }
        offset += written;
    }
    return FlushMaterializationOutput(output.value().get());
}

PackageResult<bool> CopyRuntimeMaterializedFileSafely(
    const std::filesystem::path& root,
    const std::filesystem::path& relativePath,
    const std::filesystem::path& sourcePath) {
    RuntimeFileHandle source(CreateFileW(
        sourcePath.c_str(),
        GENERIC_READ | FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL |
            FILE_FLAG_OPEN_REPARSE_POINT |
            FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr));
    if (source.get() == INVALID_HANDLE_VALUE) {
        return BootstrapError<bool>(
            PackageErrorCode::IoFailed,
            "Opening a materialization source failed.");
    }
    FILE_ATTRIBUTE_TAG_INFO sourceTag{};
    if (!GetFileInformationByHandleEx(
            source.get(),
            FileAttributeTagInfo,
            &sourceTag,
            sizeof(sourceTag)) ||
        (sourceTag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 ||
        (sourceTag.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return BootstrapError<bool>(
            PackageErrorCode::UnsafePath,
            "A materialization source is not a regular file.");
    }
    auto output = OpenMaterializationOutput(root, relativePath);
    if (!output) {
        return PackageResult<bool>::Failure(output.error());
    }
    std::vector<std::uint8_t> buffer(1024U * 1024U);
    for (;;) {
        DWORD read = 0;
        if (!ReadFile(
                source.get(),
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                &read,
                nullptr)) {
            return BootstrapError<bool>(
                PackageErrorCode::IoFailed,
                "Reading a materialization source failed.");
        }
        if (read == 0) {
            break;
        }
        DWORD written = 0;
        if (!WriteFile(
                output.value().get(),
                buffer.data(),
                read,
                &written,
                nullptr) ||
            written != read) {
            return BootstrapError<bool>(
                PackageErrorCode::IoFailed,
                "Writing a materialization destination failed.");
        }
    }
    return FlushMaterializationOutput(output.value().get());
}

PackageResult<std::unique_ptr<RuntimePackageBootstrap>>
RuntimePackageBootstrap::Start(
    const std::filesystem::path& executablePath) {
    std::error_code absoluteError;
    const auto executable =
        std::filesystem::absolute(executablePath, absoluteError)
            .lexically_normal();
    if (absoluteError) {
        return BootstrapError<
            std::unique_ptr<RuntimePackageBootstrap>>(
                PackageErrorCode::IoFailed,
                "Resolving the runtime executable path failed.");
    }

    auto bootstrap = std::unique_ptr<RuntimePackageBootstrap>(
        new RuntimePackageBootstrap());
    bootstrap->executablePath_ = executable;
    auto descriptorPath = executable;
    descriptorPath.replace_extension(L".dbpakref");
    std::error_code descriptorExistsError;
    auto descriptorExists =
        std::filesystem::exists(
            descriptorPath,
            descriptorExistsError);
    if (!descriptorExists && executable.has_parent_path() && executable.parent_path().has_parent_path()) {
        const auto parentDescriptor = executable.parent_path().parent_path() / descriptorPath.filename();
        if (std::filesystem::exists(parentDescriptor, descriptorExistsError)) {
            descriptorPath = parentDescriptor;
            descriptorExists = true;
        }
    }
    if (descriptorExistsError) {
        return BootstrapError<
            std::unique_ptr<RuntimePackageBootstrap>>(
                PackageErrorCode::IoFailed,
                "Inspecting the runtime descriptor failed.");
    }

    if (descriptorExists) {
        const auto descriptor =
            ReadRuntimeDescriptor(descriptorPath);
        if (!descriptor) {
            return PackageResult<
                std::unique_ptr<RuntimePackageBootstrap>>::Failure(
                    descriptor.error());
        }
        auto packagePath =
            executable.parent_path() /
            std::filesystem::path(
                descriptor.value().packageFileName);
        if (!std::filesystem::exists(packagePath, descriptorExistsError)) {
            if (descriptorPath.parent_path() != executable.parent_path()) {
                const auto descriptorDirPackage = descriptorPath.parent_path() / std::filesystem::path(descriptor.value().packageFileName);
                if (std::filesystem::exists(descriptorDirPackage, descriptorExistsError)) {
                    packagePath = descriptorDirPackage;
                }
            } else if (executable.has_parent_path() && executable.parent_path().has_parent_path()) {
                const auto parentPackage = executable.parent_path().parent_path() / std::filesystem::path(descriptor.value().packageFileName);
                if (std::filesystem::exists(parentPackage, descriptorExistsError)) {
                    packagePath = parentPackage;
                }
            }
        }
        auto executableKey = ReadExecutablePackageKey(
            executable,
            descriptor.value().keyId);
        if (!executableKey) {
            return PackageResult<
                std::unique_ptr<RuntimePackageBootstrap>>::Failure(
                    executableKey.error());
        }
        MemoryKeyProvider keys(
            executableKey.value().keyId,
            std::move(executableKey.value().masterKey));
        auto opened = PackageReader::Open(
            packagePath,
            keys,
            bootstrap->crypto_,
            bootstrap->compression_,
            bootstrap->publisher_);
        if (!opened) {
            return PackageResult<
                std::unique_ptr<RuntimePackageBootstrap>>::Failure(
                    opened.error());
        }
        if (opened.value()->header().packageId !=
                descriptor.value().packageId ||
            opened.value()->header().keyId !=
                descriptor.value().keyId) {
            return BootstrapError<
                std::unique_ptr<RuntimePackageBootstrap>>(
                    PackageErrorCode::InvalidFormat,
                    "The descriptor, package, and executable key "
                    "identifiers do not match.");
        }
        bootstrap->v2Reader_ =
            std::shared_ptr<PackageReader>(
                std::move(opened.value()));
        auto mounted =
            PackageMount::MountV2(bootstrap->v2Reader_);
        if (!mounted) {
            return PackageResult<
                std::unique_ptr<RuntimePackageBootstrap>>::Failure(
                    mounted.error());
        }
        bootstrap->mount_ = std::move(mounted.value());
        bootstrap->kind_ =
            RuntimePackageKind::AuthenticatedV2;
        bootstrap->mode_ = descriptor.value().mode;
        bootstrap->packagePath_ =
            packagePath.lexically_normal();
        bootstrap->descriptor_ = descriptor.value();
        return PackageResult<
            std::unique_ptr<RuntimePackageBootstrap>>::Success(
                std::move(bootstrap));
    }

    auto legacy =
        LegacyPckReader::OpenExecutable(executable);
    if (!legacy) {
        auto sidecarPath = executable;
        sidecarPath.replace_extension(L".pck");
        std::error_code sidecarError;
        auto hasSidecar =
            std::filesystem::exists(sidecarPath, sidecarError);
        if (!hasSidecar && executable.has_parent_path() && executable.parent_path().has_parent_path()) {
            const auto parentSidecar = executable.parent_path().parent_path() / sidecarPath.filename();
            if (std::filesystem::exists(parentSidecar, sidecarError)) {
                sidecarPath = parentSidecar;
                hasSidecar = true;
            }
        }
        if (sidecarError) {
            return BootstrapError<
                std::unique_ptr<RuntimePackageBootstrap>>(
                    PackageErrorCode::IoFailed,
                    "Inspecting the legacy sidecar PCK failed.");
        }
        if (!hasSidecar) {
            return PackageResult<
                std::unique_ptr<RuntimePackageBootstrap>>::Failure(
                    legacy.error());
        }
        legacy = LegacyPckReader::OpenPckFile(sidecarPath);
        if (!legacy) {
            return PackageResult<
                std::unique_ptr<RuntimePackageBootstrap>>::Failure(
                    legacy.error());
        }
    }

    const auto legacyKind =
        legacy.value()->applicationKind();
    bootstrap->legacyReader_ =
        std::shared_ptr<LegacyPckReader>(
            std::move(legacy.value()));
    auto mounted =
        PackageMount::MountLegacy(bootstrap->legacyReader_);
    if (!mounted) {
        return PackageResult<
            std::unique_ptr<RuntimePackageBootstrap>>::Failure(
                mounted.error());
    }
    bootstrap->mount_ = std::move(mounted.value());
    bootstrap->kind_ = RuntimePackageKind::LegacyReadOnly;
    bootstrap->mode_ = legacyKind == 0
        ? RuntimeMode::Application
        : RuntimeMode::Installer;
    return PackageResult<
        std::unique_ptr<RuntimePackageBootstrap>>::Success(
            std::move(bootstrap));
}

PackageResult<std::filesystem::path>
RuntimePackageBootstrap::MaterializeInstaller(
    const std::filesystem::path& outputRoot) const {
    if (mode_ != RuntimeMode::Installer) {
        return BootstrapError<std::filesystem::path>(
            PackageErrorCode::InvalidFormat,
            "Installer materialization requires installer runtime mode.");
    }
    if (kind_ != RuntimePackageKind::AuthenticatedV2 ||
        !v2Reader_) {
        return BootstrapError<std::filesystem::path>(
            PackageErrorCode::UnsupportedVersion,
            "Legacy installer images are read-only and cannot be "
            "materialized by the authenticated package runtime.");
    }

    struct MaterializedMedia {
        std::filesystem::path relativePath;
        std::shared_ptr<const std::vector<std::uint8_t>> bytes;
    };
    std::vector<MaterializedMedia> mediaEntries;
    for (const auto& record : v2Reader_->manifest().records) {
        const auto plaintext = v2Reader_->ReadEntry(record.path);
        if (!plaintext) {
            return PackageResult<std::filesystem::path>::Failure(
                plaintext.error());
        }
        constexpr std::string_view mediaPrefix = "media/";
        if (record.path.size() > mediaPrefix.size() &&
            record.path.compare(
                0,
                mediaPrefix.size(),
                mediaPrefix) == 0) {
            const auto relative =
                record.path.substr(mediaPrefix.size());
            if (relative.empty()) {
                return BootstrapError<std::filesystem::path>(
                    PackageErrorCode::UnsafePath,
                    "An installer media entry has an empty destination.");
            }
            mediaEntries.push_back({
                std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(relative.data()), relative.size())),
                plaintext.value(),
            });
        }
    }

    std::error_code pathError;
    auto root = std::filesystem::absolute(
        outputRoot,
        pathError).lexically_normal();
    if (pathError ||
        !std::filesystem::is_directory(root, pathError) ||
        pathError) {
        return BootstrapError<std::filesystem::path>(
            PackageErrorCode::IoFailed,
            "The installer output root is not an accessible directory.");
    }

    auto baseName = executablePath_.stem();
    if (baseName.empty()) {
        baseName = L"Application";
    }
    std::filesystem::path destination;
    std::filesystem::path staging;
    for (std::uint32_t suffix = 0; suffix < 10'000U; ++suffix) {
        auto candidateName = baseName;
        if (suffix != 0) {
            candidateName += L"-" + std::to_wstring(suffix);
        }
        const auto candidate = root / candidateName;
        auto candidateStaging = candidate;
        candidateStaging += L".dbp-staging";
        const auto destinationExists =
            std::filesystem::exists(candidate, pathError);
        if (pathError) {
            return BootstrapError<std::filesystem::path>(
                PackageErrorCode::IoFailed,
                "Inspecting the installer destination failed.");
        }
        const auto stagingExists =
            std::filesystem::exists(candidateStaging, pathError);
        if (pathError) {
            return BootstrapError<std::filesystem::path>(
                PackageErrorCode::IoFailed,
                "Inspecting the installer staging path failed.");
        }
        if (!destinationExists && !stagingExists) {
            destination = candidate;
            staging = std::move(candidateStaging);
            break;
        }
    }
    if (destination.empty()) {
        return BootstrapError<std::filesystem::path>(
            PackageErrorCode::PublicationFailed,
            "No unique installer destination could be selected.");
    }

    if (!std::filesystem::create_directory(staging, pathError) ||
        pathError) {
        return BootstrapError<std::filesystem::path>(
            PackageErrorCode::PublicationFailed,
            "Creating the private installer staging directory failed.");
    }

    const auto failAndClean = [&staging](
                                  PackageError error) {
        std::error_code ignored;
        std::filesystem::remove_all(staging, ignored);
        return PackageResult<std::filesystem::path>::Failure(
            std::move(error));
    };
    const auto executableCopied =
        CopyRuntimeMaterializedFileSafely(
            staging,
            executablePath_.filename(),
            executablePath_);
    if (!executableCopied) {
        return failAndClean(executableCopied.error());
    }
    const auto packageCopied =
        CopyRuntimeMaterializedFileSafely(
            staging,
            packagePath_.filename(),
            packagePath_);
    if (!packageCopied) {
        return failAndClean(packageCopied.error());
    }

    for (const auto& entry : mediaEntries) {
        const auto written =
            WriteRuntimeMaterializedFileSafely(
                staging,
                entry.relativePath,
                *entry.bytes);
        if (!written) {
            return failAndClean(written.error());
        }
    }

    auto installedDescriptor = descriptor_;
    installedDescriptor.mode = RuntimeMode::Application;
    auto installedDescriptorName =
        executablePath_.filename();
    installedDescriptorName.replace_extension(L".dbpakref");
    const auto descriptorBytes =
        SerializeRuntimeDescriptor(installedDescriptor);
    if (!descriptorBytes) {
        return failAndClean(descriptorBytes.error());
    }
    const auto descriptorWritten =
        WriteRuntimeMaterializedFileSafely(
            staging,
            installedDescriptorName,
            descriptorBytes.value());
    if (!descriptorWritten) {
        return failAndClean(descriptorWritten.error());
    }

    std::filesystem::rename(staging, destination, pathError);
    if (pathError) {
        return failAndClean({
            PackageErrorCode::PublicationFailed,
            "Atomically publishing the installed application folder "
            "failed.",
            std::nullopt,
        });
    }
    return PackageResult<std::filesystem::path>::Success(
        destination);
}
