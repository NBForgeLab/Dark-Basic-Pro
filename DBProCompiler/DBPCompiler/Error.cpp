// Error.cpp: implementation of the CError class.
//
//////////////////////////////////////////////////////////////////////

// Common Includes
#include "Error.h"
#include "macros.h"
#include "StatementList.h"
#include "DBPCompiler.h"
#include "IncludeTable.h"
#include "TextConvert.h"
#include "DiagnosticEngine.h"
#include "DBPLogger.h"
#include <iostream>

// External Class Pointer
extern CDBPCompiler* g_pDBPCompiler;
extern CStatementList* g_pStatementList;
extern CIncludeTable* g_pIncludeTable;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CError::CError()
	: m_bErrorExist(false), m_bParserErrorExist(false),
	  m_bEstablishedConnectionToMonitor(false), m_hMonitorFileMap(NULL),
	  m_lpVoidMonitor(NULL), m_dwMaxLines(0)
{
	// Establish Connection To A Progress Monitor
	m_bEstablishedConnectionToMonitor=true;
	m_hMonitorFileMap = CreateFileMappingW((HANDLE)0xFFFFFFFF,NULL,PAGE_READWRITE,0,256,L"DBPROEDITORMESSAGE");
	m_lpVoidMonitor = MapViewOfFile(m_hMonitorFileMap,FILE_MAP_WRITE,0,0,256);
}

CError::~CError()
{
	// Free Monitor Vars
	if(m_bEstablishedConnectionToMonitor)
	{
		// Release virtual file
		UnmapViewOfFile(m_lpVoidMonitor);
		CloseHandle(m_hMonitorFileMap);
	}
	// unique_ptr and vector members auto-cleanup via RAII
}

void CError::PrepareVerboseErrorHeader(DWORD LineNumber, LPSTR ErrorString)
{
	db3::CAutolock autolock(m_Lock);

	// Dump Contents of Line to Error Window
	if(g_pStatementList)
	{
		// Error Report
		AddErrorString("PARSER ERROR");
		AddErrorString("------------");
		AddErrorString(ErrorString);
		AddErrorString("");

		// Error Header
		AddErrorString("PROGRAM TRACE");
		AddErrorString("-------------");

		// Sample Of Line in error
		CStr pStr(65);
		LPSTR pPointer=g_pStatementList->GetFileDataPointer();
		LPSTR pPointerEnd=g_pStatementList->GetFileDataEnd();
		if(pPointer)
		{
			pStr.CopyFromPtr(pPointer, pPointerEnd, 64);
			pStr.SetChar(64,0);
			AddErrorString(pStr.GetStr());
			AddErrorString("");
		}

		AddErrorString("ERROR TRACE");
		AddErrorString("-----------");
	}
}

void CError::AddErrorString(LPSTR ErrorString)
{
	db3::CAutolock autolock(m_Lock);

	if(!m_pParserErrorString)
	{
		if(g_pStatementList)
		{
			const DWORD dwLineNum =
				g_pStatementList->GetTokenLineNumber();
			m_pParserErrorString.reset(new CStr(1));
			m_pParserErrorString->SetText(ErrorString);
			if(dwLineNum>0 && g_pDBPCompiler)
			{
				LPSTR pUseLineNumber = g_pDBPCompiler->GetWord(11);
				if ( strcmp ( pUseLineNumber, "")!=NULL )
				{
					m_pParserErrorString->AddText(" ");
					m_pParserErrorString->AddText(pUseLineNumber);
					m_pParserErrorString->AddText(" ");
					m_pParserErrorString->AddNumericText(dwLineNum);
				}
			}
			m_bParserErrorExist=true;

			// Dump Contents of Line to Error Window
			PrepareVerboseErrorHeader(dwLineNum, "UNDEFINED PARSER ERROR");
		}
	}

	// Calc string sizes
	DWORD oldsize = 0;
	DWORD addsize = strlen(ErrorString);
	DWORD length = addsize;

	// Concat two strings
	if(m_pErrorString)
	{
		oldsize=m_pErrorString->Length();
		length+=oldsize;
	}
	length+=2;
	auto pNewErrorString = std::make_unique<CStr>(length+1);
	if(oldsize>0) memcpy(pNewErrorString->GetStr(), m_pErrorString->GetStr(), oldsize);
	memcpy(pNewErrorString->GetStr()+oldsize, ErrorString, addsize+1);
	*((pNewErrorString->GetStr()+oldsize+addsize)+0)=13;
	*((pNewErrorString->GetStr()+oldsize+addsize)+1)=10;
	*((pNewErrorString->GetStr()+oldsize+addsize)+2)=0;

	// Replace with new
	m_pErrorString = std::move(pNewErrorString);
	m_bErrorExist=true;
}

