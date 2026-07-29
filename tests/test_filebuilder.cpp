#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <windows.h>
#include "FileBuilder.h"
#include "PublicationDiagnostics.h"
#include "DataTable.h"

// ---------------------------------------------------------------------------
// CFileBuilder contracts
// ---------------------------------------------------------------------------

TEST(FileBuilderTest, DerivesRuntimeDescriptorBesideExecutable) {
    CFileBuilder builder;

    char absoluteName[] = "C:\\Projects\\My Game\\game.exe";
    char relativeName[] = "output.exe";

    EXPECT_EQ(
        builder.GetPackageDescriptorFileFromEXEFile(absoluteName),
        std::filesystem::path(
            L"C:\\Projects\\My Game\\game.dbpakref"));
    EXPECT_EQ(
        builder.GetPackageDescriptorFileFromEXEFile(relativeName),
        std::filesystem::path(L"output.dbpakref"));
}

TEST(FileBuilderTest, ActiveProductionCannotReintroduceLegacyPckWriting) {
    const auto root = std::filesystem::path(DBP_TEST_SOURCE_ROOT);
    const std::vector<std::filesystem::path> activeSources{
        root / "DBProCompiler/DBPCompiler/FileBuilder.cpp",
        root / "DBProCompiler/DBPCompiler/FileBuilder.h",
        root / "DBProCompiler/DBPCompiler/ASMWriter.cpp",
        root / "DBProCompiler/DBPCompilerEXE/DarkEXE.cpp",
    };
    const std::vector<std::string> forbidden{
        "CEncryptor",
        "12321",
        "AddPCKToEXE",
        "ConstructPCK",
        "D:\\GitHub-repo",
    };

    for (const auto& source : activeSources) {
        std::ifstream input(source, std::ios::binary);
        ASSERT_TRUE(input) << source.string();
        const std::string contents{
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
        for (const auto& token : forbidden) {
            EXPECT_EQ(contents.find(token), std::string::npos)
                << token << " remains active in " << source.string();
        }
    }
}

TEST(FileBuilderTest, DelegatesPublicationTransactionToSharedService) {
    const auto source =
        std::filesystem::path(DBP_TEST_SOURCE_ROOT) /
        "DBProCompiler/DBPCompiler/FileBuilder.cpp";
    std::ifstream input(source, std::ios::binary);
    ASSERT_TRUE(input) << source.string();
    const std::string contents{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};

    EXPECT_NE(
        contents.find("ApplicationPublisher"),
        std::string::npos);
    for (const auto* const forbidden : {
             "ReplaceFileW(",
             "MoveFileExW(",
             "InjectExecutablePackageKeys(",
             "WriteRuntimeDescriptorAtomically(",
         }) {
        EXPECT_EQ(contents.find(forbidden), std::string::npos)
            << forbidden
            << " bypasses the shared publication transaction";
    }
}

TEST(FileBuilderTest, MapsStructuredPublicationPhasesToStableDiagnostics) {
    using dbp::package::ApplicationPublicationPhase;
    using dbp::package::PackageError;
    using dbp::package::PackageErrorCode;

    PackageError error{
        PackageErrorCode::PublicationFailed,
        "failure",
        std::nullopt,
    };
    for (const auto& expected : {
             std::pair{
                 ApplicationPublicationPhase::Package,
                 std::string_view("DBP3105")},
             std::pair{
                 ApplicationPublicationPhase::Executable,
                 std::string_view("DBP3106")},
             std::pair{
                 ApplicationPublicationPhase::Descriptor,
                 std::string_view("DBP3107")},
             std::pair{
                 ApplicationPublicationPhase::Cleanup,
                 std::string_view("DBP3107")},
         }) {
        error.applicationPublicationPhase = expected.first;
        EXPECT_EQ(
            dbp::compiler::PublicationDiagnosticCode(error),
            expected.second);
    }
}

TEST(FileBuilderTest, DefersNonCriticalCompilerStageCleanupAfterCommit) {
    const auto stage =
        std::filesystem::temp_directory_path() /
        ("dbp-compiler-stage-" +
         std::to_string(GetCurrentProcessId()) + "-" +
         std::to_string(GetCurrentThreadId()));
    {
        std::ofstream output(stage, std::ios::binary);
        output << "host without an injected package key";
        ASSERT_TRUE(output);
    }

    EXPECT_EQ(
        dbp::compiler::CleanupCompilerStageAfterCommit(
            stage,
            true),
        dbp::compiler::CompilerStageCleanupResult::Deferred);
    EXPECT_TRUE(std::filesystem::is_regular_file(stage));
    EXPECT_EQ(
        dbp::compiler::CleanupCompilerStageAfterCommit(
            stage,
            false),
        dbp::compiler::CompilerStageCleanupResult::Removed);
    EXPECT_FALSE(std::filesystem::exists(stage));
}

// ---------------------------------------------------------------------------
// CDataTable memory contracts
// ---------------------------------------------------------------------------

// The chain owns every node and string added to it (unique_ptr members free
// them on destruction); FindString locates entries by stored index.
TEST(DataTableTest, AddStringThenFindStringReturnsIndex) {
    CDataTable table;

    char text[] = "hello world";
    ASSERT_TRUE(table.AddString(text, 7));

    EXPECT_EQ(table.FindString(text), 7u);
}

// AddUniqueString must reject a duplicate and hand back the existing index
// without leaking or double-owning the rejected node.
TEST(DataTableTest, AddUniqueStringRejectsDuplicate) {
    CDataTable table;

    char text[] = "unique entry";
    DWORD dwIndex = 3;
    ASSERT_TRUE(table.AddUniqueString(text, &dwIndex));

    DWORD dwDuplicateIndex = 99;
    EXPECT_FALSE(table.AddUniqueString(text, &dwDuplicateIndex));
    EXPECT_EQ(dwDuplicateIndex, 3u);
}

// AddTwoStrings stores both strings in the owned node.
TEST(DataTableTest, AddTwoStringsStoresBothStrings) {
    CDataTable table;

    char first[] = "first";
    char second[] = "second";
    DWORD dwIndex = 5;
    ASSERT_TRUE(table.AddTwoStrings(first, second, &dwIndex));

    CDataTable* pNode = table.GetNext();
    ASSERT_NE(pNode, nullptr);
    ASSERT_NE(pNode->GetString(), nullptr);
    ASSERT_NE(pNode->GetString2(), nullptr);
    EXPECT_STREQ(pNode->GetString()->GetStr(), "first");
    EXPECT_STREQ(pNode->GetString2()->GetStr(), "second");
    EXPECT_EQ(pNode->GetIndex(), 5u);
}
