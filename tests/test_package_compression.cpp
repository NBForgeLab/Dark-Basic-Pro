#include <gtest/gtest.h>

#include "dbp/package/CompressionCodec.h"

#include <cstdint>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace dbp::package;

TEST(PackageCompressionTest, CompressesAndRestoresHighlyCompressibleData) {
    ZstdCompressionCodec codec;
    const std::vector<std::uint8_t> plaintext(2 * 1024 * 1024, 0x41);

    const auto compressed = codec.CompressIfSmaller(plaintext);

    ASSERT_TRUE(compressed) << compressed.error().message;
    EXPECT_EQ(
        compressed.value().algorithm,
        CompressionAlgorithm::Zstandard);
    EXPECT_LT(compressed.value().bytes.size(), plaintext.size());

    const auto restored = codec.Decompress(
        compressed.value(),
        plaintext.size(),
        plaintext.size());
    ASSERT_TRUE(restored) << restored.error().message;
    EXPECT_EQ(restored.value(), plaintext);
}

TEST(PackageCompressionTest, KeepsIncompressibleDataUncompressed) {
    ZstdCompressionCodec codec;
    std::mt19937 generator(0xDBA5EEDU);
    std::uniform_int_distribution<int> distribution(0, 255);
    std::vector<std::uint8_t> plaintext(64 * 1024);
    for (auto& byte : plaintext) {
        byte = static_cast<std::uint8_t>(distribution(generator));
    }

    const auto compressed = codec.CompressIfSmaller(plaintext);

    ASSERT_TRUE(compressed) << compressed.error().message;
    EXPECT_EQ(compressed.value().algorithm, CompressionAlgorithm::None);
    EXPECT_EQ(compressed.value().bytes, plaintext);
}

TEST(PackageCompressionTest, ProducesDeterministicLevelThreeOutput) {
    ZstdCompressionCodec codec;
    const std::vector<std::uint8_t> plaintext(128 * 1024, 0x5A);

    const auto first = codec.CompressIfSmaller(plaintext);
    const auto second = codec.CompressIfSmaller(plaintext);

    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_EQ(first.value().algorithm, CompressionAlgorithm::Zstandard);
    EXPECT_EQ(first.value().bytes, second.value().bytes);
}

TEST(PackageCompressionTest, StreamsAcrossMultipleBoundedBuffers) {
    ZstdCompressionCodec codec;
    std::string plaintext;
    plaintext.reserve(3 * 1024 * 1024);
    for (std::size_t index = 0; index < 3 * 1024 * 1024; ++index) {
        plaintext.push_back(static_cast<char>('A' + (index % 23)));
    }
    std::istringstream input(
        plaintext,
        std::ios::in | std::ios::binary);
    std::ostringstream compressed(
        std::ios::out | std::ios::binary);

    const auto compression =
        codec.CompressStream(input, compressed, 3);

    ASSERT_TRUE(compression) << compression.error().message;
    EXPECT_EQ(compression.value().inputSize, plaintext.size());
    EXPECT_EQ(
        compression.value().outputSize,
        compressed.str().size());

    std::istringstream compressedInput(
        compressed.str(),
        std::ios::in | std::ios::binary);
    std::ostringstream restored(
        std::ios::out | std::ios::binary);
    const auto decompression = codec.DecompressStream(
        compressedInput,
        restored,
        plaintext.size(),
        plaintext.size());
    ASSERT_TRUE(decompression) << decompression.error().message;
    EXPECT_EQ(restored.str(), plaintext);
}

TEST(PackageCompressionTest, RejectsTruncatedAndCorruptedFrames) {
    ZstdCompressionCodec codec;
    const std::vector<std::uint8_t> plaintext(64 * 1024, 0x33);
    const auto compressed = codec.CompressIfSmaller(plaintext);
    ASSERT_TRUE(compressed);
    ASSERT_EQ(
        compressed.value().algorithm,
        CompressionAlgorithm::Zstandard);

    auto truncated = compressed.value();
    truncated.bytes.resize(truncated.bytes.size() / 2);
    EXPECT_FALSE(codec.Decompress(
        truncated,
        plaintext.size(),
        plaintext.size()));

    auto corrupted = compressed.value();
    corrupted.bytes.front() ^= 0xFF;
    EXPECT_FALSE(codec.Decompress(
        corrupted,
        plaintext.size(),
        plaintext.size()));

    auto trailingBytes = compressed.value();
    trailingBytes.bytes.push_back(0);
    EXPECT_FALSE(codec.Decompress(
        trailingBytes,
        plaintext.size(),
        plaintext.size()));

    auto concatenatedFrame = compressed.value();
    concatenatedFrame.bytes.insert(
        concatenatedFrame.bytes.end(),
        compressed.value().bytes.begin(),
        compressed.value().bytes.end());
    EXPECT_FALSE(codec.Decompress(
        concatenatedFrame,
        plaintext.size(),
        plaintext.size()));
}

TEST(PackageCompressionTest, RejectsSizeMismatchAndExpansionBeyondLimit) {
    ZstdCompressionCodec codec;
    const std::vector<std::uint8_t> plaintext(64 * 1024, 0x22);
    const auto compressed = codec.CompressIfSmaller(plaintext);
    ASSERT_TRUE(compressed);

    EXPECT_FALSE(codec.Decompress(
        compressed.value(),
        plaintext.size() + 1,
        plaintext.size() + 1));
    EXPECT_FALSE(codec.Decompress(
        compressed.value(),
        plaintext.size(),
        plaintext.size() - 1));
}

} // namespace
