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
#include "StringUtils.h"
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
	  m_bEstablishedConnectionToMonitor(false), m_hMonitorFileMap(nullptr),
	  m_lpVoidMonitor(nullptr), m_dwMaxLines(0)
{
	// Establish Connection To A Progress Monitor
	m_hMonitorFileMap = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 256, L"DBPROEDITORMESSAGE");
	if (m_hMonitorFileMap != nullptr && m_hMonitorFileMap != INVALID_HANDLE_VALUE)
	{
		m_lpVoidMonitor = MapViewOfFile(m_hMonitorFileMap, FILE_MAP_WRITE, 0, 0, 256);
		if (m_lpVoidMonitor)
		{
			m_bEstablishedConnectionToMonitor = true;
		}
	}
}

CError::~CError()
{
	// Free Monitor Vars
	if (m_lpVoidMonitor)
	{
		UnmapViewOfFile(m_lpVoidMonitor);
		m_lpVoidMonitor = nullptr;
	}
	if (m_hMonitorFileMap != nullptr && m_hMonitorFileMap != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hMonitorFileMap);
		m_hMonitorFileMap = nullptr;
	}
	// unique_ptr and vector members auto-cleanup via RAII
}

void CError::PrepareVerboseErrorHeader([[maybe_unused]] DWORD LineNumber, LPCSTR ErrorString)
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

void CError::AddErrorString(std::string_view ErrorString)
{
	db3::CAutolock autolock(m_Lock);

	if (m_parserErrorString.empty() && !ErrorString.empty())
	{
		if (g_pStatementList)
		{
			const DWORD dwLineNum = g_pStatementList->GetTokenLineNumber();
			m_parserErrorString = ErrorString;
			if (dwLineNum > 0 && g_pDBPCompiler)
			{
				LPCSTR pUseLineNumber = g_pDBPCompiler->GetWord(11);
				if (pUseLineNumber != nullptr && pUseLineNumber[0] != '\0')
				{
					m_parserErrorString += " ";
					m_parserErrorString += pUseLineNumber;
					m_parserErrorString += " ";
					m_parserErrorString += std::to_string(dwLineNum);
				}
			}
			m_bParserErrorExist = true;

			// Dump Contents of Line to Error Window
			PrepareVerboseErrorHeader(dwLineNum, "UNDEFINED PARSER ERROR");
		}
	}

	m_errorString += ErrorString;
	m_errorString += "\r\n";
	m_bErrorExist = true;
}

void CError::SetParserError(DWORD dwLine, std::string_view ErrorString)
{
	db3::CAutolock autolock(m_Lock);

	if (m_parserErrorString.empty() && !ErrorString.empty())
	{
		m_parserErrorString = ErrorString;
		if (dwLine > 0 && g_pDBPCompiler)
		{
			LPCSTR pUseLineNumber = g_pDBPCompiler->GetWord(11);
			if (pUseLineNumber != nullptr && pUseLineNumber[0] != '\0')
			{
				m_parserErrorString += " ";
				m_parserErrorString += pUseLineNumber;
				m_parserErrorString += " ";
				m_parserErrorString += std::to_string(dwLine);
			}
		}
		m_bParserErrorExist = true;

		// Dump Contents of Line to Error Window
		PrepareVerboseErrorHeader(dwLine, m_parserErrorString.c_str());
	}
}

void CError::OutputInternalErrorReport(void)
{
	// Output Verbose Error Report To File
	LPCSTR lpString = GetErrorString();

	// Deposit in File
	HANDLE hFile = CreateFileW(TextConvert::UTF8ToUTF16(g_pDBPCompiler->GetInternalFile(PATH_TEMPERRORFILE)).c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if(hFile!=INVALID_HANDLE_VALUE)
	{
		DWORD BytesWritten=0;
		DWORD ActualBytesToWrite = static_cast<DWORD>(strlen(lpString));
		WriteFile(hFile, lpString, ActualBytesToWrite, &BytesWritten, nullptr);
		CloseHandle(hFile);
	}
}

// DATABASE ERROR STRING FUNCTIONS

DWORD CError::CountDatabaseSubset(LPCSTR pSection, LPCSTR pErrorFilename)
{
	char label[_MAX_PATH];
	char tempfile[_MAX_PATH];
	DWORD i = 1;
	for(i=1; i<65535; i++)
	{
		snprintf(label, sizeof(label), "%d", i);
		GetPrivateProfileString(pSection, label, "", tempfile, _MAX_PATH, pErrorFilename);
		if(tempfile[0] == '\0') break;
	}
	return i;
}

void CError::LoadDatabaseSubset(LPCSTR pSection, DWORD dwMax, LPCSTR pErrorFilename, std::vector<std::string>& outDB)
{
	// Temp Vars
	char label[_MAX_PATH];
	char tempfile[_MAX_PATH];
	outDB.resize(dwMax);
	for(DWORD i=1; i<dwMax; i++)
	{
		snprintf(label, sizeof(label), "%d", i);
		GetPrivateProfileString(pSection, label, "", tempfile, _MAX_PATH, pErrorFilename);
		if(tempfile[0] != '\0')
		{
			outDB[i] = tempfile;
		}
	}
}

void CError::LoadRuntimeDatabaseSubset(LPCSTR pSection, DWORD dwMax, LPCSTR pErrorFilename, std::vector<std::string>& outDB)
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

		// Get field (key drops the leading '1' of the composed number)
		char work[_MAX_PATH];
		snprintf(work, sizeof(work), "%d", 10000+i);
		snprintf(label, _MAX_PATH, "%s", work+1);
		GetPrivateProfileString(pSection, label, "", tempfile, _MAX_PATH, pErrorFilename);
		if(tempfile[0] != '\0')
		{
			outDB[i] = tempfile;
		}
	}
}

