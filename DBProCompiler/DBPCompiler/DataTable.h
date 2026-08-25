#pragma once
#include "ParserHeader.h"
#include "Str.h"
#include "Task.h"
#include <memory>

class CDataTable  
{
	public:
		CDataTable();
		CDataTable(const char* pInitString);
		CDataTable(std::string_view initString);
		~CDataTable();
		void Free(void);
		void Add(CDataTable* pNew);
		CDataTable* GetNext(void) { return m_pNext.get(); }

		bool			AddNumeric(double dNum, uint32_t dwIndex);
		bool			AddString(std::string_view str, uint32_t dwIndex);
		bool			AddTwoStrings(std::string_view str, std::string_view str2, uint32_t* dwIndex);
		bool			AddTwoStrings(std::string_view str, std::string_view str2, unsigned long* dwIndex) {
			uint32_t idx = dwIndex ? static_cast<uint32_t>(*dwIndex) : 0;
			bool res = AddTwoStrings(str, str2, &idx);
			if (dwIndex) *dwIndex = idx;
			return res;
		}
		bool			AddUniqueString(std::string_view str, uint32_t* dwIndex);
		bool			AddUniqueString(std::string_view str, unsigned long* dwIndex) {
			uint32_t idx = dwIndex ? static_cast<uint32_t>(*dwIndex) : 0;
			bool res = AddUniqueString(str, &idx);
			if (dwIndex) *dwIndex = idx;
			return res;
		}
		uint32_t		FindString(std::string_view findStr);
		bool			FindIndexStr(std::string_view indexAsString);

		void			SetNumeric(double dNum) noexcept { m_dwType=1; m_pNumeric=dNum; }
		void			SetString(std::string_view str) { m_dwType=2; m_pString = std::make_unique<CStr>(str); }
		void			SetString2(std::string_view str) { m_dwType=2; if (str.empty()) m_pString2.reset(); else m_pString2 = std::make_unique<CStr>(str); }
		void			SetAddedToEXEData(bool bState) noexcept { m_bAddedToEXEData=bState; }

		void			SetIndex(uint32_t dwIndex) noexcept { m_dwIndex = dwIndex; }
		[[nodiscard]] uint32_t		GetIndex(void) const noexcept { return m_dwIndex; }

		[[nodiscard]] uint32_t		GetType(void) const noexcept { return m_dwType; }
		[[nodiscard]] double		GetNumeric(void) const noexcept { return m_pNumeric; }
		[[nodiscard]] CStr*			GetString(void) const noexcept { return m_pString.get(); }
		[[nodiscard]] std::string_view GetStringView(void) const noexcept { return m_pString ? m_pString->View() : std::string_view{}; }
		[[nodiscard]] const char*	GetStringStr(void) const noexcept { return m_pString ? m_pString->c_str() : ""; }
		[[nodiscard]] CStr*			GetString2(void) const noexcept { return m_pString2.get(); }
		[[nodiscard]] std::string_view GetString2View(void) const noexcept { return m_pString2 ? m_pString2->View() : std::string_view{}; }
		[[nodiscard]] const char*	GetString2Str(void) const noexcept { return m_pString2 ? m_pString2->c_str() : ""; }
		[[nodiscard]] bool			GetAddedToEXEData(void) const noexcept { return m_bAddedToEXEData; }

		bool			NotExcluded ( std::string_view filename );
		bool			NotExcluded ( const char* pFilename ) {
			return pFilename ? NotExcluded(std::string_view(pFilename)) : true;
		}
		int				CompleteAnyLinkAssociates(void);

		bool			WriteDBMHeader(uint32_t dwKindOfTable);
		bool			WriteDBM(void);

	private:

		// Multi-Type Data Item
		uint32_t					m_dwIndex;
		uint32_t					m_dwType;
		double						m_pNumeric;
		std::unique_ptr<CStr>		m_pString;
		std::unique_ptr<CStr>		m_pString2;
		bool						m_bAddedToEXEData;

		// Hierarchy Data (RAII ownership chain)
		std::unique_ptr<CDataTable>	m_pNext;

		// Safe Access
		db3::CLock			m_Lock;
};