#include <gtest/gtest.h>

#include "dbp/package/ExecutableKeyResource.h"
#include "dbp/package/RuntimeDescriptor.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

using namespace dbp::package;

PackageId TestPackageId() {
    PackageId id{};
    for (std::size_t index = 0; index < id.size(); ++index) {
        id[index] = static_cast<std::uint8_t>(index);
    }
    return id;
}

KeyId TestKeyId(const std::uint8_t first = 0xA0) {
    KeyId id{};
    id.front() = first;
    id.back() = 0x5C;
    return id;
}

RuntimeDescriptor TestDescriptor() {
    RuntimeDescriptor descriptor;
    descriptor.mode = RuntimeMode::Application;
    descriptor.packageId = TestPackageId();
    descriptor.keyId = TestKeyId();
    descriptor.packageFileName =
        ExpectedPackageFileName(descriptor.packageId);
    return descriptor;
}

std::vector<std::uint8_t> ReadFile(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>(),
    };
}

class MetadataFixture : public testing::Test {
protected:
    void SetUp() override {
        static std::atomic<std::uint64_t> sequence{0};
        const auto uniqueValue =
            static_cast<std::uint64_t>(
                std::chrono::steady_clock::now().time_since_epoch().count()) ^
            sequence.fetch_add(1, std::memory_order_relaxed);
        root_ = std::filesystem::temp_directory_path() /
            ("dbp-runtime-metadata-" + std::to_string(uniqueValue));
        std::filesystem::create_directories(root_);

        std::array<wchar_t, 32'768> modulePath{};
        const auto length = GetModuleFileNameW(
            nullptr,
            modulePath.data(),
            static_cast<DWORD>(modulePath.size()));
        ASSERT_GT(length, 0U);
        ASSERT_LT(length, modulePath.size());
        sourceExecutable_ =
            std::filesystem::path(modulePath.data(), modulePath.data() + length);
    }

