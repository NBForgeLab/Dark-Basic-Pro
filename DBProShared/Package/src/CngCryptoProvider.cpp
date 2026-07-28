#include "dbp/package/CryptoProvider.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace dbp::package {

namespace {

constexpr std::size_t sha256Size = 32;
constexpr std::size_t aes256KeySize = 32;
constexpr std::size_t gcmTagSize = 16;
constexpr std::size_t streamBufferSize = 1024U * 1024U;

bool NtSucceeded(const NTSTATUS status) noexcept {
    return status >= 0;
}

PackageError CryptoError(
    const std::string& operation,
    const NTSTATUS status) {
    std::ostringstream message;
    message << operation << " failed with CNG status 0x"
            << std::hex << static_cast<std::uint32_t>(status) << '.';
    return {
        PackageErrorCode::CryptographyFailed,
        message.str(),
        std::nullopt,
    };
}

PackageError AuthenticationError() {
    return {
        PackageErrorCode::AuthenticationFailed,
        "Package authentication failed.",
        std::nullopt,
    };
}

PackageError CryptoInputError(std::string message) {
    return {
        PackageErrorCode::CryptographyFailed,
        std::move(message),
        std::nullopt,
    };
}

PackageError CryptoStreamError(std::string message) {
    return {
        PackageErrorCode::IoFailed,
        std::move(message),
        std::nullopt,
    };
}

class AlgorithmHandle {
public:
    AlgorithmHandle() = default;
    ~AlgorithmHandle() {
        if (value_ != nullptr) {
            BCryptCloseAlgorithmProvider(value_, 0);
        }
    }
    AlgorithmHandle(const AlgorithmHandle&) = delete;
    AlgorithmHandle& operator=(const AlgorithmHandle&) = delete;

    BCRYPT_ALG_HANDLE* out() noexcept {
        return &value_;
    }
    BCRYPT_ALG_HANDLE get() const noexcept {
        return value_;
    }

private:
    BCRYPT_ALG_HANDLE value_ = nullptr;
};

class HashHandle {
public:
    HashHandle() = default;
    ~HashHandle() {
        if (value_ != nullptr) {
            BCryptDestroyHash(value_);
        }
    }
    HashHandle(const HashHandle&) = delete;
    HashHandle& operator=(const HashHandle&) = delete;

    BCRYPT_HASH_HANDLE* out() noexcept {
        return &value_;
    }
    BCRYPT_HASH_HANDLE get() const noexcept {
        return value_;
    }

private:
    BCRYPT_HASH_HANDLE value_ = nullptr;
};

class KeyHandle {
public:
    KeyHandle() = default;
    ~KeyHandle() {
        if (value_ != nullptr) {
            BCryptDestroyKey(value_);
        }
    }
    KeyHandle(const KeyHandle&) = delete;
    KeyHandle& operator=(const KeyHandle&) = delete;
    KeyHandle(KeyHandle&& other) noexcept
        : value_(std::exchange(other.value_, nullptr)) {}
    KeyHandle& operator=(KeyHandle&& other) noexcept {
        if (this != &other) {
            if (value_ != nullptr) {
                BCryptDestroyKey(value_);
            }
            value_ = std::exchange(other.value_, nullptr);
        }
        return *this;
    }

