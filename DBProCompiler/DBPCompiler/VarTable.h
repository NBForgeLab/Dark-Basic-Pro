// VarTable.h: interface for the CVarTable class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_VARTABLE_H__4910A987_6F89_44BE_BCB7_3DE3DEDD217B__INCLUDED_)
#define AFX_VARTABLE_H__4910A987_6F89_44BE_BCB7_3DE3DEDD217B__INCLUDED_
#include "ParserHeader.h"

#include "PerfMacros.h"

#include <string>

#ifdef __AARON_VARTABLEPERF__
# include <unordered_map>
#endif

class CVarTable  
{
	public:
		CVarTable();
		CVarTable(LPCSTR pStr);
		virtual ~CVarTable();
		void Free(void);

		void Add(CVarTable* pNew);
		void Insert(CVarTable* pNew);
		void AddInOrder(LPCSTR pName, CVarTable* pNew);
		CVarTable* Advance(DWORD dwCountdown);
		CVarTable* Subtract(DWORD dwCountdown);
		[[nodiscard]] CVarTable* GetNext(void) noexcept { return m_pNext; }
		[[nodiscard]] const CVarTable* GetNext(void) const noexcept { return m_pNext; }
		[[nodiscard]] CVarTable* GetPrev(void) noexcept { return m_pPrev; }
		[[nodiscard]] const CVarTable* GetPrev(void) const noexcept { return m_pPrev; }
		void SetVarDefaults(void);

		bool			AddVariable(LPCSTR pName, LPCSTR pType, DWORD dwArrFlag, DWORD dwLineNumber, bool bFromActualCodeNotFromTypeDefing, DWORD* pdwAction, bool bIsGlobal);
		CVarTable*		FindVariable(LPCSTR pScope, LPCSTR pName, DWORD dwArrFlag);
		bool			FindVariableExist(LPCSTR pFindVar, DWORD dwArrType);
		bool			FindTypeOfVariable(LPCSTR pFindVar, DWORD dwArrType, LPSTR* pReturnType);
		DWORD			MakeDefaultVarTypeValue(LPCSTR pDecName);
		std::string		MakeDefaultVarType(LPCSTR pDecName);
		LPSTR			MakeTypeNameOfTypeValue(DWORD dwTypeValue);
		DWORD			GetBasicTypeValue(LPCSTR pTypeString);
		CStructTable*	GetStruct(LPCSTR pTypeString);
		char			GetCharOfType(DWORD dwTypeValue);
		DWORD			GetTypeValueOfChar(unsigned char cTypeChar);

		void			SetVarScope(CStr* pScope) { m_pVarScope.reset(pScope); }
		[[nodiscard]] CStr*			GetVarScope(void) noexcept { return m_pVarScope.get(); }
		[[nodiscard]] const CStr*	GetVarScope(void) const noexcept { return m_pVarScope.get(); }
		void			SetVarName(CStr* pName) { m_pVarName.reset(pName); }
		[[nodiscard]] CStr*			GetVarName(void) noexcept { return m_pVarName.get(); }
		[[nodiscard]] const CStr*	GetVarName(void) const noexcept { return m_pVarName.get(); }
		void			SetVarType(CStr* pType) { m_pVarType.reset(pType); }
		[[nodiscard]] CStr*			GetVarType(void) noexcept { return m_pVarType.get(); }
		[[nodiscard]] const CStr*	GetVarType(void) const noexcept { return m_pVarType.get(); }
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
		
		void			SetAdditionalDataString(CStr* pStr) { m_pAdditionalDataString.reset(pStr); }
		[[nodiscard]] CStr*			GetAdditionalDataString(void) noexcept { return m_pAdditionalDataString.get(); }
		[[nodiscard]] const CStr*	GetAdditionalDataString(void) const noexcept { return m_pAdditionalDataString.get(); }

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

		// Hierarchy Data
		CVarTable*		m_pNext;
		CVarTable*		m_pPrev;

		// Dictionary
#ifdef __AARON_VARTABLEPERF__
		static std::unordered_map<std::string, CVarTable*> g_Table;
#endif
};

#endif // !defined(AFX_VARTABLE_H__4910A987_6F89_44BE_BCB7_3DE3DEDD217B__INCLUDED_)
