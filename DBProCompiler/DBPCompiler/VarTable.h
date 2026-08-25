#pragma once
#include "ParserHeader.h"

#include "PerfMacros.h"

#include <string>
#include <vector>

#ifdef __AARON_VARTABLEPERF__
# include <unordered_map>
#endif

class CVarTable  
{
	public:
		CVarTable();
		CVarTable(const char* pStr);
		CVarTable(std::string_view str);
		virtual ~CVarTable();
		void Free(void);

		void Add(CVarTable* pNew);
		void Insert(CVarTable* pNew);
		void AddInOrder(std::string_view name, CVarTable* pNew);
		void AddInOrder(const char* pName, CVarTable* pNew) {
			if (pName) AddInOrder(std::string_view(pName), pNew);
		}
		CVarTable* Advance(DWORD dwCountdown);
		CVarTable* Subtract(DWORD dwCountdown);
		[[nodiscard]] CVarTable* GetNext(void) noexcept
		{
			if ( m_orderIndex==static_cast<size_t>(-1) || m_orderIndex+1>=g_Order.size() ) return nullptr;
			return g_Order[m_orderIndex+1];
		}
		[[nodiscard]] const CVarTable* GetNext(void) const noexcept
		{
			if ( m_orderIndex==static_cast<size_t>(-1) || m_orderIndex+1>=g_Order.size() ) return nullptr;
			return g_Order[m_orderIndex+1];
		}
		[[nodiscard]] CVarTable* GetPrev(void) noexcept
		{
			if ( m_orderIndex==static_cast<size_t>(-1) || m_orderIndex==0 ) return nullptr;
			return g_Order[m_orderIndex-1];
		}
		[[nodiscard]] const CVarTable* GetPrev(void) const noexcept
		{
			if ( m_orderIndex==static_cast<size_t>(-1) || m_orderIndex==0 ) return nullptr;
			return g_Order[m_orderIndex-1];
		}
		void SetVarDefaults(void);

		bool			AddVariable(std::string_view name, std::string_view type, DWORD dwArrFlag, DWORD dwLineNumber, bool bFromActualCodeNotFromTypeDefing, DWORD* pdwAction, bool bIsGlobal);
		CVarTable*		FindVariable(std::string_view scope, std::string_view name, DWORD dwArrFlag);
		CVarTable*		FindVariable(const char* pScope, std::string_view name, DWORD dwArrFlag) {
			return FindVariable(pScope ? std::string_view(pScope) : std::string_view{}, name, dwArrFlag);
		}
		bool			FindVariableExist(std::string_view findVar, DWORD dwArrType);
		bool			FindVariableExist(const char* pFindVar, DWORD dwArrType) {
			return pFindVar ? FindVariableExist(std::string_view(pFindVar), dwArrType) : false;
		}
		bool			FindTypeOfVariable(std::string_view findVar, DWORD dwArrType, LPSTR* pReturnType);
		bool			FindTypeOfVariable(const char* pFindVar, DWORD dwArrType, LPSTR* pReturnType) {
			return pFindVar ? FindTypeOfVariable(std::string_view(pFindVar), dwArrType, pReturnType) : false;
		}
		DWORD			MakeDefaultVarTypeValue(std::string_view decName);
		DWORD			MakeDefaultVarTypeValue(const char* pDecName) {
			return pDecName ? MakeDefaultVarTypeValue(std::string_view(pDecName)) : 1;
		}
		std::string		MakeDefaultVarType(std::string_view decName);
		std::string		MakeDefaultVarType(const char* pDecName) {
			return pDecName ? MakeDefaultVarType(std::string_view(pDecName)) : std::string();
		}
		LPSTR			MakeTypeNameOfTypeValue(DWORD dwTypeValue);
		DWORD			GetBasicTypeValue(std::string_view typeString);
		DWORD			GetBasicTypeValue(const char* pTypeString) {
			return pTypeString ? GetBasicTypeValue(std::string_view(pTypeString)) : 0;
		}
		CStructTable*	GetStruct(std::string_view typeString);
		CStructTable*	GetStruct(const char* pTypeString) {
			return pTypeString ? GetStruct(std::string_view(pTypeString)) : nullptr;
		}
		char			GetCharOfType(DWORD dwTypeValue);
		DWORD			GetTypeValueOfChar(unsigned char cTypeChar);

		void			SetVarScope(std::string_view scope) { m_pVarScope = std::make_unique<CStr>(scope); }
		[[nodiscard]] CStr*			GetVarScope(void) noexcept { return m_pVarScope.get(); }
		[[nodiscard]] const CStr*	GetVarScope(void) const noexcept { return m_pVarScope.get(); }
		[[nodiscard]] std::string_view GetVarScopeView(void) const noexcept { return m_pVarScope ? m_pVarScope->View() : std::string_view{}; }
		[[nodiscard]] LPCSTR		GetVarScopeStr(void) const noexcept { return m_pVarScope ? m_pVarScope->c_str() : ""; }

