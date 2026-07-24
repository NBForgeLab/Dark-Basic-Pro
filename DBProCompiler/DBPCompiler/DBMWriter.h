// DBMWriter.h: interface for the CDBMWriter class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DBMWRITER_H__C1FF6E3E_45BA_478C_88E2_D2CB3C061575__INCLUDED_)
#define AFX_DBMWRITER_H__C1FF6E3E_45BA_478C_88E2_D2CB3C061575__INCLUDED_

#include "windows.h"
#include "Str.h"
#include <vector>

class CDBMWriter  
{
	public:
		CDBMWriter();
		virtual ~CDBMWriter() = default;

		bool			OutputDBM(const char *pDBMStr, size_t length);
		bool			OutputDBM(CStr* pDBMStr);
		DWORD			EatCarriageReturn(void);
		bool			CheckAndExpandDBMMemory(DWORD dwLengthOfNewAddData);
		bool			WriteProgramAsEXEOrDEBUG(LPSTR lpEXEFilename, bool bParsingMainProgram);

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

#endif // !defined(AFX_DBMWRITER_H__C1FF6E3E_45BA_478C_88E2_D2CB3C061575__INCLUDED_)
