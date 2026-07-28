#include <gtest/gtest.h>

#include "dbp/package/KeyProvider.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <type_traits>
#include <vector>

namespace {

using namespace dbp::package;

class TemporaryKeyDirectory {
public:
    TemporaryKeyDirectory() {
        static std::atomic<std::uint64_t> sequence{0};
        const auto uniqueValue =
            static_cast<std::uint64_t>(
                std::chrono::steady_clock::now().time_since_epoch().count()) ^
            sequence.fetch_add(1, std::memory_order_relaxed);
        path_ = std::filesystem::temp_directory_path() /
            ("dbp-package-keys-" + std::to_string(uniqueValue));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryKeyDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    TemporaryKeyDirectory(const TemporaryKeyDirectory&) = delete;
    TemporaryKeyDirectory& operator=(const TemporaryKeyDirectory&) = delete;

    std::filesystem::path Write(
        const std::string& name,
        const std::vector<std::uint8_t>& bytes) const {
        const auto path = path_ / name;
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        output.close();
        EXPECT_TRUE(output);
        return path;
    }

    const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

KeyId MakeKeyId(const std::uint8_t firstByte) {
    KeyId id{};
    id.front() = firstByte;
    return id;
}

PackageId MakePackageId() {
    PackageId id{};
    for (std::size_t index = 0; index < id.size(); ++index) {
        id[index] = static_cast<std::uint8_t>(index);
    }
    return id;
}

std::vector<std::uint8_t> MasterKeyBytes() {
    std::vector<std::uint8_t> bytes(32);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::uint8_t>(0x80U + index);
    }
    return bytes;
}

std::vector<std::uint8_t> Hex(const std::string& text) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(text.size() / 2);
    for (std::size_t index = 0; index < text.size(); index += 2) {
        bytes.push_back(static_cast<std::uint8_t>(
            std::stoul(text.substr(index, 2), nullptr, 16)));
    }
    return bytes;
}

TEST(PackageKeysTest, MemoryProviderResolvesOnlyItsConfiguredKeyId) {
    const auto configuredId = MakeKeyId(0x11);
    const auto otherId = MakeKeyId(0x22);
    const auto expected = MasterKeyBytes();
    MemoryKeyProvider provider(
        configuredId,
        SecureBuffer::FromBytes(expected));

    const auto resolved = provider.Resolve(configuredId);

    ASSERT_TRUE(resolved) << resolved.error().message;
    EXPECT_EQ(resolved.value().CopyBytes(), expected);

    const auto missing = provider.Resolve(otherId);
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code, PackageErrorCode::MissingKey);
}

TEST(PackageKeysTest, ProvidersRejectMasterKeysThatAreNotExactly32Bytes) {
    const auto keyId = MakeKeyId(0x31);
    MemoryKeyProvider memory(
        keyId,
        SecureBuffer::FromBytes(std::vector<std::uint8_t>(31, 0xAA)));
    const auto invalidMemoryKey = memory.Resolve(keyId);
    ASSERT_FALSE(invalidMemoryKey);
    EXPECT_EQ(
        invalidMemoryKey.error().code,
        PackageErrorCode::MissingKey);

    TemporaryKeyDirectory directory;
    for (const std::size_t size : {0U, 31U, 33U}) {
        const auto path = directory.Write(
            "invalid-" + std::to_string(size) + ".key",
            std::vector<std::uint8_t>(size, 0xBB));
        FileKeyProvider file(keyId, path);
        const auto invalidFileKey = file.Resolve(keyId);
        ASSERT_FALSE(invalidFileKey);
        EXPECT_EQ(
            invalidFileKey.error().code,
            PackageErrorCode::MissingKey);
    }
}

TEST(PackageKeysTest, FileProviderReadsExactly32BinaryBytesForMatchingId) {
    TemporaryKeyDirectory directory;
    const auto keyId = MakeKeyId(0x41);
    const auto expected = MasterKeyBytes();
    const auto path = directory.Write("master.key", expected);
    FileKeyProvider provider(keyId, path);

    const auto resolved = provider.Resolve(keyId);

    ASSERT_TRUE(resolved) << resolved.error().message;
    EXPECT_EQ(resolved.value().CopyBytes(), expected);
    EXPECT_EQ(
        provider.Resolve(MakeKeyId(0x42)).error().code,
        PackageErrorCode::MissingKey);
}

