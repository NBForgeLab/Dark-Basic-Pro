#pragma once

#include "dbp/package/PackageError.h"
#include "dbp/package/SecureBuffer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace dbp::package {

using Sha256Digest = std::array<std::uint8_t, 32>;
using AesGcmNonce = std::array<std::uint8_t, 12>;
using AesGcmTag = std::array<std::uint8_t, 16>;

struct AeadCiphertext {
    std::vector<std::uint8_t> ciphertext;
    AesGcmTag tag{};
};

class CryptoProvider {
public:
    virtual ~CryptoProvider() = default;

    virtual PackageResult<Sha256Digest> Sha256(
        const std::vector<std::uint8_t>& input) const = 0;
    virtual PackageResult<Sha256Digest> HmacSha256(
        const SecureBuffer& key,
        const std::vector<std::uint8_t>& input) const = 0;
    virtual PackageResult<SecureBuffer> HkdfSha256(
        const SecureBuffer& inputKeyMaterial,
        const std::vector<std::uint8_t>& salt,
        const std::vector<std::uint8_t>& info,
        std::size_t outputSize) const = 0;
    virtual PackageResult<AeadCiphertext> Aes256GcmEncrypt(
        const SecureBuffer& key,
        const AesGcmNonce& nonce,
        const std::vector<std::uint8_t>& plaintext,
        const std::vector<std::uint8_t>& additionalData) const = 0;
    virtual PackageResult<std::vector<std::uint8_t>> Aes256GcmDecrypt(
        const SecureBuffer& key,
        const AesGcmNonce& nonce,
        const std::vector<std::uint8_t>& ciphertext,
        const std::vector<std::uint8_t>& additionalData,
        const AesGcmTag& tag) const = 0;
    virtual PackageResult<std::vector<std::uint8_t>> RandomBytes(
        std::size_t size) const = 0;
};

class CngCryptoProvider final : public CryptoProvider {
public:
    PackageResult<Sha256Digest> Sha256(
        const std::vector<std::uint8_t>& input) const override;
    PackageResult<Sha256Digest> HmacSha256(
        const SecureBuffer& key,
        const std::vector<std::uint8_t>& input) const override;
    PackageResult<SecureBuffer> HkdfSha256(
        const SecureBuffer& inputKeyMaterial,
        const std::vector<std::uint8_t>& salt,
        const std::vector<std::uint8_t>& info,
        std::size_t outputSize) const override;
    PackageResult<AeadCiphertext> Aes256GcmEncrypt(
        const SecureBuffer& key,
        const AesGcmNonce& nonce,
        const std::vector<std::uint8_t>& plaintext,
        const std::vector<std::uint8_t>& additionalData) const override;
    PackageResult<std::vector<std::uint8_t>> Aes256GcmDecrypt(
        const SecureBuffer& key,
        const AesGcmNonce& nonce,
        const std::vector<std::uint8_t>& ciphertext,
        const std::vector<std::uint8_t>& additionalData,
        const AesGcmTag& tag) const override;
    PackageResult<std::vector<std::uint8_t>> RandomBytes(
        std::size_t size) const override;
};

} // namespace dbp::package
