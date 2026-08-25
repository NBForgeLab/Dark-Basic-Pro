#pragma once

// Common Includes
#include <cstdint>

// Custom Includes
#include "Statement.h"

class CParseType  
{
	public:
		CParseType();
		virtual ~CParseType();

	public:
		void			SetStartLineNumber(uint32_t line) { m_dwStartLineNumber = line; }
		uint32_t			GetStartLineNumber(void) { return m_dwStartLineNumber; }
		void			SetEndLineNumber(uint32_t line) { m_dwEndLineNumber = line; }
		uint32_t			GetEndLineNumber(void) { return m_dwEndLineNumber; }

		bool			WriteDBM(void);
		bool			WriteDBMBit(uint32_t dwLineNumber, std::string_view text);
		bool			WriteDBMBit(uint32_t dwLineNumber, const char* pText) {
			return WriteDBMBit(dwLineNumber, pText ? std::string_view(pText) : std::string_view{});
		}

	private:

		// Debug Data
		uint32_t			m_dwStartLineNumber;
		uint32_t			m_dwEndLineNumber;
};