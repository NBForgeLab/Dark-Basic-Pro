#include <gtest/gtest.h>

#include "DBProTools/Publisher/PublisherManifest.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

using namespace dbp::package;
using namespace dbp::publisher;

class PublisherManifestFixture : public testing::Test {
protected:
    void SetUp() override {
        static std::atomic<std::uint64_t> sequence{0};
        const auto unique =
            static_cast<std::uint64_t>(
                std::chrono::steady_clock::now()
                    .time_since_epoch()
                    .count()) ^
            sequence.fetch_add(1, std::memory_order_relaxed);
        root_ = std::filesystem::temp_directory_path() /
            ("dbp-publisher-manifest-" +
             std::to_string(unique));
        std::filesystem::create_directories(root_);
        WriteBinary("host.exe", {0x4D, 0x5A});
        WriteBinary("asset.bin", {0x10, 0x20, 0x30});
    }

    void TearDown() override {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    std::filesystem::path WriteBinary(
        const std::string& name,
        const std::vector<std::uint8_t>& bytes) const {
        const auto path = root_ / name;
        std::ofstream output(
            path,
            std::ios::binary | std::ios::trunc);
        output.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        output.close();
        EXPECT_TRUE(output);
        return path;
    }

    std::filesystem::path WriteManifest(
        const std::string& contents) const {
        const auto path = root_ / "publisher.json";
        std::ofstream output(
            path,
            std::ios::binary | std::ios::trunc);
        output << contents;
        output.close();
        EXPECT_TRUE(output);
        return path;
    }

    PackageResult<PublisherManifest> Parse(
        const std::string& contents,
        const PackageLimits& limits = {}) const {
        return ReadPublisherManifest(
            WriteManifest(contents),
            limits);
    }

    std::filesystem::path root_;
};

TEST_F(PublisherManifestFixture, ParsesStrictVersionOneDocument) {
    const auto parsed = Parse(R"json({
        "schemaVersion": 1,
        "hostExecutable": "host.exe",
        "outputExecutable": "dist/game.exe",
        "mode": "application",
        "assets": [
            {
                "source": "asset.bin",
                "destination": "media/asset.bin",
                "compress": false
            }
        ]
    })json");

    ASSERT_TRUE(parsed) << parsed.error().message;
    EXPECT_EQ(parsed.value().schemaVersion, 1U);
    EXPECT_EQ(
        parsed.value().hostExecutable,
        (root_ / "host.exe").lexically_normal());
    EXPECT_EQ(
        parsed.value().outputExecutable,
        (root_ / "dist/game.exe").lexically_normal());
    ASSERT_EQ(parsed.value().assets.size(), 1U);
    EXPECT_EQ(
        parsed.value().assets.front().source,
        (root_ / "asset.bin").lexically_normal());
    EXPECT_EQ(
        parsed.value().assets.front().destination,
        "media/asset.bin");
    EXPECT_FALSE(parsed.value().assets.front().compress);
    EXPECT_TRUE(parsed.value().assets.front().sourceIdentity.has_value());

    const auto request = BuildApplicationPublishRequest(
        parsed.value(),
        KeyId{});
    ASSERT_EQ(request.entries.size(), 1U);
    EXPECT_EQ(
        request.entries.front().expectedIdentity,
        parsed.value().assets.front().sourceIdentity);
}

TEST_F(
    PublisherManifestFixture,
    RejectsUnknownSchemaUnknownFieldsAndDuplicateKeys) {
    for (const auto& document : {
             R"json({"schemaVersion":2,"hostExecutable":"host.exe","outputExecutable":"game.exe","assets":[]})json",
             R"json({"schemaVersion":1,"hostExecutable":"host.exe","outputExecutable":"game.exe","assets":[],"unknwon":true})json",
             R"json({"schemaVersion":1,"schemaVersion":1,"hostExecutable":"host.exe","outputExecutable":"game.exe","assets":[]})json",
             R"json({"schemaVersion":1,"hostExecutable":"host.exe","outputExecutable":"game.exe","assets":[{"source":"asset.bin","destination":"a.bin","destination":"b.bin"}]})json",
             R"json({"schemaVersion":1,"hostExecutable":"host.exe","outputExecutable":"game.exe","assets":[{"source":"asset.bin","destination":"a.bin","unknown":1}]})json"}) {
        SCOPED_TRACE(document);
        const auto parsed = Parse(document);
        ASSERT_FALSE(parsed);
        EXPECT_EQ(
            parsed.error().code,
            PackageErrorCode::InvalidFormat);
    }
}

TEST_F(
    PublisherManifestFixture,
    RejectsUnsafeDestinationsAndCaseInsensitiveCollisions) {
    for (const auto& destinations : {
             R"json([{"source":"asset.bin","destination":"../escape.bin"}])json",
             R"json([{"source":"asset.bin","destination":"C:/absolute.bin"}])json",
             R"json([{"source":"asset.bin","destination":"Media/A.bin"},{"source":"asset.bin","destination":"media/a.bin"}])json"}) {
        const auto parsed = Parse(
            std::string(
                R"json({"schemaVersion":1,"hostExecutable":"host.exe","outputExecutable":"game.exe","assets":)json") +
            destinations +
            "}");
        ASSERT_FALSE(parsed);
        EXPECT_TRUE(
            parsed.error().code == PackageErrorCode::UnsafePath ||
            parsed.error().code == PackageErrorCode::InvalidFormat);
    }
}

TEST_F(
    PublisherManifestFixture,
    RejectsMissingSourcesAndConfiguredLimits) {
    auto parsed = Parse(R"json({
        "schemaVersion":1,
        "hostExecutable":"host.exe",
        "outputExecutable":"game.exe",
        "assets":[{"source":"missing.bin","destination":"a.bin"}]
    })json");
    ASSERT_FALSE(parsed);
    EXPECT_EQ(parsed.error().code, PackageErrorCode::IoFailed);

    PackageLimits limits;
    limits.maximumEntries = 0;
    parsed = Parse(R"json({
        "schemaVersion":1,
        "hostExecutable":"host.exe",
        "outputExecutable":"game.exe",
        "assets":[{"source":"asset.bin","destination":"a.bin"}]
    })json", limits);
    ASSERT_FALSE(parsed);
    EXPECT_EQ(parsed.error().code, PackageErrorCode::LimitExceeded);

    limits = {};
    limits.maximumEntryPlaintextSize = 2;
    parsed = Parse(R"json({
        "schemaVersion":1,
        "hostExecutable":"host.exe",
        "outputExecutable":"game.exe",
        "assets":[{"source":"asset.bin","destination":"a.bin"}]
    })json", limits);
    ASSERT_FALSE(parsed);
    EXPECT_EQ(parsed.error().code, PackageErrorCode::LimitExceeded);
}

TEST_F(
    PublisherManifestFixture,
    RejectsExcessiveNestingBeforeBuildingJsonDom) {
    std::string nested(64, '[');
    nested += "0";
    nested.append(64, ']');
    const auto parsed = Parse(
        std::string(
            R"json({"schemaVersion":1,"hostExecutable":"host.exe","outputExecutable":"game.exe","assets":)json") +
        nested +
        "}");

    ASSERT_FALSE(parsed);
    EXPECT_EQ(
        parsed.error().code,
        PackageErrorCode::LimitExceeded);
}

} // namespace
