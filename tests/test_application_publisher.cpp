#include <gtest/gtest.h>

#include "dbp/package/ApplicationPublisher.h"
#include "dbp/package/ExecutableKeyResource.h"
#include "dbp/package/KeyProvider.h"
#include "dbp/package/PackageReader.h"
#include "dbp/package/PackageWriter.h"
#include "dbp/package/PublicationCheckpoint.h"
#include "dbp/package/CompressionCodec.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <iterator>
#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using namespace dbp::package;

struct MountPointReparseData {
    DWORD reparseTag;
    USHORT reparseDataLength;
    USHORT reserved;
    USHORT substituteNameOffset;
    USHORT substituteNameLength;
    USHORT printNameOffset;
    USHORT printNameLength;
    wchar_t pathBuffer[1];
};

KeyId TestKeyId(const std::uint8_t first) {
    KeyId result{};
    result.front() = first;
    result.back() = 0xC7;
    return result;
}

std::vector<std::uint8_t> ReadFileBytes(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>(),
    };
}

class RecordingCheckpoint final : public PublicationCheckpoint {
public:
    explicit RecordingCheckpoint(
        std::optional<PublicationStage> failAt = std::nullopt)
        : failAt_(failAt) {}

    PackageResult<bool> Reach(
        const PublicationStage stage) const override {
        stages_.push_back(stage);
        if (failAt_ == stage) {
            return PackageResult<bool>::Failure({
                PackageErrorCode::PublicationFailed,
                "Simulated application-publication interruption.",
                std::nullopt,
            });
        }
        return PackageResult<bool>::Success(true);
    }

    const std::vector<PublicationStage>& stages() const noexcept {
        return stages_;
    }

private:
    std::optional<PublicationStage> failAt_;
    mutable std::vector<PublicationStage> stages_;
};

class BlockingPackageCheckpoint final
    : public PublicationCheckpoint {
public:
    BlockingPackageCheckpoint()
        : entered_(enteredPromise_.get_future().share()),
          released_(releasePromise_.get_future().share()) {}

    PackageResult<bool> Reach(
        const PublicationStage stage) const override {
        if (stage == PublicationStage::PackagePublished) {
            std::call_once(enteredOnce_, [this]() {
                enteredPromise_.set_value();
            });
            released_.wait();
        }
        return PackageResult<bool>::Success(true);
    }

    bool WaitUntilEntered(
        const std::chrono::milliseconds timeout) const {
        return entered_.wait_for(timeout) ==
            std::future_status::ready;
    }

    void Release() {
        std::call_once(releaseOnce_, [this]() {
            releasePromise_.set_value();
        });
    }

private:
    mutable std::promise<void> enteredPromise_;
    std::shared_future<void> entered_;
    mutable std::once_flag enteredOnce_;
    std::promise<void> releasePromise_;
    std::shared_future<void> released_;
    std::once_flag releaseOnce_;
};

class AtomicPackageCheckpoint final
    : public PublicationCheckpoint {
public:
    PackageResult<bool> Reach(
        const PublicationStage stage) const override {
        if (stage == PublicationStage::PackagePublished) {
            packageReached_.store(
                true,
                std::memory_order_release);
        }
        return PackageResult<bool>::Success(true);
    }

    bool packageReached() const noexcept {
        return packageReached_.load(
            std::memory_order_acquire);
    }

private:
    mutable std::atomic<bool> packageReached_{false};
};

