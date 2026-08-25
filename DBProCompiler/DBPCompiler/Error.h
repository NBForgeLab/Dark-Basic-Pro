#pragma once

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
		bool IsError(void) const noexcept { return m_bErrorExist; }
		bool IsParserError(void) const noexcept { return m_bParserErrorExist; }
#ifdef DARKEXE
		const char* GetErrorString() const noexcept { return __UNKNOWN_ERR_STR__; }
		const char* GetParserErrorString() const noexcept { return __UNKNOWN_ERR_STR__; }
#else
		const char* GetErrorString(void) const noexcept { return m_errorString.empty() ? __UNKNOWN_ERR_STR__ : m_errorString.c_str(); }
		const char* GetParserErrorString(void) const noexcept { return m_parserErrorString.empty() ? __UNKNOWN_ERR_STR__ : m_parserErrorString.c_str(); }
		std::string_view GetErrorStringView(void) const noexcept { return m_errorString; }
		std::string_view GetParserErrorStringView(void) const noexcept { return m_parserErrorString; }
#endif
		void PrepareVerboseErrorHeader(DWORD LineNumber, const char* ErrorString);
		void AddErrorString(std::string_view ErrorString);
		void AddErrorString(const char* ErrorString) {
			if (ErrorString) AddErrorString(std::string_view(ErrorString));
		}
		void SetParserError(DWORD LineNumber, std::string_view ErrorString);
		void SetParserError(DWORD LineNumber, const char* ErrorString) {
			if (ErrorString) SetParserError(LineNumber, std::string_view(ErrorString));
		}
		void OutputInternalErrorReport(void);

	public:
		DWORD CountDatabaseSubset(const char* pSection, const char* pErrorFilename);
		void LoadDatabaseSubset(const char* pSection, DWORD dwMax, const char* pErrorFilename, std::vector<std::string>& outDB);
		void LoadRuntimeDatabaseSubset(const char* pSection, DWORD dwMax, const char* pErrorFilename, std::vector<std::string>& outDB);
		void LoadErrorDatabase(const char* pErrorFilename);

		DWORD GetRuntimeErrorStringMax(void) const noexcept { return static_cast<DWORD>(m_RuntimeErrors.size()); }
		const char* GetRuntimeErrorString(int iIndex) const noexcept {
			if (iIndex >= 0 && iIndex < static_cast<int>(m_RuntimeErrors.size()) && !m_RuntimeErrors[iIndex].empty())
				return m_RuntimeErrors[iIndex].c_str();
			return nullptr;
		}

	public:
		void GetErrorConstruction(DWORD dwLine, DWORD dwErrCode, std::string& outRawErrorString);
		void GetErrorConstruction(DWORD dwLine, DWORD dwErrCode, CStr** pRawErrorString);
		DWORD GetTokenIndex(std::string_view tokenField);
		DWORD GetTokenIndex(CStr* pTokenFieldString);
		std::string CreateAndReword(const char* pI);
		void ConstructError(DWORD dwLine, DWORD dwErrCode, const char* pA, const char* pB, const char* pC);
		void SetError(DWORD dwLine, DWORD dwErrCode);
		void SetError(DWORD dwLine, DWORD dwErrCode, DWORD dw1);
		void SetError(DWORD dwLine, DWORD dwErrCode, const char* lp1);
		void SetError(DWORD dwLine, DWORD dwErrCode, const char* lp1, const char* lp2);

	public:
		void ProgressReport(const char* lpString, DWORD dwValue);
		DWORD GetPerc(DWORD pPerc) { return (DWORD)((m_dwMaxLines/100.0f)*pPerc); }
		void SetMaxLines(DWORD dwMaxLines) { m_dwMaxLines=dwMaxLines;; }


	private:
		bool						m_bParserErrorExist;
		std::string					m_parserErrorString;

	private:
		bool						m_bErrorExist;
		std::string					m_errorString;

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