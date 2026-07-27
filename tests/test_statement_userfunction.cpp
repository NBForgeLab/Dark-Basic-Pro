#include <gtest/gtest.h>
#include <cstring>
#include <windows.h>
#include "DBPLogger.h"
#include "Statement.h"
#include "StatementList.h"
#include "StructTable.h"
#include "InstructionTable.h"
#include "LabelTable.h"
#include "Error.h"

#include "CompilerContext.h"

// Declare compiler global pointers defined in dbp_compiler_lib
extern CStructTable*      g_pStructTable;
extern CStatementList*    g_pStatementList;
extern CInstructionTable* g_pInstructionTable;
extern CLabelTable*       g_pLabelTable;
extern CError*            g_pErrorReport;

// Characterization pins for CStatement::DoUserFunctionExit. The legacy body
// allocates a CParseInstruction up front and only transfers its ownership to
// the emitted CStatement on the success path; every error return leaks it.
// These pins lock the observable behaviour a RAII refactor must preserve:
//   * a well-formed FUNCTION with an EXITFUNCTION whose return type matches
//     the ENDFUNCTION return type compiles cleanly (ownership transferred);
//   * an EXITFUNCTION with more than one return value fails with a clean
//     diagnostic (this is one of the historic leak paths).
// EXITFUNCTION / ENDFUNCTION are reserved-word tokens handled by the parser
// (not plugin commands), so the isolated compiler context can drive them.
class StatementUserFunctionTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext;

    void SetUp() override {
        DBPLogger::Initialize("test_statement_userfunction.log");

        m_pContext = new CompilerContext();
        m_pContext->Initialize();

        g_pStructTable->SetStructDefaults();
        g_pInstructionTable->SetInternalInstructionDatabase();
    }

    void TearDown() override {
        if (m_pContext) {
            m_pContext->Cleanup();
            delete m_pContext;
            m_pContext = nullptr;
        }
        spdlog::shutdown();
    }
};

// Contract: a function whose EXITFUNCTION return type matches the declared
// return type (both integer) compiles - the success path that transfers the
// CParseInstruction to the emitted statement.
TEST_F(StatementUserFunctionTest, ExitFunctionMatchingReturnTypeCompiles) {
    char prog[] =
        "myfunc()\r\n"
        "function myfunc()\r\n"
        "exitfunction 5\r\n"
        "endfunction 7\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Contract: an EXITFUNCTION with more than one return value must fail with a
// clean diagnostic (ERR_SYNTAX+31) - a historic leak path.
TEST_F(StatementUserFunctionTest, ExitFunctionMultipleReturnValuesFailsCleanly) {
    char prog[] =
        "myfunc()\r\n"
        "function myfunc()\r\n"
        "exitfunction 5,6\r\n"
        "endfunction 7\r\n";
    EXPECT_FALSE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Contract: an EXITFUNCTION whose return value type does not match the declared
// integer return type must fail with a clean diagnostic (ERR_SYNTAX+57) - the
// second historic pParameter free path.
TEST_F(StatementUserFunctionTest, ExitFunctionMismatchedReturnTypeFailsCleanly) {
    char prog[] =
        "myfunc()\r\n"
        "function myfunc()\r\n"
        "exitfunction \"hello\"\r\n"
        "endfunction 7\r\n";
    EXPECT_FALSE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Contract: a function defined and called WITH a bracketed argument compiles -
// this drives RemoveEdgeBracketFromSegment through the branch that actually
// erases a non-empty '(' .. ')' pair (both the definition and call sites),
// unlike the empty-bracket pins above.
TEST_F(StatementUserFunctionTest, UserFunctionWithArgumentCompiles) {
    char prog[] =
        "myfunc(5)\r\n"
        "function myfunc(n)\r\n"
        "endfunction n\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}
