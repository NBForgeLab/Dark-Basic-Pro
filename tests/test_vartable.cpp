#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <windows.h>
#include "DBPLogger.h"
#include "VarTable.h"
#include "StatementList.h"
#include "StructTable.h"
#include "Error.h"
#include "LabelTable.h"
#include "InstructionTable.h"
#include "DataTable.h"
#include "IncludeTable.h"
#include "ASMWriter.h"
#include "DBMWriter.h"
#include "TargetABI.h"

#include "CompilerContext.h"

// Declare all compiler global pointers defined in dbp_compiler_lib
extern ICodeGenerator*   g_pASMWriter;
extern CDBMWriter*       g_pDBMWriter;
extern CStructTable*     g_pStructTable;
extern CStatementList*   g_pStatementList;
extern CInstructionTable* g_pInstructionTable;
extern CLabelTable*      g_pLabelTable;
extern CDataTable*       g_pDataTable;
extern CDataTable*       g_pStringTable;
extern CDataTable*       g_pDLLTable;
extern CDataTable*       g_pCommandTable;
extern CVarTable*        g_pVarTable;
extern CIncludeTable*    g_pIncludeTable;
extern CDataTable*       g_pConstantsTable;
extern CError*           g_pErrorReport;

class VarTableTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext;

    void SetUp() override {
        DBPLogger::Initialize("test_vartable.log");
        
        m_pContext = new CompilerContext();
        m_pContext->Initialize();

        // Populate struct table with compiler defaults ("integer", "float", etc.)
        g_pStructTable->SetStructDefaults();

        // Safely initialize the internal statements (m_pProgramStatements, etc.) via public MakeStatements method
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

TEST_F(VarTableTest, AddAndFindVariable) {
    g_pStatementList->SetVariableAddParse(true);
    
    DWORD dwAction = 0;
    // Add variable using the global g_pVarTable
    bool result = g_pVarTable->AddVariable("myIntegerVar", "integer", 0, 10, true, &dwAction, false);
    
    ASSERT_TRUE(result);
    
    // Find variable
    CVarTable* pVar = g_pVarTable->FindVariable(nullptr, "myIntegerVar", 0);
    ASSERT_NE(pVar, nullptr);
}

TEST_F(VarTableTest, DictionaryCaseInsensitiveLookupAndFree) {
    g_pStatementList->SetVariableAddParse(true);
    
    DWORD dwAction = 0;
    bool bRes = g_pVarTable->AddVariable("MyIntegerVar", "integer", 0, 10, true, &dwAction, false);
    EXPECT_TRUE(bRes);
    
    // Case-insensitive find
    CVarTable* pFound = g_pVarTable->FindVariable(nullptr, "myintegervar", 0);
    ASSERT_NE(pFound, nullptr);
    EXPECT_STREQ(pFound->GetVarName()->GetStr(), "MyIntegerVar");
}

TEST_F(VarTableTest, LabelTableDictionaryCaseInsensitiveLookupAndFree) {
    bool bRes = g_pLabelTable->AddLabel("MyLabel", 100, 200, nullptr);
    EXPECT_TRUE(bRes);
    
    CLabelTable* pFound = g_pLabelTable->FindLabel("mylabel");
    ASSERT_NE(pFound, nullptr);
    EXPECT_STREQ(pFound->GetName()->GetStr(), "MyLabel");
}

TEST_F(VarTableTest, StructTableDictionaryCaseInsensitiveLookupAndFree) {
    bool bRes = g_pStructTable->AddStruct(99, "MyCustomType", 'T', 12);
    EXPECT_TRUE(bRes);
    
    CStructTable* pFound = g_pStructTable->DoesTypeEvenExist("mycustomtype");
    ASSERT_NE(pFound, nullptr);
    EXPECT_STREQ(pFound->GetTypeName()->GetStr(), "MyCustomType");
}

// --- MakeDefaultVarType contract: value-owning std::string, no raw heap handoff ---
// The legacy API returned `new char[8]` that callers released with scalar
// delete (undefined behavior). The modern contract returns std::string.

TEST_F(VarTableTest, MakeDefaultVarTypeReturnsOwningString) {
    static_assert(std::is_same_v<decltype(g_pVarTable->MakeDefaultVarType((LPSTR)nullptr)), std::string>,
                  "MakeDefaultVarType must return std::string by value");
}

TEST_F(VarTableTest, MakeDefaultVarTypeMapsSuffixToType) {
    char plainVar[] = "counter";
    char floatVar[] = "speed#";
    char stringVar[] = "name$";
    EXPECT_EQ(g_pVarTable->MakeDefaultVarType(plainVar), "integer");
    EXPECT_EQ(g_pVarTable->MakeDefaultVarType(floatVar), "float");
    EXPECT_EQ(g_pVarTable->MakeDefaultVarType(stringVar), "string");
}

TEST_F(VarTableTest, MakeDefaultVarTypeHandlesNullName) {
    EXPECT_TRUE(g_pVarTable->MakeDefaultVarType(nullptr).empty());
}

TEST(TargetAbiLayoutTest, PointerLikeTypesFollowConfiguredTargetAbi) {
    auto table = std::make_unique<CStructTable>();
    table->SetStructDefaultsFor<dbp::abi::TargetAbi64>();

    const auto sizeOf = [&table](const char* typeName) {
        return table->GetSizeOfType(const_cast<char*>(typeName));
    };

    EXPECT_EQ(sizeOf("integer"), 4U);
    EXPECT_EQ(sizeOf("double integer"), 8U);
    EXPECT_EQ(sizeOf("string"), dbp::abi::TargetAbi64::address_size);
    EXPECT_EQ(sizeOf("integer array"), dbp::abi::TargetAbi64::address_size);
    EXPECT_EQ(sizeOf("float array"), dbp::abi::TargetAbi64::address_size);
    EXPECT_EQ(sizeOf("string array"), dbp::abi::TargetAbi64::address_size);
    EXPECT_EQ(sizeOf("boolean array"), dbp::abi::TargetAbi64::address_size);
    EXPECT_EQ(sizeOf("byte array"), dbp::abi::TargetAbi64::address_size);
    EXPECT_EQ(sizeOf("word array"), dbp::abi::TargetAbi64::address_size);
    EXPECT_EQ(sizeOf("dword array"), dbp::abi::TargetAbi64::address_size);
    EXPECT_EQ(sizeOf("double float array"),
              dbp::abi::TargetAbi64::address_size);
    EXPECT_EQ(sizeOf("double integer array"),
              dbp::abi::TargetAbi64::address_size);
    EXPECT_EQ(sizeOf("userdefined var ptr"),
              dbp::abi::TargetAbi64::address_size);
    EXPECT_EQ(sizeOf("userdefined array ptr"),
              dbp::abi::TargetAbi64::address_size);

    table.release()->Free();
}

TEST(TargetAbiLayoutTest, ArrayOffsetsUseConfiguredTargetAbi) {
    auto targetTable = std::make_unique<CStructTable>();
    targetTable->SetStructDefaultsFor<dbp::abi::TargetAbi64>();

    CStructTable* const originalTable = g_pStructTable;
    g_pStructTable = targetTable.get();

    CVarTable arrayVariable("array");
    arrayVariable.SetArrFlag(1);
    DWORD offset = 0;
    const DWORD result = arrayVariable.EstablishVarOffsets(&offset);

    g_pStructTable = originalTable;
    targetTable.release()->Free();

    EXPECT_EQ(result, dbp::abi::TargetAbi64::address_size);
    EXPECT_EQ(offset, dbp::abi::TargetAbi64::address_size);
}