void CError::SetParserError(DWORD dwLine, LPSTR ErrorString)
{
	db3::CAutolock autolock(m_Lock);

	if(!m_pParserErrorString)
	{
		m_pParserErrorString.reset(new CStr(1));
		m_pParserErrorString->SetText(ErrorString);
		if(dwLine>0 && g_pDBPCompiler)
		{
			LPSTR pUseLineNumber = g_pDBPCompiler->GetWord(11);
			if ( strcmp ( pUseLineNumber, "")!=NULL )
			{
				m_pParserErrorString->AddText(" ");
				m_pParserErrorString->AddText(pUseLineNumber);
				m_pParserErrorString->AddText(" ");
				m_pParserErrorString->AddNumericText(dwLine);
			}
		}
		m_bParserErrorExist=true;

		// Dump Contents of Line to Error Window
		PrepareVerboseErrorHeader(dwLine, ErrorString);
	}
}

void CError::OutputInternalErrorReport(void)
{
	// Output Verbose Error Report To File
	LPSTR lpString = GetErrorString();

	// Deposit in File
	HANDLE hFile = CreateFileW(TextConvert::UTF8ToUTF16(g_pDBPCompiler->GetInternalFile(PATH_TEMPERRORFILE)).c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hFile!=INVALID_HANDLE_VALUE)
	{
		DWORD BytesWritten=0;
		DWORD ActualBytesToWrite=strlen(lpString);
		WriteFile(hFile, lpString, ActualBytesToWrite, &BytesWritten, NULL);
		CloseHandle(hFile);
	}
}

// DATABASE ERROR STRING FUNCTIONS

DWORD CError::CountDatabaseSubset(LPSTR pSection, LPSTR pErrorFilename)
{
	char label[_MAX_PATH];
	char tempfile[_MAX_PATH];
	DWORD i = 1;
	for(i=1; i<65535; i++)
	{
		wsprintf(label, "%d", i);
		GetPrivateProfileString(pSection, label, "", tempfile, _MAX_PATH, pErrorFilename);
		if(strcmp(tempfile,"")==NULL) break;
	}
	return i;
}

void CError::LoadDatabaseSubset(LPSTR pSection, DWORD dwMax, LPSTR pErrorFilename, std::vector<std::string>& outDB)
{
	// Temp Vars
	char label[_MAX_PATH];
	char tempfile[_MAX_PATH];
	outDB.resize(dwMax);
	for(DWORD i=1; i<dwMax; i++)
	{
		wsprintf(label, "%d", i);
		GetPrivateProfileString(pSection, label, "", tempfile, _MAX_PATH, pErrorFilename);
		if(strcmp(tempfile,"")!=0)
		{
			outDB[i] = tempfile;
		}
	}
}

