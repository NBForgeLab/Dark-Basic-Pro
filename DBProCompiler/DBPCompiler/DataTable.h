// DataTable.h: interface for the CDataTable class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DATATABLE_H__B0397B91_8123_48F0_878F_AC932878F052__INCLUDED_)
#define AFX_DATATABLE_H__B0397B91_8123_48F0_878F_AC932878F052__INCLUDED_
#include "ParserHeader.h"
#include "Task.h"
#include <memory>

class CDataTable  
{
	public:
		CDataTable();
		CDataTable(LPSTR pInitString);
		~CDataTable();
		void Free(void);
		void Add(CDataTable* pNew);
		CDataTable* GetNext(void) { return m_pNext.get(); }

		bool			AddNumeric(double dNum, DWORD dwIndex);
		bool			AddString(LPSTR pString, DWORD dwIndex);
		bool			AddTwoStrings(LPSTR pString, LPSTR pString2, DWORD* dwIndex);
		bool			AddUniqueString(LPSTR pString, DWORD* dwIndex);
		DWORD			FindString(LPSTR pFindString);
		bool			FindIndexStr(LPSTR pIndexAsString);

		void			SetNumeric(double dNum) { m_dwType=1; m_pNumeric=dNum; }
		void			SetString(CStr* pString) { m_dwType=2; m_pString.reset(pString); }
		void			SetString2(CStr* pString) { m_dwType=2; m_pString2.reset(pString); }
		void			SetAddedToEXEData(bool bState) { m_bAddedToEXEData=bState; }

		void			SetIndex(DWORD dwIndex) { m_dwIndex = dwIndex; }
		DWORD			GetIndex(void) { return m_dwIndex; }

		DWORD			GetType(void) { return m_dwType; }
		double			GetNumeric(void) { return m_pNumeric; }
		CStr*			GetString(void) { return m_pString.get(); }
		CStr*			GetString2(void) { return m_pString2.get(); }
		bool			GetAddedToEXEData(void) { return m_bAddedToEXEData; }

		bool			NotExcluded ( LPSTR pFilename );
		int				CompleteAnyLinkAssociates(void);

		bool			WriteDBMHeader(DWORD dwKindOfTable);
		bool			WriteDBM(void);

	private:

		// Multi-Type Data Item
		DWORD						m_dwIndex;
		DWORD						m_dwType;
		double						m_pNumeric;
		std::unique_ptr<CStr>		m_pString;
		std::unique_ptr<CStr>		m_pString2;
		bool						m_bAddedToEXEData;

		// Hierarchy Data (RAII ownership chain)
		std::unique_ptr<CDataTable>	m_pNext;

		// Safe Access
		db3::CLock			m_Lock;
};

#endif // !defined(AFX_DATATABLE_H__B0397B91_8123_48F0_878F_AC932878F052__INCLUDED_)
