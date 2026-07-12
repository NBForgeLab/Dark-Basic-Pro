#include <gtest/gtest.h>
#include "DiagnosticEngine.h"
#include <fstream>

TEST(DiagnosticEngineTest, StripAnsiColors) {
    std::string colored = "\033[1;31mError\033[0m: Syntax Error";
    std::string clean = DiagnosticEngine::StripAnsi(colored);
    EXPECT_EQ(clean, "Error: Syntax Error");
}

TEST(DiagnosticEngineTest, GetLineContextParsing) {
    std::string content = "first line\nsecond line is longer\nthird line";
    // Point to 'l' in "second line..."
    size_t charPos = 18; // Index of 'l' in "second line" is 11 + 7 = 18
    std::string line;
    size_t col = 0;
    
    DiagnosticEngine::GetLineContext(content, charPos, line, col);
    EXPECT_EQ(line, "second line is longer");
    EXPECT_EQ(col, 8); // 'l' is the 8th character (1-based)
}

TEST(DiagnosticEngineTest, GetLineContextEmptyAndBoundaries) {
    std::string content = "one\ntwo";
    std::string line;
    size_t col = 0;
    
    // First char
    DiagnosticEngine::GetLineContext(content, 0, line, col);
    EXPECT_EQ(line, "one");
    EXPECT_EQ(col, 1);
    
    // Out of bounds
    DiagnosticEngine::GetLineContext(content, 999, line, col);
    EXPECT_EQ(line, "two");
    EXPECT_EQ(col, 3);
}

TEST(DiagnosticEngineTest, FormatDiagnosticReport) {
    // Write a dummy file to read line context from
    std::string testFile = "temp_test_diag.dba";
    std::ofstream out(testFile);
    out << "myVariable = 5 + * 10\n";
    out.close();

    SourceLocation loc;
    loc.filePath = testFile;
    loc.line = 1;
    loc.column = 18; // Points to '*'
    loc.length = 1;

    std::string formatted = DiagnosticEngine::Format(loc, "Expected expression after operator", "Check for missing operands", false);
    
    // Clean up file
    std::remove(testFile.c_str());

    // Check formatting elements
    EXPECT_NE(formatted.find("Error: Expected expression after operator"), std::string::npos);
    EXPECT_NE(formatted.find("temp_test_diag.dba:1:18"), std::string::npos);
    EXPECT_NE(formatted.find("myVariable = 5 + * 10"), std::string::npos);
    EXPECT_NE(formatted.find("                 ^"), std::string::npos);
    EXPECT_NE(formatted.find("Help: Check for missing operands"), std::string::npos);
}

TEST(DiagnosticEngineTest, FormatWithTabs) {
    std::string testFile = "temp_test_tabs.dba";
    std::ofstream out(testFile);
    out << "\t\tmyVariable = 5\n";
    out.close();

    SourceLocation loc;
    loc.filePath = testFile;
    loc.line = 1;
    loc.column = 16; // Points to '5'
    loc.length = 1;

    std::string formatted = DiagnosticEngine::Format(loc, "Tab check", "", false);
    std::remove(testFile.c_str());

    // Tabs should be preserved in caret line prefix for alignment
    EXPECT_NE(formatted.find("\t\t             ^"), std::string::npos);
}
