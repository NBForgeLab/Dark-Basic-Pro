// test_codegen_golden.cpp — golden-file verification of the assembly
// generator.
//
// Every .dba file under tests/codegen/goldens is compiled through the full
// pipeline (parse -> statement walk -> assembly emission -> relocation) and
// the complete result — metadata, DBM listing, forward-reference table, raw
// machine bytes and relocated machine bytes — is rendered into one canonical
// document and diffed against the stored <name>.expected file.
//
// This is the regression net for "the generated code changed". A diff here
// means either an intended codegen improvement (regenerate) or a defect.
//
// Regenerate:  set DBP_UPDATE_GOLDENS=1 and re-run dbp_tests.

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "DBPLogger.h"
#include "codegen/CodegenHarness.h"

namespace fs = std::filesystem;
using namespace dbp::codegen;

namespace {

std::vector<fs::path> CollectGoldenCases()
{
    std::vector<fs::path> cases;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(GoldensDirectory(), ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() == ".dba") {
            cases.push_back(entry.path());
        }
    }
    std::sort(cases.begin(), cases.end());
    return cases;
}

std::string ReadFile(const fs::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

class CodegenGoldenTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        DBPLogger::Initialize("test_codegen_golden.log");
        EnsureEnvironment();
    }
    static void TearDownTestSuite() { spdlog::shutdown(); }
};

} // namespace

TEST_F(CodegenGoldenTest, GoldenCorpusIsNotEmpty)
{
    const auto cases = CollectGoldenCases();
    EXPECT_GE(cases.size(), 20u) << "the golden corpus should cover every major construct";
}

// One test per golden file: gtest reports each case independently so a
// regression in one construct does not mask the others.
class CodegenGoldenFileTest
    : public ::testing::TestWithParam<fs::path> {
protected:
    static void SetUpTestSuite()
    {
        DBPLogger::Initialize("test_codegen_golden.log");
        EnsureEnvironment();
    }
    static void TearDownTestSuite() { spdlog::shutdown(); }
};

TEST_P(CodegenGoldenFileTest, MatchesStoredGolden)
{
    const fs::path sourcePath = GetParam();
    const std::string caseName = sourcePath.stem().string();
    const fs::path goldenPath =
        sourcePath.parent_path() / (sourcePath.stem().string() + ".expected");

    const std::string source = ReadFile(sourcePath);
    ASSERT_FALSE(source.empty()) << "golden source is empty: " << sourcePath.string();

    Options options;
    options.relocate = true;
    options.captureListing = true;

    const Snapshot snapshot = CompileSnippet(source, options);
    const std::string document = RenderGoldenDocument(snapshot, caseName);

    if (UpdateGoldensEnabled()) {
        std::string ignored;
        ASSERT_TRUE(CompareOrUpdateGolden(goldenPath, document, &ignored))
            << "failed to write golden for " << caseName;
        return;
    }

    std::string diff;
    EXPECT_TRUE(CompareOrUpdateGolden(goldenPath, document, &diff))
        << "codegen output for '" << caseName << "' diverged from the golden file.\n"
        << "  source : " << sourcePath.string() << "\n"
        << "  golden : " << goldenPath.string() << "\n"
        << "  diff   :\n"
        << diff << "\n"
        << "Regenerate with DBP_UPDATE_GOLDENS=1 if the change is intended.";
}

// A golden case that fails to compile still produces a valid document, but it
// is almost always a defect: the corpus is deliberately made of well-formed
// programs. Asserting keeps a silent regression (e.g. a parser rule that
// starts rejecting valid code) from being baked into the goldens.
TEST_P(CodegenGoldenFileTest, GoldenCaseCompilesCleanly)
{
    const fs::path sourcePath = GetParam();
    const std::string caseName = sourcePath.stem().string();
    const std::string source = ReadFile(sourcePath);

    const Snapshot snapshot = CompileSnippet(source);

    EXPECT_FALSE(snapshot.hasParserError)
        << caseName << ": parser error -> " << snapshot.parserErrorMessage;
    EXPECT_TRUE(snapshot.parsed) << caseName << ": MakeStatements failed at " << snapshot.stage;
    EXPECT_TRUE(snapshot.emitted)
        << caseName << ": statement emission stopped at " << snapshot.stage
        << " (error: " << snapshot.errorMessage << ")";
    EXPECT_TRUE(snapshot.relocated)
        << caseName << ": relocation failed (error: " << snapshot.errorMessage << ")";
    EXPECT_FALSE(snapshot.bytes.empty()) << caseName << ": no machine code was emitted";
}

INSTANTIATE_TEST_SUITE_P(
    CodegenGoldens,
    CodegenGoldenFileTest,
    ::testing::ValuesIn(CollectGoldenCases()),
    [](const ::testing::TestParamInfo<fs::path>& info) {
        return info.param.stem().string();
    });
