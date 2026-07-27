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

// DoDataStatement parses a "data <item>,<item>,..." line, pushing each
// value into the data table and advancing the data index counter. The
// legacy body used three heap allocations released by manual SAFE_DELETE
// on every exit path (the line segment CStr, the speech-mark strip CStr,
// and the CParameter chain). These pins lock the observable behaviour
// before the RAII conversion.
class StatementDataTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext;

    void SetUp() override {
        DBPLogger::Initialize("test_statement_data.log");

        m_pContext = new CompilerContext();
        m_pContext->Initialize();

        // Populate struct table with compiler defaults ("integer"='L', ...)
        g_pStructTable->SetStructDefaults();

        // Register the internal instruction set so the parameter parser can
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

// Numeric DATA items are parsed and stored; the data index counter
// advances once per value.
TEST_F(StatementDataTest, DoDataStatementStoresNumericItems) {
    char prog[] = "data 1,2,3\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    EXPECT_EQ(g_pStatementList->GetDataIndexCounter(), (DWORD)3);
}

// Speech-marked DATA items exercise the EatSpeechMarks branch (the inner
// heap CStr in the legacy body) and are stored as strings.
TEST_F(StatementDataTest, DoDataStatementStoresStringItems) {
    char prog[] = "data \"hello\",\"world\"\r\nEND\r\n";
    ASSERT_TRUE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
    EXPECT_EQ(g_pStatementList->GetDataIndexCounter(), (DWORD)2);
}

// An unrecognised DATA item fails cleanly, walking an error path that
// hand-frees the segment string and parameter chain in the legacy body.
TEST_F(StatementDataTest, DoDataStatementFailsCleanlyOnUnknownItem) {
    char prog[] = "data abc\r\nEND\r\n";
    EXPECT_FALSE(g_pStatementList->MakeStatements(prog, (DWORD)strlen(prog) + 1));
}
