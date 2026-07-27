#include <gtest/gtest.h>
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

class StatementDeclarationTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext;

    void SetUp() override {
        DBPLogger::Initialize("test_statement_declaration.log");

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

// Characterization: a plain DIM array declaration compiles through
// DoDeclaration's DIMTK path (array name/value separation + allocation).
TEST_F(StatementDeclarationTest, DoDeclarationCompilesDimArray) {
    char prog[] = "DIM arr(5)\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Characterization: DIM with an AS type specifier exercises the ASTK branch
// (pWhatType/pTypeSpecifier token buffers and the init-separation logic).
TEST_F(StatementDeclarationTest, DoDeclarationCompilesDimArrayWithType) {
    char prog[] = "DIM arr(5) AS INTEGER\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Characterization: TYPE bodies with fields and interleaved line remarks
// exercise the REMLINETK skip loop - the legacy code leaked the remark token
// and every intermediate token produced while skipping comment lines.
TEST_F(StatementDeclarationTest, DoDeclarationCompilesTypeWithFieldsAndComments) {
    char prog[] = "TYPE vec\r\nx AS FLOAT\r\n` field comment\r\ny AS FLOAT\r\nENDTYPE\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Characterization: GLOBAL scalar with an inline init value exercises the
// '=' init capture (GetStringToEndOfLine) and the CParseInit ownership path.
TEST_F(StatementDeclarationTest, DoDeclarationCompilesGlobalWithInit) {
    char prog[] = "GLOBAL g AS INTEGER = 42\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Contract: END inside a TYPE body must fail with a clean diagnostic - this
// walks the ENDTK error path that hand-frees the token buffer before return.
TEST_F(StatementDeclarationTest, DoDeclarationFailsCleanlyOnEndInsideType) {
    char prog[] = "TYPE vec\r\nx AS FLOAT\r\nEND\r\nENDTYPE\r\nEND\r\n";
    EXPECT_FALSE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}

// Contract: a numeric DIM array name must fail cleanly - this walks the
// IsTextASingleVariable error path where the legacy code hand-freed four
// buffers (and relied on pDecName/pString aliasing to avoid a double-free).
TEST_F(StatementDeclarationTest, DoDeclarationFailsCleanlyOnNumericArrayName) {
    char prog[] = "DIM 5(3)\r\nEND\r\n";
    EXPECT_FALSE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}
