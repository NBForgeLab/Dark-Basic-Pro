#include <gtest/gtest.h>

#include "ProjectManifest.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

class TemporaryProject {
public:
    explicit TemporaryProject(const std::string& contents) {
        const auto suffix = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        directory_ = std::filesystem::temp_directory_path() /
            ("dbpro_manifest_test_" + suffix);
        std::filesystem::create_directories(directory_);
        path_ = directory_ / "Project.dbpro";
        std::ofstream stream(path_, std::ios::binary);
        stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    }

    ~TemporaryProject() {
        std::error_code error;
        std::filesystem::remove_all(directory_, error);
    }

    const std::filesystem::path& path() const noexcept { return path_; }
    const std::filesystem::path& directory() const noexcept { return directory_; }

private:
    std::filesystem::path directory_;
    std::filesystem::path path_;
};

} // namespace

TEST(ProjectManifestReaderTest, ReadsMainAndNumericIncludesInOrder) {
    TemporaryProject project(
        "MAIN=FPSCREATOR.dba\r\n"
        "include1=FPSC-Game\\FPSC-GameMain.dba\r\n"
        "include2=FPSC-Game\\FPSC-Game.dba\r\n"
        "final source=_Temp.dbsource\r\n"
        "executable=FPSC-Game.exe\r\n");

    const auto result = ProjectManifestReader::Read(project.path());

    ASSERT_TRUE(result.has_value()) << result.error().message;
    ASSERT_EQ(result.value().sources.size(), 3u);
    EXPECT_EQ(result.value().sources[0].manifestKey, "main");
    EXPECT_EQ(result.value().sources[1].manifestKey, "include1");
    EXPECT_EQ(result.value().sources[2].manifestKey, "include2");
    EXPECT_EQ(
        result.value().sources[1].resolvedPath,
        (project.directory() / "FPSC-Game" / "FPSC-GameMain.dba").lexically_normal());
    ASSERT_TRUE(result.value().finalSourcePath.has_value());
    EXPECT_EQ(
        *result.value().finalSourcePath,
        (project.directory() / "_Temp.dbsource").lexically_normal());
}

TEST(ProjectManifestReaderTest, RejectsMissingMain) {
    TemporaryProject project("include1=Only.dba\r\n");

    const auto result = ProjectManifestReader::Read(project.path());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ProjectErrorCode::MissingMain);
}

TEST(ProjectManifestReaderTest, RejectsIncludeIndexGap) {
    TemporaryProject project(
        "main=Main.dba\r\n"
        "include1=One.dba\r\n"
        "include3=Three.dba\r\n");

    const auto result = ProjectManifestReader::Read(project.path());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ProjectErrorCode::NonContiguousIncludes);
}

TEST(ProjectManifestReaderTest, RejectsMalformedIncludeKey) {
    TemporaryProject project(
        "main=Main.dba\r\n"
        "includeX=Wrong.dba\r\n");

    const auto result = ProjectManifestReader::Read(project.path());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ProjectErrorCode::MalformedIncludeKey);
}

TEST(ProjectManifestReaderTest, RejectsDuplicateIncludeIndexRegardlessOfKeyCase) {
    TemporaryProject project(
        "main=Main.dba\r\n"
        "include1=One.dba\r\n"
        "INCLUDE1=Duplicate.dba\r\n");

    const auto result = ProjectManifestReader::Read(project.path());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ProjectErrorCode::DuplicateIncludeIndex);
}

TEST(ProjectManifestReaderTest, OrdersMultiDigitIncludeIndicesNumerically) {
    std::string contents = "main=Main.dba\r\nproject name=Ignored\r\n";
    for (int index = 1; index <= 10; ++index) {
        contents += "include" + std::to_string(index) + "=Part" +
            std::to_string(index) + ".dba\r\n";
    }
    TemporaryProject project(contents);

    const auto result = ProjectManifestReader::Read(project.path());

    ASSERT_TRUE(result.has_value()) << result.error().message;
    ASSERT_EQ(result.value().sources.size(), 11u);
    EXPECT_EQ(result.value().sources[10].manifestKey, "include10");
    EXPECT_EQ(result.value().sources[10].declaredPath, "Part10.dba");
}

TEST(ProjectManifestReaderTest, RejectsEmptyMain) {
    TemporaryProject project("main=   \r\ninclude1=One.dba\r\n");

    const auto result = ProjectManifestReader::Read(project.path());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ProjectErrorCode::MissingMain);
}

TEST(ProjectManifestReaderTest, RejectsDuplicateMainRegardlessOfKeyCase) {
    TemporaryProject project("main=First.dba\r\nMAIN=Second.dba\r\n");

    const auto result = ProjectManifestReader::Read(project.path());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ProjectErrorCode::DuplicateMain);
    EXPECT_EQ(result.error().manifestKey, "MAIN");
}