void CError::LoadRuntimeDatabaseSubset(LPSTR pSection, DWORD dwMax, LPSTR pErrorFilename, std::vector<std::string>& outDB)
{
	// Temp Vars
	char label[_MAX_PATH];
	char tempfile[_MAX_PATH];
	outDB.resize(dwMax);
	for(DWORD i=1; i<dwMax; i++)
	{
		// Runtime has large gaps in the numeric sequence (speed things up)
		if(i==150) i=300;
		if(i==350) i=500;
		if(i==550) i=1000;
		if(i==1050) i=1500;
		if(i==1550) i=2000;
		if(i==2050) i=3000;
		if(i==3050) i=3200;
		if(i==3250) i=3500;
		if(i==3550) i=4000;
		if(i==4050) i=4100;
		if(i==4200) i=5000;
		if(i==5200) i=7000;
		if(i==7250) i=7300;
		if(i==7310) i=7600;
		if(i==7650) i=7700;
		if(i==7850) i=7900;
		if(i==7950) i=8000;
		if(i==9010) i=9700;
		if(i==9750) i=9998;

		// Get field
		char work[_MAX_PATH];
		wsprintf(work, "%d", 10000+i);
		strcpy(label, work+1);
		GetPrivateProfileString(pSection, label, "", tempfile, _MAX_PATH, pErrorFilename);
		if(strcmp(tempfile,"")!=0)
		{
			outDB[i] = tempfile;
		}
	}
}

void CError::LoadErrorDatabase(LPSTR pErrorFilename)
{
	// Load Internal Errors
	DWORD dwInternalMax = CountDatabaseSubset("INTERNAL", pErrorFilename);
	LoadDatabaseSubset("INTERNAL", dwInternalMax, pErrorFilename, m_InternalErrors);

	// Load Parser Errors
	DWORD dwParserMax = CountDatabaseSubset("SYNTAX", pErrorFilename);
	LoadDatabaseSubset("SYNTAX", dwParserMax, pErrorFilename, m_ParserErrors);

	// Load Runtime Errors
	DWORD dwRuntimeMax = 9999;
	LoadRuntimeDatabaseSubset("RUNTIME", dwRuntimeMax, pErrorFilename, m_RuntimeErrors);
}

void CError::GetErrorConstruction(DWORD dwLine, DWORD dwErrCode, CStr** pRawErrorString)
{
	// Find String from database
	DWORD dwIndex=0;
	std::vector<std::string>* pDatabase=nullptr;
	if(dwErrCode>=ERR_INTERNAL && dwErrCode<=ERR_SYNTAX) { pDatabase=&m_InternalErrors; dwIndex=dwErrCode; }
	if(dwErrCode>=ERR_SYNTAX && dwErrCode<=ERR_COMPILER) { pDatabase=&m_ParserErrors; dwIndex=dwErrCode-ERR_SYNTAX; }

	// Use database to return correct error construction line
	if(pRawErrorString && pDatabase && dwIndex<pDatabase->size() && !(*pDatabase)[dwIndex].empty())
	{
		// scan with dwErrCode
		(*pRawErrorString)->SetText(const_cast<LPSTR>((*pDatabase)[dwIndex].c_str()));
	}
	else
	{
		if(!m_InternalErrors.empty() && m_InternalErrors.size() > 1 && !m_InternalErrors[1].empty())
			(*pRawErrorString)->SetText(const_cast<LPSTR>(m_InternalErrors[1].c_str()));
		else
			(*pRawErrorString)->SetText("");
	}

	// Remove AT LINE X. if line is zero
	if(dwLine==0 && m_InternalErrors.size() > 3 && !m_InternalErrors[2].empty() && !m_InternalErrors[3].empty())
	{
		LPSTR pOrig = const_cast<LPSTR>(m_InternalErrors[2].c_str());
		LPSTR pRepl = const_cast<LPSTR>(m_InternalErrors[3].c_str());
		CStr* pStr = (*pRawErrorString);
		if(strnicmp((pStr->GetStr()+pStr->Length())-strlen(pOrig), pOrig, strlen(pOrig))==NULL)
		{
			// Remove at line part
			pStr->SetChar(pStr->Length()-strlen(pOrig), 0);
			pStr->AddText(pRepl);
		}
	}
}

