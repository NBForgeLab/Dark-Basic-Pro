#include <gtest/gtest.h>

#include "dbp/package/PackageReader.h"
#include "dbp/package/PackageWriter.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace dbp::package;

class ReaderFixture {
public:
    ReaderFixture() {
        static std::atomic<std::uint64_t> sequence{0};
        const auto uniqueValue =
            static_cast<std::uint64_t>(
                std::chrono::steady_clock::now().time_since_epoch().count()) ^
            sequence.fetch_add(1, std::memory_order_relaxed);
        root_ = std::filesystem::temp_directory_path() /
            ("dbp-package-reader-" + std::to_string(uniqueValue));
        std::filesystem::create_directories(root_);

        keyId_.front() = 0xD2;
        plaintext_.resize(2 * 1024 * 1024 + 37);
        for (std::size_t index = 0; index < plaintext_.size(); ++index) {
            plaintext_[index] =
                static_cast<std::uint8_t>('A' + (index % 19));
        }
        const auto source = Write("source.bin", plaintext_);
        PackageWriteRequest request{
            root_,
            keyId_,
            {{source, "media/source.asset", true}},
        };
        MemoryKeyProvider writeKeys(
            keyId_,
            SecureBuffer::FromBytes(MasterKey()));
        PackageWriter writer(crypto_, compression_, publisher_);
        const auto written = writer.Write(request, writeKeys);
        EXPECT_TRUE(written) << written.error().message;
        packagePath_ = written.value().packagePath;
    }

    ~ReaderFixture() {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    ReaderFixture(const ReaderFixture&) = delete;
    ReaderFixture& operator=(const ReaderFixture&) = delete;

    std::filesystem::path Write(
        const std::string& name,
        const std::vector<std::uint8_t>& bytes) const {
        const auto path = root_ / name;
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        output.close();
        EXPECT_TRUE(output);
        return path;
    }

    std::vector<std::uint8_t> Read(
        const std::filesystem::path& path) const {
        std::ifstream input(path, std::ios::binary);
        return std::vector<std::uint8_t>(
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>());
    }

    std::filesystem::path MutatedCopy(
        const std::string& name,
        const std::function<void(std::vector<std::uint8_t>&)>& mutate) const {
        auto bytes = Read(packagePath_);
        mutate(bytes);
        return Write(name, bytes);
    }

    PackageHeader Header() const {
        const auto bytes = Read(packagePath_);
        std::vector<std::uint8_t> headerBytes(
            bytes.begin(),
            bytes.begin() + kPackageHeaderSize);
        const auto parsed = ParsePackageHeader(
            headerBytes,
            bytes.size(),
            PackageLimits{});
        EXPECT_TRUE(parsed) << parsed.error().message;
        return parsed.value();
    }

    std::unique_ptr<MemoryKeyProvider> Keys() const {
        return std::make_unique<MemoryKeyProvider>(
            keyId_,
            SecureBuffer::FromBytes(MasterKey()));
    }

    static std::vector<std::uint8_t> MasterKey() {
        return std::vector<std::uint8_t>(32, 0x73);
    }

    const std::filesystem::path& root() const noexcept {
        return root_;
    }
    const std::filesystem::path& packagePath() const noexcept {
        return packagePath_;
    }
    const std::vector<std::uint8_t>& plaintext() const noexcept {
        return plaintext_;
    }
    const KeyId& keyId() const noexcept {
        return keyId_;
    }
    const CngCryptoProvider& crypto() const noexcept {
        return crypto_;
    }
    const ZstdCompressionCodec& compression() const noexcept {
        return compression_;
    }
    const Win32AtomicFilePublisher& publisher() const noexcept {
        return publisher_;
    }

    bool HasPrivateExtractionArtifacts() const {
        for (const auto& entry :
             std::filesystem::directory_iterator(root_)) {
            const auto name =
                entry.path().filename().string();
            if (name.rfind(".dbpak-read-", 0) == 0) {
                return true;
            }
        }
        return false;
    }

private:
    std::filesystem::path root_;
    std::filesystem::path packagePath_;
    KeyId keyId_{};
    std::vector<std::uint8_t> plaintext_;
    CngCryptoProvider crypto_;
    ZstdCompressionCodec compression_;
    Win32AtomicFilePublisher publisher_;
};

class CountingMissingKeyProvider final : public KeyProvider {
public:
    PackageResult<SecureBuffer> Resolve(
        const KeyId&) const override {
        ++calls;
        return PackageResult<SecureBuffer>::Failure({
            PackageErrorCode::MissingKey,
            "The requested package key is unavailable.",
            std::nullopt,
        });
    }

