#include <gtest/gtest.h>
#include "Error.h"
#include "CompilerArguments.h"
#include <vector>
#include <string>

TEST(CLITest, ParseCommandLineNormal) {
    std::string cmdLine = "DBPCompiler.exe --json \"D:\\My Game\\project.dbpro\"";
    std::vector<std::string> args = ParseCommandLine(cmdLine);
    
    ASSERT_EQ(args.size(), 3);
    EXPECT_EQ(args[0], "DBPCompiler.exe");
    EXPECT_EQ(args[1], "--json");
    EXPECT_EQ(args[2], "D:\\My Game\\project.dbpro");
}

TEST(CLITest, ParseCommandLineExtraSpaces) {
    std::string cmdLine = "  DBPCompiler.exe   -h   ";
    std::vector<std::string> args = ParseCommandLine(cmdLine);
    
    ASSERT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "DBPCompiler.exe");
    EXPECT_EQ(args[1], "-h");
}

TEST(CLITest, EscapeJSONSpecialChars) {
    std::string input = "Error in \"Main\": line \\ backslash \n newline \t tab";
    std::string expected = "Error in \\\"Main\\\": line \\\\ backslash \\n newline \\t tab";
    EXPECT_EQ(EscapeJSON(input), expected);
}

TEST(CLITest, StatusLoggingJSON) {
    g_bJsonDiagnostics = true;
    
    // Capture stdout
    std::stringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());
    
    ReportStatus("test_stage", "Loading game fields...");
    
    std::cout.rdbuf(old);
    std::string output = buffer.str();
    
    EXPECT_EQ(output, "{\"type\":\"status\",\"stage\":\"test_stage\",\"message\":\"Loading game fields...\"}\n");
    g_bJsonDiagnostics = false;
}

TEST(CompilerArgumentsTest, AcceptsOneRuntimeRoot) {
    const auto result = ParseCompilerArguments({
        "DBPCompiler.exe", "--runtime-root", "D:/runtime", "Game.dbpro"});

    ASSERT_TRUE(result.has_value()) << result.error();
    ASSERT_TRUE(result.value().runtimeRoot.has_value());
    EXPECT_EQ(*result.value().runtimeRoot, std::filesystem::path("D:/runtime"));
    EXPECT_EQ(result.value().inputPath, std::filesystem::path("Game.dbpro"));
}

TEST(CompilerArgumentsTest, RejectsMissingRuntimeRootValue) {
    const auto result = ParseCompilerArguments({
        "DBPCompiler.exe", "--runtime-root"});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "--runtime-root requires a directory path.");
}

TEST(CompilerArgumentsTest, RejectsOptionAsRuntimeRootValue) {
    const auto result = ParseCompilerArguments({
        "DBPCompiler.exe", "--runtime-root", "--json", "Game.dbpro"});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "--runtime-root requires a directory path.");
}

TEST(CompilerArgumentsTest, RejectsDuplicateRuntimeRoot) {
    const auto result = ParseCompilerArguments({
        "DBPCompiler.exe", "--runtime-root", "A", "--runtime-root", "B",
        "Game.dbpro"});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "--runtime-root may only be specified once.");
}
