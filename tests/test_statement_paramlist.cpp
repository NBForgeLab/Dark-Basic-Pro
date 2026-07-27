#include <gtest/gtest.h>
#include <cstring>
#include <windows.h>
#include <memory>
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

// DoParameterListString splits a "a,b,c" parameter string into a chain of
// CParameter objects. The legacy body used four heap allocations released
// by manual SAFE_DELETE across every exit path: the working line string
// (reseated as items are chopped off), a per-iteration temp string, the
// replacement string, and the CParameter produced per expression (freed on
// error, otherwise linked into the returned chain). These pins lock the
// observable splitting behaviour before the RAII conversion.
class StatementParamListTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext;

    void SetUp() override {
        DBPLogger::Initialize("test_statement_paramlist.log");

        m_pContext = new CompilerContext();
        m_pContext->Initialize();

        // Populate struct table with compiler defaults ("integer"='L', ...)
        g_pStructTable->SetStructDefaults();

        // Register the internal instruction set so the expression parser can
        // resolve tokens in the isolated context.
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

    // Count the parameter chain, then release it (deleting the head chains
    // through the CParameter unique_ptr m_pNext members).
    static int CountAndFree(CParameter* pFirst) {
        int count = 0;
        for (CParameter* p = pFirst; p; p = p->GetNext())
            ++count;
        delete pFirst;
        return count;
    }
};

// A comma-separated list produces one CParameter per item and exercises the
// reseating loop (the working string is replaced after each chop).
TEST_F(StatementParamListTest, SplitsCommaSeparatedItems) {
    CStatement statement;
    char buf[] = "1,2,3";
    CStr input(buf);
    CParameter* pFirst = nullptr;
    ASSERT_TRUE(statement.DoParameterListString(&input, &pFirst));
    EXPECT_EQ(CountAndFree(pFirst), 3);
}

// A single item produces exactly one parameter and never enters the reseat
// branch.
TEST_F(StatementParamListTest, HandlesSingleItem) {
    CStatement statement;
    char buf[] = "42";
    CStr input(buf);
    CParameter* pFirst = nullptr;
    ASSERT_TRUE(statement.DoParameterListString(&input, &pFirst));
    EXPECT_EQ(CountAndFree(pFirst), 1);
}

// Speech-marked items keep separators inside the quotes intact, yielding one
// parameter per quoted literal.
TEST_F(StatementParamListTest, HandlesStringLiterals) {
    CStatement statement;
    char buf[] = "\"hello\",\"world\"";
    CStr input(buf);
    CParameter* pFirst = nullptr;
    ASSERT_TRUE(statement.DoParameterListString(&input, &pFirst));
    EXPECT_EQ(CountAndFree(pFirst), 2);
}
