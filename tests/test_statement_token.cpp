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

// Characterization pins for CStatement::FindToken and CStatement::PeekToken.
// Both obtain the next token string from ProduceNextToken (which returns a
// new char[]) and freed it with scalar SAFE_DELETE - an array-new/scalar-delete
// mismatch a RAII refactor must fix while preserving behaviour:
//   * PeekToken classifies the token at a pointer WITHOUT advancing the global
//     file-data pointer;
//   * FindToken classifies the token AND advances the global pointer past it
//     when a token is recognised;
//   * DetermineToken forces static_cast<DWORD>(Token::End) once the global pointer is within two bytes
//     of the buffer end.
// FindToken/PeekToken read the g_pStatementList file-data range, which is
// established by MakeStatements over the caller-owned buffer (never freed).
// The pins run a valid reserved-word program to set that range, then reset the
// pointer to the start with the public SetFileDataPointer (mirroring the
// parser's own ResetParserPointers) and drive the two helpers directly. The
// leading reserved word is never mutated by the parse, so its token id is
// stable across the refactor.
class StatementTokenTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext;

    void SetUp() override {
        DBPLogger::Initialize("test_statement_token.log");

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

// Contract: PeekToken classifies the leading reserved word (DO -> static_cast<DWORD>(Token::Do)) and
// leaves the global file-data pointer untouched.
TEST_F(StatementTokenTest, PeekTokenClassifiesReservedWordWithoutAdvancing) {
    CStatement statement;
    char prog[] = "do\r\nloop\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));

    LPSTR start = g_pStatementList->GetFileDataStart();
    g_pStatementList->SetFileDataPointer(start);

    DWORD tok = statement.PeekToken(start);
    EXPECT_EQ(tok, (DWORD)static_cast<DWORD>(Token::Do));
    // PeekToken must not move the global file-data pointer
    EXPECT_EQ(g_pStatementList->GetFileDataPointer(), start);
}

// Contract: FindToken classifies the leading reserved word (DO -> static_cast<DWORD>(Token::Do)) and
// advances the global file-data pointer past it (dwToken > 0 branch).
TEST_F(StatementTokenTest, FindTokenClassifiesReservedWordAndAdvances) {
    CStatement statement;
    char prog[] = "do\r\nloop\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));

    LPSTR start = g_pStatementList->GetFileDataStart();
    g_pStatementList->SetFileDataPointer(start);

    DWORD tok = statement.FindToken(start, false);
    EXPECT_EQ(tok, (DWORD)static_cast<DWORD>(Token::Do));
    // FindToken advances the global pointer for a recognised token
    EXPECT_GT(g_pStatementList->GetFileDataPointer(), start);
}

// Contract: once the global pointer is within two bytes of the buffer end,
// DetermineToken (via PeekToken) forces static_cast<DWORD>(Token::End) regardless of the token text.
TEST_F(StatementTokenTest, PeekTokenAtBufferEndReturnsEndTk) {
    CStatement statement;
    char prog[] = "do\r\nloop\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));

    LPSTR nearEnd = g_pStatementList->GetFileDataEnd() - 1;
    g_pStatementList->SetFileDataPointer(nearEnd);

    EXPECT_EQ(statement.PeekToken(nearEnd), (DWORD)static_cast<DWORD>(Token::End));
}

// Contract: a leading identifier that is neither an instruction nor a reserved
// word is classified through GetMainToken's end-of-line fallback, which builds
// a CStr of the rest of the line and calls ContainsAssignmentOperator; an
// assignment line therefore compiles (static_cast<DWORD>(Token::Assignment) path).
TEST_F(StatementTokenTest, GetMainTokenClassifiesAssignmentLine) {
    char prog[] = "counter = 7\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}