class SwapExecutableStageCheckpoint final
    : public PublicationCheckpoint {
public:
    SwapExecutableStageCheckpoint(
        std::filesystem::path directory,
        std::filesystem::path replacement)
        : directory_(std::move(directory)),
          replacement_(std::move(replacement)) {}

    PackageResult<bool> Reach(
        const PublicationStage stage) const override {
        if (stage != PublicationStage::PackagePublished) {
            return PackageResult<bool>::Success(true);
        }
        for (const auto& entry :
             std::filesystem::directory_iterator(directory_)) {
            const auto name =
                entry.path().filename().wstring();
            if (name.find(L".dbp-stage-") ==
                    std::wstring::npos ||
                !entry.is_regular_file()) {
                continue;
            }
            std::error_code error;
            std::filesystem::remove(entry.path(), error);
            if (error) {
                return PackageResult<bool>::Failure({
                    PackageErrorCode::PublicationFailed,
                    "The test could not remove the executable stage.",
                    std::nullopt,
                });
            }
            std::filesystem::copy_file(
                replacement_,
                entry.path(),
                std::filesystem::copy_options::none,
                error);
            if (error) {
                return PackageResult<bool>::Failure({
                    PackageErrorCode::PublicationFailed,
                    "The test could not replace the executable stage.",
                    std::nullopt,
                });
            }
            replaced_.store(true, std::memory_order_release);
            return PackageResult<bool>::Success(true);
        }
        return PackageResult<bool>::Failure({
            PackageErrorCode::PublicationFailed,
            "The executable stage was not found.",
            std::nullopt,
        });
    }

    bool replaced() const noexcept {
        return replaced_.load(std::memory_order_acquire);
    }

private:
    std::filesystem::path directory_;
    std::filesystem::path replacement_;
    mutable std::atomic<bool> replaced_{false};
};

class ApplicationPublisherFixture : public testing::Test {
protected:
    void SetUp() override {
        static std::atomic<std::uint64_t> sequence{0};
        const auto uniqueValue =
            static_cast<std::uint64_t>(
                std::chrono::steady_clock::now()
                    .time_since_epoch()
                    .count()) ^
            sequence.fetch_add(1, std::memory_order_relaxed);
        root_ = std::filesystem::temp_directory_path() /
            ("dbp-application-publisher-" +
             std::to_string(uniqueValue));
        std::filesystem::create_directories(root_);

        std::array<wchar_t, 32'768> modulePath{};
        const auto length = GetModuleFileNameW(
            nullptr,
            modulePath.data(),
            static_cast<DWORD>(modulePath.size()));
        ASSERT_GT(length, 0U);
        ASSERT_LT(length, modulePath.size());
        hostExecutable_ =
            std::filesystem::path(
                modulePath.data(),
                modulePath.data() + length);
        outputExecutable_ = root_ / "published-game.exe";
    }