    void TearDown() override {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    std::filesystem::path CopyExecutable(
        const std::string& name) const {
        const auto destination = root_ / name;
        std::filesystem::copy_file(
            sourceExecutable_,
            destination,
            std::filesystem::copy_options::none);
        return destination;
    }

    static bool PutRawResource(
        const std::filesystem::path& executable,
        const std::vector<std::uint8_t>& bytes,
        const WORD language = 0) {
        const auto update =
            BeginUpdateResourceW(executable.c_str(), FALSE);
        if (update == nullptr) {
            return false;
        }
        const auto updated = UpdateResourceW(
            update,
            MAKEINTRESOURCEW(10),
            const_cast<wchar_t*>(kExecutableKeyResourceName),
            language,
            const_cast<std::uint8_t*>(bytes.data()),
            static_cast<DWORD>(bytes.size()));
        if (!updated) {
            EndUpdateResourceW(update, TRUE);
            return false;
        }
        return EndUpdateResourceW(update, FALSE) != FALSE;
    }

    std::filesystem::path root_;
    std::filesystem::path sourceExecutable_;
};

TEST(RuntimeDescriptorTest, RoundTripsCanonicalFixedBinaryDescriptor) {
    const auto descriptor = TestDescriptor();

    const auto serialized = SerializeRuntimeDescriptor(descriptor);

    ASSERT_TRUE(serialized) << serialized.error().message;
    EXPECT_EQ(serialized.value().size(), kRuntimeDescriptorSize);
    EXPECT_EQ(serialized.value()[0], 'D');
    EXPECT_EQ(serialized.value()[8], 2);
    const auto parsed = ParseRuntimeDescriptor(serialized.value());
    ASSERT_TRUE(parsed) << parsed.error().message;
    EXPECT_EQ(parsed.value().mode, descriptor.mode);
    EXPECT_EQ(parsed.value().packageId, descriptor.packageId);
    EXPECT_EQ(parsed.value().keyId, descriptor.keyId);
    EXPECT_EQ(
        parsed.value().packageFileName,
        descriptor.packageFileName);
}

TEST(RuntimeDescriptorTest, RejectsUnsafeMismatchedAndUnknownMetadata) {
    for (const auto& fileName : {
             std::string("../data.dbpak"),
             std::string("C:\\data.dbpak"),
             std::string("subdir/data.dbpak"),
             std::string("data-00000000000000000000000000000000.dbpak")}) {
        SCOPED_TRACE(fileName);
        auto descriptor = TestDescriptor();
        descriptor.packageFileName = fileName;
        const auto serialized = SerializeRuntimeDescriptor(descriptor);
        ASSERT_FALSE(serialized);
        EXPECT_EQ(
            serialized.error().code,
            PackageErrorCode::InvalidFormat);
    }

    auto bytes = SerializeRuntimeDescriptor(TestDescriptor()).value();
    bytes[12] = 9;
    auto parsed = ParseRuntimeDescriptor(bytes);
    ASSERT_FALSE(parsed);
    EXPECT_EQ(parsed.error().code, PackageErrorCode::InvalidFormat);

    bytes = SerializeRuntimeDescriptor(TestDescriptor()).value();
    bytes.push_back(0);
    parsed = ParseRuntimeDescriptor(bytes);
    ASSERT_FALSE(parsed);
    EXPECT_EQ(parsed.error().code, PackageErrorCode::InvalidFormat);
}

TEST_F(MetadataFixture, AtomicallyReplacesAndReadsDescriptor) {
    const auto path = root_ / "game.dbpakref";
    auto descriptor = TestDescriptor();
    ASSERT_TRUE(WriteRuntimeDescriptorAtomically(path, descriptor));

    descriptor.mode = RuntimeMode::Installer;
    ASSERT_TRUE(WriteRuntimeDescriptorAtomically(path, descriptor));

    const auto read = ReadRuntimeDescriptor(path);
    ASSERT_TRUE(read) << read.error().message;
    EXPECT_EQ(read.value().mode, RuntimeMode::Installer);
    EXPECT_EQ(read.value().packageId, descriptor.packageId);
}

TEST_F(MetadataFixture, InjectsAndReadsExactVersionedKeyResource) {
    const auto executable = CopyExecutable("resource-valid.exe");
    const auto originalBytes = ReadFile(sourceExecutable_);
    auto masterKey = SecureBuffer::FromBytes(
        std::vector<std::uint8_t>(kPackageMasterKeySize, 0x6D));

    const auto injected =
        InjectExecutablePackageKey(executable, TestKeyId(), masterKey);

    ASSERT_TRUE(injected) << injected.error().message;
    const auto read =
        ReadExecutablePackageKey(executable, TestKeyId());
    ASSERT_TRUE(read) << read.error().message;
    EXPECT_EQ(read.value().keyId, TestKeyId());
    EXPECT_EQ(
        read.value().masterKey.CopyBytes(),
        std::vector<std::uint8_t>(kPackageMasterKeySize, 0x6D));
    EXPECT_EQ(ReadFile(sourceExecutable_), originalBytes);
}

TEST_F(MetadataFixture, ReadsPreviousKeyDuringDescriptorCommitWindow) {
    const auto executable = CopyExecutable("resource-fallback.exe");
    const auto currentId = TestKeyId(0xA1);
    const auto previousId = TestKeyId(0xA2);
    auto currentKey = SecureBuffer::FromBytes(
        std::vector<std::uint8_t>(kPackageMasterKeySize, 0x11));
    ExecutablePackageKey previous{
        previousId,
        SecureBuffer::FromBytes(
            std::vector<std::uint8_t>(
                kPackageMasterKeySize,
                0x22)),
    };

    const auto injected = InjectExecutablePackageKeys(
        executable,
        currentId,
        currentKey,
        &previous);

    ASSERT_TRUE(injected) << injected.error().message;
    const auto current =
        ReadExecutablePackageKey(executable, currentId);
    const auto fallback =
        ReadExecutablePackageKey(executable, previousId);
    ASSERT_TRUE(current) << current.error().message;
    ASSERT_TRUE(fallback) << fallback.error().message;
    EXPECT_EQ(
        current.value().masterKey.CopyBytes(),
        std::vector<std::uint8_t>(
            kPackageMasterKeySize,
            0x11));
    EXPECT_EQ(
        fallback.value().masterKey.CopyBytes(),
        std::vector<std::uint8_t>(
            kPackageMasterKeySize,
            0x22));
}

TEST_F(MetadataFixture, RejectsMissingDuplicateAndWrongKeyResources) {
    const auto missing =
        ReadExecutablePackageKey(sourceExecutable_, TestKeyId());
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code, PackageErrorCode::MissingKey);

