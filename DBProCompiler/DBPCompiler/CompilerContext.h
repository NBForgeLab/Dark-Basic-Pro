#pragma once
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

    CompilerContext(CompilerContext&& other) noexcept;
    CompilerContext& operator=(CompilerContext&& other) noexcept;

    void Initialize();
    void Cleanup();
    void ReplaceErrorReport(CError* pNewReport);

    CEXEBlock*			pEXE = nullptr;
    CDBPCompiler*		pDBPCompiler = nullptr;
    CError*				pErrorReport = nullptr;
    ICodeGenerator*		pASMWriter = nullptr;
    CDBMWriter*			pDBMWriter = nullptr;
    CStructTable*		pStructTable = nullptr;
    CStatementList*		pStatementList = nullptr;
    CInstructionTable*	pInstructionTable = nullptr;
    CLabelTable*		pLabelTable = nullptr;
    CDataTable*			pDataTable = nullptr;
    CDataTable*			pStringTable = nullptr;
    CDataTable*			pDLLTable = nullptr;
    CDataTable*			pCommandTable = nullptr;
    CVarTable*			pVarTable = nullptr;
    CIncludeTable*		pIncludeTable = nullptr;
    CDataTable*			pConstantsTable = nullptr;
    CDebugInfo*         pDebugInfo = nullptr;

private:
    bool                m_bOwnsInstructionTable = false;
    bool                m_bOwnsErrorReport = false;
};
