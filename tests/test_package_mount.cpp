#include <gtest/gtest.h>

#include "PackageMount.h"
#include "dbp/package/PackageWriter.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace dbp::package;

std::vector<std::uint8_t> ReadAll(
    const std::shared_ptr<IVFSReadStream>& stream) {
    std::vector<std::uint8_t> bytes(
        static_cast<std::size_t>(stream->Size()));
    std::size_t total = 0;
    while (total < bytes.size()) {
        const auto read =
            stream->Read(bytes.data() + total, bytes.size() - total);
        EXPECT_TRUE(read) << read.error().message;
        if (!read || read.value() == 0) {
            break;
        }
        total += read.value();
    }
    bytes.resize(total);
    return bytes;
}

class V2MountFixture {
public:
    V2MountFixture() {
        static std::atomic<std::uint64_t> sequence{0};
        const auto uniqueValue =
            static_cast<std::uint64_t>(
                std::chrono::steady_clock::now().time_since_epoch().count()) ^
            sequence.fetch_add(1, std::memory_order_relaxed);
        root_ = std::filesystem::temp_directory_path() /
            ("dbp-package-mount-" + std::to_string(uniqueValue));
        std::filesystem::create_directories(root_);

        plaintext_.resize(128U * 1024U + 17U);
        for (std::size_t index = 0; index < plaintext_.size(); ++index) {
            plaintext_[index] =
                static_cast<std::uint8_t>('a' + (index % 13U));
        }
        const auto source = root_ / "source.bin";
        std::ofstream output(
            source,
            std::ios::binary | std::ios::trunc);
        output.write(
            reinterpret_cast<const char*>(plaintext_.data()),
            static_cast<std::streamsize>(plaintext_.size()));
        output.close();

        keyId_.front() = 0xD7;
        MemoryKeyProvider writerKeys(
            keyId_,
            SecureBuffer::FromBytes(MasterKey()));
        PackageWriter writer(crypto_, compression_, publisher_);
        const auto written = writer.Write(
            {root_, keyId_, {{source, "media/source.bin", true}}},
            writerKeys);
        EXPECT_TRUE(written) << written.error().message;
        packagePath_ = written.value().packagePath;

        MemoryKeyProvider readerKeys(
            keyId_,
            SecureBuffer::FromBytes(MasterKey()));
        auto opened = PackageReader::Open(
            packagePath_,
            readerKeys,
            crypto_,
            compression_,
            publisher_);
        EXPECT_TRUE(opened) << opened.error().message;
        reader_ =
            std::shared_ptr<PackageReader>(std::move(opened.value()));
    }

    ~V2MountFixture() {
        VFSRegistry::Clear();
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    V2MountFixture(const V2MountFixture&) = delete;
    V2MountFixture& operator=(const V2MountFixture&) = delete;

    void TamperFirstPayloadByte() {
        ASSERT_FALSE(reader_->manifest().records.empty());
        const auto& record = reader_->manifest().records.front();
        const auto offset =
            reader_->header().payloadOffset + record.payloadOffset;
        std::fstream file(
            packagePath_,
            std::ios::binary | std::ios::in | std::ios::out);
        file.seekg(static_cast<std::streamoff>(offset));
        char byte = 0;
        file.read(&byte, 1);
        byte ^= 0x01;
        file.seekp(static_cast<std::streamoff>(offset));
        file.write(&byte, 1);
        file.close();
        ASSERT_TRUE(file);
    }

    static std::vector<std::uint8_t> MasterKey() {
        return std::vector<std::uint8_t>(32, 0x6A);
    }

    const std::shared_ptr<PackageReader>& reader() const {
        return reader_;
    }

    const std::vector<std::uint8_t>& plaintext() const {
        return plaintext_;
    }

private:
    std::filesystem::path root_;
    std::filesystem::path packagePath_;
    KeyId keyId_{};
    std::vector<std::uint8_t> plaintext_;
    CngCryptoProvider crypto_;
    ZstdCompressionCodec compression_;
    Win32AtomicFilePublisher publisher_;
    std::shared_ptr<PackageReader> reader_;
};

TEST(PackageMountTest, LazilyAuthenticatesAndMountsV2Entries) {
    VFSRegistry::Clear();
    V2MountFixture fixture;
    auto mounted = PackageMount::MountV2(fixture.reader());
    ASSERT_TRUE(mounted) << mounted.error().message;

    const auto opened = VFSRegistry::Open("media/source.bin");
    ASSERT_TRUE(opened) << opened.error().message;
    EXPECT_EQ(ReadAll(opened.value()), fixture.plaintext());

    mounted.value().reset();
    EXPECT_FALSE(VFSRegistry::Exists("media/source.bin"));
}

TEST(PackageMountTest, FailsClosedWhenLazyV2PayloadWasTampered) {
    VFSRegistry::Clear();
    V2MountFixture fixture;
    auto mounted = PackageMount::MountV2(fixture.reader());
    ASSERT_TRUE(mounted) << mounted.error().message;
    fixture.TamperFirstPayloadByte();

    const auto opened = VFSRegistry::Open("media/source.bin");

    ASSERT_FALSE(opened);
    EXPECT_EQ(
        opened.error().code,
        PackageErrorCode::AuthenticationFailed);
}

TEST(PackageMountTest, MountsTrackedLegacyEntriesWithoutExtraction) {
    VFSRegistry::Clear();
    const auto executable =
        std::filesystem::path(DBP_TEST_SOURCE_ROOT) /
        "Install/Help/examples/multiplayer/mp.exe";
    if (!std::filesystem::exists(executable)) {
        GTEST_SKIP() << "Tracked legacy executable is unavailable.";
    }
    auto opened = LegacyPckReader::OpenExecutable(executable);
    ASSERT_TRUE(opened) << opened.error().message;
    auto reader =
        std::shared_ptr<LegacyPckReader>(std::move(opened.value()));

    auto mounted = PackageMount::MountLegacy(reader);

    ASSERT_TRUE(mounted) << mounted.error().message;
    const auto entry = VFSRegistry::Open("_virtual.dat");
    ASSERT_TRUE(entry) << entry.error().message;
    EXPECT_EQ(entry.value()->Size(), 55'606U);
}

} // namespace
