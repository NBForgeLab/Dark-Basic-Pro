#pragma once

// Common Includes
#include <cstdint>
#include <memory>

// Custom Includes
#include "Statement.h"

class CParseInit  
{
	public:
		CParseInit();
		virtual ~CParseInit();

		void			SetLineNumber(uint32_t line) { m_dwLineNumber = line; }
		uint32_t			GetLineNumber(void) { return m_dwLineNumber; }

		void			SetVariableNameMathOp(CMathOp* pNameMath) { m_pMathOp.reset(pNameMath); }
		CMathOp*		GetVariableNameMathOp(void) { return m_pMathOp.get(); }
		void			SetVariableParamList(CParameter* pParam) { m_pDataParamList.reset(pParam); }
		CParameter*		GetParameter(void) { return m_pDataParamList.get(); }

		bool			WriteDBM(void);
		bool			WriteDBMBit(uint32_t dwLineNumber, std::string_view text);
		bool			WriteDBMBit(uint32_t dwLineNumber, const char* pText) {
			return WriteDBMBit(dwLineNumber, pText ? std::string_view(pText) : std::string_view{});
		}

	private:

		// Debug Data
		uint32_t			m_dwLineNumber;

		// Initialisation Data
		std::unique_ptr<CMathOp>	m_pMathOp;
		std::unique_ptr<CParameter>	m_pDataParamList;
};