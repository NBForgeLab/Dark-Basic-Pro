#pragma once
#include "ParserHeader.h"

class CStatementChain  
{
	public:
		CStatementChain();
		virtual ~CStatementChain();
		CStatementChain* GetNext(void) { return m_pNext; }
		void Add(CStatementChain* pChain);

		void SetStatementBlock(CStatement* pChain) { m_FirstStatmentBlock=pChain; }
		CStatement* GetStatementBlock(void) { return m_FirstStatmentBlock; }

	private:

		// Statement Block Data
		CStatement*				m_FirstStatmentBlock;


		// Hierarchy Data
		CStatementChain*		m_pNext;
};