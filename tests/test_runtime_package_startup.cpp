#include <gtest/gtest.h>

#include "RuntimePackageBootstrap.h"
#include "dbp/package/ExecutableKeyResource.h"
#include "dbp/package/ByteCodec.h"
#include "dbp/package/PackageWriter.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace dbp::package;

std::vector<std::uint8_t> BuildLegacySidecar(
    const std::vector<std::uint8_t>& payload) {
    const std::string path = "_virtual.dat";
    ByteWriter writer;
    writer.WriteUInt32(static_cast<std::uint32_t>(path.size()));
    writer.WriteBytes(
        reinterpret_cast<const std::uint8_t*>(path.data()),
        path.size());
    writer.WriteUInt32(
        static_cast<std::uint32_t>(payload.size()));
    writer.WriteBytes(payload.data(), payload.size());
    writer.WriteUInt32(0);
    return writer.Bytes();
}

bool CreateDirectoryJunction(
    const std::filesystem::path& junction,
    const std::filesystem::path& target) {
    if (!CreateDirectoryW(junction.c_str(), nullptr)) {
        return false;
    }
    const auto handle = CreateFileW(
        junction.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS |
            FILE_FLAG_OPEN_REPARSE_POINT,
        nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    const auto substitute =
        L"\\??\\" + std::filesystem::absolute(target).wstring();
    const auto printName =
        std::filesystem::absolute(target).wstring();
    struct MountPointBuffer {
        DWORD tag;
        WORD dataLength;
        WORD reserved;
        WORD substituteOffset;
        WORD substituteLength;
        WORD printOffset;
        WORD printLength;
        wchar_t path[32'768];
    } buffer{};
    buffer.tag = IO_REPARSE_TAG_MOUNT_POINT;
    buffer.substituteLength = static_cast<WORD>(
        substitute.size() * sizeof(wchar_t));
    buffer.printOffset = static_cast<WORD>(
        buffer.substituteLength + sizeof(wchar_t));
    buffer.printLength = static_cast<WORD>(
        printName.size() * sizeof(wchar_t));
    std::memcpy(
        buffer.path,
        substitute.c_str(),
        buffer.substituteLength);
    std::memcpy(
        reinterpret_cast<std::uint8_t*>(buffer.path) +
            buffer.printOffset,
        printName.c_str(),
        buffer.printLength);
    buffer.dataLength = static_cast<WORD>(
        8U + buffer.printOffset +
        buffer.printLength + sizeof(wchar_t));
    DWORD returned = 0;
    const auto result = DeviceIoControl(
        handle,
        FSCTL_SET_REPARSE_POINT,
        &buffer,
        static_cast<DWORD>(8U + buffer.dataLength),
        nullptr,
        0,
        &returned,
        nullptr) != FALSE;
    CloseHandle(handle);
    return result;
}

class RuntimeStartupFixture : public testing::Test {
protected:
    void SetUp() override {
        VFSRegistry::Clear();
        static std::atomic<std::uint64_t> sequence{0};
        const auto uniqueValue =
            static_cast<std::uint64_t>(
                std::chrono::steady_clock::now().time_since_epoch().count()) ^
            sequence.fetch_add(1, std::memory_order_relaxed);
        root_ = std::filesystem::temp_directory_path() /
            ("dbp-runtime-startup-" + std::to_string(uniqueValue));
        std::filesystem::create_directories(root_);

        std::array<wchar_t, 32'768> path{};
        const auto length = GetModuleFileNameW(
            nullptr,
            path.data(),
            static_cast<DWORD>(path.size()));
        ASSERT_GT(length, 0U);
        executable_ = root_ / "game.exe";
        std::filesystem::copy_file(
            std::filesystem::path(path.data(), path.data() + length),
            executable_);

        payload_.assign(64U * 1024U + 3U, 0x5E);
        const auto source = root_ / "program.exb";
        std::ofstream output(
            source,
            std::ios::binary | std::ios::trunc);
        output.write(
            reinterpret_cast<const char*>(payload_.data()),
            static_cast<std::streamsize>(payload_.size()));
        output.close();
        mediaPayload_ = {0x10, 0x20, 0x30, 0x40};
        const auto mediaSource = root_ / "asset.bin";
        std::ofstream mediaOutput(
            mediaSource,
            std::ios::binary | std::ios::trunc);
        mediaOutput.write(
            reinterpret_cast<const char*>(mediaPayload_.data()),
            static_cast<std::streamsize>(mediaPayload_.size()));
        mediaOutput.close();

        keyId_.front() = 0x71;
        masterKey_.assign(kPackageMasterKeySize, 0x92);
        MemoryKeyProvider keys(
            keyId_,
            SecureBuffer::FromBytes(masterKey_));
        PackageWriter writer(crypto_, compression_, publisher_);
        const auto written = writer.Write(
            {
                root_,
                keyId_,
                {
                    {source, "_virtual.dat", true},
                    {mediaSource, "media/assets/asset.bin", true},
                },
            },
            keys);
        ASSERT_TRUE(written) << written.error().message;
        packagePath_ = written.value().packagePath;
        packageId_ = written.value().packageId;
    }

    void TearDown() override {
        VFSRegistry::Clear();
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    void PublishMetadata(
        const PackageId& descriptorPackageId,
        const KeyId& descriptorKeyId,
        const std::filesystem::path& namedPackage,
        const RuntimeMode mode = RuntimeMode::Application) {
        auto key = SecureBuffer::FromBytes(masterKey_);
        ASSERT_TRUE(InjectExecutablePackageKey(
            executable_,
            keyId_,
            key));

        RuntimeDescriptor descriptor;
        descriptor.mode = mode;
        descriptor.packageId = descriptorPackageId;
        descriptor.keyId = descriptorKeyId;
        descriptor.packageFileName =
            namedPackage.filename().string();
        auto descriptorPath = executable_;
        descriptorPath.replace_extension(L".dbpakref");
        const auto written =
            WriteRuntimeDescriptorAtomically(
                descriptorPath,
                descriptor);
        ASSERT_TRUE(written) << written.error().message;
    }

    std::filesystem::path root_;
    std::filesystem::path executable_;
    std::filesystem::path packagePath_;
    PackageId packageId_{};
    KeyId keyId_{};
    std::vector<std::uint8_t> masterKey_;
    std::vector<std::uint8_t> payload_;
    std::vector<std::uint8_t> mediaPayload_;
    CngCryptoProvider crypto_;
    ZstdCompressionCodec compression_;
    Win32AtomicFilePublisher publisher_;
};

TEST_F(
    RuntimeStartupFixture,
    MountsMatchingV2MetadataRelativeToExecutableNotCurrentDirectory) {
    PublishMetadata(packageId_, keyId_, packagePath_);

    const auto started =
        RuntimePackageBootstrap::Start(executable_);

    ASSERT_TRUE(started) << started.error().message;
    EXPECT_EQ(
        started.value()->kind(),
        RuntimePackageKind::AuthenticatedV2);
    EXPECT_EQ(started.value()->mode(), RuntimeMode::Application);
    const auto stream = VFSRegistry::Open("_virtual.dat");
    ASSERT_TRUE(stream) << stream.error().message;
    std::vector<std::uint8_t> bytes(payload_.size());
    const auto read = stream.value()->Read(bytes.data(), bytes.size());
    ASSERT_TRUE(read) << read.error().message;
    EXPECT_EQ(bytes, payload_);
}

TEST_F(RuntimeStartupFixture, RejectsDescriptorPackageAndKeyMismatches) {
    auto wrongPackageId = packageId_;
    wrongPackageId.front() ^= 0xFF;
    const auto wrongPackagePath =
        root_ / ExpectedPackageFileName(wrongPackageId);
    std::filesystem::copy_file(packagePath_, wrongPackagePath);
    PublishMetadata(wrongPackageId, keyId_, wrongPackagePath);

    auto started = RuntimePackageBootstrap::Start(executable_);
    ASSERT_FALSE(started);
    EXPECT_EQ(started.error().code, PackageErrorCode::InvalidFormat);

    auto descriptorPath = executable_;
    descriptorPath.replace_extension(L".dbpakref");
    std::filesystem::remove(descriptorPath);
    auto wrongKeyId = keyId_;
    wrongKeyId.front() ^= 0x11;
    PublishMetadata(packageId_, wrongKeyId, packagePath_);
    started = RuntimePackageBootstrap::Start(executable_);
    ASSERT_FALSE(started);
    EXPECT_EQ(started.error().code, PackageErrorCode::MissingKey);
}

TEST_F(
    RuntimeStartupFixture,
    InstallerAuthenticatesEverythingBeforePublishingApplicationFolder) {
    PublishMetadata(
        packageId_,
        keyId_,
        packagePath_,
        RuntimeMode::Installer);
    auto started = RuntimePackageBootstrap::Start(executable_);
    ASSERT_TRUE(started) << started.error().message;

    const auto installRoot = root_ / "install-output";
    std::filesystem::create_directory(installRoot);
    const auto installed =
        started.value()->MaterializeInstaller(installRoot);

    ASSERT_TRUE(installed) << installed.error().message;
    EXPECT_TRUE(std::filesystem::exists(
        installed.value() / executable_.filename()));
    EXPECT_TRUE(std::filesystem::exists(
        installed.value() / packagePath_.filename()));
    auto installedDescriptor =
        installed.value() / executable_.filename();
    installedDescriptor.replace_extension(L".dbpakref");
    const auto descriptor =
        ReadRuntimeDescriptor(installedDescriptor);
    ASSERT_TRUE(descriptor) << descriptor.error().message;
    EXPECT_EQ(descriptor.value().mode, RuntimeMode::Application);

    std::ifstream media(
        installed.value() / "assets/asset.bin",
        std::ios::binary);
    const std::vector<std::uint8_t> installedMedia(
        (std::istreambuf_iterator<char>(media)),
        std::istreambuf_iterator<char>());
    EXPECT_EQ(installedMedia, mediaPayload_);
}

TEST_F(
    RuntimeStartupFixture,
    InstallerPublishesNothingWhenAnyPayloadFailsAuthentication) {
    PublishMetadata(
        packageId_,
        keyId_,
        packagePath_,
        RuntimeMode::Installer);
    auto started = RuntimePackageBootstrap::Start(executable_);
    ASSERT_TRUE(started) << started.error().message;

    {
        std::fstream package(
            packagePath_,
            std::ios::binary | std::ios::in | std::ios::out);
        ASSERT_TRUE(package);
        package.seekg(-1, std::ios::end);
        const auto offset = package.tellg();
        char byte = 0;
        package.read(&byte, 1);
        byte ^= 0x5A;
        package.seekp(offset);
        package.write(&byte, 1);
    }

    const auto installRoot = root_ / "failed-install-output";
    std::filesystem::create_directory(installRoot);
    const auto installed =
        started.value()->MaterializeInstaller(installRoot);

    ASSERT_FALSE(installed);
    EXPECT_TRUE(std::filesystem::is_empty(installRoot));
}

TEST(RuntimePackageStartupTest, FallsBackToTrackedLegacyExecutableOnly) {
    VFSRegistry::Clear();
    const auto executable =
        std::filesystem::path(DBP_TEST_SOURCE_ROOT) /
        "Install/Help/examples/multiplayer/mp.exe";
    if (!std::filesystem::exists(executable)) {
        GTEST_SKIP() << "Tracked legacy executable is unavailable.";
    }

    const auto started =
        RuntimePackageBootstrap::Start(executable);

    ASSERT_TRUE(started) << started.error().message;
    EXPECT_EQ(
        started.value()->kind(),
        RuntimePackageKind::LegacyReadOnly);
    EXPECT_TRUE(VFSRegistry::Exists("_virtual.dat"));
}

TEST(RuntimePackageStartupTest, MountsDeterministicLegacySidecarReadOnly) {
    static std::atomic<std::uint64_t> sequence{0};
    const auto root = std::filesystem::temp_directory_path() /
        ("dbp-legacy-sidecar-startup-" + std::to_string(
            sequence.fetch_add(1, std::memory_order_relaxed)));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            VFSRegistry::Clear();
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
    } cleanup{root};
    std::filesystem::create_directories(root);
    std::array<wchar_t, 32'768> module{};
    const auto length = GetModuleFileNameW(
        nullptr,
        module.data(),
        static_cast<DWORD>(module.size()));
    ASSERT_GT(length, 0U);
    const auto executable = root / "game.exe";
    std::filesystem::copy_file(
        std::filesystem::path(
            module.data(),
            module.data() + length),
        executable);
    const std::vector<std::uint8_t> payload{1, 2, 3, 4};
    const auto image = BuildLegacySidecar(payload);
    std::ofstream sidecar(
        root / "game.pck",
        std::ios::binary | std::ios::trunc);
    sidecar.write(
        reinterpret_cast<const char*>(image.data()),
        static_cast<std::streamsize>(image.size()));
    sidecar.close();

    const auto started =
        RuntimePackageBootstrap::Start(executable);

    ASSERT_TRUE(started) << started.error().message;
    EXPECT_EQ(
        started.value()->kind(),
        RuntimePackageKind::LegacyReadOnly);
    EXPECT_TRUE(VFSRegistry::Exists("_virtual.dat"));
    EXPECT_FALSE(std::filesystem::exists(root / "_virtual.pck"));
}

TEST(RuntimePackageStartupTest, MalformedLegacySidecarLeavesNoArtifacts) {
    static std::atomic<std::uint64_t> sequence{0};
    const auto root = std::filesystem::temp_directory_path() /
        ("dbp-malformed-sidecar-startup-" + std::to_string(
            sequence.fetch_add(1, std::memory_order_relaxed)));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            VFSRegistry::Clear();
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
    } cleanup{root};
    std::filesystem::create_directories(root);
    std::array<wchar_t, 32'768> module{};
    const auto length = GetModuleFileNameW(
        nullptr,
        module.data(),
        static_cast<DWORD>(module.size()));
    ASSERT_GT(length, 0U);
    const auto executable = root / "game.exe";
    std::filesystem::copy_file(
        std::filesystem::path(
            module.data(),
            module.data() + length),
        executable);
    std::ofstream sidecar(
        root / "game.pck",
        std::ios::binary | std::ios::trunc);
    sidecar.write("\x05\x00", 2);
    sidecar.close();

    const auto started =
        RuntimePackageBootstrap::Start(executable);

    ASSERT_FALSE(started);
    EXPECT_TRUE(std::filesystem::exists(root / "game.pck"));
    EXPECT_EQ(
        std::distance(
            std::filesystem::directory_iterator(root),
            std::filesystem::directory_iterator{}),
        2);
    EXPECT_FALSE(VFSRegistry::Exists("_virtual.dat"));
}

TEST(RuntimePackageStartupTest, RejectsReparsePointMaterializationEscape) {
    static std::atomic<std::uint64_t> sequence{0};
    const auto base = std::filesystem::temp_directory_path() /
        ("dbp-reparse-test-" + std::to_string(
            sequence.fetch_add(1, std::memory_order_relaxed)));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
    } cleanup{base};
    const auto root = base / "materialized";
    const auto outside = base / "outside";
    ASSERT_TRUE(std::filesystem::create_directories(root));
    ASSERT_TRUE(std::filesystem::create_directories(outside));
    const auto link = root / "media";
    ASSERT_TRUE(CreateDirectoryJunction(link, outside));

    const auto result = WriteRuntimeMaterializedFileSafely(
        root,
        std::filesystem::path(L"media") / L"escaped.bin",
        {1, 2, 3});

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, PackageErrorCode::UnsafePath);
    EXPECT_FALSE(std::filesystem::exists(outside / "escaped.bin"));
}

TEST(RuntimePackageStartupTest, RejectsJunctionRootForStreamedCopies) {
    static std::atomic<std::uint64_t> sequence{0};
    const auto base = std::filesystem::temp_directory_path() /
        ("dbp-runtime-copy-junction-" + std::to_string(
            sequence.fetch_add(1, std::memory_order_relaxed)));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
    } cleanup{base};
    const auto outside = base / "outside";
    const auto source = base / "source.bin";
    ASSERT_TRUE(std::filesystem::create_directories(outside));
    {
        std::ofstream output(source, std::ios::binary);
        output.write("trusted", 7);
    }
    const auto root = base / "materialized";
    ASSERT_TRUE(CreateDirectoryJunction(root, outside));

    const auto result = CopyRuntimeMaterializedFileSafely(
        root,
        "copied.bin",
        source);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, PackageErrorCode::UnsafePath);
    EXPECT_FALSE(std::filesystem::exists(outside / "copied.bin"));
}

} // namespace
