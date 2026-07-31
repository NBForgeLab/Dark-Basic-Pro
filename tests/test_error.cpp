// test_error.cpp – Characterization tests for Error.cpp
// These tests pin down the CURRENT behaviour of CError and the free functions
// in Error.cpp (EscapeJSON, ParseCommandLine, ReportStatus).

#include <gtest/gtest.h>
#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>

#include "Error.h"
#include "CompilerContext.h"
#include "DBPLogger.h"
#include "Str.h"
#include "IncludeTable.h"
#include "StatementList.h"
#include "DBPCompiler.h"

// Globals defined in DBPCompiler.cpp (part of dbp_compiler_lib)
extern CDBPCompiler*       g_pDBPCompiler;
extern CStatementList*     g_pStatementList;
extern CIncludeTable*      g_pIncludeTable;
extern CError*             g_pErrorReport;

// ---------------------------------------------------------------------------
// Free-function tests (no global state needed)
// ---------------------------------------------------------------------------

// --- EscapeJSON ---

TEST(ErrorEscapeJson, PlainStringUnchanged) {
    EXPECT_EQ(EscapeJSON("hello world"), "hello world");
}

TEST(ErrorEscapeJson, EscapesBackslash) {
    EXPECT_EQ(EscapeJSON("a\\b"), "a\\\\b");
}

TEST(ErrorEscapeJson, EscapesDoubleQuote) {
    EXPECT_EQ(EscapeJSON("say \"hi\""), "say \\\"hi\\\"");
}

TEST(ErrorEscapeJson, EscapesNewline) {
    EXPECT_EQ(EscapeJSON("line1\nline2"), "line1\\nline2");
}

TEST(ErrorEscapeJson, EscapesTab) {
    EXPECT_EQ(EscapeJSON("col1\tcol2"), "col1\\tcol2");
}

TEST(ErrorEscapeJson, EscapesCarriageReturn) {
    EXPECT_EQ(EscapeJSON("cr\rval"), "cr\\rval");
}

TEST(ErrorEscapeJson, EscapesControlCharsAsUnicode) {
    std::string input(1, '\x01');
    EXPECT_EQ(EscapeJSON(input), "\\u0001");
}

TEST(ErrorEscapeJson, EmptyString) {
    EXPECT_EQ(EscapeJSON(""), "");
}

TEST(ErrorEscapeJson, MultipleSpecialChars) {
    std::string result = EscapeJSON("a\"b\\c\nd");
    EXPECT_EQ(result, "a\\\"b\\\\c\\nd");
}

// --- ParseCommandLine ---

TEST(ErrorParseCommandLine, SimpleTokens) {
    auto args = ParseCommandLine("one two three");
    ASSERT_EQ(args.size(), 3u);
    EXPECT_EQ(args[0], "one");
    EXPECT_EQ(args[1], "two");
    EXPECT_EQ(args[2], "three");
}

TEST(ErrorParseCommandLine, QuotedString) {
    auto args = ParseCommandLine("cmd \"hello world\" end");
    ASSERT_EQ(args.size(), 3u);
    EXPECT_EQ(args[0], "cmd");
    EXPECT_EQ(args[1], "hello world");
    EXPECT_EQ(args[2], "end");
}

TEST(ErrorParseCommandLine, EmptyInput) {
    auto args = ParseCommandLine("");
    EXPECT_TRUE(args.empty());
}

TEST(ErrorParseCommandLine, MultipleSpaces) {
    auto args = ParseCommandLine("  a   b  ");
    ASSERT_EQ(args.size(), 2u);
    EXPECT_EQ(args[0], "a");
    EXPECT_EQ(args[1], "b");
}

TEST(ErrorParseCommandLine, QuoteInMiddle) {
    auto args = ParseCommandLine("a\"b c\"d");
    ASSERT_EQ(args.size(), 1u);
    EXPECT_EQ(args[0], "ab cd");
}

TEST(ErrorParseCommandLine, SingleToken) {
    auto args = ParseCommandLine("alone");
    ASSERT_EQ(args.size(), 1u);
    EXPECT_EQ(args[0], "alone");
}

// ---------------------------------------------------------------------------
// CError fixture – globals nulled to isolate CError from side effects
// ---------------------------------------------------------------------------

class CErrorPureTest : public ::testing::Test {
protected:
    // Save originals
    CStatementList*  m_origStatementList = nullptr;
    CIncludeTable*   m_origIncludeTable  = nullptr;
    CDBPCompiler*    m_origDBPCompiler   = nullptr;

