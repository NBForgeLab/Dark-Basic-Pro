#include <gtest/gtest.h>
#include "CompilerContext.h"
#include "InstructionTable.h"
#include "Error.h"
#include "EXEBlock.h"

extern CInstructionTable* g_pInstructionTable;
extern CError*            g_pErrorReport;
extern CEXEBlock*         g_pEXE;

TEST(CompilerContextTest, AdoptionAndCleanLifecycles) {
    // 1. Setup pre-existing globals
    CInstructionTable* pMockInst = new CInstructionTable();
    CError* pMockErr = new CError();
    g_pInstructionTable = pMockInst;
    g_pErrorReport = pMockErr;
    g_pEXE = nullptr;

    // 2. Instantiate and Initialize
    {
        CompilerContext context;
        context.Initialize();

        // Verify globals adopted
        EXPECT_EQ(g_pInstructionTable, pMockInst);
        EXPECT_EQ(g_pErrorReport, pMockErr);
        EXPECT_NE(g_pEXE, nullptr); // Owned by context

        context.Cleanup();
    }

    // 3. Post-cleanup check: pre-existing globals should NOT be deleted or nullified
    EXPECT_EQ(g_pInstructionTable, pMockInst);
    EXPECT_EQ(g_pErrorReport, pMockErr);
    EXPECT_EQ(g_pEXE, nullptr); // Context deleted it

    delete pMockInst;
    delete pMockErr;
    g_pInstructionTable = nullptr;
    g_pErrorReport = nullptr;
}
