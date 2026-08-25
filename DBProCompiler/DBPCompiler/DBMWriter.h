#pragma once

#include <cstdint>
#include <string_view>
#include "Str.h"
#include <vector>

class CDBMWriter  
{
	public:
		CDBMWriter();
		virtual ~CDBMWriter() = default;

		bool			OutputDBM(const char *pDBMStr, size_t length);
		bool			OutputDBM(std::string_view str) { return OutputDBM(str.data(), str.size()); }
		bool			OutputDBM(CStr* pDBMStr);
		DWORD			EatCarriageReturn(void);
		bool			CheckAndExpandDBMMemory(DWORD dwLengthOfNewAddData);
		bool			WriteProgramAsEXEOrDEBUG(const char* lpEXEFilename, bool bParsingMainProgram);
		bool			WriteProgramAsEXEOrDEBUG(std::string_view exeFilename, bool bParsingMainProgram) {
			std::string fn(exeFilename);
			return WriteProgramAsEXEOrDEBUG(fn.c_str(), bParsingMainProgram);
		}

		void			SetNewCodeFlag(bool bFlag) { m_bNewCodeToParse=bFlag; }
		bool			GetNewCodeFlag(void) { return m_bNewCodeToParse; }

	public:

		void			SetDBMDataPointer(LPSTR pData) { m_dwDBMOffset = static_cast<DWORD>(pData - m_dbmData.data()); }
		LPSTR			GetDBMDataPointer(void) { return m_dbmData.data() + m_dwDBMOffset; }

#ifdef DBP_TESTS_COMPILATION
		void InitializeBufferForTests(DWORD size)
		{
			m_dbmData.assign(size, '\0');
			m_dwDBMOffset = 0;
		}
		DWORD GetUsedBufferSizeForTests(void) const
		{
			return m_dwDBMOffset;
		}
#endif

	private:

		std::vector<char>	m_dbmData;
		DWORD				m_dwDBMOffset;
		bool				m_bNewCodeToParse;
};