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

class StatementLoopTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext;

    void SetUp() override {
        DBPLogger::Initialize("test_statement_loop.log");

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

// Characterization: a valid FOR/NEXT loop compiles and DoLoop restores the
// loop-exit-label reference on the success path (no stale reference left in
// the statement list once the loop scope closes).
TEST_F(StatementLoopTest, DoLoopCompilesForNextAndRestoresExitLabelRef) {
    char prog[] = "FOR t = 1 TO 3\r\nNEXT t\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));

    EXPECT_EQ(g_pStatementList->GetLatestLoopExitLabel(), nullptr);
    EXPECT_EQ(g_pStatementList->m_iNestCount, 0);
}

// Characterization: nested loops with EXIT compile; the inner loop hands its
// exit label to the EXIT jump and the outer reference nesting unwinds cleanly.
TEST_F(StatementLoopTest, DoLoopCompilesNestedLoopsWithExit) {
    char prog[] = "WHILE 1\r\nFOR t = 1 TO 3\r\nEXIT\r\nNEXT t\r\nEXIT\r\nENDWHILE\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));

    EXPECT_EQ(g_pStatementList->GetLatestLoopExitLabel(), nullptr);
    EXPECT_EQ(g_pStatementList->m_iNestCount, 0);
}

// Contract: when loop parsing fails (numeric FOR variable is rejected), the
// loop-exit-label reference must be restored - the legacy code leaked the
// freshly allocated exit parameter AND left the statement list holding a
// stale pointer to it.
TEST_F(StatementLoopTest, DoLoopRestoresExitLabelRefWhenLoopParseFails) {
    char prog[] = "FOR 5 = 1 TO 10\r\nNEXT\r\nEND\r\n";
    EXPECT_FALSE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));

    EXPECT_EQ(g_pStatementList->GetLatestLoopExitLabel(), nullptr);
}

// Contract: the nest counter must be balanced on every DoLoop exit path -
// the legacy code only decremented it on the success and EXIT paths, leaving
// the counter inflated after any loop parse failure.
TEST_F(StatementLoopTest, DoLoopBalancesNestCountWhenLoopParseFails) {
    char prog[] = "FOR 5 = 1 TO 10\r\nNEXT\r\nEND\r\n";
    EXPECT_FALSE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));

    EXPECT_EQ(g_pStatementList->m_iNestCount, 0);
}
