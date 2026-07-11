#include <gtest/gtest.h>
#include <iostream>
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