DWORD CError::GetTokenIndex(CStr* pTokenFieldString)
{
	// Choose from tokens
	if(pTokenFieldString->CheckChar(0, 'L')) return 99;
	if(pTokenFieldString->CheckChar(0, '1')) return 1;
	if(pTokenFieldString->CheckChar(0, '2')) return 2;
	if(pTokenFieldString->CheckChar(0, '3')) return 3;

	// No tokens matched
	return 0;
}

std::string CError::CreateAndReword(LPSTR pI)
{
	if (!pI) return {};
	if (pI[0]=='@' && pI[1]=='$')
	{
		// rename
		return std::string("TEMP") + (pI+3);
	}
	else if (pI[0]=='@')
	{
		// straight copy +1
		return std::string(pI+1);
	}
	else
	{
		// straight copy
		return std::string(pI);
	}
}

void CError::ConstructError(DWORD dwLine, DWORD dwErrCode, LPSTR pIA, LPSTR pIB, LPSTR pIC)
{
	// make temp string (and reword if required)
	std::string strA = CreateAndReword(pIA);
	std::string strB = CreateAndReword(pIB);
	std::string strC = CreateAndReword(pIC);
	LPSTR pA = strA.empty() ? nullptr : const_cast<LPSTR>(strA.c_str());
	LPSTR pB = strB.empty() ? nullptr : const_cast<LPSTR>(strB.c_str());
	LPSTR pC = strC.empty() ? nullptr : const_cast<LPSTR>(strC.c_str());

	CIncludeTable* pMustBeWithin = NULL;

	// CharPos From Line
	DWORD dwCharPosAt = 0;
	if(g_pStatementList) dwCharPosAt = g_pStatementList->GetLastCharInDataPosition();

	// End of Main Program Text
	DWORD dwMainCharPosMax = 0;
	if(g_pIncludeTable->GetNext())
		dwMainCharPosMax = g_pIncludeTable->GetNext()->GetFirstByte();

	// Line as text
	bool bRemoveAtLine=false;
	CStr pLine("");
	if(dwCharPosAt<=dwMainCharPosMax || dwMainCharPosMax==0)
	{
		// Within main program
		pLine.SetNumericText(dwLine);
	}
	else
	{
		// Find which include error is in
		pMustBeWithin = g_pIncludeTable->GetNext();
		CIncludeTable* pCurrent = pMustBeWithin->GetNext();
		while(pCurrent)
		{
			if(dwCharPosAt<=pCurrent->GetFirstByte()) break;
			pMustBeWithin=pCurrent;
			pCurrent=pCurrent->GetNext();
		}

		// State which include program
		if(pMustBeWithin)
		{
			pLine.SetText("inside ");
			pLine.AddText(pMustBeWithin->GetFilename());
			bRemoveAtLine=true;
		}
	}

	// Build String
	DWORD n=0;
	int iStart=-1;
	DWORD dwTokenID=0;
	CStr pToken("");
	CStr pWorkStr("");
	CStr pConstruction("");
	CStr* pConPtr = &pConstruction;
	GetErrorConstruction(dwLine, dwErrCode, &pConPtr);
	while(n<pConstruction.Length())
	{
		// Find replacement token
		if(iStart==-1)
		{
			if(pConstruction.GetChar(n)=='#')
			{
				// Token start
				iStart=n;
			}
			else
			{
				// Build New Line
				pWorkStr.AddChar(pConstruction.GetChar(n));
			}
		}
		else
		{
			if(pConstruction.GetChar(n)=='#')
			{
				// Token end
				iStart=-1;

				// Use Token to append data to new line
				switch(GetTokenIndex(&pToken))
				{
					case 1 :	pWorkStr.AddText(pA);		break;
					case 2 :	pWorkStr.AddText(pB);		break;
					case 3 :	pWorkStr.AddText(pC);		break;

					case 99 :	// Line Or Include Program
								if(bRemoveAtLine==true)
									if(strnicmp(pWorkStr.GetStr()+pWorkStr.Length()-8, "at line ", 8)==NULL)
										pWorkStr.SetChar(pWorkStr.Length()-8, 0);

								pWorkStr.AddText(&pLine);
								break;
				}

				// Clear Token
				pToken.SetText("");
			}
			else
			{
				// Build Token
				pToken.AddChar(pConstruction.GetChar(n));
			}
		}

		// Next character
		n++;
	}

	// Set parser error with latest construction technique using DiagnosticEngine
    std::string filePath = "";
    if (g_pDBPCompiler && g_pDBPCompiler->m_pFinalDBASource) {
        filePath = g_pDBPCompiler->m_pFinalDBASource;
    }
    if (bRemoveAtLine && pMustBeWithin) {
        filePath = pMustBeWithin->GetFilename()->GetStr();
    }

    SourceLocation loc;
    loc.filePath = filePath;
    loc.line = dwLine;

    // Resolve column from character index in m_pFileData
    if (g_pDBPCompiler && g_pDBPCompiler->m_pFileData) {
        std::string fileContent(g_pDBPCompiler->m_pFileData, g_pDBPCompiler->m_FileDataSize);
        std::string lineContent;
        size_t column = 1;
        DiagnosticEngine::GetLineContext(fileContent, dwCharPosAt, lineContent, column);
        loc.column = column;
    } else {
        loc.column = 1;
    }

    // Determine token length by scanning forward
    loc.length = 1;
    if (g_pDBPCompiler && g_pDBPCompiler->m_pFileData && dwCharPosAt < g_pDBPCompiler->m_FileDataSize) {
        LPSTR pData = g_pDBPCompiler->m_pFileData;
        DWORD dwSize = g_pDBPCompiler->m_FileDataSize;
        DWORD pos = dwCharPosAt;
        if (isalnum((unsigned char)pData[pos]) || pData[pos] == '_' || pData[pos] == '$') {
            while (pos < dwSize && (isalnum((unsigned char)pData[pos]) || pData[pos] == '_' || pData[pos] == '$')) {
                pos++;
            }
            loc.length = pos - dwCharPosAt;
        }
    }
    if (loc.length == 0) loc.length = 1;

    // Determine Help hint
    std::string errMsg = pWorkStr.GetStr();
    std::string hint = "";
    if (errMsg.find("Syntax Error") != std::string::npos || errMsg.find("syntax error") != std::string::npos) {
        hint = "Check for matching brackets, parentheses, or correct operator usage.";
    } else if (errMsg.find("Type Mismatch") != std::string::npos || errMsg.find("type mismatch") != std::string::npos) {
        hint = "Ensure that the value assigned matches the declared type of the variable.";
    } else if (errMsg.find("does not exist") != std::string::npos || errMsg.find("not found") != std::string::npos) {
        hint = "Verify spelling or declare the variable/function before using it.";
    } else {
        hint = "Verify syntax structure or refer to the DarkBasic Pro language reference.";
    }

    // Generate formatted reports
    std::string formattedReportClean = DiagnosticEngine::Format(loc, errMsg, hint, false);
    std::string formattedReportColored = DiagnosticEngine::Format(loc, errMsg, hint, true);

    // Output to stderr and logger
    if (g_bJsonDiagnostics) {
        std::cout << "{\"type\":\"error\",\"file\":\"" << EscapeJSON(filePath)
                  << "\",\"line\":" << dwLine
                  << ",\"column\":" << loc.column
                  << ",\"length\":" << loc.length
                  << ",\"message\":\"" << EscapeJSON(errMsg)
                  << "\",\"hint\":\"" << EscapeJSON(hint)
                  << "\"}\n" << std::flush;
    } else {
        std::cerr << formattedReportColored;
    }
    DBP_ERROR("\n{}", formattedReportClean);

    // Set internal compiler parser error
    if (!m_pParserErrorString) {
        m_pParserErrorString.reset(new CStr(const_cast<LPSTR>(formattedReportClean.c_str())));
        m_bParserErrorExist = true;
    }

    // Append to accumulated error report for PATH_TEMPERRORFILE
    AddErrorString(const_cast<LPSTR>(formattedReportClean.c_str()));
	// Stack-allocated CStr and std::string objects auto-cleanup
}

