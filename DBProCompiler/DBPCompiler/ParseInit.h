// ParseInit.h: interface for the CParseInit class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PARSEINIT_H__5AE26014_3544_4337_9662_4B439E57C853__INCLUDED_)
#define AFX_PARSEINIT_H__5AE26014_3544_4337_9662_4B439E57C853__INCLUDED_

// Common Includes
#include "windows.h"
#include <memory>

// Custom Includes
#include "Statement.h"

class CParseInit  
{
	public:
		CParseInit();
		virtual ~CParseInit();

		void			SetLineNumber(DWORD line) { m_dwLineNumber = line; }
		DWORD			GetLineNumber(void) { return m_dwLineNumber; }

		void			SetVariableNameMathOp(CMathOp* pNameMath) { m_pMathOp.reset(pNameMath); }
		CMathOp*		GetVariableNameMathOp(void) { return m_pMathOp.get(); }
		void			SetVariableParamList(CParameter* pParam) { m_pDataParamList.reset(pParam); }
		CParameter*		GetParameter(void) { return m_pDataParamList.get(); }

		bool			WriteDBM(void);
		bool			WriteDBMBit(DWORD dwLineNumber, LPCSTR pText);

	private:

		// Debug Data
		DWORD			m_dwLineNumber;

		// Initialisation Data
		std::unique_ptr<CMathOp>	m_pMathOp;
		std::unique_ptr<CParameter>	m_pDataParamList;
};

#endif // !defined(AFX_PARSEINIT_H__5AE26014_3544_4337_9662_4B439E57C853__INCLUDED_)
