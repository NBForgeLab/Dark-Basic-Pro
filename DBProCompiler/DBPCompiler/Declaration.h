// Declaration.h: interface for the CDeclaration class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DECLARATION_H__105B07AA_795F_45E5_8648_CA2399C29110__INCLUDED_)
#define AFX_DECLARATION_H__105B07AA_795F_45E5_8648_CA2399C29110__INCLUDED_

// Common includes
#include "windows.h"
#include "macros.h"
#include "Str.h"
#include <memory>
#include <string>

class CDeclaration  
{
	public:
		CDeclaration();
		~CDeclaration();
		void Add(CDeclaration* pNew);
		[[nodiscard]] CDeclaration* GetNext(void) noexcept { return m_pNext.get(); }
		[[nodiscard]] const CDeclaration* GetNext(void) const noexcept { return m_pNext.get(); }
		[[nodiscard]] CDeclaration* GetPrev(void) noexcept { return m_pPrev; }
		[[nodiscard]] const CDeclaration* GetPrev(void) const noexcept { return m_pPrev; }
		CDeclaration* Find(LPCSTR pName, DWORD dwArrFlag);

		void SetLineNumber(DWORD dwLine) noexcept { m_dwLineNumber=dwLine; }
		[[nodiscard]] DWORD GetLineNumber(void) const noexcept { return m_dwLineNumber; }

		void SetArr(DWORD dwArr) noexcept { m_dwArr = dwArr; }
		void SetArrValue(CStr* pArrValue) { m_pArrValue.reset(pArrValue); }
		void SetName(CStr* pName) { m_pName.reset(pName); }
		void SetType(CStr* pType) { m_pType.reset(pType); }
		void SetInit(CStr* pInit) { m_pInit.reset(pInit); }
		void SetOffset(DWORD dwOffset) noexcept { m_dwOffset = dwOffset; }
		void SetDataSize(DWORD dwSize) noexcept { m_dwDataSize = dwSize; }
		void SetDecData(DWORD dwDecArr, LPCSTR pDecArrValue, LPCSTR pDecName, LPCSTR pDecType, LPCSTR pDecInit, DWORD LineNumberRef);

		bool GetNumberOfDecsInChain(DWORD* pdwCount);
		std::string GetTypeStringOfDecsInChain(void);

		[[nodiscard]] CStr* GetName(void) noexcept { return m_pName.get(); }
		[[nodiscard]] const CStr* GetName(void) const noexcept { return m_pName.get(); }
		[[nodiscard]] CStr* GetType(void) noexcept { return m_pType.get(); }
		[[nodiscard]] const CStr* GetType(void) const noexcept { return m_pType.get(); }
		[[nodiscard]] CStr* GetArrValue(void) noexcept { return m_pArrValue.get(); }
		[[nodiscard]] const CStr* GetArrValue(void) const noexcept { return m_pArrValue.get(); }
		[[nodiscard]] DWORD GetArrFlag(void) const noexcept { return m_dwArr; }
		[[nodiscard]] DWORD GetOffset(void) const noexcept { return m_dwOffset; }
		[[nodiscard]] DWORD GetDataSize(void) const noexcept { return m_dwDataSize; }

		bool WriteDBM(void);

	private:

		// Debug Data
		DWORD					m_dwLineNumber;

		// Declaration Data
		DWORD					m_dwArr;
		std::unique_ptr<CStr>	m_pArrValue;
		std::unique_ptr<CStr>	m_pName;
		std::unique_ptr<CStr>	m_pType;
		std::unique_ptr<CStr>	m_pInit;
		DWORD					m_dwOffset;
		DWORD					m_dwDataSize;

		// Hierarchy Data
		std::unique_ptr<CDeclaration>	m_pNext;
		CDeclaration*					m_pPrev;  // Non-owning back-pointer
};

#endif // !defined(AFX_DECLARATION_H__105B07AA_795F_45E5_8648_CA2399C29110__INCLUDED_)