void CError::LoadErrorDatabase(LPCSTR pErrorFilename)
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

void CError::GetErrorConstruction(DWORD dwLine, DWORD dwErrCode, std::string& outRawErrorString)
{
	// Find String from database
	DWORD dwIndex = 0;
	const std::vector<std::string>* pDatabase = nullptr;
	if (dwErrCode >= ERR_INTERNAL && dwErrCode <= ERR_SYNTAX) { pDatabase = &m_InternalErrors; dwIndex = dwErrCode; }
	if (dwErrCode >= ERR_SYNTAX && dwErrCode <= ERR_COMPILER) { pDatabase = &m_ParserErrors; dwIndex = dwErrCode - ERR_SYNTAX; }

	// Use database to return correct error construction line
	if (pDatabase && dwIndex < pDatabase->size() && !(*pDatabase)[dwIndex].empty())
	{
		outRawErrorString = (*pDatabase)[dwIndex];
	}
	else
	{
		if (!m_InternalErrors.empty() && m_InternalErrors.size() > 1 && !m_InternalErrors[1].empty())
			outRawErrorString = m_InternalErrors[1];
		else
			outRawErrorString.clear();
	}

	// Remove AT LINE X. if line is zero
	if (dwLine == 0 && m_InternalErrors.size() > 3 && !m_InternalErrors[2].empty() && !m_InternalErrors[3].empty())
	{
		const std::string& orig = m_InternalErrors[2];
		const std::string& repl = m_InternalErrors[3];
		if (outRawErrorString.size() >= orig.size() &&
		    dbp::ends_with_ci(outRawErrorString, orig))
		{
			outRawErrorString.resize(outRawErrorString.size() - orig.size());
			outRawErrorString += repl;
		}
	}
}

void CError::GetErrorConstruction(DWORD dwLine, DWORD dwErrCode, CStr** pRawErrorString)
{
	if (!pRawErrorString || !*pRawErrorString) return;
	std::string raw;
	GetErrorConstruction(dwLine, dwErrCode, raw);
	(*pRawErrorString)->SetText(raw);
}

DWORD CError::GetTokenIndex(std::string_view tokenField)
{
	if (tokenField.empty()) return 0;
	if (tokenField[0] == 'L' || tokenField[0] == 'l') return 99;
	if (tokenField[0] == '1') return 1;
	if (tokenField[0] == '2') return 2;
	if (tokenField[0] == '3') return 3;
	return 0;
}

DWORD CError::GetTokenIndex(CStr* pTokenFieldString)
{
	if (!pTokenFieldString) return 0;
	return GetTokenIndex(pTokenFieldString->View());
}

std::string CError::CreateAndReword(LPCSTR pI)
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