    void TearDown() override {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    std::filesystem::path WriteAsset(
        const std::string& name,
        const std::vector<std::uint8_t>& contents) const {
        const auto path = root_ / name;
        std::ofstream output(
            path,
            std::ios::binary | std::ios::trunc);
        output.write(
            reinterpret_cast<const char*>(contents.data()),
            static_cast<std::streamsize>(contents.size()));
        output.close();
        EXPECT_TRUE(output);
        return path;
    }

    std::filesystem::path CopyHost(
        const std::string& name) const {
        const auto destination = root_ / name;
        std::filesystem::copy_file(
            hostExecutable_,
            destination,
            std::filesystem::copy_options::none);
        return destination;
    }

    static bool CreateJunction(
        const std::filesystem::path& junction,
        const std::filesystem::path& target) {
        if (!CreateDirectoryW(junction.c_str(), nullptr)) {
            return false;
        }
        const auto absoluteTarget =
            std::filesystem::absolute(target).wstring();
        const auto substitute = L"\\??\\" + absoluteTarget;
        const auto print = absoluteTarget;
        const auto pathBytes =
            (substitute.size() + 1U + print.size() + 1U) *
            sizeof(wchar_t);
        const auto totalBytes =
            FIELD_OFFSET(
                MountPointReparseData,
                pathBuffer) +
            pathBytes;
        std::vector<std::uint8_t> storage(totalBytes, 0);
        auto* const reparse =
            reinterpret_cast<MountPointReparseData*>(
                storage.data());
        reparse->reparseTag = IO_REPARSE_TAG_MOUNT_POINT;
        reparse->substituteNameOffset = 0;
        reparse->substituteNameLength =
            static_cast<USHORT>(
                substitute.size() * sizeof(wchar_t));
        reparse->printNameOffset =
            static_cast<USHORT>(
                (substitute.size() + 1U) * sizeof(wchar_t));
        reparse->printNameLength =
            static_cast<USHORT>(
                print.size() * sizeof(wchar_t));
        std::memcpy(
            reparse->pathBuffer,
            substitute.c_str(),
            (substitute.size() + 1U) * sizeof(wchar_t));
        std::memcpy(
            reinterpret_cast<std::uint8_t*>(
                reparse->pathBuffer) +
                reparse->printNameOffset,
            print.c_str(),
            (print.size() + 1U) * sizeof(wchar_t));
        reparse->reparseDataLength = static_cast<USHORT>(
            8U + pathBytes);

        const auto handle = CreateFileW(
            junction.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_OPEN_REPARSE_POINT |
                FILE_FLAG_BACKUP_SEMANTICS,
            nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            return false;
        }
        DWORD returned = 0;
        const auto configured = DeviceIoControl(
            handle,
            FSCTL_SET_REPARSE_POINT,
            reparse,
            8U + reparse->reparseDataLength,
            nullptr,
            0,
            &returned,
            nullptr);
        CloseHandle(handle);
        return configured != FALSE;
    }

    ApplicationPublishRequest Request(
        const KeyId& keyId,
        const std::filesystem::path& asset) const {
        ApplicationPublishRequest request;
        request.hostExecutable = hostExecutable_;
        request.outputExecutable = outputExecutable_;
        request.mode = RuntimeMode::Application;
        request.keyId = keyId;
        request.entries.push_back(
            {asset, "media/asset.bin", true});
        return request;
    }

    MemoryKeyProvider Keys(
        const KeyId& keyId,
        const std::uint8_t value) const {
        return MemoryKeyProvider(
            keyId,
            SecureBuffer::FromBytes(
                std::vector<std::uint8_t>(
                    kPackageMasterKeySize,
                    value)));
    }

    void ExpectNoTransactionArtifacts() const {
        for (const auto& entry :
             std::filesystem::directory_iterator(root_)) {
            const auto name = entry.path().filename().wstring();
            EXPECT_EQ(name.find(L".dbp-stage-"), std::wstring::npos)
                << entry.path().string();
            EXPECT_EQ(name.find(L".dbp-backup-"), std::wstring::npos)
                << entry.path().string();
        }
    }

    void ExpectPublishedPayload(
        const KeyId& keyId,
        const std::vector<std::uint8_t>& expected) const {
        const auto descriptorPath =
            outputExecutable_.parent_path() /
            outputExecutable_.stem().concat(L".dbpakref");
        const auto descriptor =
            ReadRuntimeDescriptor(descriptorPath);
        ASSERT_TRUE(descriptor)
            << descriptor.error().message;
        ASSERT_EQ(descriptor.value().keyId, keyId);

        auto executableKey =
            ReadExecutablePackageKey(outputExecutable_, keyId);
        ASSERT_TRUE(executableKey)
            << executableKey.error().message;
        MemoryKeyProvider runtimeKeys(
            keyId,
            SecureBuffer::FromBytes(
                executableKey.value().masterKey.CopyBytes()));
        const auto packagePath =
            descriptorPath.parent_path() /
            descriptor.value().packageFileName;
        auto reader = PackageReader::Open(
            packagePath,
            runtimeKeys,
            crypto_,
            compression_,
            filePublisher_);
        ASSERT_TRUE(reader) << reader.error().message;
        const auto payload =
            reader.value()->ReadEntry("media/asset.bin");
        ASSERT_TRUE(payload) << payload.error().message;
        EXPECT_EQ(*payload.value(), expected);
    }

    CngCryptoProvider crypto_;
    ZstdCompressionCodec compression_;
    Win32AtomicFilePublisher filePublisher_;
    std::filesystem::path root_;
    std::filesystem::path hostExecutable_;
    std::filesystem::path outputExecutable_;
};

TEST(ApplicationPublisherTest, RequestAndResultHaveValueSemantics) {
    static_assert(
        std::is_move_constructible_v<ApplicationPublishRequest>);
    static_assert(
        std::is_move_constructible_v<ApplicationPublishResult>);
    static_assert(!std::is_copy_constructible_v<SecureBuffer>);
}

TEST(ApplicationPublisherTest, RejectsHostAndOutputPathAlias) {
    CngCryptoProvider crypto;
    ZstdCompressionCodec compression;
    Win32AtomicFilePublisher filePublisher;
    NoopPublicationCheckpoint checkpoint;
    ApplicationPublisher publisher(
        crypto,
        compression,
        filePublisher,
        checkpoint);

    KeyId keyId{};
    keyId.front() = 0x42;
    std::vector<std::uint8_t> keyBytes(
        kPackageMasterKeySize,
        0x5A);
    MemoryKeyProvider keys(
        keyId,
        SecureBuffer::FromBytes(keyBytes));

    ApplicationPublishRequest request;
    request.hostExecutable =
        std::filesystem::path(L"C:\\build\\same.exe");
    request.outputExecutable = request.hostExecutable;
    request.keyId = keyId;

    const auto result = publisher.Publish(request, keys);

    ASSERT_FALSE(result);
    EXPECT_EQ(
        PackageErrorCode::PublicationFailed,
        result.error().code);
}

TEST_F(
    ApplicationPublisherFixture,
    PublishesRunnableTupleAndReportsOrderedCheckpoints) {
    const auto keyId = TestKeyId(0x31);
    const std::vector<std::uint8_t> payload{
        0x10, 0x20, 0x30, 0x40};
    const auto asset = WriteAsset("asset-v1.bin", payload);
    auto keys = Keys(keyId, 0x51);
    RecordingCheckpoint checkpoint;
    ApplicationPublisher publisher(
        crypto_,
        compression_,
        filePublisher_,
        checkpoint);

    const auto published =
        publisher.Publish(Request(keyId, asset), keys);

    ASSERT_TRUE(published) << published.error().message;
    EXPECT_EQ(
        published.value().executablePath,
        outputExecutable_);
    EXPECT_TRUE(std::filesystem::is_regular_file(
        published.value().descriptorPath));
    EXPECT_TRUE(std::filesystem::is_regular_file(
        published.value().package.packagePath));
    EXPECT_EQ(
        checkpoint.stages(),
        (std::vector<PublicationStage>{
            PublicationStage::PackagePublished,
            PublicationStage::ExecutablePublished,
            PublicationStage::DescriptorPublished,
            PublicationStage::CleanupStarted}));
    ExpectPublishedPayload(keyId, payload);
    ExpectNoTransactionArtifacts();
}

TEST_F(
    ApplicationPublisherFixture,
    RejectsDescriptorAndExecutablePathAliasBeforePublication) {
    const auto keyId = TestKeyId(0x32);
    const auto asset = WriteAsset("asset.bin", {0x10});
    auto keys = Keys(keyId, 0x52);
    NoopPublicationCheckpoint checkpoint;
    ApplicationPublisher publisher(
        crypto_,
        compression_,
        filePublisher_,
        checkpoint);
    auto request = Request(keyId, asset);
    request.outputExecutable =
        root_ / "published-game.dbpakref";

    const auto published = publisher.Publish(request, keys);

    ASSERT_FALSE(published);
    EXPECT_EQ(
        published.error().code,
        PackageErrorCode::PublicationFailed);
    EXPECT_FALSE(std::filesystem::exists(
        request.outputExecutable));
    EXPECT_EQ(
        std::count_if(
            std::filesystem::directory_iterator(root_),
            std::filesystem::directory_iterator{},
            [](const std::filesystem::directory_entry& entry) {
                return entry.path().extension() == L".dbpak";
            }),
        0);
}

TEST_F(
    ApplicationPublisherFixture,
    RejectsDescriptorAndHostPathAliasBeforePublication) {
    const auto keyId = TestKeyId(0x33);
    const auto asset = WriteAsset("asset.bin", {0x11});
    const auto aliasedHost =
        CopyHost("published-game.dbpakref");
    const auto hostBytes = ReadFileBytes(aliasedHost);
    auto keys = Keys(keyId, 0x53);
    NoopPublicationCheckpoint checkpoint;
    ApplicationPublisher publisher(
        crypto_,
        compression_,
        filePublisher_,
        checkpoint);
    auto request = Request(keyId, asset);
    request.hostExecutable = aliasedHost;

    const auto published = publisher.Publish(request, keys);

    ASSERT_FALSE(published);
    EXPECT_EQ(
        published.error().code,
        PackageErrorCode::PublicationFailed);
    EXPECT_EQ(ReadFileBytes(aliasedHost), hostBytes);
    EXPECT_FALSE(std::filesystem::exists(
        request.outputExecutable));
}

TEST_F(
    ApplicationPublisherFixture,
    RejectsDescriptorHardLinkToHostBeforePublication) {
    const auto keyId = TestKeyId(0x34);
    const auto asset = WriteAsset("asset.bin", {0x12});
    const auto localHost = CopyHost("local-host.exe");
    const auto descriptor = root_ / "published-game.dbpakref";
    ASSERT_TRUE(CreateHardLinkW(
        descriptor.c_str(),
        localHost.c_str(),
        nullptr));
    auto keys = Keys(keyId, 0x54);
    NoopPublicationCheckpoint checkpoint;
    ApplicationPublisher publisher(
        crypto_,
        compression_,
        filePublisher_,
        checkpoint);
    auto request = Request(keyId, asset);
    request.hostExecutable = localHost;

    const auto published = publisher.Publish(request, keys);

    ASSERT_FALSE(published);
    EXPECT_EQ(
        published.error().code,
        PackageErrorCode::PublicationFailed);
    EXPECT_FALSE(std::filesystem::exists(
        request.outputExecutable));
}

TEST_F(
    ApplicationPublisherFixture,
    RejectsReparseOutputDirectoryBeforePublication) {
    const auto keyId = TestKeyId(0x35);
    const auto asset = WriteAsset("asset.bin", {0x13});
    const auto target = root_ / "real-output";
    std::filesystem::create_directory(target);
    const auto junction = root_ / "output-junction";
    ASSERT_TRUE(CreateJunction(junction, target))
        << "Win32 error " << GetLastError();
    auto keys = Keys(keyId, 0x55);
    NoopPublicationCheckpoint checkpoint;
    ApplicationPublisher publisher(
        crypto_,
        compression_,
        filePublisher_,
        checkpoint);
    auto request = Request(keyId, asset);
    request.outputExecutable =
        junction / "published-game.exe";

    const auto published =
        publisher.Publish(request, keys);

    ASSERT_FALSE(published);
    EXPECT_EQ(
        published.error().code,
        PackageErrorCode::PublicationFailed);
    EXPECT_TRUE(std::filesystem::is_empty(target));
}

TEST_F(
    ApplicationPublisherFixture,
    RejectsNonCanonicalWindowsOutputNamesBeforePublication) {
    const auto keyId = TestKeyId(0x36);
    const auto asset = WriteAsset("asset.bin", {0x14});
    NoopPublicationCheckpoint checkpoint;
    ApplicationPublisher publisher(
        crypto_,
        compression_,
        filePublisher_,
        checkpoint);

    for (const auto& name : {
             std::wstring(L"published-game.exe."),
             std::wstring(L"published-game.exe "),
             std::wstring(L"published-game.exe:stream"),
             std::wstring(L"NUL.exe")}) {
        SCOPED_TRACE(std::filesystem::path(name).string());
        auto request = Request(keyId, asset);
        request.outputExecutable = root_ / name;
        auto keys = Keys(keyId, 0x56);

        const auto published =
            publisher.Publish(request, keys);

        ASSERT_FALSE(published);
        EXPECT_EQ(
            published.error().code,
            PackageErrorCode::PublicationFailed);
    }
    EXPECT_EQ(
        std::count_if(
            std::filesystem::directory_iterator(root_),
            std::filesystem::directory_iterator{},
            [](const std::filesystem::directory_entry& entry) {
                return entry.path().extension() == L".dbpak";
            }),
        0);
}

TEST_F(
    ApplicationPublisherFixture,
    InterruptionAfterPackagePreservesPreviousRunnableTuple) {
    const auto oldKeyId = TestKeyId(0x41);
    const std::vector<std::uint8_t> oldPayload{0x01, 0x02};
    const auto oldAsset =
        WriteAsset("asset-old.bin", oldPayload);
    auto oldKeys = Keys(oldKeyId, 0x61);
    NoopPublicationCheckpoint initialCheckpoint;
    ApplicationPublisher initialPublisher(
        crypto_,
        compression_,
        filePublisher_,
        initialCheckpoint);
    ASSERT_TRUE(initialPublisher.Publish(
        Request(oldKeyId, oldAsset),
        oldKeys));
    const auto descriptorPath =
        outputExecutable_.parent_path() /
        outputExecutable_.stem().concat(L".dbpakref");
    const auto oldExecutableBytes =
        ReadFileBytes(outputExecutable_);
    const auto oldDescriptorBytes =
        ReadFileBytes(descriptorPath);

    const auto newKeyId = TestKeyId(0x42);
    const auto newAsset =
        WriteAsset("asset-new.bin", {0xA1, 0xA2, 0xA3});
    auto newKeys = Keys(newKeyId, 0x62);
    RecordingCheckpoint checkpoint(
        PublicationStage::PackagePublished);
    ApplicationPublisher publisher(
        crypto_,
        compression_,
        filePublisher_,
        checkpoint);

    const auto interrupted =
        publisher.Publish(Request(newKeyId, newAsset), newKeys);

    ASSERT_FALSE(interrupted);
    ASSERT_TRUE(interrupted.error().applicationPublicationPhase);
    EXPECT_EQ(
        *interrupted.error().applicationPublicationPhase,
        ApplicationPublicationPhase::Package);
    EXPECT_FALSE(interrupted.error().applicationTupleCommitted);
    EXPECT_EQ(
        checkpoint.stages(),
        (std::vector<PublicationStage>{
            PublicationStage::PackagePublished}));
    EXPECT_EQ(
        ReadFileBytes(outputExecutable_),
        oldExecutableBytes);
    EXPECT_EQ(ReadFileBytes(descriptorPath), oldDescriptorBytes);
    ExpectPublishedPayload(oldKeyId, oldPayload);
    ExpectNoTransactionArtifacts();
}

TEST_F(
    ApplicationPublisherFixture,
    RejectsExecutableStageIdentitySwapAtPackageCheckpoint) {
    const auto keyId = TestKeyId(0x43);
    const auto asset = WriteAsset("asset.bin", {0xA4});
    const auto replacement = CopyHost("replacement-host.exe");
    const auto replacementBytes = ReadFileBytes(replacement);
    auto keys = Keys(keyId, 0x64);
    SwapExecutableStageCheckpoint checkpoint(
        root_,
        replacement);
    ApplicationPublisher publisher(
        crypto_,
        compression_,
        filePublisher_,
        checkpoint);

    const auto published =
        publisher.Publish(Request(keyId, asset), keys);

    ASSERT_TRUE(checkpoint.replaced());
    ASSERT_FALSE(published);
    EXPECT_EQ(
        published.error().code,
        PackageErrorCode::PublicationFailed);
    EXPECT_FALSE(std::filesystem::exists(
        outputExecutable_));
    EXPECT_EQ(ReadFileBytes(replacement), replacementBytes);
    ExpectNoTransactionArtifacts();
}

TEST_F(
    ApplicationPublisherFixture,
    InterruptionAfterExecutableRollsBackPreviousRunnableTuple) {
    const auto oldKeyId = TestKeyId(0x51);
    const std::vector<std::uint8_t> oldPayload{0x11, 0x12};
    const auto oldAsset =
        WriteAsset("asset-old.bin", oldPayload);
    auto oldKeys = Keys(oldKeyId, 0x71);
    NoopPublicationCheckpoint initialCheckpoint;
    ApplicationPublisher initialPublisher(
        crypto_,
        compression_,
        filePublisher_,
        initialCheckpoint);
    ASSERT_TRUE(initialPublisher.Publish(
        Request(oldKeyId, oldAsset),
        oldKeys));
    const auto descriptorPath =
        outputExecutable_.parent_path() /
        outputExecutable_.stem().concat(L".dbpakref");
    const auto oldExecutableBytes =
        ReadFileBytes(outputExecutable_);
    const auto oldDescriptorBytes =
        ReadFileBytes(descriptorPath);

    const auto newKeyId = TestKeyId(0x52);
    const auto newAsset =
        WriteAsset("asset-new.bin", {0xB1, 0xB2, 0xB3});
    auto newKeys = Keys(newKeyId, 0x72);
    RecordingCheckpoint checkpoint(
        PublicationStage::ExecutablePublished);
    ApplicationPublisher publisher(
        crypto_,
        compression_,
        filePublisher_,
        checkpoint);

    const auto interrupted =
        publisher.Publish(Request(newKeyId, newAsset), newKeys);

    ASSERT_FALSE(interrupted);
    ASSERT_TRUE(interrupted.error().applicationPublicationPhase);
    EXPECT_EQ(
        *interrupted.error().applicationPublicationPhase,
        ApplicationPublicationPhase::Executable);
    EXPECT_FALSE(interrupted.error().applicationTupleCommitted);
    EXPECT_EQ(
        checkpoint.stages(),
        (std::vector<PublicationStage>{
            PublicationStage::PackagePublished,
            PublicationStage::ExecutablePublished}));
    EXPECT_EQ(
        ReadFileBytes(outputExecutable_),
        oldExecutableBytes);
    EXPECT_EQ(ReadFileBytes(descriptorPath), oldDescriptorBytes);
    ExpectPublishedPayload(oldKeyId, oldPayload);
    ExpectNoTransactionArtifacts();
}

TEST_F(
    ApplicationPublisherFixture,
    InterruptionAfterDescriptorRollsBackPreviousRunnableTuple) {
    const auto oldKeyId = TestKeyId(0x61);
    const std::vector<std::uint8_t> oldPayload{0x21, 0x22};
    const auto oldAsset =
        WriteAsset("asset-old.bin", oldPayload);
    auto oldKeys = Keys(oldKeyId, 0x81);
    NoopPublicationCheckpoint initialCheckpoint;
    ApplicationPublisher initialPublisher(
        crypto_,
        compression_,
        filePublisher_,
        initialCheckpoint);
    ASSERT_TRUE(initialPublisher.Publish(
        Request(oldKeyId, oldAsset),
        oldKeys));
    const auto descriptorPath =
        outputExecutable_.parent_path() /
        outputExecutable_.stem().concat(L".dbpakref");
    const auto oldExecutableBytes =
        ReadFileBytes(outputExecutable_);
    const auto oldDescriptorBytes =
        ReadFileBytes(descriptorPath);

    const auto newKeyId = TestKeyId(0x62);
    const auto newAsset =
        WriteAsset("asset-new.bin", {0xC1, 0xC2, 0xC3});
    auto newKeys = Keys(newKeyId, 0x82);
    RecordingCheckpoint checkpoint(
        PublicationStage::DescriptorPublished);
    ApplicationPublisher publisher(
        crypto_,
        compression_,
        filePublisher_,
        checkpoint);

    const auto interrupted =
        publisher.Publish(Request(newKeyId, newAsset), newKeys);

    ASSERT_FALSE(interrupted);
    ASSERT_TRUE(interrupted.error().applicationPublicationPhase);
    EXPECT_EQ(
        *interrupted.error().applicationPublicationPhase,
        ApplicationPublicationPhase::Descriptor);
    EXPECT_FALSE(interrupted.error().applicationTupleCommitted);
    EXPECT_EQ(
        checkpoint.stages(),
        (std::vector<PublicationStage>{
            PublicationStage::PackagePublished,
            PublicationStage::ExecutablePublished,
            PublicationStage::DescriptorPublished}));
    EXPECT_EQ(
        ReadFileBytes(outputExecutable_),
        oldExecutableBytes);
    EXPECT_EQ(ReadFileBytes(descriptorPath), oldDescriptorBytes);
    ExpectPublishedPayload(oldKeyId, oldPayload);
    ExpectNoTransactionArtifacts();
}

TEST_F(
    ApplicationPublisherFixture,
    CleanupFailureReportsThatTheTupleWasCommitted) {
    const auto keyId = TestKeyId(0x64);
    const std::vector<std::uint8_t> payload{0xE1, 0xE2};
    const auto asset = WriteAsset("asset-cleanup.bin", payload);
    auto keys = Keys(keyId, 0x84);
    RecordingCheckpoint checkpoint(
        PublicationStage::CleanupStarted);
    ApplicationPublisher publisher(
        crypto_,
        compression_,
        filePublisher_,
        checkpoint);

    const auto interrupted =
        publisher.Publish(Request(keyId, asset), keys);

    ASSERT_FALSE(interrupted);
    ASSERT_TRUE(interrupted.error().applicationPublicationPhase);
    EXPECT_EQ(
        *interrupted.error().applicationPublicationPhase,
        ApplicationPublicationPhase::Cleanup);
    EXPECT_TRUE(interrupted.error().applicationTupleCommitted);
    ExpectPublishedPayload(keyId, payload);
}

TEST_F(
    ApplicationPublisherFixture,
    FirstBuildInterruptionAfterDescriptorRemovesPartialTuple) {
    const auto keyId = TestKeyId(0x63);
    const auto asset =
        WriteAsset("asset-first.bin", {0xD1, 0xD2});
    auto keys = Keys(keyId, 0x83);
    RecordingCheckpoint checkpoint(
        PublicationStage::DescriptorPublished);
    ApplicationPublisher publisher(
        crypto_,
        compression_,
        filePublisher_,
        checkpoint);
    auto descriptorPath = outputExecutable_;
    descriptorPath.replace_extension(L".dbpakref");

    const auto interrupted =
        publisher.Publish(Request(keyId, asset), keys);

    ASSERT_FALSE(interrupted);
    EXPECT_FALSE(std::filesystem::exists(
        outputExecutable_));
    EXPECT_FALSE(std::filesystem::exists(
        descriptorPath));
    ExpectNoTransactionArtifacts();
}

TEST_F(
    ApplicationPublisherFixture,
    SerializesConcurrentPublishersForTheSameOutputTuple) {
    const auto firstKeyId = TestKeyId(0x71);
    const std::vector<std::uint8_t> firstPayload{0x31, 0x32};
    const auto firstAsset =
        WriteAsset("asset-first.bin", firstPayload);
    auto firstKeys = Keys(firstKeyId, 0x91);
    BlockingPackageCheckpoint firstCheckpoint;
    ApplicationPublisher firstPublisher(
        crypto_,
        compression_,
        filePublisher_,
        firstCheckpoint);

    const auto secondKeyId = TestKeyId(0x72);
    const std::vector<std::uint8_t> secondPayload{
        0x41, 0x42, 0x43};
    const auto secondAsset =
        WriteAsset("asset-second.bin", secondPayload);
    auto secondKeys = Keys(secondKeyId, 0x92);
    AtomicPackageCheckpoint secondCheckpoint;
    ApplicationPublisher secondPublisher(
        crypto_,
        compression_,
        filePublisher_,
        secondCheckpoint);

    auto first = std::async(
        std::launch::async,
        [&]() {
            return firstPublisher.Publish(
                Request(firstKeyId, firstAsset),
                firstKeys);
        });
    if (!firstCheckpoint.WaitUntilEntered(
            std::chrono::seconds(10))) {
        firstCheckpoint.Release();
        FAIL() << "The first publisher never reached its package checkpoint.";
    }

    auto second = std::async(
        std::launch::async,
        [&]() {
            return secondPublisher.Publish(
                Request(secondKeyId, secondAsset),
                secondKeys);
        });
    const auto secondStatusBeforeRelease =
        second.wait_for(std::chrono::milliseconds(250));
    EXPECT_EQ(
        secondStatusBeforeRelease,
        std::future_status::timeout);
    EXPECT_FALSE(secondCheckpoint.packageReached());

    firstCheckpoint.Release();
    const auto firstResult = first.get();
    const auto secondResult = second.get();

    ASSERT_TRUE(firstResult) << firstResult.error().message;
    ASSERT_TRUE(secondResult) << secondResult.error().message;
    ExpectPublishedPayload(secondKeyId, secondPayload);
    ExpectNoTransactionArtifacts();
}

} // namespace
