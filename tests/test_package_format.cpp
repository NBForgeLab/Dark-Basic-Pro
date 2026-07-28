#include <gtest/gtest.h>

#include "dbp/package/PackageFormat.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using namespace dbp::package;

PackageHeader ValidHeader() {
    PackageHeader header;
    header.flags =
        PackageHeaderFlag::ManifestEncrypted |
        PackageHeaderFlag::PayloadsEncrypted;
    header.entryCount = 1;
    header.manifestOffset = kPackageHeaderSize;
    header.manifestCiphertextSize = 64;
    header.payloadOffset = 224;
    header.payloadSize = 32;
    header.packageId.fill(0x11);
    header.keyId.fill(0x22);
    header.manifestNonce.fill(0x33);
    header.manifestTag.fill(0x44);
    header.manifestPlaintextSha256.fill(0x55);
    return header;
}

ManifestRecord Record(
    std::string path,
    const std::uint64_t payloadOffset,
    const std::uint64_t storedSize,
    const CompressionAlgorithm compression = CompressionAlgorithm::None) {
    ManifestRecord record;
    record.path = std::move(path);
    record.compression = compression;
    record.encryption = EncryptionAlgorithm::Aes256Gcm;
    record.plaintextSize = storedSize;
    record.storedSize = storedSize;
    record.payloadOffset = payloadOffset;
    record.plaintextSha256.fill(0x66);
    record.nonce.fill(0x77);
    record.tag.fill(0x88);
    return record;
}

