#pragma once
#include "ParserHeader.h"
#include "Str.h"
#include "Task.h"

#include "PerfMacros.h"

#include <vector>

#ifdef __AARON_LBLTBLPERF__
# include <unordered_map>
# include <string>
#endif

class CStatement;

class CLabelTable  
{
	public:
		CLabelTable();
		CLabelTable(const char* pChar);
		CLabelTable(std::string_view name);
		virtual ~CLabelTable();
		void Free(void);

		void Add(CLabelTable* pNew);
		void Insert(CLabelTable* pNew);
		void AddInOrder(std::string_view name, CLabelTable* pNew);
		void AddInOrder(const char* pName, CLabelTable* pNew) {
			if (pName) AddInOrder(std::string_view(pName), pNew);
		}
		CLabelTable* Advance(DWORD dwCountdown);
		CLabelTable* Subtract(DWORD dwCountdown);
		CLabelTable* GetNext(void)
		{
			if ( m_orderIndex==static_cast<size_t>(-1) || m_orderIndex+1>=g_Order.size() ) return nullptr;
			return g_Order[m_orderIndex+1];
		}
		CLabelTable* GetPrev(void)
		{
			if ( m_orderIndex==static_cast<size_t>(-1) || m_orderIndex==0 ) return nullptr;
			return g_Order[m_orderIndex-1];
		}

		bool			AddLabel(std::string_view strName, DWORD dwCodeIndex, DWORD dwDataIndex, CStatement* pSRef);
		CLabelTable*	FindLabel(std::string_view labelName);
		bool			UpdateLabel(std::string_view strName, DWORD dwCodeIndex, DWORD dwDataIndex, CStatement* pSRef);

		void			SetName(std::string_view name) { m_pName = std::make_unique<CStr>(name); }
		void			SetCodeIndex(DWORD dwIndex) noexcept { m_dwCodeIndex=dwIndex; }
		void			SetDataIndex(DWORD dwIndex) noexcept { m_dwDataIndex=dwIndex; }
		void			SetBytePosition(DWORD dwIndex) noexcept { m_dwBytePos=dwIndex; }
		void			SetSRef(CStatement* pRef) noexcept { m_pSRef=pRef; }

		CStr*			GetName(void) const noexcept { return m_pName.get(); }
		std::string_view GetNameView(void) const noexcept { return m_pName ? m_pName->View() : std::string_view{}; }
		const char*		GetNameStr(void) const noexcept { return m_pName ? m_pName->c_str() : ""; }
		DWORD			GetCodeIndex(void) const noexcept { return m_dwCodeIndex; }
		DWORD			GetDataIndex(void) const noexcept { return m_dwDataIndex; }
		DWORD			GetBytePosition(void) const noexcept { return m_dwBytePos; }
		CStatement*		GetSRef(void) const noexcept { return m_pSRef; }

		void			UpdateDataIndexOfLabelsAtLine(CStatement* pStatementRef, DWORD dwData);

		bool			WriteDBMHeader(void);
		bool			WriteDBM(void);

	private:

		// Data
		std::unique_ptr<CStr>	m_pName;
		DWORD					m_dwCodeIndex;
		DWORD					m_dwDataIndex;
		DWORD					m_dwBytePos;
		CStatement*				m_pSRef;

		// Position in the global declaration-order index (replaces legacy linked list)
		size_t					m_orderIndex;

		// Safe Access
		db3::CLock				m_Lock;

#ifdef __AARON_LBLTBLPERF__
		static std::unordered_map<std::string, CLabelTable*> g_Table;
#endif
		static std::vector<CLabelTable*> g_Order;
};