void CError::ConstructError(DWORD dwLine, DWORD dwErrCode, LPCSTR pIA, LPCSTR pIB, LPCSTR pIC)
{
	// make temp string (and reword if required)
	std::string strA = CreateAndReword(pIA);
	std::string strB = CreateAndReword(pIB);
	std::string strC = CreateAndReword(pIC);
	LPCSTR pA = strA.empty() ? nullptr : strA.c_str();
	LPCSTR pB = strB.empty() ? nullptr : strB.c_str();
	LPCSTR pC = strC.empty() ? nullptr : strC.c_str();

	CIncludeTable* pMustBeWithin = nullptr;

	// CharPos From Line
	DWORD dwCharPosAt = 0;
	if (g_pStatementList) dwCharPosAt = g_pStatementList->GetLastCharInDataPosition();

	// End of Main Program Text
	DWORD dwMainCharPosMax = 0;
	if (g_pIncludeTable && g_pIncludeTable->GetNext())
		dwMainCharPosMax = g_pIncludeTable->GetNext()->GetFirstByte();

	// Line as text
	bool bRemoveAtLine = false;
	std::string strLine;
	if (dwCharPosAt <= dwMainCharPosMax || dwMainCharPosMax == 0)
	{
		// Within main program
		strLine = std::to_string(dwLine);
	}
	else
	{
		// Find which include error is in
		if (g_pIncludeTable)
		{
			pMustBeWithin = g_pIncludeTable->GetNext();
			CIncludeTable* pCurrent = pMustBeWithin ? pMustBeWithin->GetNext() : nullptr;
			while (pCurrent)
			{
				if (dwCharPosAt <= pCurrent->GetFirstByte()) break;
				pMustBeWithin = pCurrent;
				pCurrent = pCurrent->GetNext();
			}

			// State which include program
			if (pMustBeWithin)
			{
				strLine = "inside ";
				if (pMustBeWithin->GetFilename())
					strLine += pMustBeWithin->GetFilename()->str();
				bRemoveAtLine = true;
			}
		}
	}

	// Build String
	std::string strConstruction;
	GetErrorConstruction(dwLine, dwErrCode, strConstruction);

	std::string strWork;
	std::string strToken;
	int iStart = -1;

	for (size_t n = 0; n < strConstruction.size(); ++n)
	{
		char ch = strConstruction[n];
		if (iStart == -1)
		{
			if (ch == '#')
			{
				iStart = static_cast<int>(n);
			}
			else
			{
				strWork.push_back(ch);
			}
		}
		else
		{
			if (ch == '#')
			{
				iStart = -1;
				switch (GetTokenIndex(strToken))
				{
					case 1:  if (pA) strWork += pA; break;
					case 2:  if (pB) strWork += pB; break;
					case 3:  if (pC) strWork += pC; break;
					case 99:
						if (bRemoveAtLine && dbp::ends_with_ci(strWork, "at line "))
						{
							strWork.resize(strWork.size() - 8);
						}
						strWork += strLine;
						break;
				}
				strToken.clear();
			}
			else
			{
				strToken.push_back(ch);
			}
		}
	}

	// Set parser error with latest construction technique using DiagnosticEngine
    std::string filePath = "";
    if (g_pDBPCompiler && g_pDBPCompiler->m_pFinalDBASource) {
        filePath = g_pDBPCompiler->m_pFinalDBASource;
    }
    if (bRemoveAtLine && pMustBeWithin && pMustBeWithin->GetFilename()) {
        filePath = pMustBeWithin->GetFilename()->str();
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
        LPCSTR pData = g_pDBPCompiler->m_pFileData;
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
    std::string errMsg = strWork;
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
    if (m_parserErrorString.empty()) {
        m_parserErrorString = formattedReportClean;
        m_bParserErrorExist = true;
    }

    // Append to accumulated error report for PATH_TEMPERRORFILE
    AddErrorString(formattedReportClean.c_str());
}

void CError::SetError(DWORD dwLine, DWORD dwErrCode)
{
	ConstructError(dwLine, dwErrCode, nullptr, nullptr, nullptr);
}

void CError::SetError(DWORD dwLine, DWORD dwErrCode, DWORD dw1)
{
	CStr pNum1;
	pNum1.SetNumericText(dw1);
	ConstructError(dwLine, dwErrCode, pNum1.GetStr(), nullptr, nullptr);
}

void CError::SetError(DWORD dwLine, DWORD dwErrCode, LPCSTR lp1)
{
	ConstructError(dwLine, dwErrCode, lp1, nullptr, nullptr);
}

void CError::SetError(DWORD dwLine, DWORD dwErrCode, LPCSTR lp1, LPCSTR lp2)
{
	ConstructError(dwLine, dwErrCode, lp1, lp2, nullptr);
}


// PROGRESS FUNCTIONS

void CError::ProgressReport(LPCSTR lpString, DWORD dwValue)
{
	if(dwValue==0)
		return;

	if(m_bEstablishedConnectionToMonitor && m_lpVoidMonitor)
	{
		// Copy to Virtual File
		char pTemp[256];
		snprintf(pTemp, sizeof(pTemp), "%s %d", lpString, dwValue);
		*(DWORD*)m_lpVoidMonitor = dwValue;
		snprintf((LPSTR)m_lpVoidMonitor+4, 252, "%s", pTemp);
	}
	else
	{
		// Find Editor to send to
		// Create Virtual File for Error Transfer
		HANDLE hFileMap = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 256, L"DBPROEDITORMESSAGE");
		if (hFileMap != nullptr && hFileMap != INVALID_HANDLE_VALUE)
		{
			LPVOID lpVoid = MapViewOfFile(hFileMap, FILE_MAP_WRITE, 0, 0, 256);
			if (lpVoid)
			{
				// Copy to Virtual File
				char pTemp[256];
				snprintf(pTemp, sizeof(pTemp), "%s %d", lpString, dwValue);
				*(DWORD*)lpVoid = dwValue;
				snprintf((LPSTR)lpVoid+4, 252, "%s", pTemp);

				// Release virtual file
				UnmapViewOfFile(lpVoid);
			}
			CloseHandle(hFileMap);
		}
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