    BCRYPT_KEY_HANDLE* out() noexcept {
        return &value_;
    }
    BCRYPT_KEY_HANDLE get() const noexcept {
        return value_;
    }

private:
    BCRYPT_KEY_HANDLE value_ = nullptr;
};

PackageResult<ULONG> GetDwordProperty(
    const BCRYPT_HANDLE handle,
    const wchar_t* const property) {
    ULONG value = 0;
    ULONG bytesWritten = 0;
    const NTSTATUS status = BCryptGetProperty(
        handle,
        property,
        reinterpret_cast<PUCHAR>(&value),
        sizeof(value),
        &bytesWritten,
        0);
    if (!NtSucceeded(status) || bytesWritten != sizeof(value)) {
        return PackageResult<ULONG>::Failure(
            CryptoError("BCryptGetProperty", status));
    }
    return PackageResult<ULONG>::Success(value);
}

bool FitsCngSize(const std::size_t size) noexcept {
    return size <= std::numeric_limits<ULONG>::max();
}

PUCHAR MutableBytes(const std::vector<std::uint8_t>& bytes) {
    return bytes.empty()
        ? nullptr
        : const_cast<PUCHAR>(bytes.data());
}

PackageResult<Sha256Digest> ComputeSha256(
    const std::vector<std::uint8_t>& input,
    const SecureBuffer* const hmacKey) {
    if (!FitsCngSize(input.size()) ||
        (hmacKey != nullptr && !FitsCngSize(hmacKey->size()))) {
        return PackageResult<Sha256Digest>::Failure(
            CryptoInputError("Hash input exceeds the CNG size limit."));
    }

    AlgorithmHandle algorithm;
    const ULONG flags =
        hmacKey == nullptr ? 0 : BCRYPT_ALG_HANDLE_HMAC_FLAG;
    NTSTATUS status = BCryptOpenAlgorithmProvider(
        algorithm.out(),
        BCRYPT_SHA256_ALGORITHM,
        nullptr,
        flags);
    if (!NtSucceeded(status)) {
        return PackageResult<Sha256Digest>::Failure(
            CryptoError("BCryptOpenAlgorithmProvider(SHA-256)", status));
    }

    const auto objectSize =
        GetDwordProperty(algorithm.get(), BCRYPT_OBJECT_LENGTH);
    if (!objectSize) {
        return PackageResult<Sha256Digest>::Failure(objectSize.error());
    }
    const auto hashSize =
        GetDwordProperty(algorithm.get(), BCRYPT_HASH_LENGTH);
    if (!hashSize) {
        return PackageResult<Sha256Digest>::Failure(hashSize.error());
    }
    if (hashSize.value() != sha256Size) {
        return PackageResult<Sha256Digest>::Failure(
            CryptoInputError("CNG SHA-256 provider returned an invalid digest size."));
    }

    auto hashObject = SecureBuffer::FromBytes(
        std::vector<std::uint8_t>(objectSize.value()));
    HashHandle hash;
    status = BCryptCreateHash(
        algorithm.get(),
        hash.out(),
        hashObject.data(),
        static_cast<ULONG>(hashObject.size()),
        hmacKey == nullptr
            ? nullptr
            : const_cast<PUCHAR>(hmacKey->data()),
        hmacKey == nullptr
            ? 0
            : static_cast<ULONG>(hmacKey->size()),
        0);
    if (!NtSucceeded(status)) {
        return PackageResult<Sha256Digest>::Failure(
            CryptoError("BCryptCreateHash", status));
    }

    status = BCryptHashData(
        hash.get(),
        MutableBytes(input),
        static_cast<ULONG>(input.size()),
        0);
    if (!NtSucceeded(status)) {
        return PackageResult<Sha256Digest>::Failure(
            CryptoError("BCryptHashData", status));
    }

    Sha256Digest digest{};
    status = BCryptFinishHash(
        hash.get(),
        digest.data(),
        static_cast<ULONG>(digest.size()),
        0);
    if (!NtSucceeded(status)) {
        return PackageResult<Sha256Digest>::Failure(
            CryptoError("BCryptFinishHash", status));
    }
    return PackageResult<Sha256Digest>::Success(digest);
}

PackageResult<HashStreamResult> ComputeSha256Stream(
    std::istream& input,
    const std::uint64_t expectedSize) {
    AlgorithmHandle algorithm;
    NTSTATUS status = BCryptOpenAlgorithmProvider(
        algorithm.out(),
        BCRYPT_SHA256_ALGORITHM,
        nullptr,
        0);
    if (!NtSucceeded(status)) {
        return PackageResult<HashStreamResult>::Failure(
            CryptoError("BCryptOpenAlgorithmProvider(SHA-256)", status));
    }

    const auto objectSize =
        GetDwordProperty(algorithm.get(), BCRYPT_OBJECT_LENGTH);
    if (!objectSize) {
        return PackageResult<HashStreamResult>::Failure(
            objectSize.error());
    }
    const auto hashSize =
        GetDwordProperty(algorithm.get(), BCRYPT_HASH_LENGTH);
    if (!hashSize) {
        return PackageResult<HashStreamResult>::Failure(
            hashSize.error());
    }
    if (hashSize.value() != sha256Size) {
        return PackageResult<HashStreamResult>::Failure(
            CryptoInputError(
                "CNG SHA-256 provider returned an invalid digest size."));
    }

    auto hashObject = SecureBuffer::FromBytes(
        std::vector<std::uint8_t>(objectSize.value()));
    HashHandle hash;
    status = BCryptCreateHash(
        algorithm.get(),
        hash.out(),
        hashObject.data(),
        static_cast<ULONG>(hashObject.size()),
        nullptr,
        0,
        0);
    if (!NtSucceeded(status)) {
        return PackageResult<HashStreamResult>::Failure(
            CryptoError("BCryptCreateHash", status));
    }

    auto inputBuffer = SecureBuffer::FromBytes(
        std::vector<std::uint8_t>(streamBufferSize));
    HashStreamResult result;
    while (true) {
        input.read(
            reinterpret_cast<char*>(inputBuffer.data()),
            static_cast<std::streamsize>(inputBuffer.size()));
        const auto bytesRead = input.gcount();
        if (bytesRead < 0 || input.bad() ||
            (input.fail() && !input.eof())) {
            return PackageResult<HashStreamResult>::Failure(
                CryptoStreamError("Reading SHA-256 input failed."));
        }
        if (bytesRead == 0) {
            break;
        }

        const auto count = static_cast<std::uint64_t>(bytesRead);
        if (result.inputSize > expectedSize ||
            count > expectedSize - result.inputSize) {
            return PackageResult<HashStreamResult>::Failure(
                CryptoStreamError(
                    "SHA-256 input size does not match its declaration."));
        }
        status = BCryptHashData(
            hash.get(),
            inputBuffer.data(),
            static_cast<ULONG>(bytesRead),
            0);
        if (!NtSucceeded(status)) {
            return PackageResult<HashStreamResult>::Failure(
                CryptoError("BCryptHashData", status));
        }
        result.inputSize += count;
    }
    if (!input.eof() || result.inputSize != expectedSize) {
        return PackageResult<HashStreamResult>::Failure(
            CryptoStreamError(
                "SHA-256 input size does not match its declaration."));
    }

    status = BCryptFinishHash(
        hash.get(),
        result.digest.data(),
        static_cast<ULONG>(result.digest.size()),
        0);
    if (!NtSucceeded(status)) {
        return PackageResult<HashStreamResult>::Failure(
            CryptoError("BCryptFinishHash", status));
    }
    return PackageResult<HashStreamResult>::Success(result);
}

PackageResult<KeyHandle> CreateAesKey(
    AlgorithmHandle& algorithm,
    SecureBuffer& keyObject,
    const SecureBuffer& key) {
    if (key.size() != aes256KeySize) {
        return PackageResult<KeyHandle>::Failure(
            CryptoInputError("AES-256-GCM requires a 32-byte key."));
    }

    NTSTATUS status = BCryptOpenAlgorithmProvider(
        algorithm.out(),
        BCRYPT_AES_ALGORITHM,
        nullptr,
        0);
    if (!NtSucceeded(status)) {
        return PackageResult<KeyHandle>::Failure(
            CryptoError("BCryptOpenAlgorithmProvider(AES)", status));
    }

    status = BCryptSetProperty(
        algorithm.get(),
        BCRYPT_CHAINING_MODE,
        reinterpret_cast<PUCHAR>(
            const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
        sizeof(BCRYPT_CHAIN_MODE_GCM),
        0);
    if (!NtSucceeded(status)) {
        return PackageResult<KeyHandle>::Failure(
            CryptoError("BCryptSetProperty(GCM)", status));
    }

    BCRYPT_KEY_LENGTHS_STRUCT tagLengths{};
    ULONG bytesWritten = 0;
    status = BCryptGetProperty(
        algorithm.get(),
        BCRYPT_AUTH_TAG_LENGTH,
        reinterpret_cast<PUCHAR>(&tagLengths),
        sizeof(tagLengths),
        &bytesWritten,
        0);
    if (!NtSucceeded(status) ||
        bytesWritten != sizeof(tagLengths) ||
        gcmTagSize < tagLengths.dwMinLength ||
        gcmTagSize > tagLengths.dwMaxLength ||
        (tagLengths.dwIncrement != 0 &&
            ((gcmTagSize - tagLengths.dwMinLength) %
                tagLengths.dwIncrement) != 0)) {
        return PackageResult<KeyHandle>::Failure(
            CryptoInputError(
                "CNG AES-GCM provider does not support a 16-byte tag."));
    }

    const auto objectSize =
        GetDwordProperty(algorithm.get(), BCRYPT_OBJECT_LENGTH);
    if (!objectSize) {
        return PackageResult<KeyHandle>::Failure(objectSize.error());
    }
    keyObject = SecureBuffer::FromBytes(
        std::vector<std::uint8_t>(objectSize.value()));

    KeyHandle result;
    status = BCryptGenerateSymmetricKey(
        algorithm.get(),
        result.out(),
        keyObject.data(),
        static_cast<ULONG>(keyObject.size()),
        const_cast<PUCHAR>(key.data()),
        static_cast<ULONG>(key.size()),
        0);
    if (!NtSucceeded(status)) {
        return PackageResult<KeyHandle>::Failure(
            CryptoError("BCryptGenerateSymmetricKey", status));
    }
    return PackageResult<KeyHandle>::Success(std::move(result));
}

PackageResult<void*> ValidateAeadInputSizes(
    const std::size_t payloadSize,
    const std::size_t additionalDataSize) {
    if (!FitsCngSize(payloadSize) || !FitsCngSize(additionalDataSize)) {
        return PackageResult<void*>::Failure(
            CryptoInputError("AES-GCM input exceeds the CNG size limit."));
    }
    return PackageResult<void*>::Success(nullptr);
}

} // namespace

PackageResult<Sha256Digest> CngCryptoProvider::Sha256(
    const std::vector<std::uint8_t>& input) const {
    return ComputeSha256(input, nullptr);
}

PackageResult<HashStreamResult> CngCryptoProvider::Sha256Stream(
    std::istream& input,
    const std::uint64_t expectedSize) const {
    return ComputeSha256Stream(input, expectedSize);
}

PackageResult<Sha256Digest> CngCryptoProvider::HmacSha256(
    const SecureBuffer& key,
    const std::vector<std::uint8_t>& input) const {
    return ComputeSha256(input, &key);
}

PackageResult<SecureBuffer> CngCryptoProvider::HkdfSha256(
    const SecureBuffer& inputKeyMaterial,
    const std::vector<std::uint8_t>& salt,
    const std::vector<std::uint8_t>& info,
    const std::size_t outputSize) const {
    constexpr std::size_t maximumOutputSize = 255 * sha256Size;
    if (outputSize > maximumOutputSize) {
        return PackageResult<SecureBuffer>::Failure(
            CryptoInputError("HKDF output exceeds the RFC 5869 limit."));
    }

    const std::vector<std::uint8_t> effectiveSalt =
        salt.empty()
        ? std::vector<std::uint8_t>(sha256Size, 0)
        : salt;
    const auto saltKey = SecureBuffer::FromBytes(effectiveSalt);
    auto inputKeyBytes = inputKeyMaterial.CopyBytes();
    auto prkDigest = HmacSha256(saltKey, inputKeyBytes);
    if (!inputKeyBytes.empty()) {
        SecureZeroMemory(inputKeyBytes.data(), inputKeyBytes.size());
    }
    if (!prkDigest) {
        return PackageResult<SecureBuffer>::Failure(prkDigest.error());
    }
    auto prk = SecureBuffer::FromBytes(std::vector<std::uint8_t>(
        prkDigest.value().begin(),
        prkDigest.value().end()));
    SecureZeroMemory(prkDigest.value().data(), prkDigest.value().size());

    std::vector<std::uint8_t> output;
    output.reserve(outputSize);
    Sha256Digest previous{};
    std::size_t previousSize = 0;
    std::vector<std::uint8_t> hmacInput;
    for (std::uint16_t counter = 1; output.size() < outputSize; ++counter) {
        hmacInput.clear();
        hmacInput.reserve(previousSize + info.size() + 1);
        hmacInput.insert(
            hmacInput.end(),
            previous.begin(),
            previous.begin() + previousSize);
        hmacInput.insert(hmacInput.end(), info.begin(), info.end());
        hmacInput.push_back(static_cast<std::uint8_t>(counter));

        const auto block = HmacSha256(prk, hmacInput);
        if (!block) {
            SecureZeroMemory(hmacInput.data(), hmacInput.size());
            SecureZeroMemory(previous.data(), previous.size());
            return PackageResult<SecureBuffer>::Failure(block.error());
        }
        previous = block.value();
        previousSize = previous.size();

        const auto remaining = outputSize - output.size();
        const auto bytesToCopy = std::min(remaining, previous.size());
        output.insert(
            output.end(),
            previous.begin(),
            previous.begin() + bytesToCopy);
    }

    if (!hmacInput.empty()) {
        SecureZeroMemory(hmacInput.data(), hmacInput.size());
    }
    SecureZeroMemory(previous.data(), previous.size());
    return PackageResult<SecureBuffer>::Success(
        SecureBuffer::FromBytes(std::move(output)));
}

PackageResult<AeadCiphertext> CngCryptoProvider::Aes256GcmEncrypt(
    const SecureBuffer& key,
    const AesGcmNonce& nonce,
    const std::vector<std::uint8_t>& plaintext,
    const std::vector<std::uint8_t>& additionalData) const {
    const auto sizes =
        ValidateAeadInputSizes(plaintext.size(), additionalData.size());
    if (!sizes) {
        return PackageResult<AeadCiphertext>::Failure(sizes.error());
    }

    AlgorithmHandle algorithm;
    SecureBuffer keyObject;
    auto generatedKey = CreateAesKey(algorithm, keyObject, key);
    if (!generatedKey) {
        return PackageResult<AeadCiphertext>::Failure(generatedKey.error());
    }

    AeadCiphertext output;
    output.ciphertext.resize(plaintext.size());
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authenticationInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authenticationInfo);
    authenticationInfo.pbNonce =
        const_cast<PUCHAR>(nonce.data());
    authenticationInfo.cbNonce =
        static_cast<ULONG>(nonce.size());
    authenticationInfo.pbAuthData = MutableBytes(additionalData);
    authenticationInfo.cbAuthData =
        static_cast<ULONG>(additionalData.size());
    authenticationInfo.pbTag = output.tag.data();
    authenticationInfo.cbTag =
        static_cast<ULONG>(output.tag.size());

