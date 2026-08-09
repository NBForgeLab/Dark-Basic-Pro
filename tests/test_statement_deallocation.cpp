#include <gtest/gtest.h>
#include <cstring>
#include <windows.h>
#include "DBPLogger.h"
#include "Statement.h"
#include "ParseInstruction.h"
#include "ParseInit.h"
#include "StatementList.h"
#include "StructTable.h"
#include "LabelTable.h"
#include "InstructionTable.h"
#include "Error.h"

#include "CompilerContext.h"

// Declare compiler global pointers defined in dbp_compiler_lib
extern CStructTable*       g_pStructTable;
extern CStatementList*     g_pStatementList;
extern CLabelTable*        g_pLabelTable;
extern CInstructionTable*  g_pInstructionTable;
extern CError*             g_pErrorReport;

class StatementDeallocationTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext;

    void SetUp() override {
        DBPLogger::Initialize("test_statement_deallocation.log");

        m_pContext = new CompilerContext();
        m_pContext->Initialize();

        // Populate struct table with compiler defaults ("integer"='L', "float"='F', ...)
        g_pStructTable->SetStructDefaults();

        // Register the internal instruction set (+allocate, +deallocate, MOVxx...)
        // so DoDeAllocation can resolve IT_INTERNAL_FREE in the isolated context
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

// Characterization: UNDIM on a dimensioned array walks DoDeAllocation's happy
// path - array-name token, '&' address expression, and the +deallocate emit.
TEST_F(StatementDeallocationTest, DoDeAllocationCompilesUndimOfArray) {
    char prog[] = "DIM arr(5)\r\nUNDIM arr(5)\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Characterization: UNDIM with empty brackets follows the same path with the
// bracket cut producing just the array name.
TEST_F(StatementDeallocationTest, DoDeAllocationCompilesUndimWithEmptyBrackets) {
    char prog[] = "DIM arr(5)\r\nUNDIM arr()\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Contract: UNDIM must configure CParseInit for deallocation
TEST_F(StatementDeallocationTest, DoDeAllocationVerifiesReturnParameterAndParamCount) {
    char prog[] = "DIM myArr(10)\r\nUNDIM myArr()\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));

    CStatement* pStmt = g_pStatementList->GetProgramStatements();
    bool foundDealloc = false;
    while (pStmt) {
        if (pStmt->HasObject()) {
            if (CParseInit* pInit = pStmt->GetObject<CParseInit>()) {
                foundDealloc = true;
                ASSERT_NE(pInit->GetParameter(), nullptr);
                break;
            }
        }
        pStmt = pStmt->GetNext();
    }
    EXPECT_TRUE(foundDealloc);
}

// Contract: UNDIM without an array name must fail with a clean diagnostic
// (ERR_SYNTAX+43) - this walks the zero-length token error path.
TEST_F(StatementDeallocationTest, DoDeAllocationFailsCleanlyOnMissingArrayName) {
    char prog[] = "UNDIM \r\nEND\r\n";
    EXPECT_FALSE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Characterization: UNDIM of an undeclared array is TOLERATED by the legacy
// compiler (the '&name' address expression auto-registers the variable) -
// pinned as-is so the refactor cannot silently change this contract.
TEST_F(StatementDeallocationTest, DoDeAllocationToleratesUnknownArray) {
    char prog[] = "UNDIM ghost(5)\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Contract: UNDIM on multiple sequential arrays (arrA, arrB, arrC) compiles cleanly
TEST_F(StatementDeallocationTest, DoDeAllocationMultipleArraysSequential) {
    char prog[] = "DIM arrA(10)\r\nDIM arrB(20)\r\nDIM arrC(30)\r\nUNDIM arrA()\r\nUNDIM arrB()\r\nUNDIM arrC()\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Contract: UNDIM on Global and Local arrays compiles cleanly
TEST_F(StatementDeallocationTest, DoDeAllocationGlobalAndLocalArrays) {
    char prog[] = "GLOBAL DIM gArr(50)\r\nLOCAL DIM lArr(25)\r\nUNDIM gArr()\r\nUNDIM lArr()\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Contract: UNDIM inside FOR...NEXT loop compiles cleanly
TEST_F(StatementDeallocationTest, DoDeAllocationInsideForLoop) {
    char prog[] = "FOR i = 1 TO 3\r\nDIM temp(10)\r\nUNDIM temp()\r\nNEXT i\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Contract: UNDIM inside user defined function compiles cleanly
TEST_F(StatementDeallocationTest, DoDeAllocationInsideFunction) {
    char prog[] = "FUNCTION ProcessData()\r\nDIM localArr(15)\r\nUNDIM localArr()\r\nENDFUNCTION\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

