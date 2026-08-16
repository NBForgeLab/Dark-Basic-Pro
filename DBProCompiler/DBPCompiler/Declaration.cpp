// Declaration.cpp: implementation of the CDeclaration class.
//
//////////////////////////////////////////////////////////////////////
#include "ParserHeader.h"

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

CDeclaration* CDeclaration::Find(LPCSTR pName, DWORD dwArrFlag)
{
	CDeclaration* pCurrent = this;
	while(pCurrent)
	{
		if(pCurrent->GetName() && _stricmp(pName, pCurrent->GetName()->GetStr())==0
		&& pCurrent->GetArrFlag()==dwArrFlag)
			return pCurrent;

		pCurrent=pCurrent->GetNext();
	}
	return nullptr;
}

void CDeclaration::SetDecData(DWORD dwDecArr, LPCSTR pDecArrValue, LPCSTR pDecName, LPCSTR pDecType, LPCSTR pDecInit, DWORD LineNumberRef)
{
	// Set data
	SetArr(dwDecArr);
	SetArrValue(new CStr(pDecArrValue));
	SetName(new CStr(pDecName));
	SetType(new CStr(pDecType));
	SetInit(new CStr(pDecInit));
	SetLineNumber(LineNumberRef);
	SetOffset(0);
	SetDataSize(0);
}

bool CDeclaration::GetNumberOfDecsInChain(DWORD* pdwCount)
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
		LPSTR pTypeNameString = pEntry->GetType()->GetStr();
		DWORD dwTypeValue = g_pVarTable->GetBasicTypeValue(pTypeNameString);
		typeString += g_pVarTable->GetCharOfType(dwTypeValue);
		pEntry=pEntry->GetNext();
	}

	// Complete
	return typeString;
}

bool CDeclaration::WriteDBM(void)
{
	// Write out text
	CStr strDBMLine(256);
	strDBMLine.SetText("STRUCT@");
	strDBMLine.AddText(m_pName.get());
	if(GetArrFlag()==1)
	{
		strDBMLine.AddText("(array)");
	}
	strDBMLine.AddText("    TYPE>");
	strDBMLine.AddText(m_pType.get());
	strDBMLine.AddText("    OFFSET>");
	strDBMLine.AddNumericText(m_dwOffset);

	// Output type details
	if(g_pDBMWriter->OutputDBM(&strDBMLine)==false) return false;

	// Write next one
	if(GetNext())
	{
		if((GetNext()->WriteDBM())==false) return false;
	}

	// Complete
	return true;
}
