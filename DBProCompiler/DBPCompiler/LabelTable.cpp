// LabelTable.cpp: implementation of the CLabelTable class.
//
//////////////////////////////////////////////////////////////////////

#include "LabelTable.h"
#include "Statement.h"
#include "StatementList.h"
#include "DBMWriter.h"
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
	m_pName = pStr ? std::make_unique<CStr>(pStr) : nullptr;
	m_dwCodeIndex=0;
	m_dwDataIndex=0;
	m_dwBytePos=0;
	m_pSRef=nullptr;

	m_orderIndex=static_cast<size_t>(-1);

#ifdef __AARON_LBLTBLPERF__
	if (pStr)
	{
		std::string lowerStr = dbp::to_lower_copy(pStr);
		assert_msg(g_Table.find(lowerStr) == g_Table.end() || g_Table[lowerStr] == nullptr, "Label already exists");
		g_Table[lowerStr] = this;
	}
#endif
}

CLabelTable::CLabelTable(std::string_view name)
{
	m_pName = std::make_unique<CStr>(name);
	m_dwCodeIndex=0;
	m_dwDataIndex=0;
	m_dwBytePos=0;
	m_pSRef=nullptr;

	m_orderIndex=static_cast<size_t>(-1);

#ifdef __AARON_LBLTBLPERF__
	std::string lowerStr = dbp::to_lower_copy(name);
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
	if ( g_Order.empty() )
	{
		delete this;
		return;
	}
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

void CLabelTable::AddInOrder(std::string_view name, CLabelTable* pNew)
{
	// Find place to insert new variable
	CLabelTable* pLocation = this->GetNext();
	while(pLocation)
	{
		if(dbp::icompare(name, pLocation->GetNameView()) < 0) break;
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

bool CLabelTable::AddLabel(std::string_view strName, DWORD dwCodeIndex, DWORD dwDataIndex, CStatement* pSRef)
{
	if (strName.empty()) return false;
	if (strName.back() == ':')
		strName.remove_suffix(1);

	// Ensure label is unique
	if (FindLabel(strName) != nullptr)
	{
		return true;
	}

	// Create new data item
	CLabelTable* pNewData = new CLabelTable;
	pNewData->SetName(strName);
	pNewData->SetCodeIndex(dwCodeIndex);
	pNewData->SetDataIndex(dwDataIndex);
	pNewData->SetBytePosition(0);
	pNewData->SetSRef(pSRef);

#ifdef __AARON_LBLTBLPERF__
	std::string lowerName = dbp::to_lower_copy(strName);
	assert_msg(g_Table.find(lowerName) == g_Table.end() || g_Table[lowerName] == nullptr, "Label already exists");
	g_Table[lowerName] = pNewData;
#endif

	// Add to Table
	AddInOrder(pNewData->GetNameStr(), pNewData);

	// Increment table entry count
	if (g_pStatementList)
		g_pStatementList->IncLabelQtyCounter(1);

	return true;
}

CLabelTable* CLabelTable::FindLabel(std::string_view labelName)
{
	if (labelName.empty())
		return nullptr;

	std::string lowerLabelName = dbp::to_lower_copy(labelName);
	auto it = g_Table.find(lowerLabelName);
	if (it == g_Table.end() || !it->second)
		return nullptr;

	return it->second;
}

bool CLabelTable::UpdateLabel(std::string_view strName, DWORD dwCodeIndex, DWORD dwDataIndex, CStatement* pSRef)
{
	CLabelTable* pLabel = FindLabel(strName);
	if (pLabel)
	{
		pLabel->SetName(strName);
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
		if(pCurrent->GetSRef()==pStatementRef)
		{
			pCurrent->SetBytePosition(dwData);
		}
		pCurrent = pCurrent->GetNext();
	}
}

bool CLabelTable::WriteDBMHeader(void)
{
	// Blank Line
	if (!g_pDBMWriter->OutputDBM("")) return false;

	// header Line
	if (!g_pDBMWriter->OutputDBM("LABELS:")) return false;

	return true;
}

bool CLabelTable::WriteDBM(void)
{
	if (m_pName)
	{
		// Write out text
		std::string dbmLine;
		dbmLine += m_pName->View();
		dbmLine += " code:";
		dbmLine += std::to_string(GetCodeIndex());
		dbmLine += " data:";
		dbmLine += std::to_string(GetDataIndex());
		dbmLine += " byte:";
		dbmLine += std::to_string(GetBytePosition());

		// Output details
		if (!g_pDBMWriter->OutputDBM(dbmLine)) return false;
	}

	// Write next one
	if (GetNext())
	{
		if (!GetNext()->WriteDBM()) return false;
	}

	// Complete
	return true;
}
