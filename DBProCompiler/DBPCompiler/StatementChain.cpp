// StatementChain.cpp: implementation of the CStatementChain class.
//
//////////////////////////////////////////////////////////////////////

#include "StatementChain.h"
#include "Statement.h"

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
	SafeDelete(m_FirstStatmentBlock);
	SafeDelete(m_pNext);
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