    mutable std::size_t calls = 0;
};

PackageResult<std::unique_ptr<PackageReader>> Open(
    const ReaderFixture& fixture,
    const std::filesystem::path& path,
    const KeyProvider& keys,
    const PackageLimits& limits = {}) {
    return PackageReader::Open(
        path,
        keys,
        fixture.crypto(),
        fixture.compression(),
        fixture.publisher(),
        limits);
}

TEST(PackageReaderTest, AuthenticatesManifestAndPublishesVerifiedEntry) {
    ReaderFixture fixture;
    const auto keys = fixture.Keys();

    auto reader = Open(fixture, fixture.packagePath(), *keys);

    ASSERT_TRUE(reader) << reader.error().message;
    ASSERT_EQ(reader.value()->manifest().records.size(), 1U);
    EXPECT_EQ(
        reader.value()->manifest().records.front().path,
        "media/source.asset");
    const auto destination = fixture.root() / "extracted.bin";
    const auto extracted = reader.value()->ExtractEntry(
        "media/source.asset",
        destination);
    ASSERT_TRUE(extracted) << extracted.error().message;
    EXPECT_EQ(fixture.Read(destination), fixture.plaintext());
    EXPECT_FALSE(fixture.HasPrivateExtractionArtifacts());
}

TEST(PackageReaderTest, RejectsStructuralDamageBeforeKeyLookup) {
    ReaderFixture fixture;
    CountingMissingKeyProvider keys;
    const auto invalidMagic = fixture.MutatedCopy(
        "invalid-magic.dbpak",
        [](std::vector<std::uint8_t>& bytes) {
            bytes.front() ^= 0x80;
        });

    const auto invalid = Open(fixture, invalidMagic, keys);

    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.error().code, PackageErrorCode::InvalidFormat);
    EXPECT_EQ(keys.calls, 0U);

    PackageLimits limits;
    limits.maximumArchiveSize =
        std::filesystem::file_size(fixture.packagePath()) - 1;
    const auto oversized =
        Open(fixture, fixture.packagePath(), keys, limits);
    ASSERT_FALSE(oversized);
    EXPECT_EQ(oversized.error().code, PackageErrorCode::LimitExceeded);
    EXPECT_EQ(keys.calls, 0U);
}

TEST(PackageReaderTest, RejectsManifestCiphertextTagAndWrongKeyUniformly) {
    ReaderFixture fixture;
    const auto header = fixture.Header();
    const auto damagedManifest = fixture.MutatedCopy(
        "manifest-byte.dbpak",
        [header](std::vector<std::uint8_t>& bytes) {
            bytes[static_cast<std::size_t>(
                header.manifestOffset)] ^= 1;
        });
    const auto damagedTag = fixture.MutatedCopy(
        "manifest-tag.dbpak",
        [](std::vector<std::uint8_t>& bytes) {
            bytes[100] ^= 1;
        });
    const auto correctKeys = fixture.Keys();

    const auto ciphertextFailure =
        Open(fixture, damagedManifest, *correctKeys);
    const auto tagFailure =
        Open(fixture, damagedTag, *correctKeys);
    KeyId wrongId = fixture.keyId();
    MemoryKeyProvider wrongKey(
        wrongId,
        SecureBuffer::FromBytes(
            std::vector<std::uint8_t>(32, 0x99)));
    const auto wrongKeyFailure =
        Open(fixture, fixture.packagePath(), wrongKey);

    ASSERT_FALSE(ciphertextFailure);
    ASSERT_FALSE(tagFailure);
    ASSERT_FALSE(wrongKeyFailure);
    EXPECT_EQ(
        ciphertextFailure.error().code,
        PackageErrorCode::AuthenticationFailed);
    EXPECT_EQ(
        tagFailure.error().code,
        PackageErrorCode::AuthenticationFailed);
    EXPECT_EQ(
        wrongKeyFailure.error().code,
        PackageErrorCode::AuthenticationFailed);
    EXPECT_EQ(
        ciphertextFailure.error().message,
        wrongKeyFailure.error().message);
}

