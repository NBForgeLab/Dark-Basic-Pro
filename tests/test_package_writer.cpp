#include <gtest/gtest.h>

#include "dbp/package/PackageWriter.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace dbp::package;

class TemporaryWriterDirectory {
public:
    TemporaryWriterDirectory() {
        static std::atomic<std::uint64_t> sequence{0};
        const auto uniqueValue =
            static_cast<std::uint64_t>(
                std::chrono::steady_clock::now().time_since_epoch().count()) ^
            sequence.fetch_add(1, std::memory_order_relaxed);
        path_ = std::filesystem::temp_directory_path() /
            ("dbp-package-writer-" + std::to_string(uniqueValue));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryWriterDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    TemporaryWriterDirectory(const TemporaryWriterDirectory&) = delete;
    TemporaryWriterDirectory& operator=(const TemporaryWriterDirectory&) =
        delete;

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

    std::vector<std::filesystem::path> Files() const {
        std::vector<std::filesystem::path> result;
        for (const auto& entry :
             std::filesystem::directory_iterator(path_)) {
            result.push_back(entry.path());
        }
        std::sort(result.begin(), result.end());
        return result;
    }

    const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

class FailingPublisher final : public AtomicFilePublisher {
public:
    PackageResult<bool> Publish(
        const std::filesystem::path&,
        const std::filesystem::path&) const override {
        return PackageResult<bool>::Failure({
            PackageErrorCode::PublicationFailed,
            "Simulated publication failure.",
            std::nullopt,
        });
    }
};

class ScriptedRandomCrypto final : public CryptoProvider {
public:
    explicit ScriptedRandomCrypto(
        std::vector<std::vector<std::uint8_t>> randomValues,
        const bool corruptPayload = false)
        : randomValues_(std::move(randomValues)),
          corruptPayload_(corruptPayload) {}

    PackageResult<Sha256Digest> Sha256(
        const std::vector<std::uint8_t>& input) const override {
        return delegate_.Sha256(input);
    }
    PackageResult<HashStreamResult> Sha256Stream(
        std::istream& input,
        const std::uint64_t expectedSize) const override {
        return delegate_.Sha256Stream(input, expectedSize);
    }
    PackageResult<Sha256Digest> HmacSha256(
        const SecureBuffer& key,
        const std::vector<std::uint8_t>& input) const override {
        return delegate_.HmacSha256(key, input);
    }
    PackageResult<SecureBuffer> HkdfSha256(
        const SecureBuffer& inputKeyMaterial,
        const std::vector<std::uint8_t>& salt,
        const std::vector<std::uint8_t>& info,
        const std::size_t outputSize) const override {
        return delegate_.HkdfSha256(
            inputKeyMaterial,
            salt,
            info,
            outputSize);
    }
    PackageResult<AeadCiphertext> Aes256GcmEncrypt(
        const SecureBuffer& key,
        const AesGcmNonce& nonce,
        const std::vector<std::uint8_t>& plaintext,
        const std::vector<std::uint8_t>& additionalData) const override {
        return delegate_.Aes256GcmEncrypt(
            key,
            nonce,
            plaintext,
            additionalData);
    }
    PackageResult<AeadEncryptStreamResult> Aes256GcmEncryptStream(
        const SecureBuffer& key,
        const AesGcmNonce& nonce,
        std::istream& input,
        std::ostream& output,
        const std::vector<std::uint8_t>& additionalData,
        const std::uint64_t expectedPlaintextSize) const override {
        auto result = delegate_.Aes256GcmEncryptStream(
            key,
            nonce,
            input,
            output,
            additionalData,
            expectedPlaintextSize);
        if (result && corruptPayload_ &&
            result.value().outputSize != 0) {
            output.seekp(
                -static_cast<std::streamoff>(
                    result.value().outputSize),
                std::ios::cur);
            output.put(static_cast<char>(0xFF));
            output.seekp(0, std::ios::end);
        }
        return result;
    }
    PackageResult<std::vector<std::uint8_t>> Aes256GcmDecrypt(
        const SecureBuffer& key,
        const AesGcmNonce& nonce,
        const std::vector<std::uint8_t>& ciphertext,
        const std::vector<std::uint8_t>& additionalData,
        const AesGcmTag& tag) const override {
        return delegate_.Aes256GcmDecrypt(
            key,
            nonce,
            ciphertext,
            additionalData,
            tag);
    }
    PackageResult<AeadDecryptStreamResult> Aes256GcmDecryptStream(
        const SecureBuffer& key,
        const AesGcmNonce& nonce,
        std::istream& input,
        std::ostream& privateOutput,
        const std::vector<std::uint8_t>& additionalData,
        const AesGcmTag& tag,
        const std::uint64_t expectedCiphertextSize) const override {
        return delegate_.Aes256GcmDecryptStream(
            key,
            nonce,
            input,
            privateOutput,
            additionalData,
            tag,
            expectedCiphertextSize);
    }
    PackageResult<std::vector<std::uint8_t>> RandomBytes(
        const std::size_t size) const override {
        if (nextRandom_ >= randomValues_.size() ||
            randomValues_[nextRandom_].size() != size) {
            return PackageResult<std::vector<std::uint8_t>>::Failure({
                PackageErrorCode::CryptographyFailed,
                "Scripted RNG request mismatch.",
                std::nullopt,
            });
        }
        return PackageResult<std::vector<std::uint8_t>>::Success(
            randomValues_[nextRandom_++]);
    }

private:
    CngCryptoProvider delegate_;
    std::vector<std::vector<std::uint8_t>> randomValues_;
    mutable std::size_t nextRandom_ = 0;
    bool corruptPayload_ = false;
};

std::vector<std::uint8_t> ReadBytes(
    const std::filesystem::path& path,
    const std::uint64_t offset,
    const std::uint64_t size) {
    std::ifstream input(path, std::ios::binary);
    input.seekg(static_cast<std::streamoff>(offset));
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    input.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    EXPECT_EQ(
        input.gcount(),
        static_cast<std::streamsize>(bytes.size()));
    return bytes;
}

PackageHeader ReadHeader(const std::filesystem::path& path) {
    const auto fileSize = std::filesystem::file_size(path);
    const auto parsed = ParsePackageHeader(
        ReadBytes(path, 0, kPackageHeaderSize),
        fileSize,
        PackageLimits{});
    EXPECT_TRUE(parsed) << parsed.error().message;
    return parsed.value();
}

PackageManifest DecryptManifest(
    const std::filesystem::path& packagePath,
    const PackageHeader& header,
    const SecureBuffer& masterKey,
    const CryptoProvider& crypto) {
    PackageKeyDeriver deriver(crypto);
    const auto manifestKey =
        deriver.DeriveManifestKey(masterKey, header.packageId);
    EXPECT_TRUE(manifestKey);
    const auto plaintext = crypto.Aes256GcmDecrypt(
        manifestKey.value(),
        header.manifestNonce,
        ReadBytes(
            packagePath,
            header.manifestOffset,
            header.manifestCiphertextSize),
        BuildManifestAdditionalData(header),
        header.manifestTag);
    EXPECT_TRUE(plaintext) << plaintext.error().message;
    const auto digest = crypto.Sha256(plaintext.value());
    EXPECT_TRUE(digest);
    EXPECT_EQ(digest.value(), header.manifestPlaintextSha256);
    const auto manifest =
        ParseManifest(plaintext.value(), header, PackageLimits{});
    EXPECT_TRUE(manifest) << manifest.error().message;
    return manifest.value();
}

KeyId TestKeyId() {
    KeyId id{};
    id.front() = 0xA5;
    return id;
}

std::vector<std::uint8_t> TestMasterKey() {
    return std::vector<std::uint8_t>(32, 0x6B);
}

TEST(PackageWriterTest, WritesAndAuthenticatesBoundarySizedEntries) {
    TemporaryWriterDirectory directory;
    std::vector<std::vector<std::uint8_t>> contents{
        {},
        {0x42},
        std::vector<std::uint8_t>(1023, 0x31),
        std::vector<std::uint8_t>(1024, 0x32),
        std::vector<std::uint8_t>(2 * 1024 * 1024 + 17, 0x33),
    };
    PackageWriteRequest request;
    request.outputDirectory = directory.path();
    request.keyId = TestKeyId();
    for (std::size_t index = 0; index < contents.size(); ++index) {
        request.entries.push_back({
            directory.Write(
                "source-" + std::to_string(index) + ".bin",
                contents[index]),
            "media/" + std::to_string(index) + ".bin",
            true,
        });
    }

    CngCryptoProvider crypto;
    ZstdCompressionCodec compression;
    Win32AtomicFilePublisher publisher;
    PackageWriter writer(crypto, compression, publisher);
    MemoryKeyProvider keys(
        request.keyId,
        SecureBuffer::FromBytes(TestMasterKey()));

    const auto result = writer.Write(request, keys);

    ASSERT_TRUE(result) << result.error().message;
    EXPECT_TRUE(std::filesystem::exists(result.value().packagePath));
    EXPECT_EQ(
        result.value().packagePath.parent_path(),
        directory.path());
    const auto header = ReadHeader(result.value().packagePath);
    EXPECT_EQ(header.entryCount, contents.size());
    EXPECT_EQ(header.packageId, result.value().packageId);
    EXPECT_EQ(header.keyId, request.keyId);

    const auto masterKey =
        SecureBuffer::FromBytes(TestMasterKey());
    const auto manifest = DecryptManifest(
        result.value().packagePath,
        header,
        masterKey,
        crypto);
    ASSERT_EQ(manifest.records.size(), contents.size());
    EXPECT_TRUE(std::any_of(
        manifest.records.begin(),
        manifest.records.end(),
        [](const ManifestRecord& record) {
            return record.compression ==
                CompressionAlgorithm::Zstandard;
        }));

    PackageKeyDeriver deriver(crypto);
    for (std::size_t index = 0; index < manifest.records.size(); ++index) {
        const auto& record = manifest.records[index];
        const auto entryKey = deriver.DeriveEntryKey(
            masterKey,
            header.packageId,
            record.path);
        ASSERT_TRUE(entryKey);
        const auto stored = crypto.Aes256GcmDecrypt(
            entryKey.value(),
            record.nonce,
            ReadBytes(
                result.value().packagePath,
                header.payloadOffset + record.payloadOffset,
                record.storedSize),
            BuildEntryAdditionalData(header.packageId, record),
            record.tag);
        ASSERT_TRUE(stored) << stored.error().message;
        const auto plaintext = compression.Decompress(
            {record.compression, stored.value()},
            record.plaintextSize,
            record.plaintextSize);
        ASSERT_TRUE(plaintext) << plaintext.error().message;
        EXPECT_EQ(plaintext.value(), contents[index]);
    }
    for (const auto& path : directory.Files()) {
        EXPECT_FALSE(std::filesystem::is_directory(path));
        EXPECT_TRUE(
            path.extension() == ".bin" ||
            path.extension() == ".dbpak") <<
            "Only source files and the final .dbpak may remain.";
    }
}

TEST(PackageWriterTest, WritesAuthenticatedEmptyPackage) {
    TemporaryWriterDirectory directory;
    PackageWriteRequest request;
    request.outputDirectory = directory.path();
    request.keyId = TestKeyId();
    CngCryptoProvider crypto;
    ZstdCompressionCodec compression;
    Win32AtomicFilePublisher publisher;
    PackageWriter writer(crypto, compression, publisher);
    MemoryKeyProvider keys(
        request.keyId,
        SecureBuffer::FromBytes(TestMasterKey()));

    const auto result = writer.Write(request, keys);

    ASSERT_TRUE(result) << result.error().message;
    const auto header = ReadHeader(result.value().packagePath);
    EXPECT_EQ(header.entryCount, 0U);
    EXPECT_EQ(header.payloadSize, 0U);
    const auto masterKey =
        SecureBuffer::FromBytes(TestMasterKey());
    const auto manifest = DecryptManifest(
        result.value().packagePath,
        header,
        masterKey,
        crypto);
    EXPECT_TRUE(manifest.records.empty());
    EXPECT_EQ(directory.Files().size(), 1U);
}

TEST(PackageWriterTest, RejectsPathCollisionsBeforeCreatingOutput) {
    TemporaryWriterDirectory directory;
    const auto first = directory.Write("one.bin", {1});
    const auto second = directory.Write("two.bin", {2});
    PackageWriteRequest request{
        directory.path(),
        TestKeyId(),
        {
            {first, "Media/file.dat", true},
            {second, "media/FILE.dat", true},
        },
    };
    const auto filesBefore = directory.Files();
    CngCryptoProvider crypto;
    ZstdCompressionCodec compression;
    Win32AtomicFilePublisher publisher;
    PackageWriter writer(crypto, compression, publisher);
    MemoryKeyProvider keys(
        request.keyId,
        SecureBuffer::FromBytes(TestMasterKey()));

    const auto result = writer.Write(request, keys);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, PackageErrorCode::UnsafePath);
    EXPECT_EQ(directory.Files(), filesBefore);
}

TEST(PackageWriterTest, RejectsSourceReplacedAfterIdentitySnapshot) {
    TemporaryWriterDirectory directory;
    const auto source = directory.Write(
        "source.bin",
        {0x10, 0x20, 0x30, 0x40});
    const auto identity = CapturePackageSourceIdentity(source);
    ASSERT_TRUE(identity) << identity.error().message;

    const auto replacement = directory.Write(
        "replacement.bin",
        {0x40, 0x30, 0x20, 0x10});
    ASSERT_TRUE(std::filesystem::remove(source));
    ASSERT_NO_THROW(std::filesystem::rename(replacement, source));

    PackageWriteRequest request{
        directory.path(),
        TestKeyId(),
        {{
            source,
            "media/source.bin",
            false,
            identity.value(),
        }},
    };
    CngCryptoProvider crypto;
    ZstdCompressionCodec compression;
    Win32AtomicFilePublisher publisher;
    PackageWriter writer(crypto, compression, publisher);
    MemoryKeyProvider keys(
        request.keyId,
        SecureBuffer::FromBytes(TestMasterKey()));

    const auto result = writer.Write(request, keys);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, PackageErrorCode::IoFailed);
    EXPECT_EQ(directory.Files(), std::vector<std::filesystem::path>{source});
}

