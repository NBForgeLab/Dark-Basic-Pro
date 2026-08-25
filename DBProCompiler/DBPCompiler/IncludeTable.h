#pragma once
#include "ParserHeader.h"
#include "Str.h"
#include "Task.h"
#include <memory>
#include <string_view>

class CIncludeTable  
{
	public:
		CIncludeTable();
		virtual ~CIncludeTable() = default;
		void Add(CIncludeTable* pNew);
		void Add(std::unique_ptr<CIncludeTable> pNew);
		CIncludeTable* GetNext(void) { return m_pNext.get(); }
		const CIncludeTable* GetNext(void) const { return m_pNext.get(); }

		bool				FindInclude(std::string_view filename) const;
		bool				FindInclude(const char* pFilename) const {
			return pFilename ? FindInclude(std::string_view(pFilename)) : false;
		}
		void				SetFilename(CStr* pFile) { m_pFilename.reset(pFile); }
		void				SetFilename(std::unique_ptr<CStr> pFile) { m_pFilename = std::move(pFile); }
		CStr*				GetFilename(void) { return m_pFilename.get(); }
		const CStr*			GetFilename(void) const { return m_pFilename.get(); }
		[[nodiscard]] std::string_view GetFilenameView() const noexcept {
			return m_pFilename && m_pFilename->GetStr() ? std::string_view(m_pFilename->GetStr()) : std::string_view{};
		}
		void				SetFirstByte(DWORD dwByte) { m_dwFirstByte=dwByte; }
		DWORD				GetFirstByte(void) const { return m_dwFirstByte; }

	private:

		// Include Data
		std::unique_ptr<CStr>	m_pFilename;
		DWORD				m_dwFirstByte;

		// Hierarchy Data
		std::unique_ptr<CIncludeTable>	m_pNext;

		// Safe Access
		mutable db3::CLock	m_Lock;
};