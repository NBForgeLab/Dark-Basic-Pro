#pragma once

#include "dbp/package/CryptoProvider.h"
#include "dbp/package/PackageFormat.h"
#include "dbp/package/SecureBuffer.h"

#include <filesystem>
#include <string_view>

namespace dbp::package {

inline constexpr std::size_t kPackageMasterKeySize = 32;

class KeyProvider {
public:
    virtual ~KeyProvider() = default;

    virtual PackageResult<SecureBuffer> Resolve(
        const KeyId& keyId) const = 0;
};

class MemoryKeyProvider final : public KeyProvider {
public:
    MemoryKeyProvider(KeyId keyId, SecureBuffer masterKey) noexcept;

    MemoryKeyProvider(const MemoryKeyProvider&) = delete;
    MemoryKeyProvider& operator=(const MemoryKeyProvider&) = delete;
    MemoryKeyProvider(MemoryKeyProvider&&) noexcept = default;
    MemoryKeyProvider& operator=(MemoryKeyProvider&&) noexcept = default;

    PackageResult<SecureBuffer> Resolve(
        const KeyId& keyId) const override;

private:
    KeyId keyId_;
    SecureBuffer masterKey_;
};

class FileKeyProvider final : public KeyProvider {
public:
    FileKeyProvider(
        KeyId keyId,
        std::filesystem::path keyFile) noexcept;

    FileKeyProvider(const FileKeyProvider&) = delete;
    FileKeyProvider& operator=(const FileKeyProvider&) = delete;
    FileKeyProvider(FileKeyProvider&&) noexcept = default;
    FileKeyProvider& operator=(FileKeyProvider&&) noexcept = default;

    PackageResult<SecureBuffer> Resolve(
        const KeyId& keyId) const override;

private:
    KeyId keyId_;
    std::filesystem::path keyFile_;
};

class PackageKeyDeriver {
public:
    explicit PackageKeyDeriver(const CryptoProvider& crypto) noexcept;

    PackageResult<SecureBuffer> DeriveManifestKey(
        const SecureBuffer& masterKey,
        const PackageId& packageId) const;
    PackageResult<SecureBuffer> DeriveEntryKey(
        const SecureBuffer& masterKey,
        const PackageId& packageId,
        std::string_view canonicalPath) const;

private:
    const CryptoProvider& crypto_;
};

} // namespace dbp::package
