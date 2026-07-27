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

class StatementTypeTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext;

    void SetUp() override {
        DBPLogger::Initialize("test_statement_type.log");

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

// Characterization: a TYPE with fields walks DoType's happy path - the
// declaration chain and type block are handed over to the struct table.
TEST_F(StatementTypeTest, DoTypeCompilesTypeWithFields) {
    char prog[] = "TYPE vec\r\nx AS FLOAT\r\ny AS FLOAT\r\nENDTYPE\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Characterization: nested user types exercise the AddStructUserType
// known-type validation loop over the declaration chain.
TEST_F(StatementTypeTest, DoTypeCompilesNestedUserTypes) {
    char prog[] = "TYPE inner\r\na AS INTEGER\r\nENDTYPE\r\n"
                  "TYPE outer\r\nfield AS inner\r\nENDTYPE\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Contract: a TYPE without any field declaration must fail with a clean
// diagnostic (ERR_SYNTAX+54) - this walks the pDecChain==NULL error path.
TEST_F(StatementTypeTest, DoTypeFailsCleanlyOnEmptyType) {
    char prog[] = "TYPE empty\r\nENDTYPE\r\nEND\r\n";
    EXPECT_FALSE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Characterization: redeclaring an existing TYPE name makes AddStructUserType
// reject the duplicate, yet DoType still reports success (legacy contract) -
// this walks the hand-free path where the rejected owners are discarded.
TEST_F(StatementTypeTest, DoTypeToleratesDuplicateTypeName) {
    char prog[] = "TYPE twin\r\na AS INTEGER\r\nENDTYPE\r\n"
                  "TYPE twin\r\nb AS INTEGER\r\nENDTYPE\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}
