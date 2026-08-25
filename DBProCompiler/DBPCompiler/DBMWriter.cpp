// DBMWriter.cpp: implementation of the CDBMWriter class.
//
//////////////////////////////////////////////////////////////////////
#include "ParserHeader.h"
#include "StatementList.h"
#include "Error.h"
#include "ICodeGenerator.h"
#include "CodeGenerationSession.h"
#include "StructTable.h"
#include "LabelTable.h"
#include "DataTable.h"
#include "VarTable.h"
#include "DBPCompiler.h"

// Custom Includes
#include "DBMWriter.h"
#include "TextConvert.h"

#include <DB3Time.h>

// External Class Pointer
extern CDBPCompiler* g_pDBPCompiler;
extern CVarTable* g_pVarTable;
extern CDataTable* g_pDataTable;
extern CDataTable* g_pStringTable;
extern CDataTable* g_pDLLTable;
extern CDataTable* g_pCommandTable;
extern CLabelTable* g_pLabelTable;
extern CStructTable* g_pStructTable;


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CDBMWriter::CDBMWriter()
	: m_dwDBMOffset(0), m_bNewCodeToParse(false)
{
}

bool CDBMWriter::OutputDBM(const char *pDBMStr, size_t length)
{
	if (m_dbmData.empty())
		return true;

	// Calculate content and complete line sizes separately.
	if (!length)
		length = strlen(pDBMStr);
	const size_t required = length + 2;

	// First ensure memory is not exceeded
	CheckAndExpandDBMMemory(static_cast<DWORD>(required));

	// Proceed to add to memory
	LPSTR pPointer = GetDBMDataPointer();
	if(pPointer+required > m_dbmData.data()+m_dbmData.size())
	{
		// Failed
		g_pErrorReport->AddErrorString("Failed to 'OutputDBM'");
		return false;
	}

	memcpy(pPointer, pDBMStr, length);
	pPointer += length;
	*(pPointer++)=13;
	*(pPointer++)=10;
	SetDBMDataPointer(pPointer);

	// Complete
	return true;
}
bool CDBMWriter::OutputDBM(CStr* pDBMStr)
{
	return OutputDBM(pDBMStr->GetStr(), pDBMStr->Length());
}

DWORD CDBMWriter::EatCarriageReturn(void)
{
	DWORD dwCount=0;
	if(!m_dbmData.empty())
	{
		// Backtrack two to eat carriage return
		LPSTR pPointer = GetDBMDataPointer()-2;
		SetDBMDataPointer(pPointer);

		// Count characters in line
		while(pPointer>m_dbmData.data())
		{
			if(*pPointer==10) break;
			pPointer--;
			dwCount++;
		}
	}
	return dwCount;
}

bool CDBMWriter::CheckAndExpandDBMMemory(DWORD dwLengthOfNewAddData)
{
	if(m_dbmData.empty())
		return true;

	// If within range of end, expand memory
	LPSTR pDBMDataBarrier=(m_dbmData.data()+m_dbmData.size())-(dwLengthOfNewAddData*2);
	if(GetDBMDataPointer()<=pDBMDataBarrier)
		// Did not expand
		return false;

	// Create New Larger memory (another 1MB)
	DWORD dwNewSize = static_cast<DWORD>(m_dbmData.size())+(102400*10);
	m_dbmData.resize(dwNewSize);

	// Mem was expanded
	return true;
}

