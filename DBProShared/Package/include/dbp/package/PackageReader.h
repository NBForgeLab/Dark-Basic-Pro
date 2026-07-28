#pragma once

#include "dbp/package/CompressionCodec.h"
#include "dbp/package/CryptoProvider.h"
#include "dbp/package/KeyProvider.h"
#include "dbp/package/PackageFormat.h"
#include "dbp/package/PackageWriter.h"

#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

namespace dbp::package {

class PackageReader {
public:
    static PackageResult<std::unique_ptr<PackageReader>> Open(
        const std::filesystem::path& packagePath,
        const KeyProvider& keys,
        const CryptoProvider& crypto,
        const ZstdCompressionCodec& compression,
        const AtomicFilePublisher& publisher,
        const PackageLimits& limits = {});

    ~PackageReader() = default;
    PackageReader(const PackageReader&) = delete;
    PackageReader& operator=(const PackageReader&) = delete;

    const PackageHeader& header() const noexcept {
        return header_;
    }

    const PackageManifest& manifest() const noexcept {
        return manifest_;
    }

    PackageResult<bool> ExtractEntry(
        std::string_view packagePath,
        const std::filesystem::path& destination) const;
    PackageResult<std::shared_ptr<const std::vector<std::uint8_t>>>
    ReadEntry(std::string_view packagePath) const;

private:
    PackageReader(
        std::filesystem::path packagePath,
        PackageHeader header,
        PackageManifest manifest,
        SecureBuffer masterKey,
        const CryptoProvider& crypto,
        const ZstdCompressionCodec& compression,
        const AtomicFilePublisher& publisher,
        PackageLimits limits) noexcept;

    std::filesystem::path packagePath_;
    PackageHeader header_;
    PackageManifest manifest_;
    SecureBuffer masterKey_;
    const CryptoProvider& crypto_;
    const ZstdCompressionCodec& compression_;
    const AtomicFilePublisher& publisher_;
    PackageLimits limits_;
};

} // namespace dbp::package
