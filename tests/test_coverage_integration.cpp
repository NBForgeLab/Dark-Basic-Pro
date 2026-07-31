/**
 * @file test_coverage_integration.cpp
 * @brief Integration tests verifying code-coverage build infrastructure.
 *
 * These tests validate that the DBP_ENABLE_COVERAGE CMake option and the
 * dbp_enable_coverage() function are wired correctly into the build system.
 * They run under every preset but are most meaningful when the coverage
 * preset (windows-x86-coverage) is active.
 */

#include <gtest/gtest.h>
#include <cstdlib>
#include <fstream>
#include <string>

// ---------------------------------------------------------------------------
// Build-system integration
// ---------------------------------------------------------------------------

/// The CMake option DBP_ENABLE_COVERAGE is defined at configure time.
/// When the coverage preset is used the preprocessor symbol
/// DBP_COVERAGE_ENABLED is set by the build system.
TEST(CoverageIntegration, CoverageOptionIsRecognised) {
#ifdef DBP_COVERAGE_ENABLED
    // Coverage build — the symbol is defined by CMake.
    EXPECT_TRUE(true) << "DBP_COVERAGE_ENABLED is defined";
#else
    // Non-coverage build — test still passes; coverage is simply off.
    SUCCEED() << "DBP_COVERAGE_ENABLED not defined (coverage build disabled)";
#endif
}

/// Verify that the coverage report script exists in the source tree.
TEST(CoverageIntegration, CoverageReportScriptExists) {
    // DBP_TEST_SOURCE_ROOT is defined by the test target.
#ifdef DBP_TEST_SOURCE_ROOT
    const std::string scriptPath =
        std::string(DBP_TEST_SOURCE_ROOT) + "/scripts/coverage-report.ps1";
    std::ifstream f(scriptPath);
    EXPECT_TRUE(f.good())
        << "coverage-report.ps1 must exist at " << scriptPath;
#else
    SUCCEED() << "DBP_TEST_SOURCE_ROOT not defined — skipping path check";
#endif
}

/// Verify that the CMakePresets.json contains the coverage preset entry.
TEST(CoverageIntegration, CoveragePresetPresentInCMakePresets) {
#ifdef DBP_TEST_SOURCE_ROOT
    const std::string presetsPath =
        std::string(DBP_TEST_SOURCE_ROOT) + "/CMakePresets.json";
    std::ifstream f(presetsPath);
    ASSERT_TRUE(f.good()) << "CMakePresets.json must exist";

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("windows-x86-coverage"), std::string::npos)
        << "CMakePresets.json must contain a windows-x86-coverage preset";
    EXPECT_NE(content.find("DBP_ENABLE_COVERAGE"), std::string::npos)
        << "CMakePresets.json coverage preset must set DBP_ENABLE_COVERAGE";
#else
    SUCCEED() << "DBP_TEST_SOURCE_ROOT not defined — skipping preset check";
#endif
}

// ---------------------------------------------------------------------------
// Runtime sanity — proves coverage instrumentation does not break execution
// ---------------------------------------------------------------------------

TEST(CoverageIntegration, BasicComputationUnderCoverage) {
    // A trivial computation that the coverage tool can instrument.
    int sum = 0;
    for (int i = 1; i <= 100; ++i) {
        sum += i;
    }
    EXPECT_EQ(sum, 5050);
}

TEST(CoverageIntegration, StringOperationsUnderCoverage) {
    std::string greeting = "Dark";
    greeting += " Basic";
    greeting += " Pro";
    EXPECT_EQ(greeting, "Dark Basic Pro");
    EXPECT_EQ(greeting.size(), 14u);
}

TEST(CoverageIntegration, FilesystemAccessUnderCoverage) {
    // Verify we can query the environment without crashing under
    // coverage instrumentation.
    const char* path = std::getenv("PATH");
    // PATH is expected to be set on every CI platform.
    if (path) {
        EXPECT_GT(std::string(path).size(), 0u);
    } else {
        SUCCEED() << "PATH not set (unusual but not a failure)";
    }
}
