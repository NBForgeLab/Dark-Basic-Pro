#pragma once
#include "ParserHeader.h"
#include <memory>

class CParseFunction  
{
	public:
		CParseFunction();
		virtual ~CParseFunction();

		void				SetParameter(CParameter* pParam) { m_pParameter.reset(pParam); }
		CParameter*			GetParameter(void) { return m_pParameter.get(); }
		void				SetResultString(CStr* pResultString) { m_pResultStringToken.reset(pResultString); }

		void				SetLineNumber(DWORD line) { m_dwLineNumber = line; }
		DWORD				GetLineNumber(void) { return m_dwLineNumber; }

		bool				WriteDBM(void);
		bool				WriteDBMBit(DWORD dwLineNumber);

	private:

		// Debug Data
		DWORD				m_dwLineNumber;

		// Function Data
		std::unique_ptr<CParameter>	m_pParameter;
		std::unique_ptr<CStr>		m_pResultStringToken;
};