// Declaration.cpp: implementation of the CDeclaration class.
//
//////////////////////////////////////////////////////////////////////
#include "ParserHeader.h"
#include "StringUtils.h"

// Common Includes
#include "Declaration.h"
#include "VarTable.h"

// External Class Pointer
extern CVarTable *g_pVarTable;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CDeclaration::CDeclaration()
	: m_dwLineNumber(0), m_dwArr(0), m_dwOffset(0), m_dwDataSize(0), m_pPrev(nullptr)
{
}

CDeclaration::~CDeclaration()
{
	// Iteratively release chain to prevent stack overflow on deep lists
	auto current = std::move(m_pNext);
	while (current) {
		current = std::move(current->m_pNext);
	}
}

void CDeclaration::Add(CDeclaration* pNew)
{
	if(!m_pNext)
	{
		pNew->m_pPrev=this;
		m_pNext.reset(pNew);
	}
	else
		m_pNext->Add(pNew);
}

CDeclaration* CDeclaration::Find(std::string_view name, uint32_t dwArrFlag)
{
	CDeclaration* pCurrent = this;
	while(pCurrent)
	{
		if(dbp::iequals(name, pCurrent->GetNameView())
		&& pCurrent->GetArrFlag()==dwArrFlag)
			return pCurrent;

		pCurrent=pCurrent->GetNext();
	}
	return nullptr;
}

void CDeclaration::SetDecData(uint32_t dwDecArr, std::string_view arrVal, std::string_view name, std::string_view type, std::string_view init, uint32_t lineNumberRef)
{
	// Set data
	SetArr(dwDecArr);
	SetArrValue(arrVal);
	SetName(name);
	SetType(type);
	SetInit(init);
	SetLineNumber(lineNumberRef);
	SetOffset(0);
	SetDataSize(0);
}

bool CDeclaration::GetNumberOfDecsInChain(uint32_t* pdwCount)
{
	(*pdwCount)++;
	if(GetNext())
		return GetNext()->GetNumberOfDecsInChain(pdwCount);

	return true;
}

std::string CDeclaration::GetTypeStringOfDecsInChain(void)
{
	// Collect characters rep. types that make up dec chain
	std::string typeString;
	CDeclaration* pEntry = this;
	while(pEntry)
	{
		std::string_view typeName = pEntry->GetTypeView();
		DWORD dwTypeValue = g_pVarTable->GetBasicTypeValue(typeName.data());
		typeString += g_pVarTable->GetCharOfType(dwTypeValue);
		pEntry = pEntry->GetNext();
	}

	// Complete
	return typeString;
}

bool CDeclaration::WriteDBM(void)
{
	// Write out text
	std::string dbmLine = "STRUCT@";
	if (m_pName) dbmLine += m_pName->View();
	if (GetArrFlag() == 1)
	{
		dbmLine += "(array)";
	}
	dbmLine += "    TYPE>";
	if (m_pType) dbmLine += m_pType->View();
	dbmLine += "    OFFSET>";
	dbmLine += std::to_string(m_dwOffset);

	// Output type details
	if (!g_pDBMWriter->OutputDBM(dbmLine)) return false;

	// Write next one
	if (GetNext())
	{
		if (!GetNext()->WriteDBM()) return false;
	}

	// Complete
	return true;
}
