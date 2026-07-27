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

class StatementAllocationTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext;

    void SetUp() override {
        DBPLogger::Initialize("test_statement_allocation.log");

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

// Characterization: a multi-dimension DIM walks DoAllocation's size-entry
// scan (per-dimension pNum/pNumStr buffers and the pSizeParameter chain).
TEST_F(StatementAllocationTest, DoAllocationCompilesMultiDimensionArray) {
    char prog[] = "DIM arr(2,3,4)\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Characterization: DIM with a user-defined type exercises the pVarType
// buffer from FindTypeOfVariable and the DoesTypeEvenExist success path.
TEST_F(StatementAllocationTest, DoAllocationCompilesArrayWithUserType) {
    char prog[] = "TYPE vec\r\nx AS FLOAT\r\ny AS FLOAT\r\nENDTYPE\r\nDIM arr(5) AS vec\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Characterization: DIM arr() means an EMPTY array (-1 sentinel) and still
// flows through DoAllocation's default zero-fill of remaining dimensions.
TEST_F(StatementAllocationTest, DoAllocationCompilesEmptyArray) {
    char prog[] = "DIM arr()\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Contract: more than nine dimensions must fail with a clean diagnostic -
// this walks the ERR_SYNTAX+40 error path that hand-frees every owner.
TEST_F(StatementAllocationTest, DoAllocationFailsCleanlyOnTooManyDimensions) {
    char prog[] = "DIM arr(1,2,3,4,5,6,7,8,9,10)\r\nEND\r\n";
    EXPECT_FALSE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}