    ULONG bytesWritten = 0;
    const NTSTATUS status = BCryptEncrypt(
        generatedKey.value().get(),
        MutableBytes(plaintext),
        static_cast<ULONG>(plaintext.size()),
        &authenticationInfo,
        nullptr,
        0,
        output.ciphertext.empty() ? nullptr : output.ciphertext.data(),
        static_cast<ULONG>(output.ciphertext.size()),
        &bytesWritten,
        0);
    if (!NtSucceeded(status) || bytesWritten != output.ciphertext.size()) {
        return PackageResult<AeadCiphertext>::Failure(
            CryptoError("BCryptEncrypt(AES-256-GCM)", status));
    }
    return PackageResult<AeadCiphertext>::Success(std::move(output));
}

PackageResult<AeadEncryptStreamResult>
CngCryptoProvider::Aes256GcmEncryptStream(
    const SecureBuffer& key,
    const AesGcmNonce& nonce,
    std::istream& input,
    std::ostream& output,
    const std::vector<std::uint8_t>& additionalData,
    const std::uint64_t expectedPlaintextSize) const {
    if (!FitsCngSize(additionalData.size())) {
        return PackageResult<AeadEncryptStreamResult>::Failure(
            CryptoInputError(
                "AES-GCM additional data exceeds the CNG size limit."));
    }

    AlgorithmHandle algorithm;
    SecureBuffer keyObject;
    auto generatedKey = CreateAesKey(algorithm, keyObject, key);
    if (!generatedKey) {
        return PackageResult<AeadEncryptStreamResult>::Failure(
            generatedKey.error());
    }

    AeadEncryptStreamResult result;
    auto macContext = SecureBuffer::FromBytes(
        std::vector<std::uint8_t>(gcmTagSize));
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authenticationInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authenticationInfo);
    authenticationInfo.pbNonce =
        const_cast<PUCHAR>(nonce.data());
    authenticationInfo.cbNonce =
        static_cast<ULONG>(nonce.size());
    authenticationInfo.pbAuthData = MutableBytes(additionalData);
    authenticationInfo.cbAuthData =
        static_cast<ULONG>(additionalData.size());
    authenticationInfo.pbTag = result.tag.data();
    authenticationInfo.cbTag =
        static_cast<ULONG>(result.tag.size());
    authenticationInfo.pbMacContext = macContext.data();
    authenticationInfo.cbMacContext =
        static_cast<ULONG>(macContext.size());

    if (expectedPlaintextSize == 0) {
        char unexpected = 0;
        input.read(&unexpected, 1);
        if (input.gcount() != 0 || !input.eof()) {
            return PackageResult<AeadEncryptStreamResult>::Failure(
                CryptoStreamError(
                    "AES-GCM input size does not match its declaration."));
        }
        authenticationInfo.pbMacContext = nullptr;
        authenticationInfo.cbMacContext = 0;
        ULONG bytesWritten = 0;
        const NTSTATUS status = BCryptEncrypt(
            generatedKey.value().get(),
            nullptr,
            0,
            &authenticationInfo,
            nullptr,
            0,
            nullptr,
            0,
            &bytesWritten,
            0);
        if (!NtSucceeded(status) || bytesWritten != 0) {
            return PackageResult<AeadEncryptStreamResult>::Failure(
                CryptoError("BCryptEncrypt(AES-256-GCM)", status));
        }
        return PackageResult<AeadEncryptStreamResult>::Success(result);
    }

    auto inputBuffer = SecureBuffer::FromBytes(
        std::vector<std::uint8_t>(streamBufferSize));
    std::vector<std::uint8_t> outputBuffer(streamBufferSize);
    const auto blockSize =
        GetDwordProperty(algorithm.get(), BCRYPT_BLOCK_LENGTH);
    if (!blockSize || blockSize.value() == 0) {
        return PackageResult<AeadEncryptStreamResult>::Failure(
            blockSize
                ? CryptoInputError(
                    "CNG AES provider returned an invalid block size.")
                : blockSize.error());
    }
    auto chainingIv = SecureBuffer::FromBytes(
        std::vector<std::uint8_t>(blockSize.value()));
    bool hasChainedCall = false;
    while (true) {
        input.read(
            reinterpret_cast<char*>(inputBuffer.data()),
            static_cast<std::streamsize>(inputBuffer.size()));
        const auto bytesRead = input.gcount();
        if (bytesRead < 0 || input.bad() ||
            (input.fail() && !input.eof())) {
            return PackageResult<AeadEncryptStreamResult>::Failure(
                CryptoStreamError("Reading AES-GCM input failed."));
        }
        if (bytesRead == 0) {
            break;
        }

        const auto count = static_cast<std::uint64_t>(bytesRead);
        if (result.inputSize > expectedPlaintextSize ||
            count > expectedPlaintextSize - result.inputSize) {
            return PackageResult<AeadEncryptStreamResult>::Failure(
                CryptoStreamError(
                    "AES-GCM input size does not match its declaration."));
        }
        const bool isFinalChunk =
            count == expectedPlaintextSize - result.inputSize;
        if (isFinalChunk) {
            const auto next = input.peek();
            if (next != std::char_traits<char>::eof()) {
                return PackageResult<AeadEncryptStreamResult>::Failure(
                    CryptoStreamError(
                        "AES-GCM input size does not match its declaration."));
            }
            if (!input.eof()) {
                return PackageResult<AeadEncryptStreamResult>::Failure(
                    CryptoStreamError("Reading AES-GCM input failed."));
            }
            authenticationInfo.dwFlags &=
                ~BCRYPT_AUTH_MODE_CHAIN_CALLS_FLAG;
            if (!hasChainedCall) {
                authenticationInfo.pbMacContext = nullptr;
                authenticationInfo.cbMacContext = 0;
            }
        } else {
            authenticationInfo.dwFlags |=
                BCRYPT_AUTH_MODE_CHAIN_CALLS_FLAG;
        }

        ULONG bytesWritten = 0;
        const NTSTATUS status = BCryptEncrypt(
            generatedKey.value().get(),
            inputBuffer.data(),
            static_cast<ULONG>(bytesRead),
            &authenticationInfo,
            chainingIv.data(),
            static_cast<ULONG>(chainingIv.size()),
            outputBuffer.data(),
            static_cast<ULONG>(outputBuffer.size()),
            &bytesWritten,
            0);
        if (!NtSucceeded(status) ||
            bytesWritten != static_cast<ULONG>(bytesRead)) {
            return PackageResult<AeadEncryptStreamResult>::Failure(
                CryptoError(
                    isFinalChunk
                        ? "BCryptEncrypt(AES-256-GCM final chunk)"
                        : "BCryptEncrypt(AES-256-GCM chained chunk)",
                    status));
        }
        output.write(
            reinterpret_cast<const char*>(outputBuffer.data()),
            static_cast<std::streamsize>(bytesWritten));
        if (!output) {
            return PackageResult<AeadEncryptStreamResult>::Failure(
                CryptoStreamError("Writing AES-GCM output failed."));
        }

        result.inputSize += count;
        result.outputSize += bytesWritten;
        authenticationInfo.pbAuthData = nullptr;
        authenticationInfo.cbAuthData = 0;
        if (isFinalChunk) {
            output.flush();
            if (!output) {
                return PackageResult<AeadEncryptStreamResult>::Failure(
                    CryptoStreamError("Flushing AES-GCM output failed."));
            }
            return PackageResult<AeadEncryptStreamResult>::Success(result);
        }
        hasChainedCall = true;
    }
    return PackageResult<AeadEncryptStreamResult>::Failure(
        CryptoStreamError(
            "AES-GCM input size does not match its declaration."));
}