		void			SetVarName(std::string_view name) { m_pVarName = std::make_unique<CStr>(name); }
		[[nodiscard]] CStr*			GetVarName(void) noexcept { return m_pVarName.get(); }
		[[nodiscard]] const CStr*	GetVarName(void) const noexcept { return m_pVarName.get(); }
		[[nodiscard]] std::string_view GetVarNameView(void) const noexcept { return m_pVarName ? m_pVarName->View() : std::string_view{}; }
		[[nodiscard]] LPCSTR		GetVarNameStr(void) const noexcept { return m_pVarName ? m_pVarName->c_str() : ""; }

		void			SetVarType(std::string_view type) { m_pVarType = std::make_unique<CStr>(type); }
		[[nodiscard]] CStr*			GetVarType(void) noexcept { return m_pVarType.get(); }
		[[nodiscard]] const CStr*	GetVarType(void) const noexcept { return m_pVarType.get(); }
		[[nodiscard]] std::string_view GetVarTypeView(void) const noexcept { return m_pVarType ? m_pVarType->View() : std::string_view{}; }
		[[nodiscard]] LPCSTR		GetVarTypeStr(void) const noexcept { return m_pVarType ? m_pVarType->c_str() : ""; }

		void			SetVarTypeValue(DWORD dwTypeValue) noexcept { m_dwVarTypeValue=dwTypeValue; }
		[[nodiscard]] DWORD			GetVarTypeValue(void) const noexcept { return m_dwVarTypeValue; }
		void			SetVarStruct(CStructTable* pStruct) noexcept { m_pVarStruct=pStruct; }
		[[nodiscard]] CStructTable*	GetVarStruct(void) noexcept { return m_pVarStruct; }
		[[nodiscard]] const CStructTable* GetVarStruct(void) const noexcept { return m_pVarStruct; }
		void			SetArrFlag(DWORD dwFlag) noexcept { m_dwArrFlag=dwFlag; }
		[[nodiscard]] DWORD			GetArrFlag(void) const noexcept { return m_dwArrFlag; }
		[[nodiscard]] DWORD			GetOffsetValue(void) const noexcept { return m_dwFinalDBMOffset; }
		void			SetPreScanAddFlag(bool bState) noexcept { m_bPreScanAdd=bState; }
		[[nodiscard]] bool			GetPreScanAddFlag(void) const noexcept { return m_bPreScanAdd; }
		void			SetSpecifiedAsGlobalFlag(bool bState) noexcept { m_bSpecifiedAsGLOBAL=bState; }
		[[nodiscard]] bool			GetSpecifiedAsGlobalFlag(void) const noexcept { return m_bSpecifiedAsGLOBAL; }
		
		void			SetAdditionalDataString(std::string_view str) { m_pAdditionalDataString = std::make_unique<CStr>(str); }
		[[nodiscard]] CStr*			GetAdditionalDataString(void) noexcept { return m_pAdditionalDataString.get(); }
		[[nodiscard]] const CStr*	GetAdditionalDataString(void) const noexcept { return m_pAdditionalDataString.get(); }
		[[nodiscard]] std::string_view GetAdditionalDataStringView(void) const noexcept { return m_pAdditionalDataString ? m_pAdditionalDataString->View() : std::string_view{}; }
		[[nodiscard]] LPCSTR		GetAdditionalDataStringStr(void) const noexcept { return m_pAdditionalDataString ? m_pAdditionalDataString->c_str() : ""; }

		void			SetLineNumber(DWORD dwLine) noexcept { m_dwLineNumber=dwLine; }
		[[nodiscard]] DWORD			GetLineNumber(void) const noexcept { return m_dwLineNumber; }
	
		bool			VerifyVariableStructures(void);
		DWORD			EstablishVarOffsets(DWORD* pdwOffsetValue);

		bool			WriteDBMHeader(void);
		bool			WriteDBM(void);
		bool			WriteDBMFooter(DWORD dwSizeOfVariableBuffer);

	private:

		// Debug Data
		DWORD			m_dwLineNumber;

		// Variable Data
		std::unique_ptr<CStr> m_pVarScope;
		std::unique_ptr<CStr> m_pVarName;
		std::unique_ptr<CStr> m_pVarType;
		DWORD			m_dwVarTypeValue;
		CStructTable*	m_pVarStruct;
		DWORD			m_dwArrFlag;
		DWORD			m_dwFinalDBMOffset;
		bool			m_bPreScanAdd;
		bool			m_bSpecifiedAsGLOBAL;

		// Used to hold array dimension string (for parser use)
		std::unique_ptr<CStr> m_pAdditionalDataString;

		// Mini-CLI Program Support
		bool			m_bOffsetAssigned;

		// Position in the global declaration-order index (replaces legacy linked list)
		size_t			m_orderIndex;

		// Dictionary
#ifdef __AARON_VARTABLEPERF__
		static std::unordered_map<std::string, CVarTable*> g_Table;
#endif
		static std::vector<CVarTable*> g_Order;
};