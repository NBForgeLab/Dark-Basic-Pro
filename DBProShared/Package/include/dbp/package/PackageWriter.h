#pragma once

#include "dbp/package/CompressionCodec.h"
#include "dbp/package/CryptoProvider.h"
#include "dbp/package/KeyProvider.h"
#include "dbp/package/PackageFormat.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace dbp::package {

struct PackageSourceIdentity {
    std::uint32_t volumeSerialNumber = 0;
    std::uint64_t fileIndex = 0;
    std::uint64_t size = 0;
    std::uint64_t lastWriteTime = 0;
    std::uint64_t changeTime = 0;

    bool operator==(
        const PackageSourceIdentity& other) const noexcept {
        return volumeSerialNumber == other.volumeSerialNumber &&
            fileIndex == other.fileIndex &&
            size == other.size &&
            lastWriteTime == other.lastWriteTime &&
            changeTime == other.changeTime;
    }

    bool operator!=(
        const PackageSourceIdentity& other) const noexcept {
        return !(*this == other);
    }
};

struct PackageSourceEntry {
    std::filesystem::path sourcePath;
    std::string packagePath;
    bool enableCompression = true;
    std::optional<PackageSourceIdentity> expectedIdentity;
};

struct PackageWriteRequest {
    std::filesystem::path outputDirectory;
    KeyId keyId{};
    std::vector<PackageSourceEntry> entries;
    PackageLimits limits;
};

struct PackageWriteResult {
    std::filesystem::path packagePath;
    PackageId packageId{};
    PackageHeader header;
};

PackageResult<PackageSourceIdentity>
CapturePackageSourceIdentity(
    const std::filesystem::path& sourcePath);

class AtomicFilePublisher {
public:
    virtual ~AtomicFilePublisher() = default;

    virtual PackageResult<bool> Publish(
        const std::filesystem::path& temporaryPath,
        const std::filesystem::path& finalPath) const = 0;
};

class Win32AtomicFilePublisher final : public AtomicFilePublisher {
public:
    PackageResult<bool> Publish(
        const std::filesystem::path& temporaryPath,
        const std::filesystem::path& finalPath) const override;
};

class PackageWriter {
public:
    PackageWriter(
        const CryptoProvider& crypto,
        const ZstdCompressionCodec& compression,
        const AtomicFilePublisher& publisher) noexcept;

    PackageResult<PackageWriteResult> Write(
        const PackageWriteRequest& request,
        const KeyProvider& keys) const;

private:
    const CryptoProvider& crypto_;
    const ZstdCompressionCodec& compression_;
    const AtomicFilePublisher& publisher_;
};

} // namespace dbp::package
