#pragma once
#include "ParserHeader.h"
#include <memory>

// Custom Includes
#include "Statement.h"

// Defines
#define LOOPTYPE_DO				1
#define LOOPTYPE_WHILE			2
#define LOOPTYPE_REPEAT			3
#define LOOPTYPE_FORNEXT		4

class CParseLoop  
{
	public:
		CParseLoop();
		virtual ~CParseLoop();

	public:
		void			SetType(DWORD dwType) { m_dwLoopType = dwType; }
		void			SetBlock(CStatement *pStatement) { m_pCodeBlock = pStatement; }
		void			SetConditionParameter(CParameter *pParam) { m_pParameter.reset(pParam); }
		void			SetSLabelParameter(CParameter *pParam) { m_pStartLabelParameter.reset(pParam); }
		void			SetELabelParameter(CParameter *pParam) { m_pEndLabelParameter.reset(pParam); }
		
		void			SetForNextInitParameter(CParameter *pParam) { m_pForNextInitParameter.reset(pParam); }
		void			SetForNextIncParameter(CParameter *pParam) { m_pForNextIncParameter.reset(pParam); }
		void			SetForNextCheckParameter(CParameter *pParam) { m_pForNextCheckParameter.reset(pParam); }

		CStatement*		GetBlock(void) { return m_pCodeBlock; }
		CParameter*		GetConditionParameter(void) { return m_pParameter.get(); }
		CParameter*		GetSLabelParameter(void) { return m_pStartLabelParameter.get(); }
		CParameter*		GetELabelParameter(void) { return m_pEndLabelParameter.get(); }

		CParameter*		GetForNextInitParameter(void) { return m_pForNextInitParameter.get(); }
		CParameter*		GetForNextIncParameter(void) { return m_pForNextIncParameter.get(); }
		CParameter*		GetForNextCheckParameter(void) { return m_pForNextCheckParameter.get(); }

		void			SetStartLineNumber(DWORD line) { m_dwStartLineNumber = line; }
		DWORD			GetStartLineNumber(void) { return m_dwStartLineNumber; }
		void			SetEndLineNumber(DWORD line) { m_dwEndLineNumber = line; }
		DWORD			GetEndLineNumber(void) { return m_dwEndLineNumber; }

		void			PassStartEndCharForPossibleDebugHook(DWORD dwS, DWORD dwE) { m_dwS=dwS, m_dwE=dwE; }

		bool			WriteDBM(DWORD PlacementCode);
		bool			WriteDBMBit(DWORD dwLineNumber, std::string_view text, std::string_view result);
		bool			WriteDBMBit(DWORD dwLineNumber, const char* pText, const char* pResult) {
			return WriteDBMBit(dwLineNumber, pText ? std::string_view(pText) : std::string_view{}, pResult ? std::string_view(pResult) : std::string_view{});
		}

	private:

		// Debug Data
		DWORD			m_dwStartLineNumber;
		DWORD			m_dwEndLineNumber;
		DWORD			m_dwS;
		DWORD			m_dwE;

		// Loop Data
		DWORD			m_dwLoopType;
		CStatement*		m_pCodeBlock;
		std::unique_ptr<CParameter>	m_pParameter;
		std::unique_ptr<CParameter>	m_pStartLabelParameter;
		std::unique_ptr<CParameter>	m_pEndLabelParameter;

		// Additional FORNEXT Data
		std::unique_ptr<CParameter>	m_pMiddleLabelParameter;
		std::unique_ptr<CParameter>	m_pForNextInitParameter;
		std::unique_ptr<CParameter>	m_pForNextIncParameter;
		std::unique_ptr<CParameter>	m_pForNextCheckParameter;
};