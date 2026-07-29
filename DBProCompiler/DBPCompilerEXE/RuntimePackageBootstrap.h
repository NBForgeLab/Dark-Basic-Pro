#pragma once

#include "../DBPCompiler/PackageMount.h"
#include "dbp/package/RuntimeDescriptor.h"

#include <filesystem>
#include <memory>
#include <vector>

dbp::package::PackageResult<bool>
WriteRuntimeMaterializedFileSafely(
    const std::filesystem::path& root,
    const std::filesystem::path& relativePath,
    const std::vector<std::uint8_t>& bytes);

dbp::package::PackageResult<bool>
CopyRuntimeMaterializedFileSafely(
    const std::filesystem::path& root,
    const std::filesystem::path& relativePath,
    const std::filesystem::path& sourcePath);

enum class RuntimePackageKind {
    AuthenticatedV2,
    LegacyReadOnly,
};

class RuntimePackageBootstrap {
public:
    static dbp::package::PackageResult<
        std::unique_ptr<RuntimePackageBootstrap>> Start(
        const std::filesystem::path& executablePath);

    ~RuntimePackageBootstrap() = default;
    RuntimePackageBootstrap(const RuntimePackageBootstrap&) = delete;
    RuntimePackageBootstrap& operator=(const RuntimePackageBootstrap&) =
        delete;

    RuntimePackageKind kind() const noexcept {
        return kind_;
    }

    dbp::package::RuntimeMode mode() const noexcept {
        return mode_;
    }

    dbp::package::PackageResult<std::filesystem::path>
    MaterializeInstaller(
        const std::filesystem::path& outputRoot) const;

private:
    RuntimePackageBootstrap() = default;

    dbp::package::CngCryptoProvider crypto_;
    dbp::package::ZstdCompressionCodec compression_;
    dbp::package::Win32AtomicFilePublisher publisher_;
    std::shared_ptr<dbp::package::PackageReader> v2Reader_;
    std::shared_ptr<dbp::package::LegacyPckReader> legacyReader_;
    std::unique_ptr<PackageMount> mount_;
    std::filesystem::path executablePath_;
    std::filesystem::path packagePath_;
    dbp::package::RuntimeDescriptor descriptor_;
    RuntimePackageKind kind_ = RuntimePackageKind::LegacyReadOnly;
    dbp::package::RuntimeMode mode_ =
        dbp::package::RuntimeMode::Application;
};
