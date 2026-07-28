#include <gtest/gtest.h>

#include "dbp/package/ByteCodec.h"
#include "dbp/package/LegacyPckReader.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace dbp::package;

constexpr std::uint32_t legacyValidityCode = 12345678U;

struct SourceEntry {
    std::string path;
    std::vector<std::uint8_t> bytes;
};

void ApplyLegacyTransform(
    std::vector<std::uint8_t>& bytes,
    const std::uint32_t key) {
    if (key == 0 || bytes.empty()) {
        return;
    }
    const auto shift = static_cast<std::uint8_t>(key % 64U);
    const auto span = std::max<std::size_t>(1U, bytes.size() / 1024U);
    for (std::size_t offset = 0; offset < bytes.size(); offset += span) {
        bytes[offset] = static_cast<std::uint8_t>(bytes[offset] + shift);
    }
}

bool IsMediaPath(const std::string& path) {
    return path.size() >= 6 &&
        (path[0] == 'm' || path[0] == 'M') &&
        (path[1] == 'e' || path[1] == 'E') &&
        (path[2] == 'd' || path[2] == 'D') &&
        (path[3] == 'i' || path[3] == 'I') &&
        (path[4] == 'a' || path[4] == 'A') &&
        (path[5] == '\\' || path[5] == '/');
}

std::vector<std::uint8_t> BuildImage(
    const std::vector<SourceEntry>& entries,
    const std::uint32_t key = 0,
    const bool includeTerminator = true,
    const std::uint32_t validity = legacyValidityCode,
    const std::uint32_t kind = 0) {
    constexpr std::uint32_t executableSize = 64;
    ByteWriter writer;
    std::vector<std::uint8_t> executable(executableSize, 0x90);
    executable[0] = 'M';
    executable[1] = 'Z';
    writer.WriteBytes(executable.data(), executable.size());

    for (const auto& entry : entries) {
        auto stored = entry.bytes;
        if (IsMediaPath(entry.path)) {
            ApplyLegacyTransform(stored, key);
        }
        writer.WriteUInt32(static_cast<std::uint32_t>(entry.path.size()));
        writer.WriteBytes(
            reinterpret_cast<const std::uint8_t*>(entry.path.data()),
            entry.path.size());
        writer.WriteUInt32(static_cast<std::uint32_t>(stored.size()));
        writer.WriteBytes(stored.data(), stored.size());
    }
    if (includeTerminator) {
        writer.WriteUInt32(0);
    }
    writer.WriteUInt32(key);
    writer.WriteUInt32(validity);
    writer.WriteUInt32(kind);
    writer.WriteUInt32(executableSize);
    return writer.Bytes();
}

class LegacyImageFile {
public:
    explicit LegacyImageFile(const std::vector<std::uint8_t>& bytes) {
        static std::atomic<std::uint64_t> sequence{0};
        const auto uniqueValue =
            static_cast<std::uint64_t>(
                std::chrono::steady_clock::now().time_since_epoch().count()) ^
            sequence.fetch_add(1, std::memory_order_relaxed);
        path_ = std::filesystem::temp_directory_path() /
            ("dbp-legacy-pck-" + std::to_string(uniqueValue) + ".exe");
        std::ofstream output(path_, std::ios::binary | std::ios::trunc);
        output.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        output.close();
        EXPECT_TRUE(output);
    }

