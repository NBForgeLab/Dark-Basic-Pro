#pragma once
#include "ParserHeader.h"
#include "TargetABI.h"

#include "PerfMacros.h"

#include <vector>

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
		[[nodiscard]] CStructTable* GetNext(void) noexcept
		{
			if ( m_orderIndex==static_cast<size_t>(-1) || m_orderIndex+1>=g_Order.size() ) return nullptr;
			return g_Order[m_orderIndex+1];
		}
		[[nodiscard]] const CStructTable* GetNext(void) const noexcept
		{
			if ( m_orderIndex==static_cast<size_t>(-1) || m_orderIndex+1>=g_Order.size() ) return nullptr;
			return g_Order[m_orderIndex+1];
		}

		void			SetStructDefaults(void);
		template <typename TargetAbi>
		void			SetStructDefaultsFor(void)
		{
			static_assert(
				TargetAbi::address_size == 8,
				"The SDK targets a native x64 address width.");
			SetStructDefaults(static_cast<DWORD>(TargetAbi::address_size));
		}
		[[nodiscard]] DWORD GetTargetAddressSize(void) const noexcept
		{
			return m_dwTargetAddressSize;
		}

		bool			SetStruct(DWORD dwValue, std::string_view structName, unsigned char cStructChar, DWORD dwSize);
		bool			AddStruct(DWORD dwValue, std::string_view structName, unsigned char cStructChar, DWORD dwSize);
		bool			AddStructUserType(DWORD dwMode, std::string_view structName, unsigned char cStructChar, CDeclaration* pDecChain, CStatement* pTypeBlock, DWORD dwStructTypeMode, bool* pbReportError = nullptr, DWORD dwParamInUserFunction = 0);

		void			SetTypeMode(DWORD dwMode) noexcept { m_dwTypeMode=dwMode; }
		void			SetTypeValue(DWORD dwValue) noexcept { m_dwTypeValue=dwValue; }
		void			SetTypeName(std::string_view name) { m_pTypeName = std::make_unique<CStr>(name); }
		void			SetTypeChar(unsigned char cChar) noexcept { m_cTypeChar=cChar; }
		void			SetTypeSize(DWORD dwSize) noexcept { m_dwSize=dwSize; }
		void			SetDecChain(CDeclaration* pDec) noexcept { m_pDecChain=pDec; }
		void			SetTypeBlock(CStatement* pBlock) noexcept { m_pDecBlock=pBlock; }
		void			SetParamInUserFunction(DWORD dwCount) noexcept { m_dwParamInUserFunction=dwCount; }

		[[nodiscard]] DWORD			GetTypeMode(void) const noexcept { return m_dwTypeMode; }
		[[nodiscard]] DWORD			GetTypeValue(void) const noexcept { return m_dwTypeValue; }
		[[nodiscard]] CStr*			GetTypeName(void) noexcept { return m_pTypeName.get(); }
		[[nodiscard]] const CStr*	GetTypeName(void) const noexcept { return m_pTypeName.get(); }
		[[nodiscard]] std::string_view GetTypeNameView(void) const noexcept { return m_pTypeName ? m_pTypeName->View() : std::string_view{}; }
		[[nodiscard]] LPCSTR		GetTypeNameStr(void) const noexcept { return m_pTypeName ? m_pTypeName->c_str() : ""; }
		[[nodiscard]] unsigned char	GetTypeChar(void) const noexcept { return m_cTypeChar; }
		[[nodiscard]] DWORD			GetTypeSize(void) const noexcept { return m_dwSize; }
		[[nodiscard]] CStatement*		GetBlock(void) noexcept { return m_pDecBlock; }
		[[nodiscard]] const CStatement* GetBlock(void) const noexcept { return m_pDecBlock; }
		[[nodiscard]] CDeclaration*	GetDecChain(void) noexcept { return m_pDecChain; }
		[[nodiscard]] const CDeclaration* GetDecChain(void) const noexcept { return m_pDecChain; }
		[[nodiscard]] DWORD			GetParamInUserFunction(void) const noexcept { return m_dwParamInUserFunction; }

		bool			CalculateAllSizes(void);
		bool			CalculateSize(void);
		CStructTable*	DoesTypeEvenExist(std::string_view name);
		CStructTable*	DoesTypeEvenExist(const char* pName) {
			return pName ? DoesTypeEvenExist(std::string_view(pName)) : nullptr;
		}
		DWORD			GetSizeOfType(std::string_view name);
		DWORD			GetSizeOfType(const char* pName) {
			return pName ? GetSizeOfType(std::string_view(pName)) : 0;
		}
		CDeclaration*	FindDecInType(std::string_view typenameStr, std::string_view fieldname);
		CDeclaration*	FindDecInType(const char* pTypename, const char* pFieldname) {
			if (!pTypename || !pFieldname) return nullptr;
			return FindDecInType(std::string_view(pTypename), std::string_view(pFieldname));
		}
		CDeclaration*	FindFieldInType(std::string_view typenameStr, std::string_view fieldname, LPSTR* pReturnType, DWORD* pdwArrFlag, DWORD* pdwOffset);
		CDeclaration*	FindFieldInType(const char* pTypename, const char* pFieldname, LPSTR* pReturnType, DWORD* pdwArrFlag, DWORD* pdwOffset) {
			if (!pTypename || !pFieldname) return nullptr;
			return FindFieldInType(std::string_view(pTypename), std::string_view(pFieldname), pReturnType, pdwArrFlag, pdwOffset);
		}
		bool			FindOffsetFromField(std::string_view typenameStr, std::string_view fieldname, DWORD* pReturnOffset, DWORD* dwSizeData);
		bool			FindOffsetFromField(const char* pTypename, const char* pFieldname, DWORD* pReturnOffset, DWORD* dwSizeData) {
			if (!pTypename || !pFieldname) return false;
			return FindOffsetFromField(std::string_view(pTypename), std::string_view(pFieldname), pReturnOffset, dwSizeData);
		}
		int				FindIndex(std::string_view typenameStr);
		int				FindIndex(const char* pTypename) {
			return pTypename ? FindIndex(std::string_view(pTypename)) : 0;
		}

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

		// Position in the global declaration-order index (replaces legacy linked list)
		size_t			m_orderIndex;

#ifdef __AARON_STRUCPERF__
		static std::unordered_map<std::string, CStructTable*> g_Table;
#endif
		static std::vector<CStructTable*> g_Order;
};