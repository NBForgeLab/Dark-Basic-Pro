#include <gtest/gtest.h>
#include <string>
#include <type_traits>
#include <utility>
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

class StatementLabelTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext;

    void SetUp() override {
        DBPLogger::Initialize("test_statement_label.log");

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

// Contract: the label token must be returned as an owning std::string by
// value. The legacy contract handed out a raw `new CStr` that the only
// consumer (DoLabel) wrapped in more heap allocations and manual deletes.
TEST_F(StatementLabelTest, GetLabelReturnsOwningString) {
    static_assert(std::is_same_v<decltype(std::declval<CStatement&>().GetLabel(std::declval<LPSTR*>())), std::string>,
                  "GetLabel must return std::string by value");
}

// GetLabel reads the raw label token (colon included) and advances the
// caller's file-data pointer past the consumed characters.
TEST_F(StatementLabelTest, GetLabelReadsTokenAndAdvancesPointer) {
    char prog[] = "mylabel:\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));

    CStatement statement;
    LPSTR pPointer = prog;
    EXPECT_EQ(statement.GetLabel(&pPointer), "mylabel:");
    EXPECT_EQ(pPointer, prog + 8);
}

// DoLabel registers "$label <name>" (trailing colon stripped) in the label
// table during the prescan pass; the full compile keeps it resolvable.
TEST_F(StatementLabelTest, DoLabelRegistersPrefixedLabelWithoutColon) {
    char prog[] = "start:\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));

    char registered[] = "$label start";
    EXPECT_NE(g_pLabelTable->FindLabel(registered), nullptr);

    char withColon[] = "$label start:";
    EXPECT_EQ(g_pLabelTable->FindLabel(withColon), nullptr);
}
