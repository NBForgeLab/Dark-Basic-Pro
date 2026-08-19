// DBPCompiler.cpp: implementation of the CDBPCompiler class.
//
//////////////////////////////////////////////////////////////////////

// Includes
#include <stdio.h>
#include "StringUtils.h"
#include <iostream>
#include "direct.h"
#include "macros.h"
#include "ASMWriter.h"
#include "DBPCompiler.h"
#include "RuntimeContract.h"
#include "CompilerContext.h"
#include "CompilationInput.h"
#include "InstructionTable.h"
#include "StatementList.h"
#include "StructTable.h"
#include "DBMWriter.h"
#include "IncludeTable.h"
#include "LabelTable.h"
#include "DataTable.h"
#include "VarTable.h"
#include "DebugInfo.h"
#include "Error.h"
#include "Str.h"
#include "shlobj.h"
#include "DBPLogger.h"
#include "TextConvert.h"
#include "DebuggerInterface.h"

#include <DB3Time.h>
#include <algorithm>
#include <cctype>
#include <memory>
#include <string_view>

// Internal Global Data Pointers
CEXEBlock*			g_pEXE				= nullptr;
CDBPCompiler*		g_pDBPCompiler		= nullptr;
CError*				g_pErrorReport		= nullptr;
ICodeGenerator*		g_pASMWriter		= nullptr;
CDBMWriter*			g_pDBMWriter		= nullptr;
CStructTable*		g_pStructTable		= nullptr;
CStatementList*		g_pStatementList	= nullptr;
CInstructionTable*	g_pInstructionTable	= nullptr;
CLabelTable*		g_pLabelTable		= nullptr;
CDataTable*			g_pDataTable		= nullptr;
CDataTable*			g_pStringTable		= nullptr;
CDataTable*			g_pDLLTable			= nullptr;
CDataTable*			g_pCommandTable		= nullptr;
CVarTable*			g_pVarTable			= nullptr;
CIncludeTable*		g_pIncludeTable		= nullptr;
CDataTable*			g_pConstantsTable	= nullptr;
CDebugInfo			g_DebugInfo;

// lee - 050406 - u6rc6 - new directive for academic non-admin users
bool				g_bLocalTempFolder	= false;
bool				g_bExternaliseDLLS	= false;

// External Values
extern DWORD_PTR g_dwEscapeValueMem;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CDBPCompiler::CDBPCompiler(LPSTR pCompilerFilename)
{
	// Store Compiler Filename
	m_pCompilerFilename = std::make_unique<CStr>(pCompilerFilename);
	m_pCompilerPathOnly = std::make_unique<CStr>(pCompilerFilename);
	m_pCompilerPathOnly->TrimToPathOnly();
	m_pContext = nullptr;

	// Initialisation of File Data Ptrs
	m_dwOriginalFileDataSize=0;
	m_FileDataSize=0;
	m_pFileData=nullptr;

	// Initialisation of Project File Data
	m_bProjectExists=false;
	m_ProjectFileDataSize=0;
	m_pProjectFileData=nullptr;

	// Initialisation of Project Setting Vars
	m_pFinalDBASource=nullptr;
	m_pEXEFilename=nullptr;

	// Project Compiler Settings
	m_bDebugModeOn=false;
	m_bRuntimeErrorsOn=true;
	m_bProduceDBMFileOn=true;
	m_bFullScreenModeOn=false;
	m_bFullDesktopModeOn=false;
	m_bDesktopModeOn=false;
	m_dwStartDisplayWidth=640;
	m_dwStartDisplayHeight=480;
	m_dwStartDisplayDepth=32;
	m_bHiddenModeOn=false;
	m_bEXEAloneState=false;
	m_bEXEInstallerState=false;
	m_bCompressPCKState=false;
	m_bEncryptionState=false;
	m_bDoubleLiterals=false;
	m_bSpeedOverStabilityState=false;
	m_bGenerateHelpTxtOn=false;
	#ifdef MAKEHELPTXT
	// lee - 210406 - can switch this to true to generate new HELPTXT in plugins folder
	m_bGenerateHelpTxtOn = true;
	#endif
}

CDBPCompiler::~CDBPCompiler()
{
	// Safe Deletions (dynamic memory buffers)
	SAFE_DELETE_ARRAY(m_pFileData);
	SAFE_DELETE_ARRAY(m_pProjectFileData);
	SAFE_DELETE_ARRAY(m_pFinalDBASource);
	SAFE_DELETE_ARRAY(m_pEXEFilename);

	// RAII handles: m_pCompilerFilename, m_pCompilerPathOnly, m_OriginalFileData,
	// m_pInternalFile[], m_pAbsolutePathToProjectFile, m_pRelativePathToProjectFile,
	// m_BreakpointList, g_ExcludeFiles
}

bool CDBPCompiler::PerformCompileOnProject(void)
{
	// Result
	bool bResult=true;

	// Get DBA Source File
	LPSTR pDBAFilename = m_pFinalDBASource;

	// Create Error Report Database
	g_pErrorReport->LoadErrorDatabase(GetInternalFile(PATH_ERRORSFILE));

	// Load DBA into memory
	ReportStatus("load_dba", "Loading main DBA file into memory...");
	if(m_compilationInput ? LoadPreparedSource() : LoadDBA(pDBAFilename))
	{
		// Expand FileData to unfold any #Includes
		ReportStatus("unfold_includes", "Expanding nested #include files...");
		if(UnfoldFileDataIncludes())
		{
			ReportStatus("instruction_table_init", "Loading instruction database...");
			// leemove - 250604 - u54 - set default and load command database
			g_pInstructionTable = new CInstructionTable;
			g_pInstructionTable->SetInternalInstructionDatabase();
			g_pInstructionTable->LoadInstructionDatabase();

			// Expand FileData to unfold any #Constants
			ReportStatus("unfold_constants", "Replacing #constant keywords...");
			if(UnfoldFileDataConstants())
			{
				ReportStatus("breakpoints", "Recording breakpoints...");
				// Remove Breakpoints and Construct BreakPoint List
				if(RemoveAndRecordBreakpoints()==false)
				{
					g_pErrorReport->AddErrorString("Failed to 'RemoveAndRecordBreakpoints'");
					bResult=false;
				}
				else
				{
					// Begin List to collect tokenised program
					ReportStatus("make_program", "Parsing statements and generating instructions...");
					if(MakeProgram()==false)
					{
						g_pErrorReport->AddErrorString("Failed to Parse Program (MakeDBM->MakeProgram)'");
						bResult=false;
					}
				}
			}
			else
			{
				g_pErrorReport->AddErrorString("Failed to 'UnfoldFileDataConstants'");
				bResult=false;
			}

			// free instruction table
			SAFE_DELETE(g_pInstructionTable);
		}
		else
		{
			g_pErrorReport->AddErrorString("Failed to 'UnfoldIncludes'");
			bResult=false;
		}
	}
	else
	{
		g_pErrorReport->AddErrorString("Failed to 'LoadDBA'");
		bResult=false;
	}

	// Report Error if available..
	if(g_pErrorReport)
	{
		if(g_pErrorReport->IsError())
		{
			// Create Virtual File for Error Transfer
			HANDLE hFileMap = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 256, L"DBPROEDITORMESSAGE");
			LPVOID lpVoid = (hFileMap != nullptr && hFileMap != INVALID_HANDLE_VALUE) ? MapViewOfFile(hFileMap, FILE_MAP_WRITE, 0, 0, 256) : nullptr;
			LPCSTR lpString = g_pErrorReport->GetParserErrorString();
			if(g_pErrorReport->IsParserError())
				lpString = g_pErrorReport->GetParserErrorString();
			else
				lpString = g_pErrorReport->GetErrorString();

			// Copy Error to Virtual File
			if (lpVoid && lpString)
			{
				if ( strlen ( lpString )>255 )
				{
					char ErrorSpace[256];
					strncpy_s ( ErrorSpace, sizeof(ErrorSpace), lpString, _TRUNCATE );
					ErrorSpace[255]=0;
					snprintf((LPSTR)lpVoid, sizeof(ErrorSpace), "%s", ErrorSpace);
				}
				else
					snprintf((LPSTR)lpVoid, 256, "%s", lpString);
			}

			// Find Editor to send to
			if (!g_bJsonDiagnostics)
			{
				HWND hWnd = FindWindowW(L"TDBPROEDITOR",nullptr);
				if(hWnd)
				{
					// Found editor, transmit
					SendMessage(hWnd, WM_USER+0, 0, 0);
				}
				else
				{
					// No Editor, use Own Window (causes crashes lots)
					MessageBoxW(nullptr, TextConvert::UTF8ToUTF16(lpString).c_str(), L"COMPILER ERROR", MB_OK);
				}
			}

			if (lpVoid)
			{
				UnmapViewOfFile(lpVoid);
			}
			if (hFileMap != nullptr && hFileMap != INVALID_HANDLE_VALUE)
			{
				CloseHandle(hFileMap);
			}

			// Deposit Verbose Error File (test mode only)
			g_pErrorReport->OutputInternalErrorReport();
		}
	}

	// Free Include Table
	SAFE_DELETE(g_pIncludeTable);

	// Complete
	return bResult;
}

bool CDBPCompiler::PrepareCompilationInput(const char* pInputFilename, bool emitFinalSource)
{
	m_compilationInput.reset();
	if (pInputFilename == nullptr || pInputFilename[0] == 0)
		return false;

	const std::filesystem::path inputPath(pInputFilename);
	std::string extension = inputPath.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(),
		[](unsigned char value) { return static_cast<char>(std::tolower(value)); });

	SourceAssemblyResult<CompilationInput> inputResult =
		extension == ".dbpro"
			? CompilationInput::FromProjectFile(inputPath, SourceAssemblyOptions{})
			: CompilationInput::FromSourceFile(inputPath);
	if (!inputResult)
	{
		if (g_bJsonDiagnostics)
		{
			const auto& error = inputResult.error();
			std::cout << "{\"type\":\"diagnostic\",\"code\":\""
				<< SourceAssemblyDiagnosticCode(error.code)
				<< "\",\"severity\":\"error\",\"stage\":\"source_assembly\","
				<< "\"project\":\"" << EscapeJSON(pInputFilename)
				<< "\",\"source\":\"" << EscapeJSON(error.sourcePath.string())
				<< "\",\"line\":0,\"message\":\""
				<< EscapeJSON(error.message) << "\"}\n" << std::flush;
		}
		if (g_pErrorReport)
			g_pErrorReport->AddErrorString(
				const_cast<char*>(inputResult.error().message.c_str()));
		return false;
	}

	m_compilationInput = std::make_unique<CompilationInput>(
		std::move(inputResult.value()));

	if (emitFinalSource)
	{
		if (extension != ".dbpro")
		{
			if (g_pErrorReport)
				g_pErrorReport->AddErrorString(
					"--emit-final-source requires a DBPro project input.");
			m_compilationInput.reset();
			return false;
		}
		const auto manifestResult = ProjectManifestReader::Read(inputPath);
		if (!manifestResult || !manifestResult.value().finalSourcePath.has_value())
		{
			if (g_pErrorReport)
				g_pErrorReport->AddErrorString(
					"--emit-final-source requires a non-empty 'final source' field.");
			m_compilationInput.reset();
			return false;
		}
		const auto writeResult = FinalSourceArtifactWriter::WriteAtomically(
			*manifestResult.value().finalSourcePath, m_compilationInput->bytes());
		if (!writeResult)
		{
			if (g_pErrorReport)
				g_pErrorReport->AddErrorString(
					const_cast<char*>(writeResult.error().message.c_str()));
			m_compilationInput.reset();
			return false;
		}
	}
	return true;
}

