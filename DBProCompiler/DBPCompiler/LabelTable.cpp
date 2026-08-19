// LabelTable.cpp: implementation of the CLabelTable class.
//
//////////////////////////////////////////////////////////////////////

#include "LabelTable.h"
#include "StringUtils.h"
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
std::vector<CLabelTable*> CLabelTable::g_Order;

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

	m_orderIndex=static_cast<size_t>(-1);
}

CLabelTable::CLabelTable(LPCSTR pStr)
{
	m_pName.reset(new CStr(pStr));
	m_dwCodeIndex=0;
	m_dwDataIndex=0;
	m_dwBytePos=0;

	m_orderIndex=static_cast<size_t>(-1);

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
	// delete every node tracked in the declaration-order index
	for ( CLabelTable* pNode : g_Order )
		delete pNode;
	g_Order.clear();
}

void CLabelTable::Add(CLabelTable* pNew)
{
	// register the head of the list (this) on first use
	if ( g_Order.empty() )
	{
		g_Order.push_back(this);
		this->m_orderIndex=0;
	}
	g_Order.push_back(pNew);
	pNew->m_orderIndex=g_Order.size()-1;
}

void CLabelTable::Insert(CLabelTable* pNew)
{
	// register the head of the list (this) on first use
	if ( g_Order.empty() )
	{
		g_Order.push_back(this);
		this->m_orderIndex=0;
	}
	// defensive: an unregistered node must be the head of the list
	if ( this->m_orderIndex==static_cast<size_t>(-1) )
	{
		g_Order.insert(g_Order.begin(), this);
		this->m_orderIndex=0;
	}
	// insert pNew immediately before this node
	g_Order.insert( g_Order.begin() + static_cast<std::ptrdiff_t>(this->m_orderIndex), pNew );
	// refresh indices from the insertion point onwards
	for ( size_t i=this->m_orderIndex; i<g_Order.size(); i++ )
		g_Order[i]->m_orderIndex=i;
}

void CLabelTable::AddInOrder(LPCSTR pName, CLabelTable* pNew)
{
	// Find place to insert new variable
	CLabelTable* pLocation = this->GetNext();
	while(pLocation)
	{
		if(dbp::icompare(pName,pLocation->GetName()->GetStr()) < 0) break;
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
	if ( dwCountdown==0 || m_orderIndex==static_cast<size_t>(-1) )
		return this;
	if ( m_orderIndex + dwCountdown >= g_Order.size() )
		return nullptr;
	return g_Order[m_orderIndex + dwCountdown];
}

CLabelTable* CLabelTable::Subtract(DWORD dwCountdown)
{
	if ( dwCountdown==0 || m_orderIndex==static_cast<size_t>(-1) )
		return this;
	if ( m_orderIndex < dwCountdown )
		return nullptr;
	return g_Order[m_orderIndex - dwCountdown];
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
