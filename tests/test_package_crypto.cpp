#include <gtest/gtest.h>

#include "dbp/package/CryptoProvider.h"
#include "dbp/package/SecureBuffer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace {

using dbp::package::AeadCiphertext;
using dbp::package::AesGcmNonce;
using dbp::package::CngCryptoProvider;
using dbp::package::PackageErrorCode;
using dbp::package::SecureBuffer;

std::vector<std::uint8_t> Hex(const std::string& text) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(text.size() / 2);
    for (std::size_t index = 0; index < text.size(); index += 2) {
        bytes.push_back(static_cast<std::uint8_t>(
            std::stoul(text.substr(index, 2), nullptr, 16)));
    }
    return bytes;
}

template <std::size_t Size>
std::array<std::uint8_t, Size> ArrayFromHex(const std::string& text) {
    const auto bytes = Hex(text);
    EXPECT_EQ(bytes.size(), Size);
    std::array<std::uint8_t, Size> result{};
    std::copy_n(bytes.begin(), std::min(bytes.size(), Size), result.begin());
    return result;
}

TEST(PackageCryptoTest, ComputesSha256KnownAnswer) {
    CngCryptoProvider crypto;
    const std::vector<std::uint8_t> input{'a', 'b', 'c'};

    const auto result = crypto.Sha256(input);

    ASSERT_TRUE(result) << result.error().message;
    EXPECT_EQ(result.value(), ArrayFromHex<32>(
        "ba7816bf8f01cfea414140de5dae2223"
        "b00361a396177a9cb410ff61f20015ad"));
}

TEST(PackageCryptoTest, ComputesHmacSha256KnownAnswer) {
    CngCryptoProvider crypto;
    const auto key = SecureBuffer::FromBytes(
        std::vector<std::uint8_t>(20, 0x0B));
    const std::vector<std::uint8_t> input{
        'H', 'i', ' ', 'T', 'h', 'e', 'r', 'e',
    };

    const auto result = crypto.HmacSha256(key, input);

    ASSERT_TRUE(result) << result.error().message;
    EXPECT_EQ(result.value(), ArrayFromHex<32>(
        "b0344c61d8db38535ca8afceaf0bf12b"
        "881dc200c9833da726e9376c2e32cff7"));
}

TEST(PackageCryptoTest, ComputesRfc5869HkdfSha256CaseOne) {
    CngCryptoProvider crypto;
    const auto ikm = SecureBuffer::FromBytes(
        std::vector<std::uint8_t>(22, 0x0B));
    const auto salt = Hex("000102030405060708090a0b0c");
    const auto info = Hex("f0f1f2f3f4f5f6f7f8f9");

    const auto result = crypto.HkdfSha256(ikm, salt, info, 42);

    ASSERT_TRUE(result) << result.error().message;
    EXPECT_EQ(result.value().CopyBytes(), Hex(
        "3cb25f25faacd57a90434f64d0362f2a"
        "2d2d0a90cf1a5a4c5db02d56ecc4c5bf"
        "34007208d5b887185865"));
}

TEST(PackageCryptoTest, EncryptsAndDecryptsNistAes256GcmVector) {
    CngCryptoProvider crypto;
    const auto key = SecureBuffer::FromBytes(
        std::vector<std::uint8_t>(32, 0));
    const std::array<std::uint8_t, 12> nonce{};
    const std::vector<std::uint8_t> plaintext(16, 0);
    const std::vector<std::uint8_t> aad;

    const auto encrypted =
        crypto.Aes256GcmEncrypt(key, nonce, plaintext, aad);

    ASSERT_TRUE(encrypted) << encrypted.error().message;
    EXPECT_EQ(encrypted.value().ciphertext, Hex(
        "cea7403d4d606b6e074ec5d3baf39d18"));
    EXPECT_EQ(encrypted.value().tag, ArrayFromHex<16>(
        "d0d1c8a799996bf0265b98b5d48ab919"));

    const auto decrypted = crypto.Aes256GcmDecrypt(
        key,
        nonce,
        encrypted.value().ciphertext,
        aad,
        encrypted.value().tag);
    ASSERT_TRUE(decrypted) << decrypted.error().message;
    EXPECT_EQ(decrypted.value(), plaintext);
}

