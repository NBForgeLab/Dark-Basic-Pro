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

// DoExpression builds a CMathOp for a single expression string and, on
// success, hands it to the CParameter via SetMathItem (which owns it). The
// legacy body allocated the CMathOp with a raw new, guarded it with a dead
// null-check (operator new never returns null), released it with SAFE_DELETE
// on the DoValue failure path, and transferred it on success. These pins lock
// the observable behaviour before the RAII conversion.
class StatementExpressionTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext;

    void SetUp() override {
        DBPLogger::Initialize("test_statement_expression.log");

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
};

// A numeric literal parses into a math item owned by the parameter.
TEST_F(StatementExpressionTest, ParsesLiteralIntoMathItem) {
    CStatement statement;
    CParameter param;
    char buf[] = "42";
    CStr expr(buf);
    ASSERT_TRUE(statement.DoExpression(&expr, &param));
    EXPECT_NE(param.GetMathItem(), nullptr);
}

// A quoted string literal parses successfully and produces a math item.
TEST_F(StatementExpressionTest, ParsesStringLiteralIntoMathItem) {
    CStatement statement;
    CParameter param;
    char buf[] = "\"hello\"";
    CStr expr(buf);
    ASSERT_TRUE(statement.DoExpression(&expr, &param));
    EXPECT_NE(param.GetMathItem(), nullptr);
}

// A malformed expression (trailing operator, empty right operand) fails,
// walking the DoValue error path that hand-freed the CMathOp in the legacy
// body. No math item is attached to the parameter.
TEST_F(StatementExpressionTest, FailsCleanlyOnMalformedExpression) {
    CStatement statement;
    CParameter param;
    char buf[] = "1+";
    CStr expr(buf);
    EXPECT_FALSE(statement.DoExpression(&expr, &param));
    EXPECT_EQ(param.GetMathItem(), nullptr);
}