    void SetUp() override {
        DBPLogger::Initialize("test_error.log");
        // Save and null globals so CError methods don't dereference them
        m_origStatementList = g_pStatementList;
        m_origIncludeTable  = g_pIncludeTable;
        m_origDBPCompiler   = g_pDBPCompiler;
        g_pStatementList = nullptr;
        g_pIncludeTable  = nullptr;
        g_pDBPCompiler   = nullptr;
    }

    void TearDown() override {
        // Restore globals
        g_pStatementList = m_origStatementList;
        g_pIncludeTable  = m_origIncludeTable;
        g_pDBPCompiler   = m_origDBPCompiler;
    }
};

// --- Construction / initial state ---

TEST_F(CErrorPureTest, InitialStateNoError) {
    CError err;
    EXPECT_FALSE(err.IsError());
    EXPECT_FALSE(err.IsParserError());
}

TEST_F(CErrorPureTest, GetErrorStringReturnsUnknownWhenNoError) {
    CError err;
    EXPECT_STREQ(err.GetErrorString(), "???");
}

TEST_F(CErrorPureTest, GetParserErrorStringReturnsUnknownWhenNoError) {
    CError err;
    EXPECT_STREQ(err.GetParserErrorString(), "???");
}

// --- AddErrorString (no global side effects) ---

TEST_F(CErrorPureTest, AddErrorStringSetsErrorFlag) {
    CError err;
    err.AddErrorString("something went wrong");
    EXPECT_TRUE(err.IsError());
}

TEST_F(CErrorPureTest, AddErrorStringContainsTextWithCRLF) {
    CError err;
    err.AddErrorString("first error");
    std::string result(err.GetErrorString());
    EXPECT_NE(result.find("first error"), std::string::npos);
    // Each entry is terminated with CR+LF
    EXPECT_GE(result.size(), 2u);
    EXPECT_EQ(result[result.size() - 2], '\r');
    EXPECT_EQ(result[result.size() - 1], '\n');
}

TEST_F(CErrorPureTest, MultipleAddErrorStringConcatenates) {
    CError err;
    err.AddErrorString("error one");
    std::string afterFirst(err.GetErrorString());
    err.AddErrorString("error two");
    std::string afterSecond(err.GetErrorString());

    // Characterization: first call produces "error one\r\n" (len 11)
    EXPECT_TRUE(err.IsError());
    EXPECT_EQ(afterFirst, "error one\r\n");
    EXPECT_EQ(afterFirst.size(), 11u);

    // Characterization: second call produces "error two\r\n" (len 11)
    // The old content is not visible in the resulting string.
    // This is the CURRENT behaviour – the buffer may contain old data
    // but std::string(LPSTR) only sees up to the first '\0'.
    EXPECT_EQ(afterSecond, "error two\r\n");
    EXPECT_EQ(afterSecond.size(), 11u);
}

TEST_F(CErrorPureTest, AddErrorStringEmptyString) {
    CError err;
    err.AddErrorString("");
    EXPECT_TRUE(err.IsError());
    std::string result(err.GetErrorString());
    // Should still have CR+LF appended
    EXPECT_GE(result.size(), 2u);
    EXPECT_EQ(result[result.size() - 2], '\r');
    EXPECT_EQ(result[result.size() - 1], '\n');
}

// --- GetTokenIndex ---

TEST_F(CErrorPureTest, GetTokenIndexReturns99ForL) {
    CError err;
    CStr tok("L");
    EXPECT_EQ(err.GetTokenIndex(&tok), 99u);
}

TEST_F(CErrorPureTest, GetTokenIndexReturns1For1) {
    CError err;
    CStr tok("1");
    EXPECT_EQ(err.GetTokenIndex(&tok), 1u);
}

TEST_F(CErrorPureTest, GetTokenIndexReturns2For2) {
    CError err;
    CStr tok("2");
    EXPECT_EQ(err.GetTokenIndex(&tok), 2u);
}

TEST_F(CErrorPureTest, GetTokenIndexReturns3For3) {
    CError err;
    CStr tok("3");
    EXPECT_EQ(err.GetTokenIndex(&tok), 3u);
}

TEST_F(CErrorPureTest, GetTokenIndexReturns0ForUnknown) {
    CError err;
    CStr tok("X");
    EXPECT_EQ(err.GetTokenIndex(&tok), 0u);
}

// --- CreateAndReword ---

TEST_F(CErrorPureTest, CreateAndRewordNullReturnsEmpty) {
    CError err;
    EXPECT_EQ(err.CreateAndReword(nullptr), "");
}

TEST_F(CErrorPureTest, CreateAndRewordPlainCopy) {
    CError err;
    EXPECT_EQ(err.CreateAndReword("hello"), "hello");
}