TEST(PackageWriterTest, RejectsDuplicateNoncesWithoutPublishing) {
    TemporaryWriterDirectory directory;
    const auto source = directory.Write("source.bin", {1, 2, 3});
    PackageWriteRequest request{
        directory.path(),
        TestKeyId(),
        {{source, "media/source.bin", true}},
    };
    const std::vector<std::uint8_t> repeatedNonce(12, 0x22);
    ScriptedRandomCrypto crypto({
        std::vector<std::uint8_t>(16, 0x11),
        repeatedNonce,
        repeatedNonce,
    });
    ZstdCompressionCodec compression;
    Win32AtomicFilePublisher publisher;
    PackageWriter writer(crypto, compression, publisher);
    MemoryKeyProvider keys(
        request.keyId,
        SecureBuffer::FromBytes(TestMasterKey()));

    const auto result = writer.Write(request, keys);

    ASSERT_FALSE(result);
    EXPECT_EQ(
        result.error().code,
        PackageErrorCode::CryptographyFailed);
    EXPECT_EQ(directory.Files(), std::vector<std::filesystem::path>{source});
}

TEST(PackageWriterTest, CleansTemporaryFilesWhenPublicationFails) {
    TemporaryWriterDirectory directory;
    const auto source = directory.Write(
        "source.bin",
        std::vector<std::uint8_t>(64 * 1024, 0x44));
    PackageWriteRequest request{
        directory.path(),
        TestKeyId(),
        {{source, "media/source.bin", true}},
    };
    CngCryptoProvider crypto;
    ZstdCompressionCodec compression;
    FailingPublisher publisher;
    PackageWriter writer(crypto, compression, publisher);
    MemoryKeyProvider keys(
        request.keyId,
        SecureBuffer::FromBytes(TestMasterKey()));

    const auto result = writer.Write(request, keys);

    ASSERT_FALSE(result);
    EXPECT_EQ(
        result.error().code,
        PackageErrorCode::PublicationFailed);
    EXPECT_EQ(directory.Files(), std::vector<std::filesystem::path>{source});
}

