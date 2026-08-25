#pragma once
#include "ParserHeader.h"
#include <memory>

// Custom Includes
#include "Statement.h"
#include "Declaration.h"

class CParseUserFunction  
{
	public:
		CParseUserFunction();
		virtual ~CParseUserFunction();

	public:
		void			SetName(CStr *pName) { m_pName.reset(pName); }
		void			SetParamMax(DWORD dwPMax) { m_dwParamMax = dwPMax; }
		void			SetBlock(CStatement *pStatement) { m_pCodeBlock = pStatement; }
		void			SetResultParameter(CParameter *pParam) { m_pParameter.reset(pParam); }
		void			SetDecChainRef(CDeclaration *pDecRef) { m_pDecChainRef = pDecRef; }
		
		CStr*			GetName(void) { return m_pName.get(); }
		DWORD			GetParamMax(void) { return m_dwParamMax; }
		CStatement*		GetBlock(void) { return m_pCodeBlock; }
		CParameter*		GetResultParameter(void) { return m_pParameter.get(); }
		CDeclaration*	GetDecChainRef(void) { return m_pDecChainRef; }

		void			SetStartLineNumber(DWORD line) { m_dwStartLineNumber = line; }
		DWORD			GetStartLineNumber(void) { return m_dwStartLineNumber; }
		void			SetEndLineNumber(DWORD line) { m_dwEndLineNumber = line; }
		DWORD			GetEndLineNumber(void) { return m_dwEndLineNumber; }

		bool			ActOnSingleVar(DWORD dwType, int iDisplacement, DWORD PlacementCode, CStr* pDoNotFree, bool bSpecialRecreate);
		bool			ActOnType(CStr* pTypeName, int iDisplacement, DWORD PlacementCode, CStr* pDoNotFree, bool bSpecialRecreate);
		bool			ActOnLocalVars(DWORD PlacementCode, CStr* pDoNotFree);

		bool			WriteDBM(DWORD PlacementCode);
		bool			WriteDBMBit(DWORD dwLineNumber, std::string_view text, std::string_view result);
		bool			WriteDBMBit(DWORD dwLineNumber, const char* pText, const char* pResult) {
			return WriteDBMBit(dwLineNumber, pText ? std::string_view(pText) : std::string_view{}, pResult ? std::string_view(pResult) : std::string_view{});
		}

	private:

		// Debug Data
		DWORD			m_dwStartLineNumber;
		DWORD			m_dwEndLineNumber;

		// User Function Data
		std::unique_ptr<CStr>	m_pName;
		DWORD			m_dwParamMax;
		CStatement*		m_pCodeBlock;
		std::unique_ptr<CParameter>	m_pParameter;

		// Reference Pointer Only
		CDeclaration*	m_pDecChainRef;
};