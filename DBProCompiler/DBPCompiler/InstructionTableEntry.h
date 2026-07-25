// InstructionTableEntry.h: interface for the CInstructionTableEntry class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_INSTRUCTIONTABLEENTRY_H__4CC0B572_0A5F_418D_8595_59DC544431D6__INCLUDED_)
#define AFX_INSTRUCTIONTABLEENTRY_H__4CC0B572_0A5F_418D_8595_59DC544431D6__INCLUDED_

#include "windows.h"
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

		DWORD						GetInternalID(void) { return m_dwInternalID; }
		CStr*						GetName(void) { return m_pName.get(); }
		CStr*						GetDLL(void) { return m_pDLL.get(); }
		CStr*						GetDecoratedName(void) { return m_pDecoratedName.get(); }
		CStr*						GetParamTypes(void) { return m_pParamTypes.get(); }
		DWORD						GetReturnParam(void) { return m_dwReturnParam; }
		DWORD						GetParamMax(void) { return m_dwParamMax; }
		DWORD						GetHardcoreInternalValue(void) { return m_dwHardcoreInternalValue; }
		DWORD						GetBuildID(void) { return m_dwBuildID; }
		DWORD						GetReturnParamPlace(void) { return m_dwReturnPlace; }
		bool						GetSpecialArrayParam(void) { return m_bSpecialArrayParam; }
		CStr*						GetFullParamDesc(void) { return m_pParamDesc.get(); }

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

#endif // !defined(AFX_INSTRUCTIONTABLEENTRY_H__4CC0B572_0A5F_418D_8595_59DC544431D6__INCLUDED_)
