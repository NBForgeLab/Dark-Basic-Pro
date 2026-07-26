#include <gtest/gtest.h>
#include <string>
#include <type_traits>
#include <utility>
#include <windows.h>
#include "DBPLogger.h"
#include "Declaration.h"
#include "VarTable.h"
#include "StatementList.h"
#include "StructTable.h"
#include "Error.h"

#include "CompilerContext.h"

// Declare compiler global pointers defined in dbp_compiler_lib
extern CStructTable*     g_pStructTable;
extern CStatementList*   g_pStatementList;
extern CVarTable*        g_pVarTable;
extern CError*           g_pErrorReport;

class DeclarationTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext;

    void SetUp() override {
        DBPLogger::Initialize("test_declaration.log");

        m_pContext = new CompilerContext();
        m_pContext->Initialize();

        // Populate struct table with compiler defaults ("integer"='L', "float"='F', "string"='S', ...)
        g_pStructTable->SetStructDefaults();

        // Safely initialize the internal statements via public MakeStatements method
        char dummyProg[] = "";
        g_pStatementList->MakeStatements(dummyProg, 0);
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

// Contract: the type string of a declaration chain must be returned as an owning
// std::string by value. The legacy contract handed out a raw `new char[]` buffer
// that every caller destroyed with scalar delete (undefined behaviour).
TEST_F(DeclarationTest, TypeStringOfDecsInChainReturnsOwningString) {
    static_assert(std::is_same_v<decltype(std::declval<CDeclaration&>().GetTypeStringOfDecsInChain()), std::string>,
                  "GetTypeStringOfDecsInChain must return std::string by value");
}

// A single-node chain (the implicit "returnvalue integer" head) maps to "L".
TEST_F(DeclarationTest, TypeStringOfSingleNodeChain) {
    CDeclaration decChain;
    decChain.SetDecData(0, "", "returnvalue", "integer", "", 1);

    EXPECT_EQ(decChain.GetTypeStringOfDecsInChain(), "L");
}

// A chain of returnvalue(integer) + float param + string param maps to "LFS",
// matching the user-function parameter type string layout (head char skipped by callers).
TEST_F(DeclarationTest, TypeStringOfMultiNodeChainMapsEachType) {
    CDeclaration decChain;
    decChain.SetDecData(0, "", "returnvalue", "integer", "", 1);

    CDeclaration* pFloatParam = new CDeclaration;
    pFloatParam->SetDecData(0, "", "speed#", "float", "", 1);
    decChain.Add(pFloatParam);

    CDeclaration* pStringParam = new CDeclaration;
    pStringParam->SetDecData(0, "", "name$", "string", "", 1);
    decChain.Add(pStringParam);

    EXPECT_EQ(decChain.GetTypeStringOfDecsInChain(), "LFS");
}

// Chain counting stays consistent with the type-string length.
TEST_F(DeclarationTest, NumberOfDecsMatchesTypeStringLength) {
    CDeclaration decChain;
    decChain.SetDecData(0, "", "returnvalue", "integer", "", 1);

    CDeclaration* pParam = new CDeclaration;
    pParam->SetDecData(0, "", "count", "integer", "", 1);
    decChain.Add(pParam);

    DWORD dwCount = 0;
    ASSERT_TRUE(decChain.GetNumberOfDecsInChain(&dwCount));
    EXPECT_EQ(dwCount, decChain.GetTypeStringOfDecsInChain().size());
}
