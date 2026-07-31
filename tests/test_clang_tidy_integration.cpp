#include <gtest/gtest.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

std::string readFileContents(const std::filesystem::path& path) {
    std::ifstream file(path);
    return std::string(std::istreambuf_iterator<char>(file),
                       std::istreambuf_iterator<char>());
}

bool fileContains(const std::string& content, const std::string& needle) {
    return content.find(needle) != std::string::npos;
}

} // namespace

// ---------------------------------------------------------------------------
// .clang-tidy configuration validation
// ---------------------------------------------------------------------------

TEST(ClangTidyIntegrationTest, ConfigFileExistsInSourceRoot) {
    const std::filesystem::path root(DBP_TEST_SOURCE_ROOT);
    const auto configPath = root / ".clang-tidy";
    EXPECT_TRUE(std::filesystem::exists(configPath))
        << ".clang-tidy configuration must exist at project root";
}

TEST(ClangTidyIntegrationTest, ConfigEnablesBugproneChecks) {
    const std::filesystem::path root(DBP_TEST_SOURCE_ROOT);
    const auto configPath = root / ".clang-tidy";
    ASSERT_TRUE(std::filesystem::exists(configPath));

    const std::string content = readFileContents(configPath);
    EXPECT_TRUE(fileContains(content, "bugprone-"))
        << "clang-tidy config must enable bugprone-* checks";
}

TEST(ClangTidyIntegrationTest, ConfigEnablesModernizeChecks) {
    const std::filesystem::path root(DBP_TEST_SOURCE_ROOT);
    const auto configPath = root / ".clang-tidy";
    ASSERT_TRUE(std::filesystem::exists(configPath));

    const std::string content = readFileContents(configPath);
    EXPECT_TRUE(fileContains(content, "modernize-"))
        << "clang-tidy config must enable modernize-* checks";
}

TEST(ClangTidyIntegrationTest, ConfigEnablesPerformanceChecks) {
    const std::filesystem::path root(DBP_TEST_SOURCE_ROOT);
    const auto configPath = root / ".clang-tidy";
    ASSERT_TRUE(std::filesystem::exists(configPath));

    const std::string content = readFileContents(configPath);
    EXPECT_TRUE(fileContains(content, "performance-"))
        << "clang-tidy config must enable performance-* checks";
}

TEST(ClangTidyIntegrationTest, ConfigEnablesReadabilityChecks) {
    const std::filesystem::path root(DBP_TEST_SOURCE_ROOT);
    const auto configPath = root / ".clang-tidy";
    ASSERT_TRUE(std::filesystem::exists(configPath));

    const std::string content = readFileContents(configPath);
    EXPECT_TRUE(fileContains(content, "readability-"))
        << "clang-tidy config must enable readability-* checks";
}

// ---------------------------------------------------------------------------
// CMake integration validation
// ---------------------------------------------------------------------------

TEST(ClangTidyIntegrationTest, CMakeListsDefinesClangTidyOption) {
    const std::filesystem::path root(DBP_TEST_SOURCE_ROOT);
    const auto cmakePath = root / "CMakeLists.txt";
    ASSERT_TRUE(std::filesystem::exists(cmakePath));

    const std::string content = readFileContents(cmakePath);
    EXPECT_TRUE(fileContains(content, "DBP_ENABLE_CLANG_TIDY"))
        << "Root CMakeLists.txt must define DBP_ENABLE_CLANG_TIDY option";
    EXPECT_TRUE(fileContains(content, "CMAKE_CXX_CLANG_TIDY"))
        << "Root CMakeLists.txt must set CMAKE_CXX_CLANG_TIDY when enabled";
}

TEST(ClangTidyIntegrationTest, CMakePresetsIncludeClangTidyPreset) {
    const std::filesystem::path root(DBP_TEST_SOURCE_ROOT);
    const auto presetsPath = root / "CMakePresets.json";
    ASSERT_TRUE(std::filesystem::exists(presetsPath));

    const std::string content = readFileContents(presetsPath);
    EXPECT_TRUE(fileContains(content, "clang-tidy"))
        << "CMakePresets.json must include a clang-tidy preset";
}
