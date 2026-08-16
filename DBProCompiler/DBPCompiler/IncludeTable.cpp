#include "IncludeTable.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CIncludeTable::CIncludeTable()
	: m_dwFirstByte(0)
{
}

void CIncludeTable::Add(CIncludeTable* pNew)
{
	if(pNew == nullptr) return;
	Add(std::unique_ptr<CIncludeTable>(pNew));
}

void CIncludeTable::Add(std::unique_ptr<CIncludeTable> pNew)
{
	if(!pNew) return;
	db3::CAutolock autolock(m_Lock);
	if(!m_pNext)
		m_pNext = std::move(pNew);
	else
		m_pNext->Add(std::move(pNew));
}

bool CIncludeTable::FindInclude(LPCSTR pFilename) const
{
	if (!pFilename || pFilename[0] == '\0') return false;
	return FindInclude(std::string_view(pFilename));
}

bool CIncludeTable::FindInclude(std::string_view filename) const
{
	if (filename.empty()) return false;
	db3::CAutolock autolock(m_Lock);
	const CIncludeTable* pCurrent = this;
	while(pCurrent)
	{
		if(pCurrent->GetFilename() && pCurrent->GetFilename()->GetStr())
		{
			if(_stricmp(filename.data(), pCurrent->GetFilename()->GetStr())==0)
				return true;
		}

		pCurrent = pCurrent->GetNext();
	}

	return false;
}