void CError::SetError(DWORD dwLine, DWORD dwErrCode)
{
	ConstructError(dwLine, dwErrCode, NULL, NULL, NULL);
	DB3_CRASH();
}

void CError::SetError(DWORD dwLine, DWORD dwErrCode, DWORD dw1)
{
	CStr pNum1;
	pNum1.SetNumericText(dw1);
	ConstructError(dwLine, dwErrCode, pNum1.GetStr(), NULL, NULL);
	DB3_CRASH();
}

void CError::SetError(DWORD dwLine, DWORD dwErrCode, LPSTR lp1)
{
	ConstructError(dwLine, dwErrCode, lp1, NULL, NULL);
	DB3_CRASH();
}

void CError::SetError(DWORD dwLine, DWORD dwErrCode, LPSTR lp1, LPSTR lp2)
{
	ConstructError(dwLine, dwErrCode, lp1, lp2, NULL);
	DB3_CRASH();
}


// PROGRESS FUNCTIONS

void CError::ProgressReport(LPSTR lpString, DWORD dwValue)
{
	if(dwValue==0)
		return;

	if(m_bEstablishedConnectionToMonitor)
	{
		// Copy to Virtual File
		char pTemp[256];
		wsprintf(pTemp, "%s %d", lpString, dwValue);
		*(DWORD*)m_lpVoidMonitor = dwValue;
		strcpy((LPSTR)m_lpVoidMonitor+4, pTemp);
	}
	else
	{
		// Find Editor to send to
		// Create Virtual File for Error Transfer
		HANDLE hFileMap = CreateFileMappingW((HANDLE)0xFFFFFFFF,NULL,PAGE_READWRITE,0,256,L"DBPROEDITORMESSAGE");
		LPVOID lpVoid = MapViewOfFile(hFileMap,FILE_MAP_WRITE,0,0,256);

		// Copy to Virtual File
		char pTemp[256];
		sprintf_s(pTemp, 256, "%s %d", lpString, dwValue);
		*(DWORD*)lpVoid = dwValue;
		strcpy((LPSTR)lpVoid+4, pTemp);

		// Release virtual file
		UnmapViewOfFile(lpVoid);
		CloseHandle(hFileMap);
	}
}

