// InstructionTableEntry.cpp: implementation of the CInstructionTableEntry class.
//
//////////////////////////////////////////////////////////////////////

// Common Includes
#include "macros.h"

// Custom Includes
#include "Declaration.h"
#include "InstructionTableEntry.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CInstructionTableEntry::CInstructionTableEntry()
{
	m_dwInternalID=0;
	m_dwReturnParam=0;
	m_dwParamMax=0;
	m_dwHardcoreInternalValue=0;
	m_dwBuildID=0;
	m_pDecChain=nullptr;

	m_pPrev=NULL;
	m_pNext=NULL;
}

CInstructionTableEntry::~CInstructionTableEntry()
{
	// unique_ptr members auto-cleanup
}

void CInstructionTableEntry::Free(void)
{
	CInstructionTableEntry* pCurrent = this;
	while(pCurrent)
	{
		CInstructionTableEntry* pNext = pCurrent->GetNext();
		delete pCurrent;
		pCurrent = pNext;
	}
}

void CInstructionTableEntry::Add(CInstructionTableEntry* pNew)
{
	CInstructionTableEntry* pCurrent = this;
	while(pCurrent->m_pNext)
		pCurrent=pCurrent->m_pNext;
	pCurrent->m_pNext=pNew;
	pNew->m_pPrev=pCurrent;
}

void CInstructionTableEntry::Insert(CInstructionTableEntry *pNew)
{
	// Get neighbors
	CInstructionTableEntry* pNeighA = m_pPrev;
	CInstructionTableEntry* pNeighB = this;

	// Instruct neighbours to point to me
	if(pNeighA) pNeighA->m_pNext = pNew;
	pNeighB->m_pPrev = pNew;

	// Insruct new to point to neighbors
	pNew->m_pNext = pNeighB;
	pNew->m_pPrev = pNeighA;
}

void CInstructionTableEntry::SetData(DWORD InternalID, CStr* pStr, CStr* pDLL, CStr* pDecoratedName, CStr* pParamTypes, DWORD returnparam, DWORD param, DWORD dwInternalId, DWORD dwBuildID)
{
	// Set Instruction Data
	m_dwInternalID = InternalID;
	m_dwReturnParam = returnparam;
	m_dwParamMax = param;
	m_pName.reset(pStr);
	m_pDLL.reset(pDLL);
	m_pDecoratedName.reset(pDecoratedName);
	m_pParamTypes.reset(pParamTypes);
	m_dwHardcoreInternalValue=dwInternalId;
	m_dwBuildID=dwBuildID;
}
