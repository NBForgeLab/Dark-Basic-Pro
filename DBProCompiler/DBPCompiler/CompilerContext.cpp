#include "ParserHeader.h"
#include "CompilerContext.h"
#include "EXEBlock.h"
#include "DBPCompiler.h"
#include "Error.h"
#include "ASMWriter.h"
#include "DBMWriter.h"
#include "StructTable.h"
#include "StatementList.h"
#include "InstructionTable.h"
#include "LabelTable.h"
#include "DataTable.h"
#include "VarTable.h"
#include "IncludeTable.h"

// External references to legacy global pointers
extern CEXEBlock*			g_pEXE;
extern CDBPCompiler*		g_pDBPCompiler;
extern CError*				g_pErrorReport;
extern ICodeGenerator*		g_pASMWriter;
extern CDBMWriter*			g_pDBMWriter;
extern CStructTable*		g_pStructTable;
extern CStatementList*		g_pStatementList;
extern CInstructionTable*	g_pInstructionTable;
extern CLabelTable*		g_pLabelTable;
extern CDataTable*			g_pDataTable;
extern CDataTable*			g_pStringTable;
extern CDataTable*			g_pDLLTable;
extern CDataTable*			g_pCommandTable;
extern CVarTable*			g_pVarTable;
extern CIncludeTable*		g_pIncludeTable;
extern CDataTable*			g_pConstantsTable;
extern CDebugInfo			g_DebugInfo;

CompilerContext::CompilerContext() {
    pEXE = nullptr;
    pDBPCompiler = nullptr;
    pErrorReport = nullptr;
    pASMWriter = nullptr;
    pDBMWriter = nullptr;
    pStructTable = nullptr;
    pStatementList = nullptr;
    pInstructionTable = nullptr;
    pLabelTable = nullptr;
    pDataTable = nullptr;
    pStringTable = nullptr;
    pDLLTable = nullptr;
    pCommandTable = nullptr;
    pVarTable = nullptr;
    pIncludeTable = nullptr;
    pConstantsTable = nullptr;
    pDebugInfo = nullptr;
    m_bOwnsInstructionTable = false;
    m_bOwnsErrorReport = false;
}

CompilerContext::~CompilerContext() {
    Cleanup();
}

void CompilerContext::Initialize() {
    pStructTable = new CStructTable();
    pASMWriter = new CASMWriter();
    pDBMWriter = new CDBMWriter();
    pLabelTable = new CLabelTable("*");
    pDataTable = new CDataTable();
    pStringTable = new CDataTable("*");
    pDLLTable = new CDataTable("*");
    pCommandTable = new CDataTable("*");
    pVarTable = new CVarTable("$_RSP_");
    pStatementList = new CStatementList();
    
    if (g_pInstructionTable) {
        pInstructionTable = g_pInstructionTable;
        m_bOwnsInstructionTable = false;
    } else {
        pInstructionTable = new CInstructionTable();
        m_bOwnsInstructionTable = true;
    }

    pIncludeTable = new CIncludeTable();
    pConstantsTable = new CDataTable();

    if (g_pErrorReport) {
        pErrorReport = g_pErrorReport;
        m_bOwnsErrorReport = false;
    } else {
        pErrorReport = new CError();
        m_bOwnsErrorReport = true;
    }

    pEXE = new CEXEBlock();
    pDebugInfo = &g_DebugInfo;

    // Bind legacy global pointers to the context's allocated members
    g_pEXE = pEXE;
    g_pErrorReport = pErrorReport;
    g_pASMWriter = pASMWriter;
    g_pDBMWriter = pDBMWriter;
    g_pStructTable = pStructTable;
    g_pStatementList = pStatementList;
    g_pInstructionTable = pInstructionTable;
    g_pLabelTable = pLabelTable;
    g_pDataTable = pDataTable;
    g_pStringTable = pStringTable;
    g_pDLLTable = pDLLTable;
    g_pCommandTable = pCommandTable;
    g_pVarTable = pVarTable;
    g_pIncludeTable = pIncludeTable;
    g_pConstantsTable = pConstantsTable;
}

void CompilerContext::ReplaceErrorReport(CError* pNewReport) {
    if (m_bOwnsErrorReport) {
        delete pErrorReport;
    }
    pErrorReport = pNewReport;
    g_pErrorReport = pNewReport;
    m_bOwnsErrorReport = true;
}

void CompilerContext::Cleanup() {
    delete pStatementList;    pStatementList = nullptr;
    delete pDBMWriter;        pDBMWriter = nullptr;
    if (pASMWriter) {
        pASMWriter->FreeAll();
        delete pASMWriter;
        pASMWriter = nullptr;
    }

    if (pConstantsTable) { pConstantsTable->Free(); pConstantsTable = nullptr; }
    delete pIncludeTable;     pIncludeTable = nullptr;
    if (pCommandTable) { pCommandTable->Free(); pCommandTable = nullptr; }
    if (pDLLTable) { pDLLTable->Free(); pDLLTable = nullptr; }
    if (pDataTable) { pDataTable->Free(); pDataTable = nullptr; }
    if (pStringTable) { pStringTable->Free(); pStringTable = nullptr; }
    if (pStructTable) { pStructTable->Free(); pStructTable = nullptr; }

    if (m_bOwnsInstructionTable) {
        delete pInstructionTable;
    }
    pInstructionTable = nullptr;

    if (pLabelTable) { pLabelTable->Free(); pLabelTable = nullptr; }
    if (pVarTable) { pVarTable->Free(); pVarTable = nullptr; }

    if (m_bOwnsErrorReport) {
        delete pErrorReport;
    }
    pErrorReport = nullptr;

    delete pEXE;              pEXE = nullptr;

    g_pEXE = nullptr;
    if (m_bOwnsErrorReport) {
        g_pErrorReport = nullptr;
    }
    g_pASMWriter = nullptr;
    g_pDBMWriter = nullptr;
    g_pStructTable = nullptr;
    g_pStatementList = nullptr;
    if (m_bOwnsInstructionTable) {
        g_pInstructionTable = nullptr;
    }
    g_pLabelTable = nullptr;
    g_pDataTable = nullptr;
    g_pStringTable = nullptr;
    g_pDLLTable = nullptr;
    g_pCommandTable = nullptr;
    g_pVarTable = nullptr;
    g_pIncludeTable = nullptr;
    g_pConstantsTable = nullptr;
}