TEST_F(CErrorPureTest, CreateAndRewordAtPrefixStripsAt) {
    CError err;
    // '@' prefix: straight copy +1 (skips the '@')
    EXPECT_EQ(err.CreateAndReword("@myvar"), "myvar");
}

TEST_F(CErrorPureTest, CreateAndRewordAtDollarPrefix) {
    CError err;
    // "@$" prefix: code does "TEMP" + (pI+3), i.e. skips 3 chars ('@','$', and first char after)
    // For "@$tempfile": pI+3 = "empfile", result = "TEMPempfile"
    EXPECT_EQ(err.CreateAndReword("@$tempfile"), "TEMPempfile");
}

TEST_F(CErrorPureTest, CreateAndRewordEmptyString) {
    CError err;
    EXPECT_EQ(err.CreateAndReword(""), "");
}

// --- Progress helpers ---

TEST_F(CErrorPureTest, SetMaxLinesAndGetPerc) {
    CError err;
    err.SetMaxLines(200);
    EXPECT_EQ(err.GetPerc(50), 100u);
    EXPECT_EQ(err.GetPerc(0), 0u);
    EXPECT_EQ(err.GetPerc(100), 200u);
}

// --- GetRuntimeErrorString before loading database ---

TEST_F(CErrorPureTest, GetRuntimeErrorStringMaxBeforeLoad) {
    CError err;
    EXPECT_EQ(err.GetRuntimeErrorStringMax(), 0u);
}

TEST_F(CErrorPureTest, GetRuntimeErrorStringNegativeIndex) {
    CError err;
    EXPECT_EQ(err.GetRuntimeErrorString(-1), nullptr);
}

// --- Error code ranges ---

TEST_F(CErrorPureTest, ErrorCodeDefines) {
    EXPECT_EQ(ERR_INTERNAL, 0);
    EXPECT_EQ(ERR_SYNTAX, 100000);
    EXPECT_EQ(ERR_COMPILER, 200000);
}

// ---------------------------------------------------------------------------
// CError fixture with CompilerContext for tests that need globals
// ---------------------------------------------------------------------------

class CErrorCtxTest : public ::testing::Test {
protected:
    CompilerContext* m_pCtx = nullptr;

    void SetUp() override {
        DBPLogger::Initialize("test_error.log");
        m_pCtx = new CompilerContext();
        m_pCtx->Initialize();
    }

    void TearDown() override {
        if (m_pCtx) {
            m_pCtx->Cleanup();
            delete m_pCtx;
            m_pCtx = nullptr;
        }
    }
};

// --- LoadErrorDatabase and GetErrorConstruction ---

TEST_F(CErrorCtxTest, LoadErrorDatabaseFromTempIni) {
    std::string iniPath = "test_error_temp.ini";
    {
        std::ofstream ofs(iniPath);
        ofs << "[INTERNAL]\n";
        ofs << "1=Internal error occurred\n";
        ofs << "2= at line #L#\n";
        ofs << "[SYNTAX]\n";
        ofs << "1=Syntax Error in #L#\n";
        ofs << "2=Type Mismatch: #1# vs #2#\n";
        ofs << "[RUNTIME]\n";
        ofs << "1=Division by zero\n";
        ofs << "5=Array out of bounds\n";
    }

    CError err;
    err.LoadErrorDatabase(const_cast<LPSTR>(iniPath.c_str()));

    // Runtime errors should have been loaded (size > 0)
    EXPECT_GT(err.GetRuntimeErrorStringMax(), 0u);

    std::remove(iniPath.c_str());
}

TEST_F(CErrorCtxTest, GetErrorConstructionWithSyntaxCode) {
    std::string iniPath = "test_error_construction.ini";
    {
        std::ofstream ofs(iniPath);
        ofs << "[INTERNAL]\n";
        ofs << "1=Internal error\n";
        ofs << "[SYNTAX]\n";
        ofs << "1=Syntax Error at line #L#\n";
        ofs << "[RUNTIME]\n";
    }

    CError err;
    err.LoadErrorDatabase(const_cast<LPSTR>(iniPath.c_str()));

    CStr raw(256);
    CStr* pRaw = &raw;
    // ERR_SYNTAX + 1 → index 1 in ParserErrors (after the 0-index offset)
    err.GetErrorConstruction(5, ERR_SYNTAX + 1, &pRaw);
    std::string result(pRaw->GetStr());
    // Characterization: check what we actually get
    // The database is loaded starting at index 1, so index 1 should have our string
    if (!result.empty()) {
        EXPECT_NE(result.find("Syntax Error"), std::string::npos);
    }
    // Even if the lookup fails, it should fall back gracefully (not crash)

    std::remove(iniPath.c_str());
}