TEST(PackageCryptoTest, RejectsWrongAesGcmTagWithoutPlaintext) {
    CngCryptoProvider crypto;
    const auto key = SecureBuffer::FromBytes(
        std::vector<std::uint8_t>(32, 0x42));
    const std::array<std::uint8_t, 12> nonce{};
    const std::vector<std::uint8_t> plaintext{'s', 'e', 'c', 'r', 'e', 't'};
    const std::vector<std::uint8_t> aad{'m', 'e', 't', 'a'};
    const auto encrypted =
        crypto.Aes256GcmEncrypt(key, nonce, plaintext, aad);
    ASSERT_TRUE(encrypted);
    auto wrongTag = encrypted.value().tag;
    wrongTag.front() ^= 0x01;

    const auto decrypted = crypto.Aes256GcmDecrypt(
        key,
        nonce,
        encrypted.value().ciphertext,
        aad,
        wrongTag);

    EXPECT_FALSE(decrypted);
}

TEST(PackageCryptoTest, GeneratesRequestedRandomByteCount) {
    CngCryptoProvider crypto;

    const auto first = crypto.RandomBytes(32);
    const auto second = crypto.RandomBytes(32);

    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_EQ(first.value().size(), 32U);
    EXPECT_EQ(second.value().size(), 32U);
    EXPECT_NE(first.value(), second.value());
}

bool secureEraseWasCalled = false;
bool secureEraseObservedZeros = false;

void ObservingSecureErase(void* memory, const std::size_t size) noexcept {
    secureEraseWasCalled = true;
    auto* writable = static_cast<volatile std::uint8_t*>(memory);
    for (std::size_t index = 0; index < size; ++index) {
        writable[index] = 0;
    }
    const auto* bytes = static_cast<const std::uint8_t*>(memory);
    secureEraseObservedZeros =
        std::all_of(bytes, bytes + size, [](const std::uint8_t byte) {
            return byte == 0;
        });
}

TEST(PackageCryptoTest, StreamsSha256AcrossBoundedBuffers) {
    CngCryptoProvider crypto;
    std::string inputBytes;
    inputBytes.reserve(3 * 1024 * 1024);
    for (std::size_t index = 0; index < 3 * 1024 * 1024; ++index) {
        inputBytes.push_back(static_cast<char>(index % 251));
    }
    std::istringstream input(
        inputBytes,
        std::ios::in | std::ios::binary);
    const auto expected = crypto.Sha256(std::vector<std::uint8_t>(
        inputBytes.begin(),
        inputBytes.end()));
    ASSERT_TRUE(expected);

    const auto streamed =
        crypto.Sha256Stream(input, inputBytes.size());

    ASSERT_TRUE(streamed) << streamed.error().message;
    EXPECT_EQ(streamed.value().digest, expected.value());
    EXPECT_EQ(streamed.value().inputSize, inputBytes.size());
}

TEST(PackageCryptoTest, StreamingAesGcmMatchesOneShotEncryption) {
    CngCryptoProvider crypto;
    const auto key = SecureBuffer::FromBytes(
        std::vector<std::uint8_t>(32, 0x5C));
    const auto nonce = ArrayFromHex<12>(
        "000102030405060708090a0b");
    const std::vector<std::uint8_t> additionalData{
        's', 't', 'r', 'e', 'a', 'm', '-', 'a', 'a', 'd',
    };
    std::string plaintext;
    plaintext.reserve(2 * 1024 * 1024 + 17);
    for (std::size_t index = 0; index < 2 * 1024 * 1024 + 17; ++index) {
        plaintext.push_back(static_cast<char>(index % 239));
    }
    const std::vector<std::uint8_t> plaintextBytes(
        plaintext.begin(),
        plaintext.end());
    const auto expected = crypto.Aes256GcmEncrypt(
        key,
        nonce,
        plaintextBytes,
        additionalData);
    ASSERT_TRUE(expected);

    std::istringstream input(
        plaintext,
        std::ios::in | std::ios::binary);
    std::ostringstream output(
        std::ios::out | std::ios::binary);
    const auto streamed = crypto.Aes256GcmEncryptStream(
        key,
        nonce,
        input,
        output,
        additionalData,
        plaintext.size());

    ASSERT_TRUE(streamed) << streamed.error().message;
    const auto ciphertext = output.str();
    EXPECT_EQ(
        std::vector<std::uint8_t>(
            ciphertext.begin(),
            ciphertext.end()),
        expected.value().ciphertext);
    EXPECT_EQ(streamed.value().tag, expected.value().tag);
    EXPECT_EQ(streamed.value().inputSize, plaintext.size());
    EXPECT_EQ(streamed.value().outputSize, plaintext.size());
}

