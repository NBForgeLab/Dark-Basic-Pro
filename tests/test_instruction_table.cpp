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
#include "InstructionTableEntry.h"
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

// Characterization suite for CInstructionTable command registration. Each test
// drives a public registration/lookup path (AddCommand, AddUniqueCommand,
// TurnStringIntoCommand, AddUserFunction) that owns heap CStr / CInstructionTableEntry
// temporaries, pinning the observable "command becomes findable / malformed input is
// rejected" contract before the RAII conversion of InstructionTable.cpp.
class InstructionTableTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext;

    void SetUp() override {
        DBPLogger::Initialize("test_instruction_table.log");

        m_pContext = new CompilerContext();
        m_pContext->Initialize();
        g_pStructTable->SetStructDefaults();

        // Populate the internal instruction database so the map-backed
        // registration and lookup paths are fully initialized.
        g_pInstructionTable->SetInternalInstructionDatabase();

        // Same backend bootstrap as the other compiler-lib characterization suites.
        char dummyProg[] = "";
        g_pStatementList->MakeStatements(dummyProg, 0);
        ASSERT_TRUE(g_pASMWriter->CreateASMHeader());
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

// AddCommand registers a command that becomes findable by its exact name and
// parameter-type signature; a mismatched signature or unknown name is not found.
TEST_F(InstructionTableTest, AddCommandBecomesFindableByNameAndParams) {
    char name[]   = "MYCUSTOMPINCMD";
    char dll[]    = "";
    char dec[]    = "";
    char params[] = "LL";

    ASSERT_TRUE(g_pInstructionTable->AddCommand(name, dll, dec, params, 0, 2));

    char sameParams[]  = "LL";
    char otherParams[] = "LLL";
    char unknownName[] = "NOSUCHCMDXYZ";
    EXPECT_TRUE(g_pInstructionTable->FindInstructionWithNameAndParams(name, sameParams));
    EXPECT_FALSE(g_pInstructionTable->FindInstructionWithNameAndParams(name, otherParams));
    EXPECT_FALSE(g_pInstructionTable->FindInstructionWithNameAndParams(unknownName, sameParams));
}

// AddUniqueCommand adds the command once and is idempotent: a second call with
// the identical name+params reports success without changing findability.
TEST_F(InstructionTableTest, AddUniqueCommandIsIdempotent) {
    char name[]   = "MYUNIQUEPINCMD";
    char dll[]    = "";
    char dec[]    = "";
    char params[] = "L";

    ASSERT_TRUE(g_pInstructionTable->AddUniqueCommand(name, dll, dec, params, 0, 1));
    EXPECT_TRUE(g_pInstructionTable->FindInstructionWithNameAndParams(name, params));

    EXPECT_TRUE(g_pInstructionTable->AddUniqueCommand(name, dll, dec, params, 0, 1));
    EXPECT_TRUE(g_pInstructionTable->FindInstructionWithNameAndParams(name, params));
}

// TurnStringIntoCommand parses a well-formed "NAME%PARAMS%DECORATED" record and
// registers the command so it is findable by the parsed name and param types.
TEST_F(InstructionTableTest, TurnStringIntoCommandValidRegistersCommand) {
    char category[] = "test";
    char dllName[]  = "MYPLUGIN.DLL";
    char record[]   = "MYTURNCMD%LL%MYDECORATED";

    ASSERT_TRUE(g_pInstructionTable->TurnStringIntoCommand(category, dllName, record));

    char name[]   = "MYTURNCMD";
    char params[] = "LL";
    EXPECT_TRUE(g_pInstructionTable->FindInstructionWithNameAndParams(name, params));
}

// A record with no '%' separator is rejected.
TEST_F(InstructionTableTest, TurnStringIntoCommandNoSeparatorFails) {
    char category[] = "test";
    char dllName[]  = "MYPLUGIN.DLL";
    char record[]   = "NOPERCENT";

    EXPECT_FALSE(g_pInstructionTable->TurnStringIntoCommand(category, dllName, record));
}

// A record with only a single '%' separator (missing the decorated section) is rejected.
TEST_F(InstructionTableTest, TurnStringIntoCommandSingleSeparatorFails) {
    char category[] = "test";
    char dllName[]  = "MYPLUGIN.DLL";
    char record[]   = "NAMEONLY%PARAMONLY";

    EXPECT_FALSE(g_pInstructionTable->TurnStringIntoCommand(category, dllName, record));
}

// AddUserFunction registers a user function that becomes findable by name; an
// unregistered name resolves to null.
TEST_F(InstructionTableTest, AddUserFunctionBecomesFindableByName) {
    char name[]   = "MYUSERFUNCPIN";
    char params[] = "";

    ASSERT_TRUE(g_pInstructionTable->AddUserFunction(name, 0, params, 0, NULL));
    EXPECT_NE(g_pInstructionTable->FindUserFunction(name), nullptr);

    char unknownName[] = "NOSUCHUSERFUNCXYZ";
    EXPECT_EQ(g_pInstructionTable->FindUserFunction(unknownName), nullptr);
}

// ResolveEntry matches command names case-insensitively: a lowercase query
// must find a command registered in uppercase (exercises the strnicmp path).
TEST_F(InstructionTableTest, ResolveEntryMatchesCaseInsensitive) {
    char name[]   = "MYCASECMD";
    char dll[]    = "";
    char dec[]    = "";
    char params[] = "L";

    ASSERT_TRUE(g_pInstructionTable->AddCommand(name, dll, dec, params, 0, 1));

    // FindInstruction delegates to the same ResolveEntry path that uses strnicmp
    DWORD dwData = 0, dwParamMax = 0, dwLength = 0;
    CInstructionTableEntry* pRef = nullptr;
    char query[] = "mycasecmd";
    EXPECT_TRUE(g_pInstructionTable->FindInstruction(
        false, query, 0, &dwData, &dwParamMax, &dwLength, &pRef));
    EXPECT_NE(pRef, nullptr);
}
