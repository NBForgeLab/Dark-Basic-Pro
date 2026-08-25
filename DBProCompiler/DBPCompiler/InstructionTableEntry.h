#pragma once

#include <cstdint>
#include <string_view>
#include "Str.h"
#include "Task.h"
#include <memory>

class CDeclaration;

class CInstructionTableEntry  
{

	public:
		CInstructionTableEntry();
		virtual ~CInstructionTableEntry();
		void Free(void);

		void						Add(CInstructionTableEntry *pNew);
		void						Insert(CInstructionTableEntry *pNew);
		CInstructionTableEntry*		GetNext(void) { return m_pNext; }
		void						SetData(DWORD id, CStr* pStr, CStr* pDLL, CStr* pDecoratedName, CStr* pParamTypes, DWORD returnparam, DWORD param, DWORD dwInternalId, DWORD dwBuildID);
		void						SetReturnParamType(DWORD returnparam) { m_dwReturnParam=returnparam; }
		void						SetReturnParamPlace(DWORD place) { m_dwReturnPlace=place; }
		void						SetSpecialArrayParam(bool bState) { m_bSpecialArrayParam=bState; }
		void						SetFullParamDesc(CStr* pStr) { m_pParamDesc.reset(pStr); }

		void						SetDecChain(CDeclaration* pRef) { m_pDecChain.reset(pRef); }
		CDeclaration*				GetDecChain(void) { return m_pDecChain.get(); }

		DWORD						GetInternalID(void) const noexcept { return m_dwInternalID; }
		[[nodiscard]] CStr*			GetName(void) const noexcept { return m_pName.get(); }
		[[nodiscard]] std::string_view GetNameView(void) const noexcept { return m_pName ? m_pName->View() : std::string_view{}; }
		[[nodiscard]] const char*	GetNameStr(void) const noexcept { return m_pName ? m_pName->c_str() : ""; }

		[[nodiscard]] CStr*			GetDLL(void) const noexcept { return m_pDLL.get(); }
		[[nodiscard]] std::string_view GetDLLView(void) const noexcept { return m_pDLL ? m_pDLL->View() : std::string_view{}; }
		[[nodiscard]] const char*	GetDLLStr(void) const noexcept { return m_pDLL ? m_pDLL->c_str() : ""; }

		[[nodiscard]] CStr*			GetDecoratedName(void) const noexcept { return m_pDecoratedName.get(); }
		[[nodiscard]] std::string_view GetDecoratedNameView(void) const noexcept { return m_pDecoratedName ? m_pDecoratedName->View() : std::string_view{}; }
		[[nodiscard]] const char*	GetDecoratedNameStr(void) const noexcept { return m_pDecoratedName ? m_pDecoratedName->c_str() : ""; }

		[[nodiscard]] CStr*			GetParamTypes(void) const noexcept { return m_pParamTypes.get(); }
		[[nodiscard]] std::string_view GetParamTypesView(void) const noexcept { return m_pParamTypes ? m_pParamTypes->View() : std::string_view{}; }
		[[nodiscard]] const char*	GetParamTypesStr(void) const noexcept { return m_pParamTypes ? m_pParamTypes->c_str() : ""; }

		DWORD						GetReturnParam(void) const noexcept { return m_dwReturnParam; }
		DWORD						GetParamMax(void) const noexcept { return m_dwParamMax; }
		DWORD						GetHardcoreInternalValue(void) const noexcept { return m_dwHardcoreInternalValue; }
		DWORD						GetBuildID(void) const noexcept { return m_dwBuildID; }
		DWORD						GetReturnParamPlace(void) const noexcept { return m_dwReturnPlace; }
		bool						GetSpecialArrayParam(void) const noexcept { return m_bSpecialArrayParam; }

		[[nodiscard]] CStr*			GetFullParamDesc(void) const noexcept { return m_pParamDesc.get(); }
		[[nodiscard]] std::string_view GetFullParamDescView(void) const noexcept { return m_pParamDesc ? m_pParamDesc->View() : std::string_view{}; }
		[[nodiscard]] const char*	GetFullParamDescStr(void) const noexcept { return m_pParamDesc ? m_pParamDesc->c_str() : ""; }

	private:
		// Instruction Entry Data
		DWORD						m_dwInternalID;
		DWORD						m_dwReturnParam;
		DWORD						m_dwParamMax;
		std::unique_ptr<CStr>		m_pName;
		std::unique_ptr<CStr>		m_pDLL;
		std::unique_ptr<CStr>		m_pDecoratedName;
		std::unique_ptr<CStr>		m_pParamTypes;
		std::unique_ptr<CStr>		m_pParamDesc;
		DWORD						m_dwHardcoreInternalValue;
		DWORD						m_dwBuildID;
		DWORD						m_dwReturnPlace;
		bool						m_bSpecialArrayParam;
		std::unique_ptr<CDeclaration> m_pDecChain;

		// Hierarchy Data
		CInstructionTableEntry*		m_pPrev;
		CInstructionTableEntry*		m_pNext;
};