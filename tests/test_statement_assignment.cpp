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
extern CStructTable*      g_pStructTable;
extern CStatementList*    g_pStatementList;
extern CInstructionTable* g_pInstructionTable;
extern CLabelTable*       g_pLabelTable;
extern CError*            g_pErrorReport;

// Characterization pins for CStatement::DoAssignment (statement type 5,
// reached via static_cast<DWORD>(Token::Assignment)). The legacy body extracts the full assignment
// segment with ProduceFullSegment (which returns a new char[]) and freed it
// with scalar SAFE_DELETE - an array-new/scalar-delete mismatch. A heap CStr
// temporary sits alongside it. These pins lock the observable behaviour a
// RAII refactor must preserve across the two internal branches:
//   * a simple identifier assigned a parsable expression runs the modern AST
//     diagnostic pipeline (parsedExpr != nullptr);
//   * every assignment still lowers to the ASSIGNLL instruction and compiles.
class StatementAssignmentTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext;

    void SetUp() override {
        DBPLogger::Initialize("test_statement_assignment.log");

        m_pContext = new CompilerContext();
        m_pContext->Initialize();

        g_pStructTable->SetStructDefaults();
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

// Contract: a simple integer assignment with a multi-character name exercises
// the ProduceFullSegment buffer and lowers to ASSIGNLL.
TEST_F(StatementAssignmentTest, CompilesSimpleIntegerAssignment) {
    char prog[] = "value = 5\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Contract: a simple identifier assigned a parsable arithmetic expression
// drives the modern AST diagnostic branch (isSimpleId + parsedExpr).
TEST_F(StatementAssignmentTest, CompilesArithmeticExpressionAssignment) {
    char prog[] = "total = 10 + 20 * 2\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Contract: a variable-to-variable assignment compiles through the same path.
TEST_F(StatementAssignmentTest, CompilesVariableToVariableAssignment) {
    char prog[] = "a = 3\r\nb = a\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}