TEST(PackageKeysTest, FileProviderFailsSafelyForMissingAndUnreadablePaths) {
    TemporaryKeyDirectory directory;
    const auto keyId = MakeKeyId(0x51);
    FileKeyProvider missing(keyId, directory.path() / "missing.key");
    const auto missingResult = missing.Resolve(keyId);
    ASSERT_FALSE(missingResult);
    EXPECT_EQ(missingResult.error().code, PackageErrorCode::MissingKey);
    EXPECT_EQ(
        missingResult.error().message.find("missing.key"),
        std::string::npos);

    FileKeyProvider directoryPath(keyId, directory.path());
    const auto unreadableResult = directoryPath.Resolve(keyId);
    ASSERT_FALSE(unreadableResult);
    EXPECT_EQ(unreadableResult.error().code, PackageErrorCode::MissingKey);
}

TEST(PackageKeysTest, DerivesSeparatedManifestAndCanonicalEntryKeys) {
    CngCryptoProvider crypto;
    PackageKeyDeriver deriver(crypto);
    const auto packageId = MakePackageId();
    const auto masterKey =
        SecureBuffer::FromBytes(MasterKeyBytes());

    const auto manifest =
        deriver.DeriveManifestKey(masterKey, packageId);
    const auto firstEntry =
        deriver.DeriveEntryKey(masterKey, packageId, "media/one.dat");
    const auto secondEntry =
        deriver.DeriveEntryKey(masterKey, packageId, "media/two.dat");
    const auto repeatedEntry =
        deriver.DeriveEntryKey(masterKey, packageId, "media/one.dat");

    ASSERT_TRUE(manifest) << manifest.error().message;
    ASSERT_TRUE(firstEntry) << firstEntry.error().message;
    ASSERT_TRUE(secondEntry) << secondEntry.error().message;
    ASSERT_TRUE(repeatedEntry) << repeatedEntry.error().message;
    EXPECT_EQ(manifest.value().size(), 32U);
    EXPECT_NE(
        manifest.value().CopyBytes(),
        firstEntry.value().CopyBytes());
    EXPECT_NE(
        firstEntry.value().CopyBytes(),
        secondEntry.value().CopyBytes());
    EXPECT_EQ(
        firstEntry.value().CopyBytes(),
        repeatedEntry.value().CopyBytes());
    EXPECT_EQ(
        manifest.value().CopyBytes(),
        Hex("70d145f95f3b575f8018006d7afeff6a"
            "b82cdb542adfca6a954a93f9100eacb6"));
    EXPECT_EQ(
        firstEntry.value().CopyBytes(),
        Hex("31292fe23e2f56a6747b75c3a03a53f1"
            "5192d0deb98c93457f1c29fef32bc8c5"));
}

TEST(PackageKeysTest, EntryDerivationRejectsNonCanonicalPaths) {
    CngCryptoProvider crypto;
    PackageKeyDeriver deriver(crypto);
    const auto masterKey =
        SecureBuffer::FromBytes(MasterKeyBytes());

    const auto result = deriver.DeriveEntryKey(
        masterKey,
        MakePackageId(),
        "media\\unsafe.dat");

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, PackageErrorCode::UnsafePath);
}

TEST(PackageKeysTest, KeyOwningTypesCannotBeCopied) {
    static_assert(!std::is_copy_constructible_v<SecureBuffer>);
    static_assert(!std::is_copy_assignable_v<SecureBuffer>);
    static_assert(!std::is_copy_constructible_v<MemoryKeyProvider>);
    static_assert(!std::is_copy_assignable_v<MemoryKeyProvider>);
    static_assert(!std::is_copy_constructible_v<FileKeyProvider>);
    static_assert(!std::is_copy_assignable_v<FileKeyProvider>);
}

} // namespace
