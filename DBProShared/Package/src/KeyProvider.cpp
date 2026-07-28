#include "dbp/package/KeyProvider.h"

#include "dbp/package/PackagePath.h"

#include <cstdint>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace dbp::package {

namespace {

constexpr char manifestKeyInfo[] = "DBP-PAK-v2/manifest";
constexpr char entryKeyInfoPrefix[] = "DBP-PAK-v2/entry/";

PackageError MissingKeyError() {
    return {
        PackageErrorCode::MissingKey,
        "The requested package key is unavailable.",
        std::nullopt,
    };
}

PackageResult<SecureBuffer> CloneValidatedMasterKey(
    const SecureBuffer& masterKey) {
    if (masterKey.size() != kPackageMasterKeySize) {
        return PackageResult<SecureBuffer>::Failure(
            MissingKeyError());
    }
    return PackageResult<SecureBuffer>::Success(
        SecureBuffer::FromBytes(masterKey.CopyBytes()));
}

std::vector<std::uint8_t> Bytes(
    const char* const begin,
    const std::size_t size) {
    return std::vector<std::uint8_t>(begin, begin + size);
}

std::vector<std::uint8_t> PackageSalt(
    const PackageId& packageId) {
    return std::vector<std::uint8_t>(
        packageId.begin(),
        packageId.end());
}

} // namespace

MemoryKeyProvider::MemoryKeyProvider(
    const KeyId keyId,
    SecureBuffer masterKey) noexcept
    : keyId_(keyId),
      masterKey_(std::move(masterKey)) {}

PackageResult<SecureBuffer> MemoryKeyProvider::Resolve(
    const KeyId& keyId) const {
    if (keyId != keyId_) {
        return PackageResult<SecureBuffer>::Failure(
            MissingKeyError());
    }
    return CloneValidatedMasterKey(masterKey_);
}

FileKeyProvider::FileKeyProvider(
    const KeyId keyId,
    std::filesystem::path keyFile) noexcept
    : keyId_(keyId),
      keyFile_(std::move(keyFile)) {}

PackageResult<SecureBuffer> FileKeyProvider::Resolve(
    const KeyId& keyId) const {
    if (keyId != keyId_) {
        return PackageResult<SecureBuffer>::Failure(
            MissingKeyError());
    }

    std::error_code statusError;
    const auto fileStatus =
        std::filesystem::symlink_status(keyFile_, statusError);
    if (statusError ||
        !std::filesystem::is_regular_file(fileStatus) ||
        std::filesystem::is_symlink(fileStatus)) {
        return PackageResult<SecureBuffer>::Failure(
            MissingKeyError());
    }

    std::ifstream input(keyFile_, std::ios::binary);
    if (!input) {
        return PackageResult<SecureBuffer>::Failure(
            MissingKeyError());
    }

    auto bytes = SecureBuffer::FromBytes(
        std::vector<std::uint8_t>(kPackageMasterKeySize + 1));
    input.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    const auto bytesRead = input.gcount();
    if (input.bad() ||
        bytesRead !=
            static_cast<std::streamsize>(kPackageMasterKeySize)) {
        return PackageResult<SecureBuffer>::Failure(
            MissingKeyError());
    }

    return PackageResult<SecureBuffer>::Success(
        SecureBuffer::FromBytes(std::vector<std::uint8_t>(
            bytes.data(),
            bytes.data() + kPackageMasterKeySize)));
}

PackageKeyDeriver::PackageKeyDeriver(
    const CryptoProvider& crypto) noexcept
    : crypto_(crypto) {}

PackageResult<SecureBuffer> PackageKeyDeriver::DeriveManifestKey(
    const SecureBuffer& masterKey,
    const PackageId& packageId) const {
    if (masterKey.size() != kPackageMasterKeySize) {
        return PackageResult<SecureBuffer>::Failure(
            MissingKeyError());
    }
    const auto info = Bytes(
        manifestKeyInfo,
        sizeof(manifestKeyInfo) - 1);
    return crypto_.HkdfSha256(
        masterKey,
        PackageSalt(packageId),
        info,
        kPackageMasterKeySize);
}

PackageResult<SecureBuffer> PackageKeyDeriver::DeriveEntryKey(
    const SecureBuffer& masterKey,
    const PackageId& packageId,
    const std::string_view canonicalPath) const {
    if (masterKey.size() != kPackageMasterKeySize) {
        return PackageResult<SecureBuffer>::Failure(
            MissingKeyError());
    }

    const auto path = ValidatePersistedPackagePath(canonicalPath);
    if (!path) {
        return PackageResult<SecureBuffer>::Failure(
            path.error());
    }
    const auto pathBytes = std::vector<std::uint8_t>(
        path.value().begin(),
        path.value().end());
    const auto pathDigest = crypto_.Sha256(pathBytes);
    if (!pathDigest) {
        return PackageResult<SecureBuffer>::Failure(
            pathDigest.error());
    }

    auto info = Bytes(
        entryKeyInfoPrefix,
        sizeof(entryKeyInfoPrefix) - 1);
    info.insert(
        info.end(),
        pathDigest.value().begin(),
        pathDigest.value().end());
    return crypto_.HkdfSha256(
        masterKey,
        PackageSalt(packageId),
        info,
        kPackageMasterKeySize);
}

} // namespace dbp::package
