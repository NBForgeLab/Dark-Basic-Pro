#include <gtest/gtest.h>
#include "CompilerContext.h"
#include "VarTable.h"

// Declare the legacy global pointer we will check
extern CVarTable* g_pVarTable;

TEST(CompilerContextTest, LifecycleAndGlobalBinding) {
    // 1. Initially, g_pVarTable should be null (or we set it to null)
    g_pVarTable = nullptr;

    // 2. Instantiate the context
    CompilerContext context;

    // 3. Call Initialize
    context.Initialize();

    // 4. Verify that the global pointer g_pVarTable is no longer null
    ASSERT_NE(g_pVarTable, nullptr);

    // 5. Call Cleanup (or let destructor handle it)
    context.Cleanup();

    // 6. Verify that g_pVarTable is reset to nullptr
    EXPECT_EQ(g_pVarTable, nullptr);
}
