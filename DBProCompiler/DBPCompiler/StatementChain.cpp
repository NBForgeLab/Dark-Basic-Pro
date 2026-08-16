// StatementChain.cpp: implementation of the CStatementChain class.
//
//////////////////////////////////////////////////////////////////////

#include "StatementChain.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CStatementChain::CStatementChain()
{
	m_FirstStatmentBlock=nullptr;
	m_pNext=nullptr;
}

CStatementChain::~CStatementChain()
{
	SAFE_DELETE(m_FirstStatmentBlock);
	SAFE_DELETE(m_pNext);
}

void CStatementChain::Add(CStatementChain *pNext)
{
	if(m_pNext==nullptr)
	{
		m_pNext=pNext;
	}
	else
	{	
		m_pNext->Add(pNext);
	}
}