PackageResult<std::vector<std::uint8_t>>
CngCryptoProvider::Aes256GcmDecrypt(
    const SecureBuffer& key,
    const AesGcmNonce& nonce,
    const std::vector<std::uint8_t>& ciphertext,
    const std::vector<std::uint8_t>& additionalData,
    const AesGcmTag& tag) const {
    const auto sizes =
        ValidateAeadInputSizes(ciphertext.size(), additionalData.size());
    if (!sizes) {
        return PackageResult<std::vector<std::uint8_t>>::Failure(
            sizes.error());
    }

    AlgorithmHandle algorithm;
    SecureBuffer keyObject;
    auto generatedKey = CreateAesKey(algorithm, keyObject, key);
    if (!generatedKey) {
        return PackageResult<std::vector<std::uint8_t>>::Failure(
            generatedKey.error());
    }

    auto mutableTag = tag;
    std::vector<std::uint8_t> plaintext(ciphertext.size());
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authenticationInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authenticationInfo);
    authenticationInfo.pbNonce =
        const_cast<PUCHAR>(nonce.data());
    authenticationInfo.cbNonce =
        static_cast<ULONG>(nonce.size());
    authenticationInfo.pbAuthData = MutableBytes(additionalData);
    authenticationInfo.cbAuthData =
        static_cast<ULONG>(additionalData.size());
    authenticationInfo.pbTag = mutableTag.data();
    authenticationInfo.cbTag =
        static_cast<ULONG>(mutableTag.size());

    ULONG bytesWritten = 0;
    const NTSTATUS status = BCryptDecrypt(
        generatedKey.value().get(),
        MutableBytes(ciphertext),
        static_cast<ULONG>(ciphertext.size()),
        &authenticationInfo,
        nullptr,
        0,
        plaintext.empty() ? nullptr : plaintext.data(),
        static_cast<ULONG>(plaintext.size()),
        &bytesWritten,
        0);
    if (!NtSucceeded(status) || bytesWritten != plaintext.size()) {
        if (!plaintext.empty()) {
            SecureZeroMemory(plaintext.data(), plaintext.size());
        }
        return PackageResult<std::vector<std::uint8_t>>::Failure(
            AuthenticationError());
    }
    return PackageResult<std::vector<std::uint8_t>>::Success(
        std::move(plaintext));
}

PackageResult<std::vector<std::uint8_t>> CngCryptoProvider::RandomBytes(
    const std::size_t size) const {
    if (!FitsCngSize(size)) {
        return PackageResult<std::vector<std::uint8_t>>::Failure(
            CryptoInputError("Random byte request exceeds the CNG size limit."));
    }

    std::vector<std::uint8_t> output(size);
    if (output.empty()) {
        return PackageResult<std::vector<std::uint8_t>>::Success(
            std::move(output));
    }

    const NTSTATUS status = BCryptGenRandom(
        nullptr,
        output.data(),
        static_cast<ULONG>(output.size()),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (!NtSucceeded(status)) {
        return PackageResult<std::vector<std::uint8_t>>::Failure(
            CryptoError("BCryptGenRandom", status));
    }
    return PackageResult<std::vector<std::uint8_t>>::Success(
        std::move(output));
}

} // namespace dbp::package