TEST(PackageWriterTest, AuthenticatesEveryPayloadBeforePublication) {
    TemporaryWriterDirectory directory;
    const auto source = directory.Write(
        "source.bin",
        std::vector<std::uint8_t>(4096, 0x44));
    PackageWriteRequest request{
        directory.path(),
        TestKeyId(),
        {{source, "media/source.bin", false}},
    };
    ScriptedRandomCrypto crypto({
        std::vector<std::uint8_t>(16, 0x11),
        std::vector<std::uint8_t>(12, 0x22),
        std::vector<std::uint8_t>(12, 0x33),
    }, true);
    ZstdCompressionCodec compression;
    Win32AtomicFilePublisher publisher;
    PackageWriter writer(crypto, compression, publisher);
    MemoryKeyProvider keys(
        request.keyId,
        SecureBuffer::FromBytes(TestMasterKey()));

    const auto result = writer.Write(request, keys);

    ASSERT_FALSE(result);
    EXPECT_EQ(
        result.error().code,
        PackageErrorCode::AuthenticationFailed);
    EXPECT_EQ(directory.Files(), std::vector<std::filesystem::path>{source});
}

TEST(PackageWriterTest, NeverOverwritesImmutablePackageName) {
    TemporaryWriterDirectory directory;
    const auto source = directory.Write("source.bin", {7, 8, 9});
    const std::vector<std::uint8_t> packageId(16, 0xAB);
    const auto existing = directory.Write(
        "data-abababababababababababababababab.dbpak",
        {0xEE});
    PackageWriteRequest request{
        directory.path(),
        TestKeyId(),
        {{source, "media/source.bin", false}},
    };
    ScriptedRandomCrypto crypto({
        packageId,
        std::vector<std::uint8_t>(12, 1),
        std::vector<std::uint8_t>(12, 2),
    });
    ZstdCompressionCodec compression;
    Win32AtomicFilePublisher publisher;
    PackageWriter writer(crypto, compression, publisher);
    MemoryKeyProvider keys(
        request.keyId,
        SecureBuffer::FromBytes(TestMasterKey()));

    const auto result = writer.Write(request, keys);

    ASSERT_FALSE(result);
    EXPECT_EQ(
        result.error().code,
        PackageErrorCode::PublicationFailed);
    EXPECT_EQ(ReadBytes(existing, 0, 1), std::vector<std::uint8_t>{0xEE});
}

} // namespace
