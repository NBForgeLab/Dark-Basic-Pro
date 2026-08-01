// StructTable.h: interface for the CStructTable class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_STRUCTTABLE_H__0EDF6884_E537_492E_806D_71DD644FE9B4__INCLUDED_)
#define AFX_STRUCTTABLE_H__0EDF6884_E537_492E_806D_71DD644FE9B4__INCLUDED_
#include "ParserHeader.h"
#include "TargetABI.h"

#include "PerfMacros.h"

#ifdef __AARON_STRUCPERF__
# include <unordered_map>
# include <string>
#endif

class CStructTable  
{
	public:
		CStructTable();
		virtual ~CStructTable();
		void Free(void);

		void Add(CStructTable* pNew);
		[[nodiscard]] CStructTable* GetNext(void) const noexcept { return m_pNext; }

		void			SetStructDefaults(void);
		template <typename TargetAbi>
		void			SetStructDefaultsFor(void)
		{
			static_assert(
				TargetAbi::address_size == 4 || TargetAbi::address_size == 8,
				"Only PE32 and PE32+ target address widths are supported.");
			SetStructDefaults(static_cast<DWORD>(TargetAbi::address_size));
		}
		[[nodiscard]] DWORD GetTargetAddressSize(void) const noexcept
		{
			return m_dwTargetAddressSize;
		}

		bool			SetStruct(DWORD dwValue, LPSTR pStructName, unsigned char cStructChar, DWORD dwSize);
		bool			AddStruct(DWORD dwValue, LPSTR pStructName, unsigned char cStructChar, DWORD dwSize);
		bool			AddStructUserType(DWORD dwMode, LPSTR pStructName, unsigned char cStructChar, CDeclaration* pDecChain, CStatement* pTypeBlock, DWORD dwStructTypeMode);
		bool			AddStructUserType(DWORD dwMode, LPSTR pStructName, unsigned char cStructChar, CDeclaration* pDecChain, CStatement* pTypeBlock, DWORD dwStructTypeMode, bool* bReportError );
		bool			AddStructUserType(DWORD dwMode, LPSTR pStructName, unsigned char cStructChar, CDeclaration* pDecChain, CStatement* pTypeBlock, DWORD dwStructTypeMode, bool* pbReportError, DWORD dwParamInUserFunction );

		void			SetTypeMode(DWORD dwMode) noexcept { m_dwTypeMode=dwMode; }
		void			SetTypeValue(DWORD dwValue) noexcept { m_dwTypeValue=dwValue; }
		void			SetTypeName(CStr* pName) { m_pTypeName.reset(pName); }
		void			SetTypeChar(unsigned char cChar) noexcept { m_cTypeChar=cChar; }
		void			SetTypeSize(DWORD dwSize) noexcept { m_dwSize=dwSize; }
		void			SetDecChain(CDeclaration* pDec) noexcept { m_pDecChain=pDec; }
		void			SetTypeBlock(CStatement* pBlock) noexcept { m_pDecBlock=pBlock; }
		void			SetParamInUserFunction(DWORD dwCount) noexcept { m_dwParamInUserFunction=dwCount; }

		[[nodiscard]] DWORD			GetTypeMode(void) const noexcept { return m_dwTypeMode; }
		[[nodiscard]] DWORD			GetTypeValue(void) const noexcept { return m_dwTypeValue; }
		[[nodiscard]] CStr*			GetTypeName(void) const noexcept { return m_pTypeName.get(); }
		[[nodiscard]] unsigned char	GetTypeChar(void) const noexcept { return m_cTypeChar; }
		[[nodiscard]] DWORD			GetTypeSize(void) const noexcept { return m_dwSize; }
		[[nodiscard]] CStatement*		GetBlock(void) const noexcept { return m_pDecBlock; }
		[[nodiscard]] CDeclaration*	GetDecChain(void) const noexcept { return m_pDecChain; }
		[[nodiscard]] DWORD			GetParamInUserFunction(void) const noexcept { return m_dwParamInUserFunction; }

		bool			CalculateAllSizes(void);
		bool			CalculateSize(void);
		CStructTable*	DoesTypeEvenExist(LPSTR pName);
		DWORD			GetSizeOfType(LPSTR pName);
		CDeclaration*	FindDecInType(LPSTR pTypename, LPSTR pFieldname);
		CDeclaration*	FindFieldInType(LPSTR pTypename, LPSTR pFieldname, LPSTR* pReturnType, DWORD* pdwArrFlag, DWORD* pdwOffset);
		bool			FindOffsetFromField(LPSTR pTypename, LPSTR pFieldname, DWORD* pReturnOffset, DWORD* dwSizeData);
		int				FindIndex(LPSTR pTypename);

		bool			WriteDBMHeader(void);
		bool			WriteDBM(void);

	private:
		void			SetStructDefaults(DWORD dwTargetAddressSize);

		// Structure Type Data
		DWORD			m_dwTypeMode;
		DWORD			m_dwTypeValue;
		std::unique_ptr<CStr> m_pTypeName;
		unsigned char	m_cTypeChar;
		DWORD			m_dwSize;
		DWORD			m_dwTargetAddressSize;
		DWORD			m_dwParamInUserFunction;

		// Chaining Pointer
		CDeclaration*	m_pDecChain;

		// Stores Link to Type Object
		CStatement*		m_pDecBlock;

		// Hierarchy Data
		CStructTable*	m_pNext;

#ifdef __AARON_STRUCPERF__
		static std::unordered_map<std::string, CStructTable*> g_Table;
#endif
};

#endif // !defined(AFX_STRUCTTABLE_H__0EDF6884_E537_492E_806D_71DD644FE9B4__INCLUDED_)
