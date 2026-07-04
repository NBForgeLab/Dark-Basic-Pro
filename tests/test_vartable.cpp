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
    void SetUp() override {
        DBPLogger::Initialize("test_vartable.log");
        
        // Allocate all global compiler environments
        g_pErrorReport      = new CError();
        g_pVarTable         = new CVarTable();
        g_pLabelTable       = new CLabelTable();
        g_pInstructionTable = new CInstructionTable();
        g_pStructTable      = new CStructTable();
        g_pStringTable      = new CDataTable();
        g_pDataTable        = new CDataTable();
        g_pDLLTable         = new CDataTable();
        g_pCommandTable     = new CDataTable();
        g_pIncludeTable     = new CIncludeTable();
        g_pConstantsTable   = new CDataTable();
        g_pASMWriter        = new CASMWriter();
        g_pDBMWriter        = new CDBMWriter();
        g_pStatementList    = new CStatementList();

        // Populate struct table with compiler defaults ("integer", "float", etc.)
        g_pStructTable->SetStructDefaults();

        // Safely initialize the internal statements (m_pProgramStatements, etc.) via public MakeStatements method
        char dummyProg[] = "";
        g_pStatementList->MakeStatements(dummyProg, 0);
    }

    void TearDown() override {
        delete g_pStatementList;    g_pStatementList = nullptr;
        delete g_pDBMWriter;        g_pDBMWriter = nullptr;
        delete g_pASMWriter;        g_pASMWriter = nullptr;
        delete g_pConstantsTable;   g_pConstantsTable = nullptr;
        delete g_pIncludeTable;     g_pIncludeTable = nullptr;
        delete g_pCommandTable;     g_pCommandTable = nullptr;
        delete g_pDLLTable;         g_pDLLTable = nullptr;
        delete g_pDataTable;        g_pDataTable = nullptr;
        delete g_pStringTable;      g_pStringTable = nullptr;
        delete g_pStructTable;      g_pStructTable = nullptr;
        delete g_pInstructionTable; g_pInstructionTable = nullptr;
        delete g_pLabelTable;       g_pLabelTable = nullptr;
        delete g_pVarTable;         g_pVarTable = nullptr;
        delete g_pErrorReport;      g_pErrorReport = nullptr;
        
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