bool g_bJsonDiagnostics = false;

std::string EscapeJSON(const std::string& str) {
    std::string res;
    for (char c : str) {
        unsigned char uc = (unsigned char)c;
        if (uc == '\\') {
            res += "\\\\";
        } else if (uc == '"') {
            res += "\\\"";
        } else if (uc == '\n') {
            res += "\\n";
        } else if (uc == '\t') {
            res += "\\t";
        } else if (uc == '\r') {
            res += "\\r";
        } else if (uc < 0x20) {
            char buf[8];
            sprintf_s(buf, "\\u%04x", uc);
            res += buf;
        } else {
            res += c;
        }
    }
    return res;
}

std::vector<std::string> ParseCommandLine(const std::string& cmdLine) {
    std::vector<std::string> args;
    std::string current;
    bool inQuotes = false;
    for (size_t i = 0; i < cmdLine.size(); i++) {
        char c = cmdLine[i];
        if (c == '\"') {
            inQuotes = !inQuotes;
        } else if (c == ' ' && !inQuotes) {
            if (!current.empty()) {
                args.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        args.push_back(current);
    }
    return args;
}

void ReportStatus(const std::string& stage, const std::string& message) {
    if (g_bJsonDiagnostics) {
        std::cout << "{\"type\":\"status\",\"stage\":\"" << EscapeJSON(stage) 
                  << "\",\"message\":\"" << EscapeJSON(message) << "\"}\n" << std::flush;
    } else {
        std::cout << "[" << stage << "] " << message << "\n" << std::flush;
    }
}