bool CDBPCompiler::LoadPreparedSource(void)
{
	if (!m_compilationInput)
		return false;

	const auto& sourceBytes = m_compilationInput->bytes();
	if (sourceBytes.size() > MAXDWORD)
		return false;

	SAFE_DELETE_ARRAY(m_pFileData);
	m_pFileData = new char[sourceBytes.size() + 1]();
	if (!m_pFileData)
		return false;
	if (!sourceBytes.empty())
		memcpy(m_pFileData, sourceBytes.data(), sourceBytes.size());
	m_FileDataSize = static_cast<DWORD>(sourceBytes.size());

	CStr trimmed(m_pFileData);
	trimmed.EatTrailingEdgeSpacesandTabs();
	const DWORD trimmedSize = static_cast<DWORD>(strlen(trimmed.GetStr()));
	ZeroMemory(m_pFileData, sourceBytes.size() + 1);
	memcpy(m_pFileData, trimmed.GetStr(), trimmedSize);
	m_FileDataSize = trimmedSize;

	m_dwOriginalFileDataSize = m_FileDataSize;
	m_OriginalFileData.assign(m_dwOriginalFileDataSize + 256, 0);
	memcpy(m_OriginalFileData.data(), m_pFileData, m_FileDataSize);
	return true;
}

bool CDBPCompiler::LoadDBA(LPSTR pDBAFilename)
{
	db3::CProfile<> prof("CDBPCompiler::LoadDBA");

	if (g_bJsonDiagnostics) {
		std::cout << "{\"type\":\"status\",\"stage\":\"debug\",\"message\":\"LoadDBA target path: " << EscapeJSON(pDBAFilename) << "\"}\n" << std::flush;
	}

	// Release any previous usage
	SAFE_DELETE_ARRAY(m_pFileData);

	// Load DBA Data (by file or MMF)
	bool bFileLoaded = false;
	if ( m_bSourceIsMMF )
	{
		// Load DBA MMF into memory
		if(LoadRawFromMMF(pDBAFilename, &m_pFileData, &m_FileDataSize))
			bFileLoaded=true;
	}
	else
	{
		// Load DBA file into memory
		if(LoadRaw(pDBAFilename, &m_pFileData, &m_FileDataSize))
			bFileLoaded=true;
	}

	// If loaded okay..
	if( bFileLoaded )
	{
		// Eat EdgeSpaces
		CStr newStr(m_pFileData);
		newStr.EatTrailingEdgeSpacesandTabs();
		ZeroMemory(m_pFileData, m_FileDataSize);
		snprintf(m_pFileData, m_FileDataSize, "%s", newStr.GetStr());
	}
	else
	{
		g_pErrorReport->AddErrorString("Failed to 'LoadDBA'");
		return false;
	}

	// Make snapshot of filedata in original store
	m_dwOriginalFileDataSize = m_FileDataSize;
	m_OriginalFileData.assign(m_dwOriginalFileDataSize+256, 0);
	memcpy(m_OriginalFileData.data(), m_pFileData, m_FileDataSize);

	// Complete
	return true;
}

