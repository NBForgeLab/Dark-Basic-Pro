// Error.h: interface for the CError class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ERROR_H__F0A3CDF6_50BC_4230_92B8_2FA62212053E__INCLUDED_)
#define AFX_ERROR_H__F0A3CDF6_50BC_4230_92B8_2FA62212053E__INCLUDED_

#include <string>
#include <vector>
#include <memory>

// Common Includes
#ifndef DARKEXE
# include "Str.h"
#else
typedef void CStr;
#endif
#include "DB3Task.h"

// Error Value Defines
#define ERR_INTERNAL	0
#define ERR_SYNTAX		100000
#define ERR_COMPILER	200000

#define __UNKNOWN_ERR_STR__ "??" "?"
class CError  
{
	public:
		CError();
		virtual ~CError();

	public:
		bool IsError(void) { return m_bErrorExist; }
		bool IsParserError(void) { return m_bParserErrorExist; }
#ifdef DARKEXE
		LPSTR GetErrorString() { return __UNKNOWN_ERR_STR__; }
		LPSTR GetParserErrorString() { return __UNKNOWN_ERR_STR__; }
#else
		LPSTR GetErrorString(void) { if(m_pErrorString) return m_pErrorString->GetStr(); else return __UNKNOWN_ERR_STR__; }
		LPSTR GetParserErrorString(void) { if(m_pParserErrorString) return m_pParserErrorString->GetStr(); else return __UNKNOWN_ERR_STR__; }
#endif
		void PrepareVerboseErrorHeader(DWORD LineNumber, LPSTR ErrorString);
		void AddErrorString(LPSTR ErrorString);
		void SetParserError(DWORD LineNumber, LPSTR ErrorString);
		void OutputInternalErrorReport(void);

	public:
		DWORD CountDatabaseSubset(LPSTR pSection, LPSTR pErrorFilename);
		void LoadDatabaseSubset(LPSTR pSection, DWORD dwMax, LPSTR pErrorFilename, std::vector<std::string>& outDB);
		void LoadRuntimeDatabaseSubset(LPSTR pSection, DWORD dwMax, LPSTR pErrorFilename, std::vector<std::string>& outDB);
		void LoadErrorDatabase(LPSTR pErrorFilename);

		DWORD GetRuntimeErrorStringMax(void) { return static_cast<DWORD>(m_RuntimeErrors.size()); }
		LPSTR GetRuntimeErrorString(int iIndex) {
			if (iIndex >= 0 && iIndex < (int)m_RuntimeErrors.size() && !m_RuntimeErrors[iIndex].empty())
				return const_cast<LPSTR>(m_RuntimeErrors[iIndex].c_str());
			return nullptr;
		}

	public:
		void GetErrorConstruction(DWORD dwLine, DWORD dwErrCode, CStr** pRawErrorString);
		DWORD GetTokenIndex(CStr* pTokenFieldString);
		std::string CreateAndReword(LPSTR pI);
		void ConstructError(DWORD dwLine, DWORD dwErrCode, LPSTR pA, LPSTR pB, LPSTR pC);
		void SetError(DWORD dwLine, DWORD dwErrCode);
		void SetError(DWORD dwLine, DWORD dwErrCode, DWORD dw1);
		void SetError(DWORD dwLine, DWORD dwErrCode, LPSTR lp1);
		void SetError(DWORD dwLine, DWORD dwErrCode, LPSTR lp1, LPSTR lp2);

	public:
		void ProgressReport(LPSTR lpString, DWORD dwValue);
		DWORD GetPerc(DWORD pPerc) { return (DWORD)((m_dwMaxLines/100.0f)*pPerc); }
		void SetMaxLines(DWORD dwMaxLines) { m_dwMaxLines=dwMaxLines;; }


	private:
		bool						m_bParserErrorExist;
		std::unique_ptr<CStr>		m_pParserErrorString;

	private:
		bool						m_bErrorExist;
		std::unique_ptr<CStr>		m_pErrorString;

	private:
		bool		m_bEstablishedConnectionToMonitor;
		HANDLE		m_hMonitorFileMap;
		LPVOID		m_lpVoidMonitor;
		DWORD		m_dwMaxLines;

	private:
		std::vector<std::string>	m_InternalErrors;
		std::vector<std::string>	m_ParserErrors;
		std::vector<std::string>	m_RuntimeErrors;

	private:
		db3::CLock	m_Lock;
};

extern bool g_bJsonDiagnostics;
std::string EscapeJSON(const std::string& str);
std::vector<std::string> ParseCommandLine(const std::string& cmdLine);
void ReportStatus(const std::string& stage, const std::string& message);

#endif // !defined(AFX_ERROR_H__F0A3CDF6_50BC_4230_92B8_2FA62212053E__INCLUDED_)
