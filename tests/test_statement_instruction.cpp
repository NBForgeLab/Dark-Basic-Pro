#include <gtest/gtest.h>
#include <cstring>
#include <windows.h>
#include "DBPLogger.h"
#include "Statement.h"
#include "StatementList.h"
#include "StructTable.h"
#include "InstructionTable.h"
#include "LabelTable.h"
#include "Error.h"

#include "CompilerContext.h"

// Declare compiler global pointers defined in dbp_compiler_lib
extern CStructTable*     g_pStructTable;
extern CStatementList*   g_pStatementList;
extern CInstructionTable* g_pInstructionTable;
extern CLabelTable*      g_pLabelTable;
extern CError*           g_pErrorReport;

// NOTE: the isolated test context only carries the compiler-internal
// instruction set (assignment MOVxx, INC/DEC build commands...) - runtime
// plugin commands such as PRINT/INPUT are not loaded here, so the suite
// pins DoInstruction through the assignment and build-command branches.
class StatementInstructionTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext;

    void SetUp() override {
        DBPLogger::Initialize("test_statement_instruction.log");

        m_pContext = new CompilerContext();
        m_pContext->Initialize();

        // Populate struct table with compiler defaults ("integer"='L', "float"='F', ...)
        g_pStructTable->SetStructDefaults();

        // Register the internal instruction set (MOVxx assigns, INC/DEC, END...)
        // so DoInstruction can resolve instructions in the isolated context
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

// Characterization: integer assignment refines ASSIGNLL and hands the
// pFirstParameter chain to the CParseInstruction (L-value + R-value walk).
TEST_F(StatementInstructionTest, DoInstructionCompilesIntegerAssignment) {
    char prog[] = "a = 5\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Characterization: variable-to-variable assignment walks the L-value and
// R-value chain with two variable operands.
// NOTE: float literal assignment (a = 2.5 with a AS FLOAT) hits a
// pre-existing access violation in the legacy cast path - not pinnable.
TEST_F(StatementInstructionTest, DoInstructionCompilesVariableToVariableAssignment) {
    char prog[] = "a = 7\r\nb = a\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Characterization: string L-value selects the ASSIGNSS refinement.
TEST_F(StatementInstructionTest, DoInstructionCompilesStringAssignment) {
    char prog[] = "a$ = \"hello\"\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Characterization: a compound R-value walks DoExpressionList's chain build
// and CastAllParametersToInstruction before the instruction is emitted.
TEST_F(StatementInstructionTest, DoInstructionCompilesExpressionAssignment) {
    char prog[] = "a = 1 + 2 * 3\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Characterization: INC/DEC are build commands resolved through the
// non-assignment branch (FindCorrectInstruction + instruction emit loop).
TEST_F(StatementInstructionTest, DoInstructionCompilesIncAndDecCommands) {
    char prog[] = "a = 1\r\nINC a\r\nDEC a\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Contract: assigning into a literal L-value must fail with a clean
// diagnostic - walks the ERR_SYNTAX+28 error path that hand-frees the chain.
TEST_F(StatementInstructionTest, DoInstructionFailsCleanlyOnLiteralLValue) {
    char prog[] = "5 = 1\r\nEND\r\n";
    EXPECT_FALSE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Contract: an assignment missing its R-value must fail with a clean
// diagnostic - walks the ERR_SYNTAX+42 error path.
TEST_F(StatementInstructionTest, DoInstructionFailsCleanlyOnMissingRValue) {
    char prog[] = "a =\r\nEND\r\n";
    EXPECT_FALSE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Contract: a malformed R-value expression (dangling binary operator) must
// fail cleanly - this drives DoExpressionList's length>0 branch, which builds
// the pUptoSeperator working string and walks DoExpressionListString; the
// parse failure is the branch that hand-freed pUptoSeperator before return.
TEST_F(StatementInstructionTest, DoInstructionFailsCleanlyOnMalformedRValueExpression) {
    char prog[] = "a = 1 +\r\nEND\r\n";
    EXPECT_FALSE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}