    const auto duplicateExecutable =
        CopyExecutable("resource-duplicate.exe");
    auto resource = SerializeExecutableKeyResource(
        TestKeyId(),
        SecureBuffer::FromBytes(
            std::vector<std::uint8_t>(kPackageMasterKeySize, 0x22)));
    ASSERT_TRUE(resource);
    ASSERT_TRUE(PutRawResource(duplicateExecutable, resource.value(), 0));
    ASSERT_TRUE(PutRawResource(
        duplicateExecutable,
        resource.value(),
        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)));
    const auto duplicate =
        ReadExecutablePackageKey(duplicateExecutable, TestKeyId());
    ASSERT_FALSE(duplicate);
    EXPECT_EQ(duplicate.error().code, PackageErrorCode::InvalidFormat);

    const auto wrongIdExecutable = CopyExecutable("resource-id.exe");
    auto masterKey = SecureBuffer::FromBytes(
        std::vector<std::uint8_t>(kPackageMasterKeySize, 0x33));
    ASSERT_TRUE(InjectExecutablePackageKey(
        wrongIdExecutable,
        TestKeyId(),
        masterKey));
    const auto wrongId =
        ReadExecutablePackageKey(wrongIdExecutable, TestKeyId(0xB0));
    ASSERT_FALSE(wrongId);
    EXPECT_EQ(wrongId.error().code, PackageErrorCode::MissingKey);
}

TEST_F(MetadataFixture, RejectsMalformedVersionAndSize) {
    const auto malformedExecutable =
        CopyExecutable("resource-malformed.exe");
    auto resource = SerializeExecutableKeyResource(
        TestKeyId(),
        SecureBuffer::FromBytes(
            std::vector<std::uint8_t>(kPackageMasterKeySize, 0x44)));
    ASSERT_TRUE(resource);
    resource.value()[8] = 3;
    ASSERT_TRUE(PutRawResource(malformedExecutable, resource.value()));
    const auto malformed =
        ReadExecutablePackageKey(malformedExecutable, TestKeyId());
    ASSERT_FALSE(malformed);
    EXPECT_EQ(
        malformed.error().code,
        PackageErrorCode::UnsupportedVersion);

    const auto shortExecutable = CopyExecutable("resource-short.exe");
    resource.value().pop_back();
    ASSERT_TRUE(PutRawResource(shortExecutable, resource.value()));
    const auto tooShort =
        ReadExecutablePackageKey(shortExecutable, TestKeyId());
    ASSERT_FALSE(tooShort);
    EXPECT_EQ(tooShort.error().code, PackageErrorCode::InvalidFormat);

    auto shortKey =
        SecureBuffer::FromBytes(std::vector<std::uint8_t>(31, 0x55));
    const auto invalidKey = InjectExecutablePackageKey(
        CopyExecutable("resource-key-size.exe"),
        TestKeyId(),
        shortKey);
    ASSERT_FALSE(invalidKey);
    EXPECT_EQ(invalidKey.error().code, PackageErrorCode::InvalidFormat);
}

} // namespace
