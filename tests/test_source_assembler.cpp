#include <gtest/gtest.h>

#include "SourceAssembler.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

class SourceFixture {
public:
    SourceFixture() {
        const auto suffix = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        directory_ = std::filesystem::temp_directory_path() /
            ("dbpro_source_assembler_test_" + suffix);
        std::filesystem::create_directories(directory_);
    }

    ~SourceFixture() {
        std::error_code error;
        std::filesystem::remove_all(directory_, error);
    }

    std::filesystem::path Write(const std::string& name, const std::string& bytes) {
        const auto path = directory_ / name;
        std::ofstream stream(path, std::ios::binary);
        stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        return path;
    }

    ProjectManifest Manifest(
        const std::vector<std::pair<std::string, std::filesystem::path>>& sources) const {
        ProjectManifest manifest;
        manifest.projectDirectory = directory_;
        manifest.projectPath = directory_ / "Project.dbpro";
        for (const auto& source : sources) {
            manifest.sources.push_back({source.first, source.second.filename(), source.second});
        }
        return manifest;
    }

private:
    std::filesystem::path directory_;
};

std::string AsString(const std::vector<std::byte>& bytes) {
    return std::string(
        reinterpret_cast<const char*>(bytes.data()),
        reinterpret_cast<const char*>(bytes.data()) + bytes.size());
}

} // namespace

TEST(SourceAssemblerTest, PreservesOrderAndAddsOnlyMissingLineBoundary) {
    SourceFixture fixture;
    const auto main = fixture.Write("Main.dba", "alpha");
    const auto include1 = fixture.Write("One.dba", "beta\r\n");
    const auto include2 = fixture.Write("Two.dba", "gamma\n");
    const auto manifest = fixture.Manifest({
        {"main", main}, {"include1", include1}, {"include2", include2}});

    SourceAssemblyOptions options;
    options.maxBytes = 1024;
    const auto result = SourceAssembler::Assemble(manifest, options);

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(AsString(result.value().bytes), "alpha\r\nbeta\r\ngamma\n");
    ASSERT_EQ(result.value().sourceMap.size(), 3u);
    EXPECT_EQ(result.value().sourceMap[0].combinedLineStart, 1u);
    EXPECT_EQ(result.value().sourceMap[1].combinedLineStart, 2u);
    EXPECT_EQ(result.value().sourceMap[2].combinedLineStart, 3u);
}

TEST(SourceAssemblerTest, RejectsMissingSourceWithManifestContext) {
    SourceFixture fixture;
    const auto missing = std::filesystem::temp_directory_path() /
        "dbpro_source_that_must_not_exist.dba";
    std::error_code ignored;
    std::filesystem::remove(missing, ignored);
    const auto manifest = fixture.Manifest({{"main", missing}});

    const auto result = SourceAssembler::Assemble(manifest, {});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SourceAssemblyErrorCode::SourceNotFound);
    EXPECT_EQ(result.error().manifestKey, "main");
}

TEST(SourceAssemblerTest, RejectsCombinedSourceLargerThanConfiguredLimit) {
    SourceFixture fixture;
    const auto main = fixture.Write("Main.dba", "12345");
    const auto include = fixture.Write("Include.dba", "67890");
    const auto manifest = fixture.Manifest({{"main", main}, {"include1", include}});

    SourceAssemblyOptions options;
    options.maxBytes = 10;
    const auto result = SourceAssembler::Assemble(manifest, options);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SourceAssemblyErrorCode::SourceTooLarge);
}

TEST(SourceAssemblerTest, PublishesFinalSourceAtomically) {
    SourceFixture fixture;
    const auto destination = fixture.Write("_Temp.dbsource", "old");
    const std::string newText = "new source\r\n";
    const std::vector<std::byte> bytes(
        reinterpret_cast<const std::byte*>(newText.data()),
        reinterpret_cast<const std::byte*>(newText.data() + newText.size()));

    const auto result = FinalSourceArtifactWriter::WriteAtomically(destination, bytes);

    ASSERT_TRUE(result.has_value()) << result.error().message;
    std::ifstream stream(destination, std::ios::binary);
    const std::string actual(
        (std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    EXPECT_EQ(actual, newText);
    EXPECT_FALSE(std::filesystem::exists(destination.string() + ".tmp"));
}