bool CDBMWriter::WriteProgramAsEXEOrDEBUG(const char* lpEXEFilename, bool bParsingMainProgram)
{
	// Free any previous usage
	m_dbmData.clear();
	m_dwDBMOffset = 0;

	// Only parse if new code to parse
	if(GetNewCodeFlag()==true)
	{
		CodeGenerationSession codeGeneration(*g_pASMWriter);
		if (!codeGeneration.Begin())
			return false;
		if (!codeGeneration.RequireInitialized("statement emission"))
			return false;

		// Create DBM Buffer (default 1MB)?
		if(g_pDBPCompiler->GetProduceDBMFile())
		{
			m_dbmData.assign(102400*10, '\0');
			m_dwDBMOffset = 0;
		}
		else
		{
			m_dbmData.clear();
			m_dwDBMOffset = 0;
		}

		// Write DBM starting with first statement
		g_pStatementList->SetWriteStarted(true);
		if(bParsingMainProgram)
		{
			// Main program
			{
				db3::CProfile<> prof("CDBMWriter::WriteProgramAsEXEOrDEBUG() -> g_pStatementList->WriteDBM()");
				g_pStatementList->WriteDBM();
			}

			// Write program code
			{
			db3::CProfile<> prof("CDBMWriter::WriteProgramAsEXEOrDEBUG() -> \"Write program code\"");
			if (g_pStatementList->GetProgramStatements() != nullptr &&
				!g_pStatementList->GetProgramStatements()->WriteDBM())
				return false;
			}

			// Write prescan code
			{
			db3::CProfile<> prof("CDBMWriter::WriteProgramAsEXEOrDEBUG() -> \"Write prescan code\"");
			if (g_pStatementList->GetPreScanStatements() != nullptr &&
				!g_pStatementList->GetPreScanStatements()->WriteDBM())
				return false;
			}

			// Figure Out Var Offset and Final Varspace Size
			DWORD dwVarSize=0;
			g_pVarTable->EstablishVarOffsets(&dwVarSize);
			g_pStatementList->SetVarOffsetCounter(dwVarSize);
		}
		else
		{
			// Mini Program from CLI
			if (g_pStatementList->GetMiniStatements() != nullptr &&
				!g_pStatementList->GetMiniStatements()->WriteDBM())
				return false;

			// Figure Out Var Offset and Final Varspace Size
			DWORD dwVarSize=g_pStatementList->GetVarOffsetCounter();
			g_pVarTable->EstablishVarOffsets(&dwVarSize);
			g_pStatementList->SetVarOffsetCounter(dwVarSize);
		}

		// Scan all DLLS, and add any that are link-associated
		int iCount=g_pDLLTable->CompleteAnyLinkAssociates();
		if(iCount>0) g_pStatementList->IncDLLIndexCounter(iCount);

		// Write DBM of Variables (recalcualtes offset to track offset positions)
		if(g_pDBPCompiler->GetProduceDBMFile())
		{
			db3::CProfile<> prof("CDBMWriter::WriteProgramAsEXEOrDEBUG() -> \"Write DBM or Variables\"");
			g_pVarTable->WriteDBMHeader();
			g_pVarTable->WriteDBM();
			g_pVarTable->WriteDBMFooter(g_pStatementList->GetVarOffsetCounter());	
		}

		// Write DBM Data
		if(g_pDBPCompiler->GetProduceDBMFile())
		{
			db3::CProfile<> prof("CDBMWriter::WriteProgramAsEXEOrDEBUG() -> Write DBM Data (g_pStringTable)");
			if(g_pStringTable->GetNext())
			{
				g_pStringTable->WriteDBMHeader(1);
				g_pStringTable->GetNext()->WriteDBM();
			}
		}

		// Write DBM Data
		if(g_pDBPCompiler->GetProduceDBMFile())
		{
			db3::CProfile<> prof("CDBMWriter::WriteProgramAsEXEOrDEBUG() -> Write DBM Data (g_pDataTable)");
			if(g_pDataTable->GetNext())
			{
				g_pDataTable->WriteDBMHeader(2);
				g_pDataTable->GetNext()->WriteDBM();
			}
		}

		// Write DBM Data
		if(g_pDBPCompiler->GetProduceDBMFile())
		{
			db3::CProfile<> prof("CDBMWriter::WriteProgramAsEXEOrDEBUG() -> Write DBM Data (g_pDLLTable)");
			if(g_pDLLTable->GetNext())
			{
				g_pDLLTable->WriteDBMHeader(3);
				g_pDLLTable->GetNext()->WriteDBM();
			}
		}

		// Write DBM Data
		if(g_pDBPCompiler->GetProduceDBMFile())
		{
			db3::CProfile<> prof("CDBMWriter::WriteProgramAsEXEOrDEBUG() -> Write DBM Data (g_pCommandTable)");
			if(g_pCommandTable->GetNext())
			{
				g_pCommandTable->WriteDBMHeader(4);
				g_pCommandTable->GetNext()->WriteDBM();
			}
		}

		// Write DBM Data
		if(g_pDBPCompiler->GetProduceDBMFile())
		{
			db3::CProfile<> prof("CDBMWriter::WriteProgramAsEXEOrDEBUG() -> Write DBM Data (g_pLabelTable)");
			if(g_pLabelTable->GetNext())
			{
				g_pLabelTable->WriteDBMHeader();
				g_pLabelTable->GetNext()->WriteDBM();
			}
		}

		// Write DBM of Structures (debug)
		if(g_pDBPCompiler->GetProduceDBMFile())
		{
			db3::CProfile<> prof("CDBMWriter::WriteProgramAsEXEOrDEBUG() -> Write DBM Data (g_pStructTable)");
			if(g_pStructTable)
			{
				g_pStructTable->WriteDBMHeader();
				g_pStructTable->WriteDBM();
			}
		}

		// Deposit in DBM File
		//#ifdef _DEBUG
		//{
		if(g_pDBPCompiler->GetProduceDBMFile())
		{
			db3::CProfile<> prof("CDBMWriter::WriteProgramAsEXEOrDEBUG() -> Deposit in DBM File");
			HANDLE hFile = CreateFileW(TextConvert::UTF8ToUTF16(g_pDBPCompiler->GetInternalFile(PATH_TEMPDBMFILE)).c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if(hFile!=INVALID_HANDLE_VALUE)
			{
				DWORD BytesWritten=0;
				DWORD ActualBytesToWrite=m_dwDBMOffset;
				WriteFile(hFile, m_dbmData.data(), ActualBytesToWrite, &BytesWritten, nullptr);
				CloseHandle(hFile);
			}
		}
		//}
		//#endif

		// Free DBM memory
		g_pStatementList->SetWriteStarted(false);
		if(g_pDBPCompiler->GetProduceDBMFile()) m_dbmData.clear();
		if (!codeGeneration.Finish())
			return false;
	}

	// Progress Reporting Tool Reset For Percentage Step Through
	g_pErrorReport->SetMaxLines(g_pStatementList->GetLineNumber());

	// Create ASM Header
	return g_pASMWriter->PrepareEXE(lpEXEFilename, bParsingMainProgram, GetNewCodeFlag());
}