bool CDBPCompiler::LoadRaw(LPSTR pDBAFilename, LPSTR* ppData, DWORD* pdwDataSize)
{
	// Load DBA into memory
	HANDLE hFile = CreateFileW(TextConvert::UTF8ToUTF16(pDBAFilename).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if(hFile!=INVALID_HANDLE_VALUE)
	{
		// Create memory and transfer file data to it
		DWORD dwLoadSize = GetFileSize(hFile, nullptr);
		LPSTR pLoadData = new char[dwLoadSize + 1]();
	
		// Transfer Data
		DWORD BytesRead=0;
		ReadFile(hFile, pLoadData, dwLoadSize, &BytesRead, nullptr);

		// Close File
		SAFE_CLOSE(hFile);

		// Set Data for return vars
		*ppData = pLoadData;
		*pdwDataSize = dwLoadSize;
	}
	else
	{
		return false;
	}

	// Complete
	return true;
}

bool CDBPCompiler::LoadRawFromMMF(LPSTR pDBAMMFName, LPSTR* ppData, DWORD* pdwDataSize)
{
	// Memory to be used to store string sent
	bool bResult=true;

	// First Four Bytes are Size of Message
	HANDLE hFileMap = OpenFileMappingW(FILE_MAP_READ,FALSE,TextConvert::UTF8ToUTF16(pDBAMMFName).c_str());
	if(hFileMap)
	{
		LPVOID lpVoid = MapViewOfFile(hFileMap,FILE_MAP_READ,0,0,0);
		if(lpVoid)
		{
			// First DWORD is data size
			DWORD dwDataSize = *((LPDWORD) lpVoid);
			if ( dwDataSize>0 )
			{
				// Create memory and transfer file data to it
				LPSTR pLoadData = new char[dwDataSize + 1]();
			
				// Transfer Data
				memcpy(pLoadData, (LPSTR)lpVoid+4, dwDataSize);

				// Set Data for return vars
				*ppData = pLoadData;
				*pdwDataSize = dwDataSize;
			}
			else
			{
				// MMF not found
				bResult=false;
			}

			// Close Message
			UnmapViewOfFile(lpVoid);
		}
		else
		{
			// MMF not found
			bResult=false;
		}
		CloseHandle(hFileMap);
	}
	else
	{
		// MMF not found
		bResult=false;
	}

	// Complete
	return bResult;
}

bool CDBPCompiler::UnfoldFileDataIncludes(void)
{
	db3::CProfile<> prof("CDBPCompiler::UnfoldFileDataIncludes");

	// Root FileData
	LPSTR pRootData=m_pFileData;
	DWORD dwRootDataSize=m_FileDataSize;

	// New FileData
	LPSTR pNewData=nullptr;
	DWORD dwNewDataSize=0;

	// Create Include Table Entry (ROOT)
	g_pIncludeTable = new CIncludeTable;
	CStr* pRootName = new CStr("root");
	g_pIncludeTable->SetFilename(pRootName);
	g_pIncludeTable->SetFirstByte(0);

	// Fill with root source code
	CopyData(&pNewData, &dwNewDataSize, pRootData, dwRootDataSize);

	// Go through entire data (as it is being built)
	DWORD dwPtrOffset=0;
	while(pNewData && dwPtrOffset < dwNewDataSize)
	{
		// Seek #include
		DWORD dwCount=0;
		LPSTR pIncludeFilenameRaw = nullptr;
		LPSTR pPtr = pNewData + dwPtrOffset;
		LPSTR pPtrEnd = pNewData + dwNewDataSize;
		bool bSeekResult = SeekIncludeToken(&pPtr, pPtrEnd, &dwCount, &pIncludeFilenameRaw);
		std::unique_ptr<char[]> pIncludeFilename(pIncludeFilenameRaw);
		if(bSeekResult==false)
		{
			g_pErrorReport->AddErrorString("Failed to 'SeekIncludeToken'");
			return false;
		}

		// Leave when no more
		if(pPtr >= pPtrEnd)
		{
			// Ready for parsing single filedata block
			break;
		}

		// Advance offset by distance scanned
		dwPtrOffset += dwCount;

		// Ensure include has not already been added
		if(pIncludeFilename && g_pIncludeTable->FindInclude(pIncludeFilename.get())==false)
		{
			// Create Include Table Entry (INCLUDE BLOCK)
			CIncludeTable* pIncludeEntry = new CIncludeTable;
			CStr* pStrName = new CStr(pIncludeFilename.get());
			pIncludeEntry->SetFilename(pStrName);
			pIncludeEntry->SetFirstByte(dwNewDataSize);
			g_pIncludeTable->Add(pIncludeEntry);

			// Construct full include absolute path
			std::unique_ptr<char[]> pMediaRoot(GetProjectMediaRoot());
			CStr absoluteIncludeFile(pMediaRoot.get());
			absoluteIncludeFile.AddText(pIncludeFilename.get());

			// Produce single FileData Block from multiple DBA Files
			LPSTR pData=nullptr;
			DWORD dwDataSize=0;
			if(LoadRaw(absoluteIncludeFile.GetStr(), &pData, &dwDataSize))
			{
				// Add Raw DBA to end of FileData
				CopyData(&pNewData, &dwNewDataSize, pData, dwDataSize);

				// Delete raw data after use
				SAFE_DELETE_ARRAY(pData);
			}
			else
			{
				g_pErrorReport->SetError(0, ERR_SYNTAX+39, pIncludeFilename.get());
				return false;
			}
		}
	}

	// Now erase old root data
	SAFE_DELETE_ARRAY(m_pFileData);

	// Replace Root FileData with New Data
	m_pFileData=pNewData;
	m_FileDataSize=dwNewDataSize;

	// Make snapshot of filedata in original store
	m_dwOriginalFileDataSize = m_FileDataSize;
	m_OriginalFileData.assign(m_dwOriginalFileDataSize, 0);
	memcpy(m_OriginalFileData.data(), m_pFileData, m_FileDataSize);

	// As a later compiler option, dump entire source to debugfile
	if ( 1 )
	{
		// Delete any old file that may exist
		const std::string pFullSourceDump =
			std::string(g_pDBPCompiler->GetInternalFile(PATH_TEMPFOLDER)) + "FullSourceDump.dba";
		if ( FileExists(pFullSourceDump.c_str()) ) DeleteFileW ( TextConvert::UTF8ToUTF16(pFullSourceDump).c_str() );

		// Write Full Source Dump File
		HANDLE hFile = CreateFileW(TextConvert::UTF8ToUTF16(pFullSourceDump).c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if(hFile!=INVALID_HANDLE_VALUE)
		{
			DWORD BytesWritten=0;
			WriteFile(hFile, m_pFileData, m_FileDataSize, &BytesWritten, nullptr);
			SAFE_CLOSE(hFile);
		}
	}

	// Complete
	return true;
}

#define __AARON_UNFOLDPERF__ 1
#define __TOSTRING(x) #x
#define TOSTRING(x) __TOSTRING(x)
#define UNFOLD_MAX_CONSTANT_NAME 2048

void CDBPCompiler::EnsureDataMemBugEnough([[maybe_unused]] LPSTR pPtr, DWORD dwPredictSize, LPSTR* pNewData, DWORD* dwNewDataSize, LPSTR* pWritePtr)
{
	if((*pWritePtr-*pNewData)+dwPredictSize>*dwNewDataSize)
	{
		// Expand new data memory
#ifdef __AARON_UNFOLDPERF__
# define EXPAND_MULTIPLY_AMOUNT 3
#else
# define EXPAND_MULTIPLY_AMOUNT 2
#endif
		DWORD dwBiggerSize = (*dwNewDataSize)*EXPAND_MULTIPLY_AMOUNT;
		LPSTR pBiggerMem = new char[dwBiggerSize + 1]();
		memcpy(pBiggerMem, *pNewData, *dwNewDataSize);
		DWORD dwWriteOffset = static_cast<DWORD>((*pWritePtr)-(*pNewData));
		SAFE_DELETE_ARRAY(*pNewData);
		*pNewData=pBiggerMem;
		*dwNewDataSize=dwBiggerSize;
		*pWritePtr=*pNewData+dwWriteOffset;
	}
}

bool CDBPCompiler::UnfoldFileDataConstants(void)
{
	db3::CProfile<> prof("CDBPCompiler::UnfoldFileDataConstants");

	// Also unfolds REMARK symbols

	// Create Constants Table
	g_pConstantsTable = new CDataTable;

	// Find all constants
	bool bLineIsRem=false;
	bool bBlockIsRem=false;

	DWORD dwIndex=0;
	LPSTR pPtr = m_pFileData;
	LPSTR pPtrEnd = m_pFileData + m_FileDataSize;
	while(pPtr<pPtrEnd)
	{
		// Record remarks
		if(pPtr + 8 <= pPtrEnd && _strnicmp(pPtr, "remstart", 8)==0) bBlockIsRem=true;
		if(pPtr + 6 <= pPtrEnd && _strnicmp(pPtr, "remend", 6)==0) bBlockIsRem=false;
		if(pPtr + 2 <= pPtrEnd && _strnicmp(pPtr, "//", 2)==0) bLineIsRem=false;
		if(bBlockIsRem==true) bLineIsRem=true;
		if(pPtr + 4 <= pPtrEnd && _strnicmp(pPtr, "rem ", 4)==0) bLineIsRem=true;
		if(*pPtr=='`') bLineIsRem=true;

		// Free line comment
		if(*pPtr==13 || *pPtr==10) bLineIsRem=false;

		// Seek #constant token
		if(bLineIsRem==false && bBlockIsRem==false)
		{
			LPSTR pStartByteOfToken=pPtr;
			if(pPtr + 10 <= pPtrEnd && _strnicmp(pPtr, "#constant ", 10)==0)
			{
				pPtr+=10;
				while ( pPtr<pPtrEnd && *(unsigned char*)pPtr<=32 )
					pPtr++;

				// Record root start of constant label+value string 
				LPSTR pStringRootStart = pPtr;

				int iSpeechMark = 0;
				LPSTR pStringStart=pPtr;
				while(pPtr<pPtrEnd)
				{
					if ( *(pPtr)=='"' ) iSpeechMark=1-iSpeechMark;
					if ( iSpeechMark==0 && *(pPtr)==':' )
						break;

					if(*pPtr==10 || *pPtr==13) break;
					pPtr++;
				}

				LPSTR pStringEnd = pPtr;
				LPSTR pPtrScan = pStringRootStart;
				while(pPtrScan<pPtrEnd)
				{
					if(*pPtrScan==10 || *pPtrScan==13 || *pPtrScan=='`') break;
					pPtrScan++;
				}
				if ( pPtrScan < pPtr ) 
				{
					// found comment before carriage return
					pStringEnd = pPtrScan;
				}

				// cut out label and value of constant item
				DWORD dwStringLength = static_cast<DWORD>(pStringEnd-pStringStart);
				if(dwStringLength>0)
				{
					// Prepare CStr
					auto pStrValueOwner = std::make_unique<CStr>(dwStringLength);
					CStr* pStrValue = pStrValueOwner.get();

					pStrValue->CopyFromPtr(pStringStart, pStringStart, dwStringLength);

					// Skip white chars
					pStrValue->EatEdgeSpacesandTabs(nullptr);
					DWORD dwFirstSpaceOrEquate = pStrValue->FindFirstChar(' ');
					if(dwFirstSpaceOrEquate==0) dwFirstSpaceOrEquate = pStrValue->FindFirstChar('=');
					if(dwFirstSpaceOrEquate==0) dwFirstSpaceOrEquate = pStrValue->FindFirstChar(9);

					// if still no space, no second param in constant expression
					if ( dwFirstSpaceOrEquate>0 )
					{
						// Next item is constant name
#ifdef __AARON_UNFOLDPERF__
						char pConstantName[UNFOLD_MAX_CONSTANT_NAME];
						if (dwFirstSpaceOrEquate >= UNFOLD_MAX_CONSTANT_NAME)
						{
							g_pErrorReport->AddErrorString("Constant name exceeds " TOSTRING(UNFOLD_MAX_CONSTANT_NAME) " characters!");
							return false;
						}
#else
						std::unique_ptr<char[]> pConstantNameOwner(new char[dwFirstSpaceOrEquate+1]);
						LPSTR pConstantName = pConstantNameOwner.get();
#endif
						memcpy(pConstantName, pStrValue->GetStr(), dwFirstSpaceOrEquate);
						pConstantName[dwFirstSpaceOrEquate]=0;

						// LEEFIX - 191102 - Validate that it ONLY containts constant character
						CStr validConst(pConstantName);

						if(validConst.IsConstant())
						{
							// Yes, so rest is constant value
							std::unique_ptr<char[]> pConstantValue(pStrValue->GetRightOfPosition(dwFirstSpaceOrEquate));
							pStrValue->SetText(pConstantValue.get());

							// Check if constant is within another string or is a reserved command
							bool bConstantNameWithinAnother = false;
							CDataTable* pCheckConst = g_pConstantsTable->GetNext();
							while(pCheckConst)
							{
								if(pCheckConst->GetString() && pCheckConst->GetString()->GetStr())
								{
									if(dbp::iequals(pConstantName, pCheckConst->GetString()->GetStr()))
									{
										bConstantNameWithinAnother = true;
										break;
									}
								}
								pCheckConst = pCheckConst->GetNext();
							}

							if(!bConstantNameWithinAnother)
							{
								// Add Unique Constant to Table (used later to replace instances of it)
								DWORD dwTry=dwIndex+1;
								if(!g_pConstantsTable->AddTwoStrings(pConstantName, pStrValue->GetStr(), &dwTry))
								{
									g_pErrorReport->SetError(0, ERR_SYNTAX+67, pConstantName);
									return false;
								}
								else
								{
									dwIndex=dwTry;
								}

								// Mark token as a used constant
								*(pStartByteOfToken)='`';
							}
						}
						else
						{
							// Report that it is not valud constant name
							g_pErrorReport->SetError(0, ERR_SYNTAX+48, pConstantName);
							return false;
						}
					}
					else
					{
						// Report that it is not valud constant name
						g_pErrorReport->SetError(0, ERR_SYNTAX+52, pStrValue->GetStr());
						return false;
					}

				}
			}
		}
		if (pPtr < pPtrEnd) pPtr++;
	}

	// Record speech mark flag
	int iSpeechMark=0;

	// Create a New Space to expand into
	DWORD dwNewDataSize = m_FileDataSize + 32768;
	LPSTR pNewData = new char[dwNewDataSize + 1]();
	LPSTR pWritePtr = pNewData;

	// Search Data for instance of constant and replace it with value
	pPtr = m_pFileData;
	pPtrEnd = m_pFileData + m_FileDataSize;
	LPSTR pLastReadPtr = pPtr;
	while(pPtr<pPtrEnd)
	{
		// Search for comment lines to skip
		if ( iSpeechMark==0 )
		{
			DWORD dwFillWithMarks = 0;
			LPSTR pFillStart = nullptr;

			// skip code
			if(pPtr + 8 <= pPtrEnd && _strnicmp(pPtr, "remstart", 8)==0)
			{
				pFillStart = pPtr;
				if(pPtr + 8 == pPtrEnd || (unsigned char)*(pPtr+8)<=32)
				{
					pPtr += 8;
					while(pPtr<pPtrEnd)
					{
						if(pPtr + 6 <= pPtrEnd && _strnicmp(pPtr, "remend", 6)==0)
						{
							pPtr += 6;
							break;
						}
						pPtr++;
					}
				}
				iSpeechMark=0;
				continue;
			}
			else if(*pPtr == '`' || *pPtr == '\'')
			{
				pFillStart = pPtr;
				while(pPtr<pPtrEnd && *pPtr != 10 && *pPtr != 13) pPtr++;
				dwFillWithMarks = static_cast<DWORD>(pPtr - pFillStart);
				iSpeechMark=0;
			}
			else if(pPtr + 2 <= pPtrEnd && _strnicmp(pPtr, "//", 2)==0)
			{
				pFillStart = pPtr;
				while(pPtr<pPtrEnd && *pPtr != 10 && *pPtr != 13) pPtr++;
				dwFillWithMarks = static_cast<DWORD>(pPtr - pFillStart);
				iSpeechMark=0;
			}
			else if(pPtr + 4 <= pPtrEnd && _strnicmp(pPtr, "rem ", 4)==0 && (pPtr == m_pFileData || (unsigned char)*(pPtr-1)<=32))
			{
				pFillStart = pPtr;
				while(pPtr<pPtrEnd && *pPtr != 10 && *pPtr != 13) pPtr++;
				dwFillWithMarks = static_cast<DWORD>(pPtr - pFillStart);
				iSpeechMark=0;
			}

			// blank out remark areas now
			if ( pFillStart && dwFillWithMarks>0 )
				for ( LPSTR pN=pFillStart; pN<pFillStart+dwFillWithMarks && pN<pPtrEnd; pN++ )
					*(pN)=(unsigned char)96; // ` symbol
		}

		// REPLACE ALL CONSTANTS : deal with speech marks, and constants outside them
		if(pPtr < pPtrEnd && *(pPtr)=='"') iSpeechMark=1-iSpeechMark;
		if(iSpeechMark==0 && pPtr < pPtrEnd)
		{
			// Replace all constants with const-values
			CDataTable* pCurrentConst = g_pConstantsTable->GetNext();
			while(pCurrentConst)
			{
				if (pCurrentConst->GetString() && pCurrentConst->GetString()->GetStr())
				{
					LPSTR pToken = pCurrentConst->GetString()->GetStr();
					DWORD dwTokenLength = static_cast<DWORD>(strlen(pToken));

					// Try to match program text with this token
					if(dwTokenLength > 0 && pPtr + dwTokenLength <= pPtrEnd && _strnicmp(pPtr, pToken, dwTokenLength)==0)
					{
						bool bValidConst=true;
						// Left side check
						if (pPtr > m_pFileData)
						{
							unsigned char cLeft = *(unsigned char*)(pPtr - 1);
							if (cLeft == '_' || (cLeft >= 'a' && cLeft <= 'z') || (cLeft >= 'A' && cLeft <= 'Z') || (cLeft >= '0' && cLeft <= '9'))
								bValidConst = false;
						}
						// Right side check
						if (pPtr + dwTokenLength < pPtrEnd)
						{
							unsigned char cRight = *(unsigned char*)(pPtr + dwTokenLength);
							if (cRight == '_' || cRight == '(' || (cRight >= 'a' && cRight <= 'z') || (cRight >= 'A' && cRight <= 'Z') || (cRight >= '0' && cRight <= '9'))
								bValidConst = false;
						}
			
						if(bValidConst==true)
						{
							LPCSTR pTokenValue = pCurrentConst->GetString2() ? pCurrentConst->GetString2()->GetStr() : "";
							DWORD dwTokenValueLength = static_cast<DWORD>(strlen(pTokenValue));

							// Calculate predicted near end of data space
							DWORD dwPredictSize = static_cast<DWORD>(pPtr-pLastReadPtr) + dwTokenValueLength + 32;

							// Ensure new data size is big enough for addition
							EnsureDataMemBugEnough(pPtr, dwPredictSize, &pNewData, &dwNewDataSize, &pWritePtr);

							// Copy upto this point
							DWORD dwWriteSize = static_cast<DWORD>(pPtr-pLastReadPtr);
							memcpy(pWritePtr, pLastReadPtr, dwWriteSize);
							pWritePtr+=dwWriteSize;

							// Copy constant value to new data
							if (dwTokenValueLength > 0)
							{
								memcpy(pWritePtr, pTokenValue, dwTokenValueLength);
								pWritePtr+=dwTokenValueLength;
							}

							// Advance past token
							pPtr+=dwTokenLength;
							pLastReadPtr=pPtr;
						}
					}
				}

				// Get Next Constant
				pCurrentConst=pCurrentConst->GetNext();
			}
		}

		// next byte
		if (pPtr < pPtrEnd) pPtr++;
	}

	// Calculate predicted near end of data space
	DWORD dwPredictSize = static_cast<DWORD>(pPtr-pLastReadPtr) + 32;

	// Ensure new data size is big enough for addition
	EnsureDataMemBugEnough(pPtr, dwPredictSize, &pNewData, &dwNewDataSize, &pWritePtr);

	// Copy upto this point
	DWORD dwWriteSize = static_cast<DWORD>(pPtr-pLastReadPtr);
	memcpy(pWritePtr, pLastReadPtr, dwWriteSize);
	pWritePtr+=dwWriteSize;

	// Assign new data as latest file data for next pass (or final task)
	DWORD dwDataSizeOfNewData = static_cast<DWORD>(pWritePtr-pNewData);
	SAFE_DELETE_ARRAY(m_pFileData);
	m_pFileData=pNewData;
	m_FileDataSize=dwDataSizeOfNewData;

	// Finished with Constants Table
	if (g_pConstantsTable)
	{
		g_pConstantsTable->Free();
		g_pConstantsTable=nullptr;
	}

	// Piggyback replacement of semicolons to colons except where speech marks preceed
	int iSpeechMarks=0;
	pPtr = m_pFileData;
	pPtrEnd = m_pFileData + m_FileDataSize;
	while(pPtr<pPtrEnd)
	{
		if(*pPtr=='"') iSpeechMarks=1-iSpeechMarks;
		if(iSpeechMarks==0)
		{
			// replaces ; with : to reduce compiler errors (c programmer accidents)
			if(*pPtr==';')
			{
				bool bValid=false;
				int iCount=1024;
				LPSTR pAtPtr = pPtr-1;
				while(iCount>0 && pAtPtr>=m_pFileData && *pAtPtr!=10 && *pAtPtr!=13)
				{
					if((pAtPtr + 5 <= pPtrEnd && _strnicmp(pAtPtr,"print",5)==0)
					|| (pAtPtr + 5 <= pPtrEnd && _strnicmp(pAtPtr,"input",5)==0))
					{
						bValid=true;
						break;
					}
					pAtPtr--; iCount--;
				}
				if(bValid==false)
				{
					// Replace is
					*pPtr = ':';
				}
			}

			// allows the c programmers to document their lines easily
			if(*pPtr=='`' && pPtr + 1 < pPtrEnd && *(unsigned char*)(pPtr+1)>=32)
			{
				// Replace then skip so we dont recurse the replacement!
				*(pPtr+0) = ':';
				*(pPtr+1) = '`';

				// leefix - 130306 - u60b3 - ensure speech marks are erased from comment lines
				LPSTR pQuickScanPtr = pPtr + 1;
				while ( pQuickScanPtr<pPtrEnd && *(unsigned char*)(pQuickScanPtr)!=13 && *(unsigned char*)(pQuickScanPtr)!=10 && *(unsigned char*)(pQuickScanPtr)!=0 )
				{
					if ( *(pQuickScanPtr)=='"') *(pQuickScanPtr) = ' ';
					pQuickScanPtr++;
				}

				// continue;
				pPtr++;
			}
		}
		if (pPtr < pPtrEnd) pPtr++;
	}

	// Complete
	return true;
}

bool CDBPCompiler::CopyData(LPSTR* ppData, DWORD* pdwDataSize, LPSTR pAdd, DWORD dwAddSize)
{
	// Local vars
	DWORD dwNewSize = 0;

	// New Size of Data
	dwNewSize = (*pdwDataSize) + dwAddSize + 2;

	// Create New Data Memory
	LPSTR pNewData = new char[dwNewSize]();
	if(pNewData)
	{
		// Copy Current
		if(*ppData && *pdwDataSize>0)
		{
			CopyMemory(pNewData, *ppData, *pdwDataSize);
		}

		// Copy Add Data
		if(pAdd && dwAddSize>0)
		{
			if(*pdwDataSize==0)
			{
				// Initial data block
				CopyMemory(pNewData, pAdd, dwAddSize);
			}
			else
			{
				// Scan back if zeros
				while(*pdwDataSize>0)
				{
					if(*(pNewData+(*pdwDataSize-2))!=0) break;
					*pdwDataSize = *pdwDataSize - 1;
				}

				// Add carriage return
				*(pNewData+(*pdwDataSize)-1)=13;
				*(pNewData+(*pdwDataSize)+0)=10;

				// Add data block
				CopyMemory(pNewData+(*pdwDataSize)+1, pAdd, dwAddSize);
			}
		}

		// Release current data memory
		if(*ppData)
		{
			delete[] *ppData;
			*ppData=nullptr;
		}

		// Reassign pointers
		*ppData = pNewData;
		*pdwDataSize = dwNewSize;
	}
	else
	{
		g_pErrorReport->AddErrorString("Failed to allocate in 'CopyData'");
		return false;
	}

	// Complete
	return true;
}

bool CDBPCompiler::SeekIncludeToken(LPSTR* ppPtr, LPSTR pPtrEnd, DWORD* pdwAdvance, LPSTR* ppIncludeFilename)
{
	int iSpeechMark=0;
	LPSTR pStartPtr = (*ppPtr);
	LPSTR pPtr = pStartPtr;
	while(pPtr<pPtrEnd)
	{
		// Search for #INCLUDE token
		if(pPtr + 9 <= pPtrEnd && _strnicmp(pPtr, "#include ", 9)==0)
		{
			// First char of include token
			LPSTR pFirstChar=pPtr;

			// Skip past include token
			pPtr+=9;

			// Read in include name
			CStr name("");
			while(pPtr<pPtrEnd)
			{
				// Build name string
				name.AddChar(*(pPtr));

				// Leave at end of line
				if(*pPtr==10 || *pPtr==13) break;
				if(*(pPtr)=='"') iSpeechMark=1-iSpeechMark;
				if(iSpeechMark==0) if(*(pPtr)==':') break;

				// Next character
				pPtr++;
			}

			// Blank out reference to INCLUDE token
			if(pPtr < pPtrEnd && *(pPtr)==10 && pPtr > pFirstChar) pPtr--;
			for(LPSTR pP=pFirstChar; pP<pPtr && pP<pPtrEnd; pP++) *(pP)=32;

			// Strip spaces and stuff from name
			name.EatEdgeSpacesandTabs(nullptr);

			// Strip speech marks from name
			name.EatSpeechMarks();

			// Convert any forwards slashes to backwards slashes
			for(DWORD d=0; d<name.Length(); d++)
				if(name.GetChar(d)=='/')
					name.SetChar(d,'\\');

			// Copy name and return 
			*ppIncludeFilename = new char[name.Length()+1];
			snprintf(*ppIncludeFilename, name.Length()+1, "%s", name.GetStr());
			break;
		}

		// Search for comment lines to skip
		if(pPtr + 8 <= pPtrEnd && _strnicmp(pPtr, "remstart", 8)==0)
		{
			if(pPtr + 8 == pPtrEnd || (unsigned char)*(pPtr+8)<=32)
			{
				pPtr += 8;
				// Skip code block until remend
				while(pPtr<pPtrEnd)
				{
					if(pPtr + 6 <= pPtrEnd && _strnicmp(pPtr, "remend", 6)==0)
					{
						pPtr += 6;
						break;
					}
					pPtr++;
				}
				continue;
			}
		}
		else if(*pPtr == '`' || *pPtr == '\'')
		{
			while(pPtr<pPtrEnd && *pPtr != 10 && *pPtr != 13)
			{
				pPtr++;
			}
			continue;
		}
		else if(pPtr + 3 <= pPtrEnd && _strnicmp(pPtr, "rem", 3)==0 && (pPtr + 3 == pPtrEnd || (unsigned char)*(pPtr+3)<=32 || *(pPtr+3)==':'))
		{
			while(pPtr<pPtrEnd && *pPtr != 10 && *pPtr != 13)
			{
				pPtr++;
			}
			continue;
		}

		// Advance pointer
		if(pPtr<pPtrEnd) pPtr++;
	}

	// Calculate advance count
	(*ppPtr)=pPtr;
	(*pdwAdvance)=static_cast<DWORD>(pPtr-pStartPtr);

	// Complete
	return true;
}

bool CDBPCompiler::MakeProgram(void)
{
	db3::CProfile<> prof1("CDBPCompiler::MakeProgram");

	// Make result
	bool bResult=true;

	// Create New Program
	m_pContext = new CompilerContext();
	m_pContext->Initialize();

	// Set Compile Defaults
	g_pASMWriter->SetDefaultCompileFlags ( m_bSafeArrays );

	// Set struct default
	g_pStructTable->SetStructDefaults();

	// Clear last DBM file
	DeleteFileW(TextConvert::UTF8ToUTF16(GetInternalFile(PATH_TEMPDBMFILE)).c_str());
	DeleteFileW(TextConvert::UTF8ToUTF16(GetInternalFile(PATH_TEMPEXBFILE)).c_str());

	// Settings for Executable
	g_pEXE->m_dwInitialDisplayMode=1;
	if(g_pDBPCompiler->GetFullScreenMode()) g_pEXE->m_dwInitialDisplayMode=3;
	if(g_pDBPCompiler->GetDesktopMode()) g_pEXE->m_dwInitialDisplayMode=2;
	if(g_pDBPCompiler->GetFullDesktopMode()) g_pEXE->m_dwInitialDisplayMode=4;
	if(g_pDBPCompiler->GetHiddenMode()) g_pEXE->m_dwInitialDisplayMode=0;

	// Start Display Dimensions
	g_pEXE->m_dwInitialDisplayWidth=g_pDBPCompiler->GetStartDisplayWidth();
	g_pEXE->m_dwInitialDisplayHeight=g_pDBPCompiler->GetStartDisplayHeight();
	g_pEXE->m_dwInitialDisplayDepth=g_pDBPCompiler->GetStartDisplayDepth();

	// Create Appname String
	g_pEXE->m_pInitialAppName=g_pDBPCompiler->GetProjectField("app title");
	if ( g_pEXE->m_pInitialAppName==nullptr )
	{
		// 280203 - default app name required or exe will not have a classname
		DWORD dwLength = static_cast<DWORD>(strlen ( m_pFinalDBASource ));
		std::unique_ptr<char[]> pAppName(new char[dwLength+1]);
		snprintf(pAppName.get(), dwLength+1, "%s", m_pFinalDBASource);
		if ( dwLength>4 ) pAppName [ dwLength-4 ] = 0;
		if ( strlen ( pAppName.get() )==0 )
		{
			// leefix - 230105 - proj with no appname would corrupt
			pAppName.reset(new char[strlen("DB3 Application")+1]);
			snprintf(pAppName.get(), strlen("DB3 Application")+1, "%s", "DB3 Application");
		}
		g_pEXE->m_pInitialAppName = pAppName.release();
	}
	
	// IN Debug Mode, cannot have non-desktop mode
	if(g_DebugInfo.DebugModeOn())
	{
		// Switch fullscreen to desktop mode
		if(g_pEXE->m_dwInitialDisplayMode==3 || g_pEXE->m_dwInitialDisplayMode==4)
		{
			// Debug Fullscreen has taskbar to help with debugger switching
			g_pEXE->m_dwInitialDisplayMode=2;
		}
	}

	// Deposit Runtime Strings Database if flagged
	if(1)
	{
		// Entire Runtime Database Part of EXE
		g_pEXE->m_dwNumberOfRuntimeErrorStrings=g_pErrorReport->GetRuntimeErrorStringMax();
		g_pEXE->m_pRuntimeErrorStringsArray = g_pEXE->CreatePtrArray(g_pEXE->m_dwNumberOfRuntimeErrorStrings);
		for(DWORD err=0; err<g_pEXE->m_dwNumberOfRuntimeErrorStrings; err++)
		{
			// Get Data from error runtime string database
			LPSTR pStringData=g_pErrorReport->GetRuntimeErrorString(err);
			
			// Create Dynamic String
			LPSTR pDynamicString=nullptr;
			if(pStringData)
			{
				pDynamicString = new char[strlen(pStringData)+1];
				snprintf(pDynamicString, strlen(pStringData)+1, "%s", pStringData);
			}

			// Copy to EXEData
			g_pEXE->m_pRuntimeErrorStringsArray[err]=(uintptr_t)pDynamicString;
		}
	}
	else
	{
		// No Runtime Database Used
		g_pEXE->m_dwNumberOfRuntimeErrorStrings=0;
		g_pEXE->m_pRuntimeErrorStringsArray=nullptr;
	}

	// Start With Main Program
	bool bParsingMainProgram = true;

	// CLI can make some temp memory used for mini-program parsing
	DWORD dwMiniSize = 0;
	std::unique_ptr<char[]> pMiniData;

	// Skips DBM Data production
	m_bProduceDBMFileOn=true;

	// Parser Loop (does main program, and then any CLI mini-programs)
	bool bBeenInCLI=false;
	bool bContinueParsing=true;
	while(bContinueParsing)
	{
		db3::CProfile<> prof2("CDBPCompiler::MakeProgram() -> Main Loop");

		// Start with parsing in mind
		g_pDBMWriter->SetNewCodeFlag(true);

		// Gather Debug Information on Main Program Only
		g_pStatementList->SetWriteStarted(false);
		if(bParsingMainProgram==true)
		{
			// Load Up Debug Info For Main Program
// leefix - 2450604 - u54 - original program is different size due to CONSTANTS
//			g_DebugInfo.SetProgramSize(m_dwOriginalFileDataSize);
//			g_DebugInfo.SetProgramPtr(m_pOriginalFileData);
			g_DebugInfo.SetProgramSize(m_FileDataSize);
			g_DebugInfo.SetProgramPtr(m_pFileData);
			g_DebugInfo.SetParsingMain(true);

			// Parse Main Program
			db3::CProfile<> prof3("CDBPCompiler::MakeProgram() -> g_pStatementList->MakeStatements()");
			if(!g_pStatementList->MakeStatements(m_pFileData, m_FileDataSize))
			{
				g_pErrorReport->AddErrorString("Failed to 'MakeStatements'");
				bResult=false;
			}
		}
		else
		{
			// Useful flag to some code
			g_DebugInfo.SetParsingMain(false);

			// Load MiniCLI Text from debugger
			pMiniData.reset();
			LPSTR pMiniDataRaw = nullptr;
			CDebuggerInterface::GetDataFromDebugger(51, &pMiniDataRaw, &dwMiniSize);
			pMiniData.reset(pMiniDataRaw);

			// Parse Mini Program
			if(!g_pStatementList->AddMiniStatements(pMiniData.get(), dwMiniSize))
			{
				// Report Error to Debugger
			LPCSTR pData = g_pErrorReport->GetParserErrorString();
				DWORD dwSize = 0;
				if(pData) dwSize = static_cast<DWORD>(strlen(pData));
				CDebuggerInterface::SendDataToDebugger(31, pData, dwSize);

				// Clear miniprogram for empty parse
				g_pDBMWriter->SetNewCodeFlag(false);
				pMiniData.reset();
				bResult=true;
			}

			// So we dont carry error back to editor
			bBeenInCLI=true;
		}

		// If Parse Successful
		if(bResult==true)
		{
			// Verify all variables use valid types
			if(g_pVarTable->VerifyVariableStructures())
			{
				// Produce EXE or DEBUGRUN from Statements
				db3::CProfile<> prof4("CDBPCompiler::MakeProgram() -> g_pDBMWriter->WriteProgramAsEXEOrDEBUG()");
				if(g_pDBMWriter->WriteProgramAsEXEOrDEBUG(GetProgramName(), bParsingMainProgram))
				{
					bResult=true;
				}
				else
				{
					g_pErrorReport->AddErrorString("Failed to 'WriteProgramAs EXEOrDEBUG'");
					bResult=false;
				}
			}
			else
			{
				g_pErrorReport->AddErrorString("Failed to 'VerifyVariableStructures'");
				bResult=false;
			}
		}

		// If Debug Mode Only
		if(bResult==true && g_DebugInfo.DebugModeOn())
		{
			// If BreakForCLI..
			if(g_dwEscapeValueMem!=2)
			{
				// Trigger Mini-Parsing from now on
				bParsingMainProgram=false;
			}
			else
			{
				// Exit Parser-loop
				bContinueParsing=false;
			}
		}
		else
		{
			// Compile only
			bContinueParsing=false;
		}
	}

	// If been in CLI (so editor does not report old errors)
	if(bBeenInCLI==true)
	{
		if (m_pContext) {
			m_pContext->ReplaceErrorReport(new CError);
		} else {
			SAFE_DELETE(g_pErrorReport);
			g_pErrorReport = new CError;
		}
	}

	// Free MiniFileData Mem
	pMiniData.reset();

	// Free The EXE when no more to run
	if (g_pEXE) {
		g_pEXE->FreeUptoDisplay();
		g_pEXE->Free();
	}

	// Free Objects from Statement List First and List itself
	// Clean up environment context
	if (m_pContext)
	{
		m_pContext->Cleanup();
		delete m_pContext;
		m_pContext = nullptr;
	}

	// Complete
	return bResult;
}

bool CDBPCompiler::LoadProjectFile(LPSTR pFilename)
{
	// Release any previous usage
	SAFE_DELETE_ARRAY(m_pProjectFileData);

	// Get last six chars of filename
	DWORD length = static_cast<DWORD>(strlen(pFilename));
	//LPSTR pStrExt = new char[7]; //<-- LOL no
	char pStrExt[7] = { '\0', };
	if(length>6)
	{
		DWORD readn=length-6;
		DWORD n = 0;
		for(n=0; n<6; n++)
		{
			pStrExt[n]=pFilename[readn++];
		}
		pStrExt[n]=0;
	}

	// Check for .DBPRO Extension
	bool bFilenameIsProjectFile=false;
	if(dbp::iequals(pStrExt, ".dbpro")) bFilenameIsProjectFile=true;
	//SAFE_DELETE(pStrExt);

	// Resolve the project directory once; downstream outputs and media must not
	// depend on the process current working directory.
	std::error_code projectPathError;
	std::filesystem::path projectDirectory = std::filesystem::absolute(
		std::filesystem::path(pFilename), projectPathError).parent_path();
	if(projectPathError)
		projectDirectory = std::filesystem::path(pFilename).parent_path();
	std::string projectDirectoryText = projectDirectory.lexically_normal().string();
	if(!projectDirectoryText.empty() &&
		projectDirectoryText.back() != '\\' && projectDirectoryText.back() != '/')
		projectDirectoryText.push_back(std::filesystem::path::preferred_separator);

	m_pRelativePathToProjectFile = std::make_unique<CStr>();
	m_pRelativePathToProjectFile->SetText(&projectDirectoryText[0]);

	m_pAbsolutePathToProjectFile = std::make_unique<CStr>();
	m_pAbsolutePathToProjectFile->SetText(&projectDirectoryText[0]);

	// Read in project file if it is one
	if(bFilenameIsProjectFile==true)
	{
		// Load into memory
		if(LoadRaw(pFilename, &m_pProjectFileData, &m_ProjectFileDataSize))
		{
			// Eat EdgeSpaces
			CStr newStr(m_pProjectFileData);
			newStr.EatEdgeSpacesandTabs(nullptr);
			ZeroMemory(m_pProjectFileData, m_ProjectFileDataSize);
			snprintf(m_pProjectFileData, m_ProjectFileDataSize, "%s", newStr.GetStr());

			// Project File loaded ok
			m_bProjectExists=true;
		}
		else
		{
			g_pErrorReport->AddErrorString("Failed to 'LoadProjectFile'");
			return false;
		}
	}
	else
	{
		// Returns with no project loaded - still valid
		m_pProjectFileData=nullptr;
		m_bProjectExists=false;
		return true;
	}

	// Complete
	return true;
}

bool CDBPCompiler::FreeProjectFile(void)
{
	// Delete DBPCompiler Strings
	m_pCompilerFilename.reset();
	m_pCompilerPathOnly.reset();
	m_pAbsolutePathToProjectFile.reset();
	m_pRelativePathToProjectFile.reset();

	// Delete Project Setting Strings
	SAFE_DELETE_ARRAY(m_pFinalDBASource);
	SAFE_DELETE_ARRAY(m_pEXEFilename);

	// Free Project Settings File Data
	SAFE_DELETE_ARRAY(m_pProjectFileData);

	return true;
}

LPSTR CDBPCompiler::ReplaceTokens(LPSTR pFilename)
{
	CStr newStr("");
	DWORD dwLen = static_cast<DWORD>(strlen(pFilename));
	for(DWORD n=0; n<dwLen; n++)
	{
		if(_strnicmp(pFilename+n, "%temp", 5)==0)
		{
			newStr.AddText(GetInternalFile(PATH_TEMPFOLDER));
			n+=5;
		}
		newStr.AddChar(*(pFilename+n));
	}
	if(newStr.Length()>0)
	{
		delete[] pFilename;
		pFilename = new char[newStr.Length()+1];
		snprintf(pFilename, newStr.Length()+1, "%s", newStr.GetStr());
		pFilename[newStr.Length()]=0;
	}
	return pFilename;
}

bool CDBPCompiler::GetAllProjectFields(LPSTR pFilename)
{
	// Project or direct file
	if(ProjectExists()==true)
	{
		// Read In Project Settings
		m_pFinalDBASource = GetProjectFile("final source");
		m_bSourceIsMMF = GetProjectState("source as mmf");
		m_pEXEFilename = GetProjectFile("executable");
		m_bDebugModeOn = GetProjectState("CLI");

		// Replace any tokens
		m_pEXEFilename = ReplaceTokens(m_pEXEFilename);

		// Ensure its either absolute path or clean fileonly
		if(m_pEXEFilename[1]!=':')
		{
			// Not absolute, make sire its just the exe
			CStr exeOnly("");
			exeOnly.SetText(m_pEXEFilename);
			exeOnly.Reverse();
			DWORD dwPos = exeOnly.FindFirstChar('\\');
			if(dwPos>0) exeOnly.SetChar(dwPos, 0);
			dwPos = exeOnly.FindFirstChar('/');
			if(dwPos>0) exeOnly.SetChar(dwPos, 0);
			exeOnly.Reverse();
			SAFE_DELETE_ARRAY(m_pEXEFilename);
			m_pEXEFilename = new char[exeOnly.Length()+1];
			snprintf(m_pEXEFilename, exeOnly.Length()+1, "%s", exeOnly.GetStr());
		}

		// Compiler Display Settings
		m_bFullScreenModeOn = GetProjectStateMatch("graphics mode", "fullscreen");
		m_bDesktopModeOn = GetProjectStateMatch("graphics mode", "desktop");
		m_bFullDesktopModeOn = GetProjectStateMatch("graphics mode", "fulldesktop");
		m_bHiddenModeOn = GetProjectStateMatch("graphics mode", "hidden");
		if ( m_bFullScreenModeOn )
		{
			m_dwStartDisplayWidth = GetProjectDisplayInfo("fullscreen resolution", 1);
			m_dwStartDisplayHeight = GetProjectDisplayInfo("fullscreen resolution", 2);
			m_dwStartDisplayDepth = GetProjectDisplayInfo("fullscreen resolution", 3);
		}
		else
		{
			m_dwStartDisplayWidth = GetProjectDisplayInfo("window resolution", 1);
			m_dwStartDisplayHeight = GetProjectDisplayInfo("window resolution", 2);
			m_dwStartDisplayDepth = GetProjectDisplayInfo("window resolution", 3);
		}

		// Compiler settings that canot be changed externally
		m_bGenerateHelpTxtOn = GetProjectState("generatehelptxt");
		#ifdef MAKEHELPTXT
		// lee - 210406 - can switch this to true to generate new HELPTXT in plugins folder
		m_bGenerateHelpTxtOn = true;
		#endif

		// Compiler General Settings
		m_bRuntimeErrorsOn = GetProjectState("runtimeerrors", true);
		m_bInternalMediaState = GetProjectStateMatch("build type", "media");
		m_bEXEInstallerState = GetProjectStateMatch("build type", "installer");
		m_bEXEAloneState = GetProjectStateMatch("build type", "alone");
		m_bCompressPCKState = GetProjectState("compression"); 
		if(m_bEXEInstallerState)
		{
			// Installer assumes media addition and no safecast pure-exe-stub
			// and also cannot compress installer executables too
			m_bInternalMediaState=true;
			m_bCompressPCKState=false;
			m_bEXEAloneState=false;
		}
		m_bEncryptionState = GetProjectState("encryption"); 
		if(m_bInternalMediaState==false) m_bEncryptionState=false;
		m_bSpeedOverStabilityState = GetProjectState("speed over stability"); 
	}
	else
	{
		// Use filename as DBA Source
		m_bSourceIsMMF=false;
		DWORD length = static_cast<DWORD>(strlen(pFilename));
		m_pFinalDBASource = new char[length+1];
		snprintf(m_pFinalDBASource, length+1, "%s", pFilename);
		m_pEXEFilename = new char[_MAX_PATH];
		snprintf(m_pEXEFilename, _MAX_PATH, "%s", "default.exe");
		m_bDebugModeOn=false;
		m_bRuntimeErrorsOn=true;
		m_bProduceDBMFileOn=true;
		m_bFullScreenModeOn=false;
		m_bFullDesktopModeOn=true;
		m_bDesktopModeOn=false;
		m_dwStartDisplayWidth=640;
		m_dwStartDisplayHeight=480;
		m_dwStartDisplayDepth=32;
		m_bHiddenModeOn=false;
		m_bEXEAloneState=false;
		m_bEXEInstallerState=false; 
		m_bCompressPCKState=false;
		m_bInternalMediaState=false;
		m_bEncryptionState=false;
		m_bSpeedOverStabilityState=false;
		m_bGenerateHelpTxtOn=false;
		#ifdef MAKEHELPTXT
		// lee - 210406 - can switch this to true to generate new HELPTXT in plugins folder
		m_bGenerateHelpTxtOn = true;
		#endif
	}
	return true;
}

bool CDBPCompiler::GetProjectState(LPCSTR pFieldName, bool bDefault)
{
	bool bState=bDefault;
	std::unique_ptr<char[]> pState(GetProjectField(pFieldName));
	if(pState)
	{
		if(dbp::iequals(pState.get(),"yes"))
			bState=true;
		else
			bState=false;
	}
	return bState;
}

bool CDBPCompiler::GetProjectState(LPCSTR pFieldName)
{
	return GetProjectState(pFieldName, false);
}

bool CDBPCompiler::GetProjectStateMatch(LPCSTR pFieldName, LPCSTR pCompareStr)
{
	bool bState=false;
	std::unique_ptr<char[]> pState(GetProjectField(pFieldName));
	if(pState)
	{
		if(dbp::iequals(pState.get(),pCompareStr))
			bState=true;
		else
			bState=false;
	}
	return bState;
}

DWORD CDBPCompiler::GetProjectDisplayInfo(LPCSTR pFieldName, DWORD dwDisplayItem)
{
	DWORD dwDisplayData=0;
	std::unique_ptr<char[]> pStateOwner(GetProjectField(pFieldName));
	LPSTR pState = pStateOwner.get();
	if(pState)
	{
		// U75 - 260210 - old editor used 0,0 for window resolution
		// whereas new editor uses 0x0, so handle both
		bool bOldStyleResolution = false;
		for ( int n=0; n<(int)strlen(pState); n++)
			if ( pState[n]==',' )
				bOldStyleResolution = true;

		if(dwDisplayItem==1 || dwDisplayItem==2)
		{
			// U75 - 260210 - old editor or new editor
			DWORD w = 0, h = 0;
			if ( bOldStyleResolution==true )
				sscanf_s(pState, "%lu,%lu", &w, &h);
			else
				sscanf_s(pState, "%lux%lu", &w, &h);
			if(dwDisplayItem==1) dwDisplayData=w;
			if(dwDisplayItem==2) dwDisplayData=h;
		}
		else
		{
			// U75 - 260210 - old editor or new editor
			DWORD w, h;
			char depth[32];
			if ( bOldStyleResolution==true )
				sscanf_s(pState, "%lu,%lu,%31s", &w, &h, depth, static_cast<unsigned int>(_countof(depth)));
			else
				sscanf_s(pState, "%lux%lux%31s", &w, &h, depth, static_cast<unsigned int>(_countof(depth)));
			if(dbp::iequals(depth, "16")) dwDisplayData=16;
			if(dbp::iequals(depth, "16M")) dwDisplayData=32;
		}
	}
	if(dwDisplayData==0)
	{
		if(dwDisplayItem==1) dwDisplayData=640;
		if(dwDisplayItem==2) dwDisplayData=480;
		if(dwDisplayItem==3) dwDisplayData=32;
	}
	return dwDisplayData;
}

LPSTR CDBPCompiler::GetProjectFile(LPCSTR pFieldName)
{
	// Find filename
	std::unique_ptr<char[]> pFileOnly(GetProjectField(pFieldName));

	// Create Full Path and File
	CStr TempStr;
	TempStr.SetText(m_pRelativePathToProjectFile->GetStr());
	TempStr.AddText(pFileOnly.get());

	// Create new string
	LPSTR pFullFile = new char[TempStr.Length()+1];
	snprintf(pFullFile, TempStr.Length()+1, "%s", TempStr.GetStr());

	// Return new string
	return pFullFile;
}

LPSTR CDBPCompiler::GetProjectMediaRoot(void)
{
	LPSTR pConfiguredRoot = GetProjectField("media root path");
	std::unique_ptr<char[]> pConfiguredRootOwner(pConfiguredRoot);
	std::filesystem::path root;
	if(pConfiguredRoot && pConfiguredRoot[0] != 0)
	{
		root = std::filesystem::path(pConfiguredRoot);
		if(root.is_relative())
			root = std::filesystem::path(m_pRelativePathToProjectFile->GetStr()) / root;
	}
	else
	{
		root = std::filesystem::path(m_pRelativePathToProjectFile->GetStr());
	}

	std::string text = root.lexically_normal().string();
	if(!text.empty() && text.back() != '\\' && text.back() != '/')
		text.push_back(std::filesystem::path::preferred_separator);
	LPSTR pResult = new char[text.size() + 1];
	snprintf(pResult, text.size()+1, "%s", text.c_str());
	return pResult;
}

LPSTR CDBPCompiler::GetProjectField(LPCSTR pFieldName)
{
	if (m_pProjectFileData == nullptr || m_ProjectFileDataSize == 0 ||
		pFieldName == nullptr || pFieldName[0] == '\0')
	{
		return nullptr;
	}

	const std::string_view contents{
		m_pProjectFileData, static_cast<std::size_t>(m_ProjectFileDataSize)};
	const std::string_view requestedField{pFieldName};
	const auto trim = [](std::string_view value) noexcept
	{
		const auto first = value.find_first_not_of(" \t");
		if (first == std::string_view::npos)
		{
			return std::string_view{};
		}
		const auto last = value.find_last_not_of(" \t");
		return value.substr(first, last - first + 1);
	};

	std::size_t lineStart = 0;
	while (lineStart < contents.size())
	{
		const auto lineEnd = contents.find_first_of("\r\n", lineStart);
		const auto length = lineEnd == std::string_view::npos
			? contents.size() - lineStart
			: lineEnd - lineStart;
		const auto line = trim(contents.substr(lineStart, length));

		if (!line.empty() && line.front() != ';')
		{
			const auto equals = line.find('=');
			if (equals != std::string_view::npos)
			{
				const auto field = trim(line.substr(0, equals));
				if (field.size() == requestedField.size() &&
					_strnicmp(field.data(), requestedField.data(), field.size()) == 0)
				{
					const auto value = trim(line.substr(equals + 1));
					if (value.empty())
					{
						return nullptr;
					}
					auto result = std::make_unique<char[]>(value.size() + 1);
					std::copy(value.begin(), value.end(), result.get());
					result[value.size()] = '\0';
					return result.release();
				}
			}
		}

		if (lineEnd == std::string_view::npos)
		{
			break;
		}
		lineStart = lineEnd + 1;
		if (lineStart < contents.size() && contents[lineEnd] == '\r' &&
			contents[lineStart] == '\n')
		{
			++lineStart;
		}
	}
	return nullptr;
}

void CDBPCompiler::SetInternalFile(DWORD dwFileID, const char* pFilename)
{
	if(!m_pInternalFile[dwFileID]) m_pInternalFile[dwFileID] = std::unique_ptr<CStr>(new CStr(""));
	m_pInternalFile[dwFileID]->SetText(pFilename);
}

LPSTR CDBPCompiler::GetInternalFile(DWORD dwFileID)
{
	if(m_pInternalFile[dwFileID])
		return m_pInternalFile[dwFileID]->GetStr();
	else
		return nullptr;
}

bool CDBPCompiler::FileExists(const char* pFilename)
{
	HANDLE hFile = CreateFileW(TextConvert::UTF8ToUTF16(pFilename).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if(hFile!=INVALID_HANDLE_VALUE)
	{
		// Close File
		SAFE_CLOSE(hFile);
		return true;
	}
	// soft fail
	return false;
}

bool CDBPCompiler::PathExists(LPCSTR pPath)
{
	if (!pPath || pPath[0] == '\0')
		return false;

	std::error_code ec;
	return std::filesystem::exists(pPath, ec) && std::filesystem::is_directory(pPath, ec);
}

void CDBPCompiler::GatherAllExternalWords(LPSTR pWordsFile)
{
	// Read All External Words From Words File
	int id=0;
	while(id<EXTWORDSMAX)
	{
		// create id
		char idstr[8];
		snprintf(idstr, sizeof(idstr), "%d", id);

		// get word from id
		GetPrivateProfileString("WORDS", idstr, "", m_pWord[id], _MAX_PATH, pWordsFile);

		// next word
		id++;
	}
}

LPSTR CDBPCompiler::GetWord ( int iID )
{
	return m_pWord [ iID ];
}

bool CDBPCompiler::EstablishRequiredBaseFiles(void)
{
	// Resolve the runtime before command discovery so the instruction table and
	// the executable packager consume one coherent DLL bundle.
	if(!ValidateRuntimeBundle(0))
		return false;
	const auto* runtimeBundle = GetResolvedRuntimeBundle();
	if(runtimeBundle == nullptr)
		return false;

	// Temp Strings
	static char missing[8192];
	char path[_MAX_PATH];
	char textfiles[_MAX_PATH];
	bool filesAreMissing = false;
#define CHECK_MISSING_FILEPATH(which)\
	if ((which==0 && !FileExists(path)) || (which==1 && !PathExists(path)))\
	{\
		if(filesAreMissing) strcat_s(missing, ", ");\
		strcat_s(missing, path);\
		filesAreMissing=true;\
	}
#define CHECK_MISSING_FILE() CHECK_MISSING_FILEPATH(0)
#define CHECK_MISSING_PATH() CHECK_MISSING_FILEPATH(1)

	strcpy_s(missing, "Some files appear to be missing from your installation: ");

	// Root Folder to Compiler (not necessarily current dir)
	SetInternalFile(PATH_ROOTPATH, m_pCompilerPathOnly->GetStr());

	// All SETUP.INI reads share one computed settings path
	const std::string setupIniPath = std::string(GetInternalFile(PATH_ROOTPATH)) + "SETUP.INI";
	SetInternalFile(PATH_SETUPFILE, setupIniPath.c_str());
	strcpy_s(path, sizeof(path), setupIniPath.c_str());
	CHECK_MISSING_FILE();

	// Get Path to Language Folder
	GetPrivateProfileString("SETTINGS", "TEXTLANGUAGE", "ENGLISH", textfiles, 256, setupIniPath.c_str());
	_strupr_s(textfiles, sizeof(textfiles));
	SetInternalFile(
		PATH_ERRORSFILE,
		(std::string(GetInternalFile(PATH_ROOTPATH)) + "LANG\\" + textfiles + "\\ERRORS.TXT").c_str());
	strcpy_s(path, sizeof(path), GetInternalFile(PATH_ERRORSFILE));
	CHECK_MISSING_FILE();

	// Get Path to WORDS File
	GetPrivateProfileString("SETTINGS", "TEXTLANGUAGE", "ENGLISH", textfiles, 256, setupIniPath.c_str());
	_strupr_s(textfiles, sizeof(textfiles));
	SetInternalFile(
		PATH_WORDSFILE,
		(std::string(GetInternalFile(PATH_ROOTPATH)) + "LANG\\" + textfiles + "\\WORDS.TXT").c_str());
	strcpy_s(path, sizeof(path), GetInternalFile(PATH_WORDSFILE));

	// GATHER ALL EXTERNAL WORDS
	GatherAllExternalWords(path);

	// Get All Exclusion Names (DLLs we shall not include)
	g_dwExcludeFilesCount=0;
	for ( int i=0; i<MAX_EXCLUSIONS; i++) g_ExcludeFiles[i].clear();
	for ( int i=1; i<MAX_EXCLUSIONS; i++)
	{
		char pExDLL[256];
		snprintf(pExDLL, sizeof(pExDLL), "exdll%d", i);
		GetPrivateProfileString("EXCLUSIONS", pExDLL, "", textfiles, 256, setupIniPath.c_str());
		if ( strcmp ( textfiles, "" )!=0 )
		{
			g_ExcludeFiles [ i ] = textfiles;
			g_dwExcludeFilesCount = 1+i;
		}
		else
			break;
	}

	//
	//	TODO: Move this part to somewhere more appropriate...
	//

	// Get multi-threading options
	GetPrivateProfileString("MULTITHREADING", "ThreadCount", "0", textfiles, sizeof(textfiles), setupIniPath.c_str());
	const db3::uint threadCount =
		static_cast<db3::uint>(atoi(textfiles));

	// === TESTING ===
#if 0
	// indicate the number of threads used
	sprintf_s(textfiles, "%u", static_cast<unsigned int>(m_WorkQueue.GetThreadCount()));
	MessageBoxW(0, TextConvert::UTF8ToUTF16(textfiles).c_str(), L"Thread Count", 64);

	// provide some work
	db3::CSignal sig;
	const char *items[] = { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P" };
	void(*func)(const char *) = [](const char *text)->void{MessageBoxW(0, TextConvert::UTF8ToUTF16(text).c_str(), L"Worker", 64);};
	for(db3::uint i=0; i<sizeof(items)/sizeof(items[0]); i++)
	{
		m_WorkQueue.Enqueue(func, items[i], &sig);
	}
	sig.Sync();
	//m_WorkQueue.Sync();
	//m_WorkQueue.Enqueue([&](char *text)->void{MessageBoxW(0, TextConvert::UTF8ToUTF16(text).c_str(), L"Worker", 64);},textfiles);
#endif

	// Get all directives from the global directives SETUP.INI
	m_bRemoveSafetyCode = false;
	m_bSafeArrays = false;
	g_bLocalTempFolder = false;
	g_bExternaliseDLLS = false;
	GetPrivateProfileString("DIRECTIVES", "RemoveSafetyCode", "no", textfiles, 256, setupIniPath.c_str());
	if ( dbp::iequals( textfiles, "yes" ) ) m_bRemoveSafetyCode = true;
	GetPrivateProfileString("DIRECTIVES", "SafeArrays", "yes", textfiles, 256, setupIniPath.c_str());
	if ( dbp::iequals( textfiles, "yes" ) ) m_bSafeArrays = true;

	// lee - 050406 - u6rc6 - new diretive
	GetPrivateProfileString("DIRECTIVES", "LocalTempFolder", "no", textfiles, 256, setupIniPath.c_str());
	if ( dbp::iequals( textfiles, "yes" ) ) g_bLocalTempFolder = true;
	
	// lee - 270308 - u67 - can externalise all DLLs to make exe smaller and rely on outside DLLs being dropped in
	GetPrivateProfileString("DIRECTIVES", "ExternaliseDLLS", "no", textfiles, 256, setupIniPath.c_str());
	if ( dbp::iequals( textfiles, "yes" ) ) g_bExternaliseDLLS = true;

	// Get Path to Debugger Program
	SetInternalFile(
		PATH_DEBUGGERFILE,
		(std::string(GetInternalFile(PATH_ROOTPATH)) + "\\DBPDebugger.exe").c_str());
	strcpy_s(path, sizeof(path), GetInternalFile(PATH_DEBUGGERFILE));
	CHECK_MISSING_FILE();
	
	// The host installation defines the product command surface. The selected
	// runtime overlays the ABI-sensitive core component only.
	SetInternalFile(
		PATH_PLUGINSFOLDER,
		(std::string(GetInternalFile(PATH_ROOTPATH)) + "plugins\\").c_str());
	strcpy_s(path, sizeof(path), GetInternalFile(PATH_PLUGINSFOLDER));
	CHECK_MISSING_PATH();

	// Get Path to PLUGINS-USER Folder
	SetInternalFile(
		PATH_PLUGINSUSERFOLDER,
		(std::string(GetInternalFile(PATH_ROOTPATH)) + "plugins-user\\").c_str());
	strcpy_s(path, sizeof(path), GetInternalFile(PATH_PLUGINSUSERFOLDER));
	CHECK_MISSING_PATH();

	// Get Path to PLUGINS-LICENSED Folder
	SetInternalFile(
		PATH_PLUGINSLICENSEDFOLDER,
		(std::string(GetInternalFile(PATH_ROOTPATH)) + "plugins-licensed\\").c_str());
	strcpy_s(path, sizeof(path), GetInternalFile(PATH_PLUGINSLICENSEDFOLDER));
	CHECK_MISSING_PATH();

	// Get Path to TEMP Folder
	SetInternalFile(
		PATH_TEMPFOLDER,
		(std::string(GetInternalFile(PATH_ROOTPATH)) + "..\\temp\\").c_str());

	bool bUseUserFolderForTemp = false;
	if(!PathExists(GetInternalFile(PATH_TEMPFOLDER)))
	{
		bUseUserFolderForTemp = true;
	}
	else
	{
		// if DBPRO\TEMP exists, make sure we can WRITE to it, otherwise use USER temp
		std::filesystem::path testFile = std::filesystem::path(GetInternalFile(PATH_TEMPFOLDER)) / "_temp.temp";
		HANDLE hFile = CreateFileW( testFile.wstring().c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr );
		if ( hFile==INVALID_HANDLE_VALUE )
		{
			// no create access to temp - do not use
			bUseUserFolderForTemp = true;
		}
		else
		{
			// remove - successful - temp file valid to use
			CloseHandle( hFile );
			DeleteFileW ( testFile.wstring().c_str() );
		}
	}
	if ( bUseUserFolderForTemp )
	{
		// create new USER TEMP folder
		char appDataPath[MAX_PATH];
		SHGetFolderPathA( nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appDataPath );
		std::filesystem::path userTemp = std::filesystem::path(appDataPath) / "Dark Basic Professional TEMP";
		std::error_code ec;
		std::filesystem::create_directories(userTemp, ec);
		std::string userTempStr = userTemp.string() + "\\";
		SetInternalFile(PATH_TEMPFOLDER, userTempStr.c_str());
	}
	strcpy_s(path, sizeof(path), GetInternalFile(PATH_TEMPFOLDER));
	CHECK_MISSING_PATH();

	// Get Path to DBM File
	SetInternalFile(
		PATH_TEMPDBMFILE,
		(std::string(GetInternalFile(PATH_TEMPFOLDER)) + "_Temp.dbm").c_str());

	// Get Path to DBM File
	SetInternalFile(
		PATH_TEMPEXBFILE,
		(std::string(GetInternalFile(PATH_TEMPFOLDER)) + "_Temp.exb").c_str());

	// Get Path to DBM File
	SetInternalFile(
		PATH_TEMPERRORFILE,
		(std::string(GetInternalFile(PATH_TEMPFOLDER)) + "ErrorReport.txt").c_str());

	// Verify Importanr Files Exist
#if 0
	bool bAllFilesAvailable=true;
	if(FileExists(GetInternalFile(PATH_SETUPFILE))==false) bAllFilesAvailable=false;
	if(FileExists(GetInternalFile(PATH_ERRORSFILE))==false) bAllFilesAvailable=false;
	if(FileExists(GetInternalFile(PATH_DEBUGGERFILE))==false) bAllFilesAvailable=false;
	if(PathExists(GetInternalFile(PATH_PLUGINSFOLDER))==false) bAllFilesAvailable=false;
	if(PathExists(GetInternalFile(PATH_PLUGINSUSERFOLDER))==false) bAllFilesAvailable=false;
	if(PathExists(GetInternalFile(PATH_PLUGINSLICENSEDFOLDER))==false) bAllFilesAvailable=false;
	if(PathExists(GetInternalFile(PATH_TEMPFOLDER))==false) bAllFilesAvailable=false;
#else
	bool bAllFilesAvailable = !filesAreMissing;
#endif
	if(bAllFilesAvailable==false)
	{
		//
		//	TODO: Make this error more descriptive.
		//

		// Not all files could be found
		// Not all internal files exist for compiler
		if(!db3::g_bHeadlessMode)
		{
			MessageBoxW(
				nullptr,
				TextConvert::UTF8ToUTF16(missing).c_str(),
				TextConvert::UTF8ToUTF16(
					g_pDBPCompiler->GetWordString(9)).c_str(),
				MB_OK|MB_ICONERROR);
		}
		return false;
	}

	// Do not start worker threads for an installation that has already failed
	// validation. Besides wasting resources, early initialization made this
	// routine unsafe to exercise repeatedly in tests and tools.
	if (!g_WorkQueue.Init(threadCount))
	{
		if(!db3::g_bHeadlessMode)
			MessageBoxW(
				GetActiveWindow(),
				L"Failed to initialize work queue",
				L"Error",
				MB_ICONERROR|MB_OK);
		return false;
	}

	// Complete
	return true;
}

bool CDBPCompiler::RemoveAndRecordBreakpoints(void)
{
	db3::CProfile<> prof("CDBPCompiler::RemoveAndRecordBreakpoints");

	// Temp Vars
	DWORD dwLine=1;
	DWORD dwSpeechMarks=0;

	// Clear BP list
	ClearBreakPointList();

	// Scan entire filedata, and record/remove all ! symbols
	LPSTR pPtr = m_pFileData;
	LPSTR pEnd = m_pFileData+m_FileDataSize;
	while(pPtr<pEnd)
	{
		if(*pPtr=='"') dwSpeechMarks=1-dwSpeechMarks;
		if(dwSpeechMarks==0)
		{
			if(*pPtr=='!')
			{
				// leefix-060803-before remove, check if left-hand neighbor is carriage return or program start
				bool bMustBeAtStartOfLine=false;
				if ( pPtr==m_pFileData )
				{
					// leefix-090903-step-through-bug - ! at first char no longer causes bug
					bMustBeAtStartOfLine=true;
				}
				else
				{
					for ( LPSTR pCheckPtr=pPtr-1; pCheckPtr>=m_pFileData; pCheckPtr-- )
					{
						if ( *pCheckPtr==10 || *pCheckPtr==13 || pCheckPtr==m_pFileData )
						{
							bMustBeAtStartOfLine=true;
							break;
						}
						if ( *pCheckPtr!=9 && *pCheckPtr!=32 )
							break;
					}
				}

				// only if start of line
				if ( bMustBeAtStartOfLine )
				{
					// remove symbol
					*pPtr=32;

					// record line number in breakpint list
					AddToBreakPointList(dwLine);
				}
			}
		}
		if(pPtr>m_pFileData && *(pPtr-1)==13 && *pPtr==10) dwLine++;
		pPtr++;
	}

	// Complete List 
	FinishBreakPointList();

	// Complete
	return true;
}

bool CDBPCompiler::ClearBreakPointList(void)
{
	m_BreakpointList.assign(m_FileDataSize+1, 0);
	m_dwBreakpointSize = m_FileDataSize;
	m_dwBreakpointIndex = 0;
	return true;
}

bool CDBPCompiler::AddToBreakPointList(DWORD dwLine)
{
	if(m_dwBreakpointIndex<m_dwBreakpointSize)
	{
		bool bLineIsUnique=true;
		if(m_dwBreakpointIndex>0)
			if(m_BreakpointList[m_dwBreakpointIndex-1]==dwLine) bLineIsUnique=false;

		if(bLineIsUnique)
		{
			m_BreakpointList[m_dwBreakpointIndex]=dwLine;
			m_dwBreakpointIndex++;
		}
		return true;
	}
	else
		return false;
}

bool CDBPCompiler::FinishBreakPointList(void)
{
	m_dwBreakpointMax = m_dwBreakpointIndex;
	m_dwBreakpointIndex = 0;
	return true;
}
void CDBPCompiler::SetExecutableOutputOverride(
	std::optional<std::filesystem::path> outputPath)
{
	if(outputPath && !outputPath->is_absolute())
		outputPath = std::filesystem::absolute(*outputPath);
	if(outputPath)
		outputPath = outputPath->lexically_normal();
	m_executableOutputOverride = std::move(outputPath);
	m_executableOutputOverrideText = m_executableOutputOverride
		? m_executableOutputOverride->string()
		: std::string();
}

LPSTR CDBPCompiler::GetProgramName(void)
{
	return m_executableOutputOverride
		? const_cast<LPSTR>(m_executableOutputOverrideText.c_str())
		: m_pEXEFilename;
}

bool CDBPCompiler::PrepareExecutableOutputDirectory(void) const
{
	if(!m_executableOutputOverride)
		return true;
	const auto parent = m_executableOutputOverride->parent_path();
	if(parent.empty())
		return true;
	std::error_code error;
	std::filesystem::create_directories(parent, error);
	return !error && std::filesystem::is_directory(parent, error) && !error;
}

void CDBPCompiler::SetRuntimeRootOverride(
	std::optional<std::filesystem::path> runtimeRoot)
{
	m_runtimeRootOverride = std::move(runtimeRoot);
	m_resolvedRuntimeBundle.reset();
}

void CDBPCompiler::SetPackageKeyFile(
	std::optional<std::filesystem::path> keyFile)
{
	if(keyFile && !keyFile->is_absolute())
		keyFile = std::filesystem::absolute(*keyFile);
	if(keyFile)
		keyFile = keyFile->lexically_normal();
	m_packageKeyFile = std::move(keyFile);
}

const std::optional<std::filesystem::path>&
CDBPCompiler::GetPackageKeyFile(void) const
{
	return m_packageKeyFile;
}

bool CDBPCompiler::ValidateRuntimeBundle(DWORD structurePatternCount)
{
	const RuntimeSelection selection{
		m_runtimeRootOverride,
		std::filesystem::path(m_pCompilerPathOnly->GetStr())};
	const auto result = RuntimeBundleResolver::Resolve(
		selection,
		DeriveProgramRuntimeRequirements(structurePatternCount));
	if(!result)
	{
		const char* code = "DBP3002";
		if(result.error().code == RuntimeErrorCode::IncompatibleArchitecture) code = "DBP3003";
		if(result.error().code == RuntimeErrorCode::MissingCapability) code = "DBP3004";
		std::string message = std::string(code) + ": " + result.error().message;
		if(!result.error().componentPath.empty())
			message += " Component: " + result.error().componentPath.string();
		g_pErrorReport->AddErrorString(message.data());
		m_resolvedRuntimeBundle.reset();
		return false;
	}
	m_resolvedRuntimeBundle = result.value();
	return true;
}

const ResolvedRuntimeBundle* CDBPCompiler::GetResolvedRuntimeBundle(void) const
{
	return m_resolvedRuntimeBundle ? &*m_resolvedRuntimeBundle : nullptr;
}
