#pragma once
#include "windows.h"
#include "DebugInfo.h"

// Forward declarations
class CEXEBlock;
class CDBPCompiler;
class CError;
class ICodeGenerator;
class CDBMWriter;
class CStructTable;
class CStatementList;
class CInstructionTable;
class CLabelTable;
class CDataTable;
class CVarTable;
class CIncludeTable;

class CompilerContext {
public:
    CompilerContext();
    ~CompilerContext();

    void Initialize();
    void Cleanup();

    CEXEBlock*			pEXE;
    CDBPCompiler*		pDBPCompiler;
    CError*				pErrorReport;
    ICodeGenerator*		pASMWriter;
    CDBMWriter*			pDBMWriter;
    CStructTable*		pStructTable;
    CStatementList*		pStatementList;
    CInstructionTable*	pInstructionTable;
    CLabelTable*		pLabelTable;
    CDataTable*			pDataTable;
    CDataTable*			pStringTable;
    CDataTable*			pDLLTable;
    CDataTable*			pCommandTable;
    CVarTable*			pVarTable;
    CIncludeTable*		pIncludeTable;
    CDataTable*			pConstantsTable;
    CDebugInfo*         pDebugInfo;
};
