// IncludeTable.cpp: implementation of the CIncludeTable class.
//
//////////////////////////////////////////////////////////////////////

#define _CRT_SECURE_NO_DEPRECATE
#include "IncludeTable.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CIncludeTable::CIncludeTable()
{
	m_dwFirstByte=0;
}

CIncludeTable::~CIncludeTable()
{
	// unique_ptr members auto-cleanup
}

void CIncludeTable::Add(CIncludeTable* pNew)
{
	if(m_pNext==nullptr)
		m_pNext.reset(pNew);
	else
		m_pNext->Add(pNew);
}

bool CIncludeTable::FindInclude(LPSTR pFilename)
{
	CIncludeTable* pCurrent = this;
	while(pCurrent)
	{
		if(stricmp(pFilename, pCurrent->GetFilename()->GetStr())==NULL)
			return true;

		pCurrent=pCurrent->GetNext();
	}

	// Could not find soft fail
	return false;
}
