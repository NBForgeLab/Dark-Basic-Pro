#include <gtest/gtest.h>
#include <cstring>
#include <windows.h>
#include "DBPLogger.h"
#include "Str.h"
#include "Statement.h"
#include "StatementList.h"
#include "StructTable.h"
#include "VarTable.h"
#include "InstructionTable.h"
#include "Error.h"
#include "ASMWriter.h"

#include "CompilerContext.h"

// Declare compiler global pointers defined in dbp_compiler_lib
extern CStructTable*      g_pStructTable;
extern CStatementList*    g_pStatementList;
extern CVarTable*         g_pVarTable;
extern CInstructionTable* g_pInstructionTable;
extern ICodeGenerator*    g_pASMWriter;
extern CError*            g_pErrorReport;

// Characterization suite for CMathOp expression resolution. Each test drives a
// value-resolution path (literal, label, single variable, complex/array
// variable, struct value, temp-token production, function detection) that owns
// heap CStr / new[] temporaries, pinning the observable "resolves to this token
// and type" contract before the RAII conversion of MathOp.cpp.
class MathOpExpressionTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext;

    void SetUp() override {
        DBPLogger::Initialize("test_mathop_expression.log");

        m_pContext = new CompilerContext();
        m_pContext->Initialize();
        g_pStructTable->SetStructDefaults();

        // Instruction database is required for function/command resolution and
        // for type-name lookups used while resolving variables.
        g_pInstructionTable->SetInternalInstructionDatabase();

        // Same backend bootstrap as ASTCodeGenTest / ASMWriterEmissionTest.
        char dummyProg[] = "";
        g_pStatementList->MakeStatements(dummyProg, 0);
        ASSERT_TRUE(g_pASMWriter->CreateASMHeader());

        // Variable resolution paths add symbols to the variable table.
        g_pStatementList->SetVariableAddParse(true);
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

// DoValueLiteral stores the literal verbatim and preserves the supplied type
// value (owns the pStr formatting temporary).
TEST_F(MathOpExpressionTest, LiteralIntegerKeepsTokenAndType) {
    CMathOp op;
    CStr expr("42");
    ASSERT_TRUE(op.DoValueLiteral(&expr, 1));
    EXPECT_STREQ(op.GetResultStringToken()->GetStr(), "42");
    EXPECT_EQ(op.GetResultType(), 1u);
}

// DoValueLiteral strips redundant leading zeros for non-DWORD/non-string types.
TEST_F(MathOpExpressionTest, LiteralStripsExtraLeadingZeros) {
    CMathOp op;
    CStr expr("007");
    ASSERT_TRUE(op.DoValueLiteral(&expr, 1));
    EXPECT_STREQ(op.GetResultStringToken()->GetStr(), "7");
}

// DoValueLabel stores the label verbatim and marks it as a label (type 10).
TEST_F(MathOpExpressionTest, LabelSetsTypeTen) {
    CMathOp op;
    CStr expr("mylabel");
    ASSERT_TRUE(op.DoValueLabel(&expr));
    EXPECT_STREQ(op.GetResultStringToken()->GetStr(), "mylabel");
    EXPECT_EQ(op.GetResultType(), 10u);
}

// DoValueSingleVariable adds the variable, prefixes '@' for a global scalar and
// resolves its basic type (owns pStr / pTypeName temporaries).
TEST_F(MathOpExpressionTest, SingleVariableGlobalPrefixesAtAndResolvesType) {
    CMathOp op;
    CStr expr("myvar");
    ASSERT_TRUE(op.DoValueSingleVariable(&expr));
    EXPECT_STREQ(op.GetResultStringToken()->GetStr(), "@myvar");
    EXPECT_EQ(op.GetResultType(), 1u);
}

// DoValueComplexVariable resolves a registered integer array subscript access,
// walking the largest raw-memory cluster (pName/pFixedDataOffset/
// pSubscriptString/pResultName/subscript CMathOp chain).
TEST_F(MathOpExpressionTest, ComplexVariableResolvesRegisteredArray) {
    DWORD dwAction = 0;
    ASSERT_TRUE(g_pVarTable->AddVariable("myarr", "integer", 1, 1, true, &dwAction, false));

    CMathOp op;
    CStr expr("myarr(0)");
    EXPECT_TRUE(op.DoValueComplexVariable(&expr));
}

// ResolveStructValue on a type-specifier ("FS@<type>") resolves to the type
// size with DWORD type 7 (owns pString / pRest / pStr temporaries).
TEST_F(MathOpExpressionTest, ResolveStructValueTypeSpecifierReturnsDwordType) {
    CMathOp op;
    CStr expr("FS@sometype");
    ASSERT_TRUE(op.ResolveStructValue(&expr));
    EXPECT_EQ(op.GetResultType(), 7u);
}

// ResolveStructValue on a field specifier ("FS@<type>@<field>") walks the
// GetLeftOfPosition/GetRightOfPosition new[] buffers (pSubtypename/pFieldname)
// that are freed via SAFE_DELETE.
TEST_F(MathOpExpressionTest, ResolveStructValueFieldSpecifierResolves) {
    CMathOp op;
    CStr expr("FS@sometype@field");
    EXPECT_TRUE(op.ResolveStructValue(&expr));
}

// ProduceNewTempToken generates and registers a temp variable token
// (owns pTempName and the MakeTypeNameOfTypeValue new[] buffer pDecType).
TEST_F(MathOpExpressionTest, ProduceNewTempTokenGeneratesRegisteredName) {
    CMathOp op;
    CStr token(1);
    ASSERT_TRUE(op.ProduceNewTempToken(&token, 1));
    ASSERT_GT(strlen(token.GetStr()), 0u);
    EXPECT_EQ(token.GetChar(0), '@');
}

// IsFunction rejects a plain numeric literal (owns the pPossibleName temporary).
TEST_F(MathOpExpressionTest, IsFunctionRejectsPlainLiteral) {
    CMathOp op;
    CStr expr("42");
    EXPECT_FALSE(op.IsFunction(&expr));
}

TEST_F(MathOpExpressionTest, ComparisonsBindBeforeLogicalAnd) {
    DWORD action = 0;
    ASSERT_TRUE(g_pVarTable->AddVariable(
        "d", "integer", 0, 1, true, &action, false));
    ASSERT_TRUE(g_pVarTable->AddVariable(
        "e", "integer", 0, 1, true, &action, false));
    CMathOp op;
    CStr expression("d = 14 AND e = 20");

    ASSERT_TRUE(op.DoValue(&expression));
    ASSERT_EQ(op.GetMathSymbol(), 27u);
    ASSERT_NE(op.GetNext(), nullptr);
    EXPECT_EQ(op.GetNext()->GetMathSymbol(), 27u);
    ASSERT_NE(op.GetNext()->GetNext(), nullptr);
    EXPECT_EQ(op.GetNext()->GetNext()->GetMathSymbol(), 41u);
}
