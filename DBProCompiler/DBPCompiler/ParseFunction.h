// ParseFunction.h: interface for the CParseFunction class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PARSEFUNCTION_H__AE8F41D3_7DB7_44A6_B7FB_0F7B5DD429E4__INCLUDED_)
#define AFX_PARSEFUNCTION_H__AE8F41D3_7DB7_44A6_B7FB_0F7B5DD429E4__INCLUDED_
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

#endif // !defined(AFX_PARSEFUNCTION_H__AE8F41D3_7DB7_44A6_B7FB_0F7B5DD429E4__INCLUDED_)