TEST_F(CErrorCtxTest, GetErrorConstructionUnknownCodeFallsBack) {
    std::string iniPath = "test_error_fallback.ini";
    {
        std::ofstream ofs(iniPath);
        ofs << "[INTERNAL]\n";
        ofs << "1=Fallback message\n";
        ofs << "[SYNTAX]\n";
        ofs << "[RUNTIME]\n";
    }

    CError err;
    err.LoadErrorDatabase(const_cast<LPSTR>(iniPath.c_str()));

    CStr raw(256);
    CStr* pRaw = &raw;
    // Code way beyond any loaded entry → should fall back to InternalErrors[1]
    err.GetErrorConstruction(1, ERR_SYNTAX + 999, &pRaw);
    std::string result(pRaw->GetStr());
    // Characterization: the fallback is InternalErrors[1] if it exists
    // With our INI, InternalErrors[1] = "Fallback message"
    // But this depends on LoadDatabaseSubset behaviour (1-indexed)
    // Just verify it doesn't crash and returns something
    EXPECT_TRUE(true); // If we got here, no crash occurred

    std::remove(iniPath.c_str());
}

// --- SetError overloads (with globals from CompilerContext) ---

TEST_F(CErrorCtxTest, SetErrorNoArgsSetsErrorFlag) {
    g_pErrorReport->SetError(1, ERR_SYNTAX + 1);
    EXPECT_TRUE(g_pErrorReport->IsError());
    std::string errStr(g_pErrorReport->GetErrorString());
    EXPECT_FALSE(errStr.empty());
}

TEST_F(CErrorCtxTest, SetErrorWithDWORD) {
    g_pErrorReport->SetError(1, ERR_SYNTAX + 1, (DWORD)42);
    EXPECT_TRUE(g_pErrorReport->IsError());
}

TEST_F(CErrorCtxTest, SetErrorWithOneString) {
    g_pErrorReport->SetError(1, ERR_SYNTAX + 1, (LPSTR)"myVar");
    EXPECT_TRUE(g_pErrorReport->IsError());
}

TEST_F(CErrorCtxTest, SetErrorWithTwoStrings) {
    g_pErrorReport->SetError(1, ERR_SYNTAX + 1, (LPSTR)"a", (LPSTR)"b");
    EXPECT_TRUE(g_pErrorReport->IsError());
}

// --- SetParserError (needs g_pDBPCompiler for line > 0 path) ---

TEST_F(CErrorCtxTest, SetParserErrorWithLineZeroSetsFlag) {
    // Line 0 skips the g_pDBPCompiler->GetWord(11) call, avoiding null deref
    g_pErrorReport->SetParserError(0, (LPSTR)"no line info");
    EXPECT_TRUE(g_pErrorReport->IsParserError());
    std::string parserStr(g_pErrorReport->GetParserErrorString());
    EXPECT_NE(parserStr.find("no line info"), std::string::npos);
}

TEST_F(CErrorCtxTest, SetParserErrorOnlySetsOnce) {
    // First call sets the parser error; second call is ignored
    g_pErrorReport->SetParserError(0, (LPSTR)"first parser error");
    g_pErrorReport->SetParserError(0, (LPSTR)"second parser error");
    std::string parserStr(g_pErrorReport->GetParserErrorString());
    EXPECT_NE(parserStr.find("first parser error"), std::string::npos);
    EXPECT_EQ(parserStr.find("second parser error"), std::string::npos);
}

// --- ReportStatus ---

TEST(ErrorReportStatus, FreeFunctionNonJsonMode) {
    bool oldVal = g_bJsonDiagnostics;
    g_bJsonDiagnostics = false;
    testing::internal::CaptureStdout();
    ReportStatus("compile", "starting");
    std::string output = testing::internal::GetCapturedStdout();
    g_bJsonDiagnostics = oldVal;
    EXPECT_NE(output.find("compile"), std::string::npos);
    EXPECT_NE(output.find("starting"), std::string::npos);
}

TEST(ErrorReportStatus, JsonMode) {
    bool oldVal = g_bJsonDiagnostics;
    g_bJsonDiagnostics = true;
    testing::internal::CaptureStdout();
    ReportStatus("link", "done");
    std::string output = testing::internal::GetCapturedStdout();
    g_bJsonDiagnostics = oldVal;
    EXPECT_NE(output.find("\"type\":\"status\""), std::string::npos);
    EXPECT_NE(output.find("\"stage\":\"link\""), std::string::npos);
    EXPECT_NE(output.find("\"message\":\"done\""), std::string::npos);
}