TEST(PackageCryptoTest, StreamingAesGcmHandlesEmptyInputAndSizeMismatch) {
    CngCryptoProvider crypto;
    const auto key = SecureBuffer::FromBytes(
        std::vector<std::uint8_t>(32, 0x17));
    const AesGcmNonce nonce{};
    const std::vector<std::uint8_t> additionalData{'a', 'a', 'd'};
    std::istringstream emptyInput(
        std::string{},
        std::ios::in | std::ios::binary);
    std::ostringstream output(
        std::ios::out | std::ios::binary);

    const auto empty = crypto.Aes256GcmEncryptStream(
        key,
        nonce,
        emptyInput,
        output,
        additionalData,
        0);

    ASSERT_TRUE(empty) << empty.error().message;
    const auto expected = crypto.Aes256GcmEncrypt(
        key,
        nonce,
        {},
        additionalData);
    ASSERT_TRUE(expected);
    EXPECT_EQ(empty.value().tag, expected.value().tag);
    EXPECT_TRUE(output.str().empty());

    std::istringstream shortInput(
        std::string{"short"},
        std::ios::in | std::ios::binary);
    std::ostringstream ignored(
        std::ios::out | std::ios::binary);
    EXPECT_FALSE(crypto.Aes256GcmEncryptStream(
        key,
        nonce,
        shortInput,
        ignored,
        additionalData,
        6));
}

TEST(PackageCryptoTest, StreamingAesGcmDecryptsAndAuthenticatesAtFinalChunk) {
    CngCryptoProvider crypto;
    const auto key = SecureBuffer::FromBytes(
        std::vector<std::uint8_t>(32, 0xA7));
    const auto nonce = ArrayFromHex<12>(
        "101112131415161718191a1b");
    const std::vector<std::uint8_t> additionalData{
        'd', 'e', 'c', 'r', 'y', 'p', 't', '-', 'a', 'a', 'd',
    };
    std::string plaintext;
    plaintext.reserve(2 * 1024 * 1024 + 29);
    for (std::size_t index = 0; index < 2 * 1024 * 1024 + 29; ++index) {
        plaintext.push_back(static_cast<char>(index % 233));
    }
    const auto encrypted = crypto.Aes256GcmEncrypt(
        key,
        nonce,
        std::vector<std::uint8_t>(
            plaintext.begin(),
            plaintext.end()),
        additionalData);
    ASSERT_TRUE(encrypted);
    const std::string ciphertext(
        encrypted.value().ciphertext.begin(),
        encrypted.value().ciphertext.end());
    std::istringstream input(
        ciphertext,
        std::ios::in | std::ios::binary);
    std::ostringstream output(
        std::ios::out | std::ios::binary);

    const auto decrypted = crypto.Aes256GcmDecryptStream(
        key,
        nonce,
        input,
        output,
        additionalData,
        encrypted.value().tag,
        ciphertext.size());

    ASSERT_TRUE(decrypted) << decrypted.error().message;
    EXPECT_EQ(output.str(), plaintext);
    EXPECT_EQ(decrypted.value().inputSize, ciphertext.size());
    EXPECT_EQ(decrypted.value().outputSize, plaintext.size());

    auto wrongTag = encrypted.value().tag;
    wrongTag.back() ^= 1;
    std::istringstream tamperedInput(
        ciphertext,
        std::ios::in | std::ios::binary);
    std::ostringstream privateOutput(
        std::ios::out | std::ios::binary);
    const auto rejected = crypto.Aes256GcmDecryptStream(
        key,
        nonce,
        tamperedInput,
        privateOutput,
        additionalData,
        wrongTag,
        ciphertext.size());
    ASSERT_FALSE(rejected);
    EXPECT_EQ(
        rejected.error().code,
        PackageErrorCode::AuthenticationFailed);
}

TEST(PackageCryptoTest, SecureBufferIsMoveOnlyAndErasesOwnedBytes) {
    static_assert(!std::is_copy_constructible_v<SecureBuffer>);
    static_assert(!std::is_copy_assignable_v<SecureBuffer>);
    static_assert(std::is_move_constructible_v<SecureBuffer>);
    static_assert(std::is_move_assignable_v<SecureBuffer>);
    secureEraseWasCalled = false;
    secureEraseObservedZeros = false;

    {
        auto buffer = SecureBuffer::FromBytesForTesting(
            std::vector<std::uint8_t>{1, 2, 3, 4},
            &ObservingSecureErase);
        EXPECT_EQ(buffer.size(), 4U);
    }

    EXPECT_TRUE(secureEraseWasCalled);
    EXPECT_TRUE(secureEraseObservedZeros);
}

} // namespace
