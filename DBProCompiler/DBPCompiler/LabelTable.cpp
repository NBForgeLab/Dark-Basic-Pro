// LabelTable.cpp: implementation of the CLabelTable class.
//
//////////////////////////////////////////////////////////////////////

#include "LabelTable.h"
#include "time.h"

#ifdef __AARON_LBLTBLPERF__
# define ALLOWED_LOWER "abcdefghijklmnopqrstuvwxyz"
# define ALLOWED_UPPER "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
# define ALLOWED_ALPHA ALLOWED_LOWER ALLOWED_UPPER
# define ALLOWED_DIGIT "0123456789"
# define ALLOWED_ALNUM ALLOWED_ALPHA ALLOWED_DIGIT
# define ALLOWED_IDENT ALLOWED_ALNUM "_"
# define ALLOWED_TYPES "#$%"
# define ALLOWED_INTRN "&@* [](){}"
# define ALLOWED_MISCL ALLOWED_TYPES ALLOWED_INTRN

# define ALLOWED_DBLBL ALLOWED_IDENT ALLOWED_MISCL

#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_map>

std::unordered_map<std::string, CLabelTable*> CLabelTable::g_Table;

static std::string to_lower(std::string_view s)
{
	std::string res(s);
	std::transform(res.begin(), res.end(), res.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return res;
}
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CLabelTable::CLabelTable()
{
	m_pName=nullptr;
	m_dwCodeIndex=0;
	m_dwDataIndex=0;
	m_dwBytePos=0;
	m_pSRef=nullptr; // Reference Only

	m_pNext=nullptr;
	m_pPrev=nullptr;
}

CLabelTable::CLabelTable(LPCSTR pStr)
{
	m_pName.reset(new CStr(pStr));
	m_dwCodeIndex=0;
	m_dwDataIndex=0;
	m_dwBytePos=0;

	m_pNext=nullptr;
	m_pPrev=nullptr;

#ifdef __AARON_LBLTBLPERF__
	std::string lowerStr = to_lower(pStr);
	assert_msg(g_Table.find(lowerStr) == g_Table.end() || g_Table[lowerStr] == nullptr, "Label already exists");
	g_Table[lowerStr] = this;
#endif
}

CLabelTable::~CLabelTable()
{
#ifdef __AARON_LBLTBLPERF__
	if (m_pName)
	{
		std::string lowerStr = to_lower(m_pName->GetStr());
		auto it = g_Table.find(lowerStr);
		if (it != g_Table.end() && it->second == this)
		{
			g_Table.erase(it);
		}
	}
#endif
}

void CLabelTable::Free(void)
{
#ifdef __AARON_LBLTBLPERF__
	g_Table.clear();
#endif
	CLabelTable* pCurrent = this;
	while(pCurrent)
	{
		CLabelTable* pNext = pCurrent->GetNext();
		delete pCurrent;
		pCurrent = pNext;
	}
}

void CLabelTable::Add(CLabelTable* pNew)
{
	CLabelTable* pCurrent = this;
	while(pCurrent->m_pNext)
	{
		pCurrent=pCurrent->GetNext();
	}
	pCurrent->m_pNext=pNew;
	pNew->m_pPrev=pCurrent;
}

void CLabelTable::Insert(CLabelTable* pNew)
{
	// Get neighbors
	CLabelTable* pNeighA = m_pPrev;
	CLabelTable* pNeighB = this;

	// Instruct neighbours to point to me
	if(pNeighA) pNeighA->m_pNext = pNew;
	pNeighB->m_pPrev = pNew;

	// Insruct new to point to neighbors
	pNew->m_pNext = pNeighB;
	pNew->m_pPrev = pNeighA;
}

void CLabelTable::AddInOrder(LPCSTR pName, CLabelTable* pNew)
{
	// Find place to insert new variable
	CLabelTable* pLocation = this->GetNext();
	while(pLocation)
	{
		if(_stricmp(pName,pLocation->GetName()->GetStr())<0) break;
		pLocation=pLocation->GetNext();
	}
	if(pLocation)
	{
		// Insert before this location
		pLocation->Insert(pNew);
	}
	else
	{
		// Add to end of list
		Add(pNew);
	}
}

CLabelTable* CLabelTable::Advance(DWORD dwCountdown)
{
	if(dwCountdown==0)
		return this;
	else
		if(m_pNext)
			return m_pNext->Advance(dwCountdown-1);

	return nullptr;
}

CLabelTable* CLabelTable::Subtract(DWORD dwCountdown)
{
	if(dwCountdown==0)
		return this;
	else
		if(m_pPrev)
			return m_pPrev->Subtract(dwCountdown-1);

	return nullptr;
}

bool CLabelTable::AddLabel(LPCSTR pStrName, DWORD dwCodeIndex, DWORD dwDataIndex, CStatement* pSRef)
{
	// Make string
	auto pStr = std::make_unique<CStr>(pStrName);

	// Remove colon from label
	DWORD length = pStr->Length()-1;
	if(pStr->GetChar(length)==':')
		pStr->SetChar(length,0);

	// Ensure label is unique (already got)
	if(FindLabel(pStr->GetStr())!=nullptr)
	{
		return true;
	}

	// Create new data item
	CLabelTable* pNewData = new CLabelTable;

	// Set data for label
	CStr* pStrRaw = pStr.get();
	pNewData->SetName(pStr.release());
	pNewData->SetCodeIndex(dwCodeIndex);
	pNewData->SetDataIndex(dwDataIndex);
	pNewData->SetBytePosition(0);
	pNewData->SetSRef(pSRef);

#ifdef __AARON_LBLTBLPERF__
	std::string lowerName = to_lower(pStrRaw->GetStr());
	assert_msg(g_Table.find(lowerName) == g_Table.end() || g_Table[lowerName] == nullptr, "Label already exists");
	g_Table[lowerName] = pNewData;
#endif

	// Add to Table
	AddInOrder(pStrRaw->GetStr(), pNewData);

	// Increment table entry count
	g_pStatementList->IncLabelQtyCounter(1);

	// Complete
	return true;
}

CLabelTable* CLabelTable::FindLabel(LPCSTR pLabelName)
{
	if (!pLabelName)
		return nullptr;

	std::string lowerLabelName = to_lower(pLabelName);
	auto it = g_Table.find(lowerLabelName);
	if (it == g_Table.end() || !it->second)
		return nullptr;

	return it->second;
}

bool CLabelTable::UpdateLabel(LPCSTR pStrName, DWORD dwCodeIndex, DWORD dwDataIndex, CStatement* pSRef)
{
	CLabelTable* pLabel = FindLabel(pStrName);
	if(pLabel)
	{
		// Set data for label
		pLabel->GetName()->SetText(pStrName);
		pLabel->SetCodeIndex(dwCodeIndex);
		pLabel->SetDataIndex(dwDataIndex);
		pLabel->SetBytePosition(0);
		pLabel->SetSRef(pSRef);
	}
	return true;
}

void CLabelTable::UpdateDataIndexOfLabelsAtLine(CStatement* pStatementRef, DWORD dwData)
{
	CLabelTable* pCurrent = GetNext();
	while(pCurrent)
	{
		// Store DBM write position to label for later ref-replacement
		if(pCurrent->GetSRef()==pStatementRef) pCurrent->SetBytePosition(dwData);
		pCurrent = pCurrent->GetNext();
	}
}

bool CLabelTable::WriteDBMHeader(void)
{
	// Blank Line
	CStr strDBMBlank(1);
	if(g_pDBMWriter->OutputDBM(&strDBMBlank)==false) return false;

	// header Line
	CStr strDBMLine(256);
	strDBMLine.SetText("LABELS:");
	if(g_pDBMWriter->OutputDBM(&strDBMLine)==false) return false;

	return true;
}

bool CLabelTable::WriteDBM(void)
{
	if(GetName())
	{
		// Write out text
		CStr strDBMLine(256);
		strDBMLine.SetText(GetName());
		strDBMLine.AddText(" code:");
		strDBMLine.AddNumericText(GetCodeIndex());
		strDBMLine.AddText(" data:");
		strDBMLine.AddNumericText(GetDataIndex());
		strDBMLine.AddText(" byte:");
		strDBMLine.AddNumericText(GetBytePosition());

		// Output details
		if(g_pDBMWriter->OutputDBM(&strDBMLine)==false) return false;
	}

	// Write next one
	if(GetNext())
	{
		if((GetNext()->WriteDBM())==false) return false;
	}

	// Complete
	return true;
}