TEST(PackageReaderTest, RejectsPaddingTrailingBytesAndTruncation) {
    ReaderFixture fixture;
    const auto header = fixture.Header();
    const auto paddingOffset =
        header.manifestOffset + header.manifestCiphertextSize;
    ASSERT_LT(paddingOffset, header.payloadOffset);
    const auto damagedPadding = fixture.MutatedCopy(
        "padding.dbpak",
        [paddingOffset](std::vector<std::uint8_t>& bytes) {
            bytes[static_cast<std::size_t>(paddingOffset)] = 1;
        });
    const auto trailing = fixture.MutatedCopy(
        "trailing.dbpak",
        [](std::vector<std::uint8_t>& bytes) {
            bytes.push_back(0);
        });
    const auto truncated = fixture.MutatedCopy(
        "truncated.dbpak",
        [](std::vector<std::uint8_t>& bytes) {
            bytes.resize(bytes.size() - 1);
        });
    const auto keys = fixture.Keys();

    EXPECT_FALSE(Open(fixture, damagedPadding, *keys));
    EXPECT_FALSE(Open(fixture, trailing, *keys));
    EXPECT_FALSE(Open(fixture, truncated, *keys));
}

TEST(PackageReaderTest, RejectsPayloadTamperingWithoutPublishingPlaintext) {
    ReaderFixture fixture;
    const auto header = fixture.Header();
    const auto tampered = fixture.MutatedCopy(
        "payload.dbpak",
        [header](std::vector<std::uint8_t>& bytes) {
            bytes[static_cast<std::size_t>(
                header.payloadOffset)] ^= 1;
        });
    const auto keys = fixture.Keys();
    auto reader = Open(fixture, tampered, *keys);
    ASSERT_TRUE(reader) << reader.error().message;
    const auto destination = fixture.root() / "must-not-exist.bin";

    const auto extracted = reader.value()->ExtractEntry(
        "media/source.asset",
        destination);

    ASSERT_FALSE(extracted);
    EXPECT_EQ(
        extracted.error().code,
        PackageErrorCode::AuthenticationFailed);
    EXPECT_FALSE(std::filesystem::exists(destination));
    EXPECT_FALSE(fixture.HasPrivateExtractionArtifacts());
}

TEST(PackageReaderTest, PreservesExistingDestinationOnFailure) {
    ReaderFixture fixture;
    const auto keys = fixture.Keys();
    auto reader = Open(fixture, fixture.packagePath(), *keys);
    ASSERT_TRUE(reader);
    const auto destination =
        fixture.Write("existing.bin", {0xFE, 0xED});

    const auto extracted = reader.value()->ExtractEntry(
        "media/source.asset",
        destination);

    ASSERT_FALSE(extracted);
    EXPECT_EQ(
        extracted.error().code,
        PackageErrorCode::PublicationFailed);
    EXPECT_EQ(
        fixture.Read(destination),
        (std::vector<std::uint8_t>{0xFE, 0xED}));
}

TEST(PackageReaderTest, ReportsMissingKeyAndUnknownEntry) {
    ReaderFixture fixture;
    CountingMissingKeyProvider missingKeys;
    const auto missing =
        Open(fixture, fixture.packagePath(), missingKeys);
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code, PackageErrorCode::MissingKey);

    const auto keys = fixture.Keys();
    auto reader = Open(fixture, fixture.packagePath(), *keys);
    ASSERT_TRUE(reader);
    const auto unknown = reader.value()->ExtractEntry(
        "media/unknown.bin",
        fixture.root() / "unknown.bin");
    ASSERT_FALSE(unknown);
    EXPECT_EQ(unknown.error().code, PackageErrorCode::InvalidFormat);
}

TEST(PackageReaderTest, SupportsIndependentConcurrentReaders) {
    ReaderFixture fixture;
    const auto firstKeys = fixture.Keys();
    const auto secondKeys = fixture.Keys();
    auto first = Open(
        fixture,
        fixture.packagePath(),
        *firstKeys);
    auto second = Open(
        fixture,
        fixture.packagePath(),
        *secondKeys);
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    const auto firstDestination =
        fixture.root() / "concurrent-one.bin";
    const auto secondDestination =
        fixture.root() / "concurrent-two.bin";

    auto firstExtraction = std::async(
        std::launch::async,
        [&]() {
            return first.value()->ExtractEntry(
                "media/source.asset",
                firstDestination);
        });
    auto secondExtraction = std::async(
        std::launch::async,
        [&]() {
            return second.value()->ExtractEntry(
                "media/source.asset",
                secondDestination);
        });

    const auto firstResult = firstExtraction.get();
    const auto secondResult = secondExtraction.get();
    ASSERT_TRUE(firstResult) << firstResult.error().message;
    ASSERT_TRUE(secondResult) << secondResult.error().message;
    EXPECT_EQ(
        fixture.Read(firstDestination),
        fixture.plaintext());
    EXPECT_EQ(
        fixture.Read(secondDestination),
        fixture.plaintext());
    EXPECT_FALSE(fixture.HasPrivateExtractionArtifacts());
}

} // namespace
