#include <gtest/gtest.h>
#include <optional>
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
#include "ParseJump.h"
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

// Contract: AddInternalLabel must return the generated label name as an
// owning std::optional<std::string>. The legacy contract published a raw
// `new CStr` through an out-param before the fallible table registration,
// leaving the caller's pointer dangling on failure (double delete) and
// forcing manual SAFE_DELETE lines on every exit path.
TEST_F(StatementLabelTest, AddInternalLabelReturnsOwningOptionalString) {
    static_assert(std::is_same_v<decltype(std::declval<CStatement&>().AddInternalLabel()), std::optional<std::string>>,
                  "AddInternalLabel must return std::optional<std::string> by value");
}

// AddInternalLabel composes "$label<index>[<line>]", registers it in the
// label table and advances the label index counter.
TEST_F(StatementLabelTest, AddInternalLabelRegistersNameInLabelTable) {
    char prog[] = "END\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));

    CStatement statement;
    statement.SetLineNumber(7);
    const DWORD indexBefore = g_pStatementList->GetLabelIndexCounter();

    std::optional<std::string> labelName = statement.AddInternalLabel();
    ASSERT_TRUE(labelName.has_value());
    EXPECT_EQ(*labelName, "$label" + std::to_string(indexBefore) + "[7]");
    EXPECT_NE(g_pLabelTable->FindLabel(labelName->data()), nullptr);
    EXPECT_EQ(g_pStatementList->GetLabelIndexCounter(), indexBefore + 1);
}

// SetParamAsLabel takes the label name by value and copies it into the
// parameter's math result; no caller-side heap CStr or SAFE_DELETE needed.
TEST_F(StatementLabelTest, SetParamAsLabelCopiesOwningString) {
    CParameter parameter;
    ASSERT_TRUE(parameter.SetParamAsLabel(std::string("$label0[1]")));
    ASSERT_NE(parameter.GetMathItem(), nullptr);
    ASSERT_NE(parameter.GetMathItem()->GetResultStringToken(), nullptr);
    EXPECT_EQ(parameter.GetMathItem()->GetResultStringToken()->View(), "$label0[1]");
}

// CParseJump owns its block labels by value; an empty string means "no
// label" (the legacy NULL CStr*), removing the raw-pointer handoff.
TEST_F(StatementLabelTest, ParseJumpOwnsBlockLabelsByValue) {
    static_assert(std::is_same_v<decltype(std::declval<CParseJump&>().GetBlockLabelA()), const std::string&>,
                  "GetBlockLabelA must return const std::string&");
    static_assert(std::is_same_v<decltype(std::declval<CParseJump&>().GetBlockLabelB()), const std::string&>,
                  "GetBlockLabelB must return const std::string&");

    CParseJump jump;
    EXPECT_TRUE(jump.GetBlockLabelA().empty());
    EXPECT_TRUE(jump.GetBlockLabelB().empty());

    jump.SetBlockALabel("$label3[9]");
    EXPECT_EQ(jump.GetBlockLabelA(), "$label3[9]");
}
