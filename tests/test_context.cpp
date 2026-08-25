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

TEST(CompilerContextTest, MoveSemanticsTransferOwnership) {
    CompilerContext context1;
    context1.Initialize();

    void* originalExe = context1.pEXE;
    ASSERT_NE(originalExe, nullptr);
    ASSERT_NE(context1.pStructTable, nullptr);

    // Test move construction
    CompilerContext context2(std::move(context1));
    EXPECT_EQ(context1.pEXE, nullptr);
    EXPECT_EQ(context1.pStructTable, nullptr);
    EXPECT_EQ(context2.pEXE, originalExe);
    EXPECT_NE(context2.pStructTable, nullptr);

    // Test move assignment
    CompilerContext context3;
    context3 = std::move(context2);
    EXPECT_EQ(context2.pEXE, nullptr);
    EXPECT_EQ(context3.pEXE, originalExe);

    context3.Cleanup();
    EXPECT_EQ(context3.pEXE, nullptr);
}

TEST(CompilerContextTest, IdempotentCleanupIsSafe) {
    CompilerContext context;
    context.Initialize();
    ASSERT_NE(context.pEXE, nullptr);

    // Calling cleanup multiple times must be safely handled without double-free
    context.Cleanup();
    EXPECT_EQ(context.pEXE, nullptr);
    context.Cleanup();
    EXPECT_EQ(context.pEXE, nullptr);
}
