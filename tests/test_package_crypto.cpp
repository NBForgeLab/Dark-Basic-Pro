#include <gtest/gtest.h>

#include "dbp/package/CryptoProvider.h"
#include "dbp/package/SecureBuffer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

namespace {

using dbp::package::AeadCiphertext;
using dbp::package::CngCryptoProvider;
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
