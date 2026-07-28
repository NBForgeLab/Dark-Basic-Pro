#pragma once

#include "dbp/package/CryptoProvider.h"
#include "dbp/package/PackageError.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace dbp::package {

inline constexpr std::uint32_t kPackageHeaderSize = 160;
inline constexpr std::uint32_t kManifestHeaderSize = 32;
inline constexpr std::uint32_t kManifestRecordSize = 112;

using PackageId = std::array<std::uint8_t, 16>;
using KeyId = std::array<std::uint8_t, 16>;

enum class PackageHeaderFlag : std::uint32_t {
    ManifestEncrypted = 1U << 0U,
    PayloadsEncrypted = 1U << 1U,
    HasCompressedEntries = 1U << 2U,
};

constexpr std::uint32_t operator|(
    const PackageHeaderFlag left,
    const PackageHeaderFlag right) noexcept {
    return static_cast<std::uint32_t>(left) |
        static_cast<std::uint32_t>(right);
}

inline std::uint32_t& operator|=(
    std::uint32_t& flags,
    const PackageHeaderFlag flag) noexcept {
    flags |= static_cast<std::uint32_t>(flag);
    return flags;
}

enum class CompressionAlgorithm : std::uint16_t {
    None = 0,
    Zstandard = 1,
};

enum class EncryptionAlgorithm : std::uint16_t {
    Aes256Gcm = 1,
};

struct PackageLimits {
    std::uint32_t maximumEntries = 100'000;
    std::uint32_t maximumPathBytes = 1'024;
    std::uint64_t maximumManifestSize = 256ULL * 1024ULL * 1024ULL;
    std::uint64_t maximumEntryPlaintextSize = 8ULL * 1024ULL * 1024ULL * 1024ULL;
    std::uint64_t maximumTotalPlaintextSize = 64ULL * 1024ULL * 1024ULL * 1024ULL;
    std::uint64_t maximumArchiveSize = 64ULL * 1024ULL * 1024ULL * 1024ULL;
};

struct PackageHeader {
    std::uint16_t majorVersion = 2;
    std::uint16_t minorVersion = 0;
    std::uint32_t flags =
        PackageHeaderFlag::ManifestEncrypted |
        PackageHeaderFlag::PayloadsEncrypted;
    std::uint32_t entryCount = 0;
    std::uint64_t manifestOffset = kPackageHeaderSize;
    std::uint64_t manifestCiphertextSize = 0;
    std::uint64_t payloadOffset = kPackageHeaderSize;
    std::uint64_t payloadSize = 0;
    PackageId packageId{};
    KeyId keyId{};
    AesGcmNonce manifestNonce{};
    AesGcmTag manifestTag{};
    Sha256Digest manifestPlaintextSha256{};
};

struct ManifestRecord {
    std::string path;
    std::uint32_t flags = 0;
    CompressionAlgorithm compression = CompressionAlgorithm::None;
    EncryptionAlgorithm encryption = EncryptionAlgorithm::Aes256Gcm;
    std::uint64_t plaintextSize = 0;
    std::uint64_t storedSize = 0;
    std::uint64_t payloadOffset = 0;
    Sha256Digest plaintextSha256{};
    AesGcmNonce nonce{};
    AesGcmTag tag{};
};

struct PackageManifest {
    std::vector<ManifestRecord> records;
};

std::vector<std::uint8_t> SerializePackageHeader(
    const PackageHeader& header);
PackageResult<PackageHeader> ParsePackageHeader(
    const std::vector<std::uint8_t>& bytes,
    std::uint64_t fileSize,
    const PackageLimits& limits);

PackageResult<std::vector<std::uint8_t>> SerializeManifest(
    const PackageManifest& manifest);
PackageResult<PackageManifest> ParseManifest(
    const std::vector<std::uint8_t>& plaintext,
    const PackageHeader& header,
    const PackageLimits& limits);

std::vector<std::uint8_t> BuildManifestAdditionalData(
    const PackageHeader& header);
std::vector<std::uint8_t> BuildEntryAdditionalData(
    const PackageId& packageId,
    const ManifestRecord& record);

} // namespace dbp::package