    ~LegacyImageFile() {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    LegacyImageFile(const LegacyImageFile&) = delete;
    LegacyImageFile& operator=(const LegacyImageFile&) = delete;

    const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

PackageResult<std::unique_ptr<LegacyPckReader>> OpenImage(
    const std::vector<std::uint8_t>& image,
    const LegacyPckLimits& limits = {}) {
    LegacyImageFile file(image);
    return LegacyPckReader::OpenExecutable(file.path(), limits);
}

TEST(LegacyPckReaderTest, ReadsUnencryptedEntriesAndCanonicalizesPaths) {
    const auto image = BuildImage({
        {"media\\textures\\wall.png", {1, 2, 3, 4}},
        {"core.dll", {5, 6}},
    });

    const auto opened = OpenImage(image);

    ASSERT_TRUE(opened) << opened.error().message;
    ASSERT_EQ(opened.value()->entries().size(), 2U);
    EXPECT_EQ(opened.value()->entries()[0].path, "core.dll");
    EXPECT_EQ(opened.value()->entries()[1].path, "media/textures/wall.png");
    EXPECT_EQ(
        *opened.value()->entries()[1].bytes,
        (std::vector<std::uint8_t>{1, 2, 3, 4}));
}

TEST(LegacyPckReaderTest, DecryptsOnlyHistoricalMediaEntries) {
    const std::vector<std::uint8_t> mediaBytes(2053, 0xA7);
    const std::vector<std::uint8_t> rootBytes(2053, 0x42);
    const auto image = BuildImage({
        {"MEDIA\\sound.dat", mediaBytes},
        {"core.dll", rootBytes},
    }, 12321U);

    const auto opened = OpenImage(image);

    ASSERT_TRUE(opened) << opened.error().message;
    ASSERT_EQ(opened.value()->entries().size(), 2U);
    const auto* rootEntry = opened.value()->FindEntry("core.dll");
    const auto* mediaEntry =
        opened.value()->FindEntry("media/sound.dat");
    ASSERT_NE(rootEntry, nullptr);
    ASSERT_NE(mediaEntry, nullptr);
    EXPECT_EQ(*rootEntry->bytes, rootBytes);
    EXPECT_EQ(*mediaEntry->bytes, mediaBytes);
}

TEST(LegacyPckReaderTest, TransformTerminatesAtHistoricalBoundarySizes) {
    for (const auto size : {0U, 1U, 1023U, 1024U}) {
        SCOPED_TRACE(size);
        std::vector<std::uint8_t> plaintext(size, 0x71);
        const auto opened = OpenImage(
            BuildImage({{"media\\boundary.bin", plaintext}}, 12321U));
        ASSERT_TRUE(opened) << opened.error().message;
        ASSERT_EQ(opened.value()->entries().size(), 1U);
        EXPECT_EQ(*opened.value()->entries()[0].bytes, plaintext);
    }
}

TEST(LegacyPckReaderTest, RejectsTruncatedFooterAndInvalidMetadata) {
    {
        const auto opened = OpenImage(std::vector<std::uint8_t>(15, 0));
        ASSERT_FALSE(opened);
        EXPECT_EQ(opened.error().code, PackageErrorCode::InvalidFormat);
    }
    {
        const auto opened = OpenImage(BuildImage({}, 0, true, 7));
        ASSERT_FALSE(opened);
        EXPECT_EQ(opened.error().code, PackageErrorCode::InvalidFormat);
    }
    {
        auto image = BuildImage({});
        const auto invalidSize =
            static_cast<std::uint32_t>(image.size() + 1U);
        const auto offset = image.size() - sizeof(std::uint32_t);
        image[offset] = static_cast<std::uint8_t>(invalidSize);
        image[offset + 1] =
            static_cast<std::uint8_t>(invalidSize >> 8U);
        image[offset + 2] =
            static_cast<std::uint8_t>(invalidSize >> 16U);
        image[offset + 3] =
            static_cast<std::uint8_t>(invalidSize >> 24U);
        const auto opened = OpenImage(image);
        ASSERT_FALSE(opened);
        EXPECT_EQ(opened.error().code, PackageErrorCode::InvalidFormat);
    }
}

TEST(LegacyPckReaderTest, RejectsTruncatedRecordsAndMissingTerminator) {
    constexpr std::size_t executableSize = 64;
    {
        auto image = BuildImage({{"a", {1}}});
        image[executableSize] = 0xFF;
        image[executableSize + 1] = 0xFF;
        image[executableSize + 2] = 0xFF;
        image[executableSize + 3] = 0x7F;
        const auto opened = OpenImage(image);
        ASSERT_FALSE(opened);
        EXPECT_EQ(opened.error().code, PackageErrorCode::LimitExceeded);
    }
    {
        auto image = BuildImage({{"a", {1}}});
        const auto dataLengthOffset =
            executableSize + sizeof(std::uint32_t) + 1U;
        image[dataLengthOffset] = 0xFF;
        image[dataLengthOffset + 1] = 0xFF;
        image[dataLengthOffset + 2] = 0xFF;
        image[dataLengthOffset + 3] = 0x7F;
        const auto opened = OpenImage(image);
        ASSERT_FALSE(opened);
        EXPECT_EQ(opened.error().code, PackageErrorCode::LimitExceeded);
    }
    {
        const auto opened = OpenImage(
            BuildImage({{"a", {1}}}, 0, false));
        ASSERT_FALSE(opened);
        EXPECT_EQ(opened.error().code, PackageErrorCode::InvalidFormat);
    }
}

TEST(LegacyPckReaderTest, RejectsUnsafeAndDuplicatePaths) {
    for (const auto& path : {
             std::string("..\\escape.dat"),
             std::string("C:\\absolute.dat"),
             std::string("\\\\server\\share.dat")}) {
        SCOPED_TRACE(path);
        const auto opened = OpenImage(BuildImage({{path, {1}}}));
        ASSERT_FALSE(opened);
        EXPECT_EQ(opened.error().code, PackageErrorCode::UnsafePath);
    }

    const auto duplicate = OpenImage(BuildImage({
        {"media\\same.dat", {1}},
        {"MEDIA/same.dat", {2}},
    }));
    ASSERT_FALSE(duplicate);
    EXPECT_EQ(duplicate.error().code, PackageErrorCode::UnsafePath);
}

TEST(LegacyPckReaderTest, EnforcesConfiguredEntryAndPayloadLimits) {
    LegacyPckLimits entryLimits;
    entryLimits.maximumEntryCount = 1;
    const auto tooMany = OpenImage(
        BuildImage({{"a", {1}}, {"b", {2}}}),
        entryLimits);
    ASSERT_FALSE(tooMany);
    EXPECT_EQ(tooMany.error().code, PackageErrorCode::LimitExceeded);

    LegacyPckLimits payloadLimits;
    payloadLimits.maximumEntryBytes = 2;
    const auto tooLarge = OpenImage(
        BuildImage({{"a", {1, 2, 3}}}),
        payloadLimits);
    ASSERT_FALSE(tooLarge);
    EXPECT_EQ(tooLarge.error().code, PackageErrorCode::LimitExceeded);
}

TEST(LegacyPckReaderTest, NeverExecutesLegacyCompressionPlugins) {
    const auto opened =
        OpenImage(BuildImage({{"compress.dll", {1, 2, 3}}}));

    ASSERT_FALSE(opened);
    EXPECT_EQ(
        opened.error().code,
        PackageErrorCode::UnsupportedAlgorithm);
}

TEST(LegacyPckReaderTest, OpensTrackedHistoricalExecutableWhenPresent) {
    const auto fixture =
        std::filesystem::path(DBP_TEST_SOURCE_ROOT) /
        "Install/Help/examples/multiplayer/mp.exe";
    if (!std::filesystem::exists(fixture)) {
        GTEST_SKIP() << "Tracked historical executable is unavailable.";
    }

    const auto opened = LegacyPckReader::OpenExecutable(fixture);

    ASSERT_TRUE(opened) << opened.error().message;
    ASSERT_FALSE(opened.value()->entries().empty());
    EXPECT_NE(opened.value()->FindEntry("_virtual.dat"), nullptr);
    EXPECT_NE(opened.value()->FindEntry("core.dll"), nullptr);
}

} // namespace
