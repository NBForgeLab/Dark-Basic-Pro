// ParseJump.cpp: implementation of the CParseJump class.
//
//////////////////////////////////////////////////////////////////////

#include "ParseJump.h"
#include "ParseInstruction.h"
#include "InstructionTable.h"
#include "DBPCompiler.h"

// Externals to assist in math call
extern CInstructionTable* g_pInstructionTable;
extern CDBPCompiler* g_pDBPCompiler;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CParseJump::CParseJump()
{
	m_dwStartLineNumber=0;
	m_dwMiddleLineNumber=0;
	m_dwEndLineNumber=0;

	m_dwJumpType=0;
	m_pCodeBlockA=nullptr;
	m_pCodeBlockB=nullptr;
	m_pExitLabelParameterRef=nullptr;//ref only
}

CParseJump::~CParseJump()
{
	// Statements Deleted One by One
	m_pCodeBlockA->Free();
	m_pCodeBlockA=nullptr;

	// Statements Deleted One by One
	m_pCodeBlockB->Free();
	m_pCodeBlockB=nullptr;

	// unique_ptr members auto-cleanup
}

bool CParseJump::WriteDBM([[maybe_unused]] DWORD PlacementCode)
{
	if(GetJumpType()==JUMPTYPE_IF)
	{
		// IF Statement
		// Use parameters if valid
		if(m_pParameter)
		{
			// Write out parameter traversals
			m_pParameter->WriteDBM();

			// Set the CMP Instruction for the Condition
			g_pASMWriter->WriteASMTaskP1(GetStartLineNumber(), static_cast<DWORD>(ASMTask::Condition), m_pParameter->GetMathItem()->FindResultData());

			// Set the JE Instruction for the Condition
			std::string jumpToLabel = GetBlockLabelB();
			if(jumpToLabel.empty()) jumpToLabel = GetBlockLabelA();
			CStr pJumpToLabel(jumpToLabel.data());
			g_pASMWriter->WriteASMTaskCoreP1(GetStartLineNumber(), static_cast<DWORD>(ASMTask::CondJumpE), &pJumpToLabel, 10);
		}
		if(GetBlockA())
		{
			// BLOCK A
			GetBlockA()->WriteDBM();

			// Jump if ELSE
			if(GetBlockB())
			{
				// Set the JMP Instruction for the Condition
				std::string jumpToLabel = GetBlockLabelA();
				CStr pJumpToLabel(jumpToLabel.data());
				g_pASMWriter->WriteASMTaskCoreP1(GetStartLineNumber(), static_cast<DWORD>(ASMTask::Jump), &pJumpToLabel, 10);
			}
		}
		if(GetBlockB())
		{
			// BLOCK B
			GetBlockB()->WriteDBM();
		}
	}
	if(GetJumpType()==JUMPTYPE_GOTO)
	{
		// LEEFIX - Nono - 111002 - GOT is dangerous and fast - fast main loop option!!
		// LEEFIX - 101102 - Add Compiler Switch STABLE/FAST option
		if(g_pDBPCompiler->GetSpeedOverStabilityFlag()==false)
		{
			// As GOTO can be used to bypass a loop (causing freezes), insert a message call
			if ( g_pDBPCompiler->m_bRemoveSafetyCode==false )
			{
				CParseInstruction pTemp;
				pTemp.SetLineNumber(GetStartLineNumber());
				pTemp.PassStartEndCharForPossibleDebugHook(0, 0);
				pTemp.WriteDBMHardCode(static_cast<DWORD>(BuildTask::Sync), nullptr, nullptr, nullptr);
			}
		}

		// Get Label to Jump To
		LPSTR pLabelStr = nullptr;
		if(GetExitLabelRefParameterRef())
			pLabelStr = GetExitLabelRefParameterRef()->GetMathItem()->GetResultStringToken()->GetStr();
		else
			pLabelStr = m_pParameter->GetMathItem()->GetResultStringToken()->GetStr();

		// Set the JMP Instruction
		CStr pData(pLabelStr);
		g_pASMWriter->WriteASMTaskCoreP1(GetStartLineNumber(), static_cast<DWORD>(ASMTask::Jump), &pData, 10);

	}
	if(GetJumpType()==JUMPTYPE_GOSUB)
	{
		// Get Label to Jump To
		LPSTR pLabelStr = m_pParameter->GetMathItem()->GetResultStringToken()->GetStr();

		// Set the JMP Instruction
		CStr pData(pLabelStr);
		g_pASMWriter->WriteASMTaskCoreP1(GetStartLineNumber(), static_cast<DWORD>(ASMTask::JumpSubroutine), &pData, 10);
	}
	if(GetJumpType()==JUMPTYPE_SELECT)
	{
		// SELECT Statement
		m_pParameter->WriteDBM();

		// Create Condition Jump Chain
		CParameter* pCaseLabel=m_pLabelParameter.get();
		CParameter* pCaseCondition=m_pCaseParameter.get();

		// If string select, use string comparison chain
		// LEEFIX - 201102 - Added support for 103 which is a sting 'array'! && GetResultType to FindResultTypeValueForDBM
		if(m_pParameter->GetMathItem()->FindResultTypeValueForDBM()==3
		|| m_pParameter->GetMathItem()->FindResultTypeValueForDBM()==103)
		{
			// Compares strings against string pointed to by m_pParameter
			while(pCaseCondition)
			{
				// Push Strings to stack
				// LEEFIX - 081102 - SELECT in Functions did not resolve from decorated function-var-name
//				g_pASMWriter->WriteASMTaskP1(GetStartLineNumber(), static_cast<DWORD>(ASMTask::Push), m_pParameter->GetMathItem()->GetResultData());
				g_pASMWriter->WriteASMTaskP1(GetStartLineNumber(), static_cast<DWORD>(ASMTask::Push), m_pParameter->GetMathItem()->FindResultData());
				g_pASMWriter->WriteASMTaskP1(GetStartLineNumber(), static_cast<DWORD>(ASMTask::Push), pCaseCondition->GetMathItem()->FindResultData());

				// CALL String Comparison, result in RAX
				CInstructionTableEntry* pRef=g_pInstructionTable->GetRef(static_cast<DWORD>(InternalInstruction::EqualSS));
				LPSTR pMathCommand=pRef->GetDecoratedName()->GetStr();
				g_pASMWriter->WriteASMCall(GetStartLineNumber(), "dbprocore.dll", pMathCommand);

				// Free stack
				g_pASMWriter->WriteASMTaskP1(GetStartLineNumber(), static_cast<DWORD>(ASMTask::PopRbx), nullptr);
				g_pASMWriter->WriteASMTaskP1(GetStartLineNumber(), static_cast<DWORD>(ASMTask::PopRbx), nullptr);

				// If RAX is one, jump to the label colding the case code
				CStr pOne("1");
				g_pASMWriter->WriteASMTaskCoreP1(GetStartLineNumber(), static_cast<DWORD>(ASMTask::ConditionData), &pOne, 7);
				CStr* pJumpToLabel=pCaseLabel->GetMathItem()->FindResultStringTokenForDBM();
				g_pASMWriter->WriteASMTaskCoreP1(GetStartLineNumber(), static_cast<DWORD>(ASMTask::CondJumpE), pJumpToLabel, 10);

				// Next in chain
				pCaseCondition=pCaseCondition->GetNext();
				pCaseLabel=pCaseLabel->GetNext();
			}
		}
		else
		{
			// Move Variable into RAX
			g_pASMWriter->WriteASMTaskP1(GetStartLineNumber(), static_cast<DWORD>(ASMTask::AssignToRax), m_pParameter->GetMathItem()->FindResultData());

			// Compares numeric values against RAX
			while(pCaseCondition)
			{
				// Set the CMP Instruction for the Condition
				g_pASMWriter->WriteASMTaskP1(GetStartLineNumber(), static_cast<DWORD>(ASMTask::ConditionData), pCaseCondition->GetMathItem()->FindResultData());

				// Set the JE Instruction for the Condition
				CStr* pJumpToLabel=pCaseLabel->GetMathItem()->FindResultStringTokenForDBM();
				g_pASMWriter->WriteASMTaskCoreP1(GetStartLineNumber(), static_cast<DWORD>(ASMTask::CondJumpE), pJumpToLabel, 10);

				// Next in chain
				pCaseCondition=pCaseCondition->GetNext();
				pCaseLabel=pCaseLabel->GetNext();
			}
		}

		// Add case default block if present
		if(GetBlockB())
		{
			// Write Out Case Default Block
			GetBlockB()->WriteDBM();
		}

		// Set the JMP Instruction to skip all case code
		std::string skipToLabel = GetBlockLabelA();
		CStr pSkipToLabel(skipToLabel.data());
		g_pASMWriter->WriteASMTaskCoreP1(GetStartLineNumber(), static_cast<DWORD>(ASMTask::Jump), &pSkipToLabel, 10);

		// Write Out Case Blocks
		CStatementChain* pStatementBlock = GetBlockChain();
		while(pStatementBlock)
		{
			// Get Statement
			CStatement* pStatementRef = pStatementBlock->GetStatementBlock();

			// Write out block
			pStatementRef->WriteDBM();

			// Set the JMP Instruction to skip all case code
			g_pASMWriter->WriteASMTaskCoreP1(pStatementRef->GetLineNumber(), static_cast<DWORD>(ASMTask::Jump), &pSkipToLabel, 10);

			// Next in chain
			pStatementBlock=pStatementBlock->GetNext();
		}
	}
	return true;
}

bool CParseJump::WriteDBMBit(DWORD dwLineNumber, std::string_view text, std::string_view result)
{
	// Write out text
	CStr strDBMLine(256);
	strDBMLine.SetNumericText(dwLineNumber);
	strDBMLine.AddText(" ");
	strDBMLine.AddText(text);
	strDBMLine.AddText(" ");
	strDBMLine.AddText(result);
	if(g_pDBMWriter->OutputDBM(&strDBMLine)==false) return false;

	return true;
}
