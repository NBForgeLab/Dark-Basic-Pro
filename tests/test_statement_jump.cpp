#include <gtest/gtest.h>
#include <cstring>
#include <windows.h>
#include "DBPLogger.h"
#include "Statement.h"
#include "StatementList.h"
#include "StructTable.h"
#include "LabelTable.h"
#include "Error.h"

#include "CompilerContext.h"

// Declare compiler global pointers defined in dbp_compiler_lib
extern CStructTable*     g_pStructTable;
extern CStatementList*   g_pStatementList;
extern CLabelTable*      g_pLabelTable;
extern CError*           g_pErrorReport;

class StatementJumpTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext;

    void SetUp() override {
        DBPLogger::Initialize("test_statement_jump.log");

        m_pContext = new CompilerContext();
        m_pContext->Initialize();

        // Populate struct table with compiler defaults ("integer"='L', "float"='F', ...)
        g_pStructTable->SetStructDefaults();
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

// Characterization: GOTO to an existing label compiles and DoJump switches
// the label-as-value parsing mode off again on the success path.
TEST_F(StatementJumpTest, DoJumpCompilesGotoAndRestoresLabelAsValue) {
    char prog[] = "mylabel:\r\nGOTO mylabel\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));

    EXPECT_FALSE(g_pStatementList->GetAllowLabelAsValue());
}

// Characterization: IF/ELSE/ENDIF blocks compile through DoJump.
TEST_F(StatementJumpTest, DoJumpCompilesIfElseBlocks) {
    char prog[] = "IF 1\r\nELSE\r\nENDIF\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Characterization: SELECT/CASE/CASE DEFAULT/ENDSELECT compiles through the
// DoJump case-chain path (block chain + case labels ownership transfer).
TEST_F(StatementJumpTest, DoJumpCompilesSelectCaseBlocks) {
    char prog[] = "SELECT 1\r\nCASE 1\r\nENDCASE\r\n"
                  "CASE DEFAULT\r\nENDCASE\r\nENDSELECT\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Characterization: GOTO to a missing label fails cleanly and the
// label-as-value mode is already restored before the label lookup.
TEST_F(StatementJumpTest, DoJumpFailsCleanlyOnMissingLabel) {
    char prog[] = "GOTO nowhere\r\nEND\r\n";
    EXPECT_FALSE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));

    EXPECT_FALSE(g_pStatementList->GetAllowLabelAsValue());
}

// Contract: when the GOTO expression list itself fails to parse, the
// label-as-value parsing mode must still be switched off - the legacy code
// returned early and leaked the global mode as 'true', silently changing how
// every later statement in the compilation unit is parsed.
TEST_F(StatementJumpTest, DoJumpRestoresLabelAsValueWhenExpressionFails) {
    char prog[] = "GOTO (\r\nEND\r\n";
    EXPECT_FALSE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));

    EXPECT_FALSE(g_pStatementList->GetAllowLabelAsValue());
}

// Characterization: a one-line IF ... THEN <stmt> compiles - DoJump calls
// ReplaceTHENandELSEwithSep which rewrites the THEN keyword into a ':'
// separator (dwStage==1, the success path the multi-line IF pin never
// reaches) so the statement becomes a single-line conditional.
TEST_F(StatementJumpTest, DoJumpCompilesOneLineIfThen) {
    char prog[] = "mylabel:\r\nIF 1 THEN GOTO mylabel\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Characterization: a one-line IF ... THEN <stmt> ELSE <stmt> compiles -
// ReplaceTHENandELSEwithSep also rewrites the ELSE keyword into ':'
// separators (dwStage==2), the deepest branch of the helper.
TEST_F(StatementJumpTest, DoJumpCompilesOneLineIfThenElse) {
    char prog[] = "mylabel:\r\nIF 1 THEN GOTO mylabel ELSE GOTO mylabel\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}