std::uint32_t ReadUInt32(
    const std::vector<std::uint8_t>& bytes,
    const std::size_t offset) {
    return
        static_cast<std::uint32_t>(bytes[offset]) |
        (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
        (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
        (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
}

void WriteUInt16(
    std::vector<std::uint8_t>& bytes,
    const std::size_t offset,
    const std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
}

void WriteUInt32(
    std::vector<std::uint8_t>& bytes,
    const std::size_t offset,
    const std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index) {
        bytes[offset + index] =
            static_cast<std::uint8_t>(value >> (index * 8U));
    }
}

void WriteUInt64(
    std::vector<std::uint8_t>& bytes,
    const std::size_t offset,
    const std::uint64_t value) {
    for (std::size_t index = 0; index < 8; ++index) {
        bytes[offset + index] =
            static_cast<std::uint8_t>(value >> (index * 8U));
    }
}

TEST(PackageFormatTest, SerializesHeaderAtCanonicalSizeAndOffsets) {
    const auto header = ValidHeader();

    const auto bytes = SerializePackageHeader(header);

    ASSERT_EQ(bytes.size(), kPackageHeaderSize);
    EXPECT_EQ(std::string(bytes.begin(), bytes.begin() + 8),
        std::string("DBPPAK2\0", 8));
    EXPECT_EQ(ReadUInt32(bytes, 12), kPackageHeaderSize);
    EXPECT_EQ(ReadUInt32(bytes, 16), 3U);
    EXPECT_EQ(ReadUInt32(bytes, 20), 1U);
    EXPECT_TRUE(std::equal(
        header.manifestTag.begin(),
        header.manifestTag.end(),
        bytes.begin() + 100));
    EXPECT_TRUE(std::all_of(
        bytes.begin() + 148,
        bytes.end(),
        [](const std::uint8_t byte) { return byte == 0; }));
}

TEST(PackageFormatTest, ParsesValidHeaderAndRejectsStructuralMutations) {
    const auto header = ValidHeader();
    const auto bytes = SerializePackageHeader(header);
    const PackageLimits limits;

    const auto parsed = ParsePackageHeader(bytes, 256, limits);
    ASSERT_TRUE(parsed) << parsed.error().message;
    EXPECT_EQ(parsed.value().payloadOffset, header.payloadOffset);
    EXPECT_EQ(parsed.value().packageId, header.packageId);

    auto wrongMajor = bytes;
    WriteUInt16(wrongMajor, 8, 3);
    EXPECT_FALSE(ParsePackageHeader(wrongMajor, 256, limits));

    auto unknownFlag = bytes;
    WriteUInt32(unknownFlag, 16, 0x80000003U);
    EXPECT_FALSE(ParsePackageHeader(unknownFlag, 256, limits));

    auto missingEncryption = bytes;
    WriteUInt32(missingEncryption, 16, 1U);
    EXPECT_FALSE(ParsePackageHeader(missingEncryption, 256, limits));

    auto nonzeroReserved = bytes;
    nonzeroReserved[159] = 1;
    EXPECT_FALSE(ParsePackageHeader(nonzeroReserved, 256, limits));

    auto overflowingPayload = bytes;
    std::fill(
        overflowingPayload.begin() + 48,
        overflowingPayload.begin() + 56,
        0xFF);
    EXPECT_FALSE(ParsePackageHeader(overflowingPayload, 256, limits));

    auto arbitraryGap = bytes;
    WriteUInt64(arbitraryGap, 40, 240);
    EXPECT_FALSE(ParsePackageHeader(arbitraryGap, 272, limits));

    auto emptyManifest = bytes;
    WriteUInt64(emptyManifest, 32, 0);
    WriteUInt64(emptyManifest, 40, 160);
    EXPECT_FALSE(ParsePackageHeader(emptyManifest, 192, limits));
}

TEST(PackageFormatTest, SerializesAndParsesCanonicalManifest) {
    PackageManifest manifest;
    manifest.records = {
        Record("a/file.dat", 0, 4),
        Record("media/file.dat", 4, 8, CompressionAlgorithm::Zstandard),
    };
    auto header = ValidHeader();
    header.flags |= PackageHeaderFlag::HasCompressedEntries;
    header.entryCount = 2;
    header.payloadSize = 12;

    const auto serialized = SerializeManifest(manifest);

    ASSERT_TRUE(serialized) << serialized.error().message;
    EXPECT_EQ(
        serialized.value().size(),
        kManifestHeaderSize +
            2 * kManifestRecordSize +
            std::string("a/file.dat").size() +
            std::string("media/file.dat").size());
    EXPECT_EQ(ReadUInt32(serialized.value(), 12), 2U);
    const auto parsed =
        ParseManifest(serialized.value(), header, PackageLimits{});
    ASSERT_TRUE(parsed) << parsed.error().message;
    ASSERT_EQ(parsed.value().records.size(), 2U);
    EXPECT_EQ(parsed.value().records[0].path, "a/file.dat");
    EXPECT_EQ(
        parsed.value().records[1].compression,
        CompressionAlgorithm::Zstandard);
}

TEST(PackageFormatTest, RejectsManifestAlgorithmsPathsAndCollisions) {
    PackageManifest duplicate;
    duplicate.records = {
        Record("media/file.dat", 0, 4),
        Record("Media/File.dat", 4, 4),
    };
    EXPECT_FALSE(SerializeManifest(duplicate));

    PackageManifest manifest;
    manifest.records = {Record("safe", 0, 4)};
    auto header = ValidHeader();
    header.payloadSize = 4;
    const auto serialized = SerializeManifest(manifest);
    ASSERT_TRUE(serialized);

    auto badEncryption = serialized.value();
    WriteUInt16(
        badEncryption,
        kManifestHeaderSize + 18,
        2);
    EXPECT_FALSE(ParseManifest(
        badEncryption,
        header,
        PackageLimits{}));

    auto unsafePath = serialized.value();
    const auto stringTableOffset =
        kManifestHeaderSize + kManifestRecordSize;
    std::copy_n(
        reinterpret_cast<const std::uint8_t*>("../x"),
        4,
        unsafePath.begin() + stringTableOffset);
    EXPECT_FALSE(ParseManifest(
        unsafePath,
        header,
        PackageLimits{}));
}

TEST(PackageFormatTest, RejectsManifestExtentAndCompressionFlagMismatch) {
    PackageManifest manifest;
    manifest.records = {
        Record("a", 0, 4),
        Record("b", 4, 4),
    };
    auto header = ValidHeader();
    header.entryCount = 2;
    header.payloadSize = 8;
    const auto serialized = SerializeManifest(manifest);
    ASSERT_TRUE(serialized);

    auto gapped = serialized.value();
    WriteUInt64(
        gapped,
        kManifestHeaderSize + kManifestRecordSize + 40,
        8);
    EXPECT_FALSE(ParseManifest(
        gapped,
        header,
        PackageLimits{}));

    PackageManifest compressed;
    compressed.records = {
        Record("a", 0, 4, CompressionAlgorithm::Zstandard),
    };
    header.entryCount = 1;
    header.payloadSize = 4;
    const auto compressedBytes = SerializeManifest(compressed);
    ASSERT_TRUE(compressedBytes);
    EXPECT_FALSE(ParseManifest(
        compressedBytes.value(),
        header,
        PackageLimits{}));
}

TEST(PackageFormatTest, BuildsCanonicalAuthenticatedData) {
    auto header = ValidHeader();
    const auto headerBytes = SerializePackageHeader(header);

    const auto manifestAad = BuildManifestAdditionalData(header);

    ASSERT_EQ(manifestAad.size(), kPackageHeaderSize);
    EXPECT_TRUE(std::equal(
        headerBytes.begin(),
        headerBytes.begin() + 100,
        manifestAad.begin()));
    EXPECT_TRUE(std::all_of(
        manifestAad.begin() + 100,
        manifestAad.begin() + 116,
        [](const std::uint8_t byte) { return byte == 0; }));
    EXPECT_TRUE(std::equal(
        headerBytes.begin() + 116,
        headerBytes.end(),
        manifestAad.begin() + 116));

    const auto record = Record("media/file.dat", 7, 11);
    const auto entryAad =
        BuildEntryAdditionalData(header.packageId, record);
    EXPECT_EQ(
        entryAad.size(),
        112U + record.path.size());
    EXPECT_EQ(
        std::string(entryAad.begin(), entryAad.begin() + 16),
        "DBP-PAK-v2/entry");
}

} // namespace
