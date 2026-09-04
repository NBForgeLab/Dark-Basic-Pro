// ParseInstruction.cpp: implementation of the CParseInstruction class.
//
//////////////////////////////////////////////////////////////////////

// Includes
#include "VarTable.h"
#include "StructTable.h"
#include "Declaration.h"
#include "Statement.h"
#include "StatementList.h"
#include "Error.h"
#include "InstructionTable.h"
#include "InstructionTableEntry.h"
#include "ParserHeader.h"
#include "ParseInstruction.h"
#include "ParseuserFunction.h"
#include "ASMWriter.h"
#include "DebugInfo.h"
#include "DBPCompiler.h"

// External class pointers
extern CVarTable* g_pVarTable;
extern CStructTable* g_pStructTable;
extern CInstructionTable* g_pInstructionTable;
extern CDebugInfo g_DebugInfo;
extern CDBPCompiler* g_pDBPCompiler;

// External Scope Ptrs
extern CParseUserFunction* g_pUserFunctionWithin;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CParseInstruction::CParseInstruction()
{
	m_dwLineNumber=0;
	m_dwInstructionType=0;
	m_dwInstructionValue=0;
	m_dwInstructionParamMax=0;

	// Reference Only
	m_pRefInstructionEntry=nullptr;
}

CParseInstruction::~CParseInstruction()
{
	// unique_ptr members auto-cleanup
}

bool CParseInstruction::ActOnSingleVar ( CResultData* pVar, uint32_t dwType, int iDisplacement)
{
	// Only strings
	if(dwType==3)
	{
		// Write offset out
		CStr pData("");
		pData.AddNumericText(iDisplacement);

		// Local Var - not part of param-in data
		CStr pNull("0");

		// Determine natural mode, and make sure its an offset to get at the UDT element
		DWORD dwAccessMode = g_pASMWriter->DetMode(pVar->m_pStringToken.get(), pVar->m_dwType, pVar->m_dwDataOffset, pVar->m_pAdditionalOffset.get());
		if ( dwAccessMode==static_cast<DWORD>(ParamMode::Mem) ) dwAccessMode=static_cast<DWORD>(ParamMode::MemOff);
		if ( dwAccessMode==static_cast<DWORD>(ParamMode::Rbp) ) dwAccessMode=static_cast<DWORD>(ParamMode::RbpOff);

		// An array element is addressed through the direct-layout element data, where
		// the emitter selects its operand width from switch(dwPType-100). The bare
		// field type underflows that switch into the relative-address default arm,
		// which truncates the 8-byte string pointer to a DWORD on x64.
		const bool bIsArrayElement = dwAccessMode==static_cast<DWORD>(ParamMode::MemArr)
		                          || dwAccessMode==static_cast<DWORD>(ParamMode::RbpArr);
		const uint32_t dwMemberType = bIsArrayElement ? dwType+100 : dwType;

		// PUSH STRING FROM UDT TO STACK
		g_pASMWriter->WriteASMXtoRAX(dwAccessMode, pVar->m_pStringToken.get(), pVar->m_pAdditionalOffset.get(), dwMemberType, iDisplacement);
		g_pASMWriter->WriteASMRAXtoX(static_cast<DWORD>(ParamMode::Stack), nullptr, nullptr, 3, iDisplacement);
		g_pASMWriter->RecordPendingCallArg(3, 1u);
		g_pASMWriter->WriteASMComment("PUSH TO STACK", "", "", "");

		// Pass DEST + CURRENT STRING (same address)
		g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::Push), &pNull, 7);

		// CALL EQUATE to create a NEW STRING from CURRENT STRING
		g_pASMWriter->WriteASMCall(m_dwLineNumber, "dbprocore.dll", "?EquateSS@@YA_K_K0@Z");

		// Put RAX overwrites DEST
		g_pASMWriter->WriteASMRAXtoX(dwAccessMode, pVar->m_pStringToken.get(), pVar->m_pAdditionalOffset.get(), dwMemberType, iDisplacement);
		g_pASMWriter->WriteASMComment("ASSIGN RAX TO X", "", "", "");

		// Pop param data
		g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::PopRbx), nullptr);
		g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::PopRbx), nullptr);
	}

	// Complete
	return true;
}

bool CParseInstruction::ActOnType ( CResultData* pVar, CStr* pTypeName, int iDisplacement)
{
	int iGlobalDisplacement = iDisplacement;
	CStructTable* pStruct = g_pStructTable->DoesTypeEvenExist(pTypeName->GetStr());
	if(pStruct)
	{
		CDeclaration* pCurrent = pStruct->GetDecChain();
		while(pCurrent)
		{
			// Determine local func-mem from local var name
			DWORD dwOffset=0, dwSizeOfData=0;
			if(g_pStructTable->FindOffsetFromField(pTypeName->GetStr(), pCurrent->GetName()->GetStr(), &dwOffset, &dwSizeOfData))
			{
				// Determine type of Field and Offset
				DWORD dwFieldType = g_pVarTable->GetBasicTypeValue(pCurrent->GetType()->GetStr());
				const int iFieldDisplacement = iGlobalDisplacement + static_cast<int>(dwOffset);
		
				// Process Element
				if(dwFieldType==1001)
					ActOnType(pVar, pCurrent->GetType(), iFieldDisplacement);
				else
					ActOnSingleVar(pVar, dwFieldType, iFieldDisplacement);
			}

			// Next declaration
			pCurrent = pCurrent->GetNext();
		}
	}

	// Complete
	return true;
}

bool CParseInstruction::ActOnLocalVars ( CResultData* pVar )
{
	// Apply any data offset to ensure correct place in UDT structure from pVar
	DWORD dwDataOffset=pVar->m_dwDataOffset;

	// Duplicate strings in UDT
	CDeclaration* pCurrent = pVar->m_pStruct->GetDecChain();
	while(pCurrent)
	{
		// Determine type of Local Variable
		DWORD dwDecType = g_pVarTable->GetBasicTypeValue(pCurrent->GetType()->GetStr());
		if(pCurrent->GetArrFlag()==1) dwDecType+=100;
		if(dwDecType==3 || dwDecType>=1001)
		{
			// Determine local func-mem from local var name
			DWORD dwOffset=0, dwSizeOfData=0;
			if(g_pStructTable->FindOffsetFromField(pVar->m_pStruct->GetTypeName()->GetStr(), pCurrent->GetName()->GetStr(), &dwOffset, &dwSizeOfData))
			{
				// Process Element
				if(dwDecType==1001)
				{
					// Element is a user type
					ActOnType(pVar, pCurrent->GetType(), dwDataOffset+dwOffset);
				}
				else
				{
					// Element is a basic type
					ActOnSingleVar(pVar, dwDecType, dwDataOffset+dwOffset);
				}
			}
		}
		pCurrent=pCurrent->GetNext();
	}
	
	// Complete
	return true;
}

bool CParseInstruction::WriteDBM(void)
{
	// Write out param chain
	if(m_pParameter)
	{
		if(m_pParameter->WriteDBM()==false) return false;
	}

	// Write out function call
	WriteDBMBit();

	return true;
}

bool CParseInstruction::WriteDBMBit(void)
{
	// Write Out parameters to Stack if jump or call instruction
	int iRemoveInParam=0;
	bool bAddParamsToStack=false;
	if(m_dwInstructionType==3) bAddParamsToStack=true;
	if(m_dwInstructionType==2)
	{
		// Instruction Requires Stack Items
		if(m_pRefInstructionEntry)
		{
			if(m_pRefInstructionEntry->GetDLL()->Length()>0)
			{
				bAddParamsToStack=true;
			}

			// Except in cases where instruction input params are output (input a)
			if(m_pRefInstructionEntry->GetReturnParam()>=11
			&& m_pRefInstructionEntry->GetReturnParam()<=19)
			{
				iRemoveInParam=m_pRefInstructionEntry->GetReturnParamPlace();
			}
		}
	}

	// Prepare Return Param Flag
	CResultData* pPutEAXReturn = nullptr;
	std::unique_ptr<CResultData> pPutEAXReturnDynCreated;
	CParameter* pInputParamToUseAsOutput = nullptr;

	// When userfunctions return a string/array, return var must be freed/cleared
	if(m_dwInstructionType==3)
	{
		// Handle Return Value later..
		if(m_pRefInstructionEntry)
		{
			if(GetReturnParameter())
			{
				pPutEAXReturnDynCreated = std::make_unique<CResultData>();
				pPutEAXReturn = pPutEAXReturnDynCreated.get();
				pPutEAXReturn->m_pStringToken = std::make_unique<CStr>(GetReturnParameter()->GetStr());
				pPutEAXReturn->m_dwType = m_pRefInstructionEntry->GetReturnParam();
				pPutEAXReturn->m_pAdditionalOffset.reset();
				pPutEAXReturn->m_dwDataOffset = 0;

				// Ensure Return Param From UserFunction is freed if string or array
				if(pPutEAXReturn->m_dwType==3)
				{
					if(pPutEAXReturn->m_pStringToken) pPutEAXReturn->m_pStringToken->TranslateForDBM(pPutEAXReturn);
					if(pPutEAXReturn->m_pAdditionalOffset) pPutEAXReturn->m_pAdditionalOffset->TranslateForDBM(pPutEAXReturn);

					g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::Push), pPutEAXReturn);
					CInstructionTableEntry* pRef=g_pInstructionTable->GetRef(static_cast<DWORD>(InternalInstruction::StrFree));
					LPSTR pMathCommand=pRef->GetDecoratedName()->GetStr();
					g_pASMWriter->WriteASMCall(m_dwLineNumber, "dbprocore.dll", pMathCommand);
					g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::PopRbx), nullptr);
				}
			}
		}
	}

	DWORD dwMustPopStack=0;
	if(bAddParamsToStack==true)
	{
		// Add to Stack - In Reverse Order
		if(GetParameter())
		{
			// How many params
			int iThisPosition=m_pRefInstructionEntry->GetParamMax();
			CParameter* pCurrent = GetParameter()->GetLast();
			if(pCurrent)
			{
				while(pCurrent)
				{
					// Add to stack or not
					DWORD dwDataType = pCurrent->GetMathItem()->FindResultTypeValueForDBM();
					if(iRemoveInParam!=iThisPosition || dwDataType==103 || dwDataType==13 || dwDataType==3)
					{
						// Finally push result to stack for main instruction
						if ( dwDataType==1001 )
						{
							// UDT is variable size
							CResultData* pResultData = pCurrent->GetMathItem()->FindResultData();
							const DWORD dwSlotBytes = g_pStructTable ? g_pStructTable->GetTargetAddressSize() : 8;
							DWORD dwUDTSize = dwSlotBytes;
							if ( pResultData->m_pStruct ) dwUDTSize = pResultData->m_pStruct->GetTypeSize();
							DWORD dwStackSize = dwUDTSize / dwSlotBytes;
							if ( dwStackSize * dwSlotBytes != dwUDTSize ) dwStackSize++;
							DWORD dwStackSizeInBytes = dwStackSize * dwSlotBytes;

							// Push entire UDT data onto stack (force data through resultstructure)
							CResultData pSizeData;
							pSizeData.m_pStringToken = std::make_unique<CStr>();
							pSizeData.m_pStringToken->SetNumericText(dwStackSizeInBytes);
							pSizeData.m_dwDataOffset = dwStackSize;
							g_pASMWriter->WriteASMTaskP2(m_dwLineNumber, static_cast<DWORD>(ASMTask::PushUdt), pResultData, &pSizeData);

							// Record size on stack (for later removal)
							dwMustPopStack+=dwStackSize;
						}
						else
						{
							// regular DWORD(x2) push
							g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::Push), pCurrent->GetMathItem()->FindResultData());
							if(dwDataType==8 || dwDataType==9 || dwDataType==108 || dwDataType==109)
								dwMustPopStack+=2;
							else
								dwMustPopStack++;
						}
					}

					// Record param for output purposes...
					if(iRemoveInParam>0 && iRemoveInParam==iThisPosition)
						pInputParamToUseAsOutput = pCurrent;
					
					// Prev param
					pCurrent=pCurrent->GetPrev();
					iThisPosition--;
				}
			}
		}
	}

	if(m_dwInstructionType==2)
	{
		// Called Instructions
		if(m_pRefInstructionEntry)
		{
			if(m_pRefInstructionEntry->GetDLL()->Length()>0)
			{
				// Prepare return result
				pPutEAXReturnDynCreated.reset();
				if(m_pRefInstructionEntry->GetReturnParam()>0)
				{
					// If input-specified output present, use it (except if require non-typical datatypes)
					if(pInputParamToUseAsOutput && m_pRefInstructionEntry->GetReturnParam()!=18 && m_pRefInstructionEntry->GetReturnParam()!=19)
					{
						pPutEAXReturn = pInputParamToUseAsOutput->GetMathItem()->FindResultData();
					}
					else
					{
						// Use Return Param, else last param of expression chain
						if(GetReturnParameter())
						{
							pPutEAXReturnDynCreated = std::make_unique<CResultData>();
							pPutEAXReturn = pPutEAXReturnDynCreated.get();
							pPutEAXReturn->m_pStringToken = std::make_unique<CStr>(GetReturnParameter()->GetStr());
							pPutEAXReturn->m_dwType = m_pRefInstructionEntry->GetReturnParam();
							if(pPutEAXReturn->m_dwType>10 && pPutEAXReturn->m_dwType<20) pPutEAXReturn->m_dwType-=10;//INPUT marks param as an output var by setting type as 11-19
							pPutEAXReturn->m_pAdditionalOffset.reset();
							pPutEAXReturn->m_dwDataOffset = 0;
						}
						else
						{
							if(GetParameter()) pPutEAXReturn = GetParameter()->GetMathItem()->FindResultData();
						}
					}
					if(pPutEAXReturn->m_dwType==0) pPutEAXReturn=nullptr;
				}

				// Ensure result is translated
				if(pPutEAXReturn)
				{
					if(pPutEAXReturn->m_pStringToken) pPutEAXReturn->m_pStringToken->TranslateForDBM(pPutEAXReturn);
					if(pPutEAXReturn->m_pAdditionalOffset) pPutEAXReturn->m_pAdditionalOffset->TranslateForDBM(pPutEAXReturn);
				}

				// A Return String must be passed (so it can be freed) All xxxxx[%S
				if(m_pRefInstructionEntry->GetReturnParam()==3
				&& m_pRefInstructionEntry->GetHardcoreInternalValue()!=static_cast<DWORD>(InternalInstruction::AssignSS))
				{
					g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::Push), pPutEAXReturn);
					dwMustPopStack++;
				}

				// DLL Call
				g_pASMWriter->WriteASMCall(m_dwLineNumber, m_pRefInstructionEntry->GetDLL()->GetStr(), m_pRefInstructionEntry->GetDecoratedName()->GetStr());
			}
			else
			{
				DWORD dwBuildID=m_pRefInstructionEntry->GetBuildID();
				if(dwBuildID==0)
				{
					// HARDCODED INTERNAL MATHS AND CASTS
					CParameter* pCurrent = GetParameter();
					if(pCurrent)
					{
						g_pASMWriter->WriteASMTaskP2(m_dwLineNumber, static_cast<DWORD>(ASMTask::Assign), pCurrent->GetMathItem()->FindResultData(), pCurrent->GetNext()->GetMathItem()->FindResultData());
					}
				}
				else
				{
					// HARDCODED INSTRUCTIONS (NO CORE CALLS)
					CParameter* pCurrent = GetParameter();
					if(pCurrent)
					{
						if(pCurrent->GetMathItem())
						{
							if(pCurrent->GetNext()==nullptr)
							{
								// With 1 Param
								WriteDBMHardCode(dwBuildID, pCurrent->GetMathItem()->FindResultData(), nullptr, nullptr);
							}
							else
							{
								if(pCurrent->GetNext()->GetNext()==nullptr)
								{
									// With 2 Params
									WriteDBMHardCode(dwBuildID, pCurrent->GetMathItem()->FindResultData(), pCurrent->GetNext()->GetMathItem()->FindResultData(), nullptr);
								}
								else
								{
									// With 3 Params
									WriteDBMHardCode(dwBuildID, pCurrent->GetMathItem()->FindResultData(), pCurrent->GetNext()->GetMathItem()->FindResultData(), pCurrent->GetNext()->GetNext()->GetMathItem()->FindResultData());
								}
							}
						}
					}
					else
					{
						// No Params
						WriteDBMHardCode(dwBuildID, nullptr, nullptr, nullptr);
					}
				}
			}
		}
	}
	if(m_dwInstructionType==3)
	{
		// User Defined CALL to User Function
		if(m_pRefInstructionEntry)
		{
			// Handle Return Value later..
			if(pPutEAXReturn)
			{
				pPutEAXReturn->m_pStringToken = std::make_unique<CStr>(GetReturnParameter()->GetStr());
				pPutEAXReturn->m_dwType = m_pRefInstructionEntry->GetReturnParam();
			}

			// Set the JMP Instruction
			CStr* pLabelParam=GetLabelParam();
			if(pLabelParam) g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::JumpSubroutine), pLabelParam, 10);
			
			// Return stack pointer to normal
			if(dwMustPopStack>0)
			{
				const DWORD dwSlotBytes = g_pStructTable ? g_pStructTable->GetTargetAddressSize() : 8;
				CStr pData("");
				pData.SetNumericText(dwMustPopStack * dwSlotBytes);
				g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::AddRsp), &pData, 7);
			}

			// No need to pop stack
			dwMustPopStack=0;
		}
	}

	// Handle Return Value from RAX to Return Param
	if(pPutEAXReturn)
	{
		// Ensure return value has been translated (ie @fs  to :-19)
		if(pPutEAXReturn->m_pStringToken) pPutEAXReturn->m_pStringToken->TranslateForDBM(pPutEAXReturn);

		// Copy RAX/RDX contents to Required Return Param
		g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::Assign), pPutEAXReturn);
	}

	// Free usages (unique_ptr auto-cleanup)
	pPutEAXReturnDynCreated.reset();

	// Optional function return param
	if(dwMustPopStack>0)
	{
		while(dwMustPopStack)
		{
			g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::PopRbx), nullptr, 0);
			dwMustPopStack--;
		}
	}

	return true;
}

bool CParseInstruction::WriteDBMHardCode(uint32_t dwBuildID, CResultData* pP1, CResultData* pP2, CResultData* pP3)
{
	switch(dwBuildID)
	{
		case static_cast<DWORD>(BuildTask::Ret):
			if ( g_pUserFunctionWithin && g_pDBPCompiler->m_bRemoveSafetyCode==false )
			{
				// U74 - 110509 - ensure RETURN commands not inside a user function
				char err[512];
				sprintf_s ( err, 512, "Cannot use RETURN command inside a function at line %d.", m_dwLineNumber );
				g_pErrorReport->AddErrorString(err);
				return false;
			}
			else
				g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::Return), nullptr, 0);
			break;

		case static_cast<DWORD>(BuildTask::PureRet):
			g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::PureReturn), nullptr, 0);
			break;

		case static_cast<DWORD>(BuildTask::End):
			{
				// Always jump to END OF PROGRAM
				CStr pAlwaysJump("$labelend");
				g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::Jump), &pAlwaysJump, 10);
			}
			break;

		case static_cast<DWORD>(BuildTask::EndError):
			{
				// Report An Error (user hit the function declaration mid-program)
				g_pASMWriter->WriteASMCall(m_dwLineNumber, "dbprocore.dll", "?EarlyEnd@@YAXXZ");

				// Always jump to END OF PROGRAM
				CStr pAlwaysJump("$labelend");
				g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::Jump), &pAlwaysJump, 10);
			}
			break;

		case static_cast<DWORD>(BuildTask::Sync):
			{
				// If Debug Mode, always push code position to stack
				if(g_DebugInfo.DebugModeOn())
				{
					// Calculate Current Position Of Program
					CStr pData("");
					DWORD dwPosition=g_pASMWriter->GetCurrentMCPosition();
					g_DebugInfo.SetLastBreakPoint(dwPosition);
					pData.SetNumericText(dwPosition);
					g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::Push), &pData, 7);

					// Call PROCESSMESSAGES Function with Debug Prog-Position
					g_pASMWriter->WriteASMCall(m_dwLineNumber, "dbprocore.dll", "?ProcessMessages@@YAKK@Z");

					// Removes Position DWORD back off stack
					g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::PopRbx), nullptr, 0);

					// Check if RAX=1 (got a message to QUIT)
					CStr pCondData("1");
					g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::ConditionData), &pCondData, 7);

					// Only snapshot if parsing main, not mini
					if(g_DebugInfo.GetParsingMain())
					{
						// LEAP-FORWARDS Marker OpCode
						g_pASMWriter->WriteASMLeapMarkerJumpNotEqual(1);

// No Stack save - data would be useless when new m/c executed
//						// Push all registers to stack before snapshot
//						g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::PushRegisters), nullptr, 0);

//						// Push stack location to stack
//						g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::PushRsp), nullptr, 0);

//						// Ending program - Snapshot everything on stack back to beginning of program run
//						g_pASMWriter->WriteASMCall(m_dwLineNumber, "dbprocore.dll", "?StackSnapshotStore@@YAXK@Z");

						// Jump to END OF PROGRAM
						CStr pJumpToLabel("$labelend");
						g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::Jump), &pJumpToLabel, 10);

						// Complete LEAP-FORWARD Marker
						g_pASMWriter->WriteASMLeapMarkerEnd(1);
					}
					else
					{
						// Jump to END OF PROGRAM
						CStr pJumpToLabel("$labelend");
						g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::CondJumpE), &pJumpToLabel, 10);
					}
				}
				else
				{
					// Call PROCESSMESSAGES Function for fullspeed mode
					g_pASMWriter->WriteASMCall(m_dwLineNumber, "dbprocore.dll", "?ProcessMessages@@YAKXZ");

					// Check if RAX=1 (got a message to QUIT)
					CStr pData("1");
					g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::ConditionData), &pData, 7);

					// If so, jump to END OF PROGRAM
					CStr pJumpToLabel("$labelend");
					g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::CondJumpE), &pJumpToLabel, 10);
				}
			}							
			break;

		case static_cast<DWORD>(BuildTask::StartProgram):
			{
				// Store Registers before program begins
				CStr pParam1("@$_RSP_");
				g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::PushRegisters), nullptr, 0);
				g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::StoreRsp), &pParam1, 7);

				// If Debug Mode, special jump if breakpoint needs to be jumped to
				if(g_DebugInfo.DebugModeOn() && g_DebugInfo.GetParsingMain())
				{
					g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::BreakpointResume), nullptr, 0);
				}
			}
			break;
			
		case static_cast<DWORD>(BuildTask::EndProgramAndQuit):
			{
				// Call QUIT Function (to close message loop only for main program)
				if(g_DebugInfo.GetParsingMain())
				{
					// leefix - 120108 - U71 - correct line number
					g_pASMWriter->m_dwLineNumber = m_dwLineNumber;

					// Check if breakpoint active
					g_pASMWriter->WriteASMCheckBreakPointVar();

					// LEAP-FORWARDS Marker OpCode to skip breakpoint-resume
					g_pASMWriter->WriteASMLeapMarkerJumpNotEqual(0);

					// Call Quit which closes the app window
					g_pASMWriter->WriteASMCall(m_dwLineNumber, "dbprocore.dll", "?Quit@@YAKXZ");

					// Complete LEAP-FORWARD Marker (jump here if skip breakpoint resume)
					g_pASMWriter->WriteASMLeapMarkerEnd(0);
				}

				// Compare _RSP_ store with stack
				if(g_DebugInfo.DebugModeOn())
				{
					// If they are different, broke from function or stack leak
					// set special escapecode so cannot resume this program
					CStr pParam1("@$_RSP_");
					g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::SetNoReturnIfRspLeak), &pParam1, 7);
				}

				// Restore STACK Pointer
				CStr pParam1("@$_RSP_");
				g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::RestoreRsp), &pParam1, 7);
				g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::PopRegisters), nullptr, 0);
			}
			break;

		case static_cast<DWORD>(BuildTask::Inc):
		case static_cast<DWORD>(BuildTask::Dec):
			{
				// By 1 value, or Qty
				if(pP2==nullptr)
				{
					// Inc or Dec
					DWORD dwASMToBuild=0;
					if(dwBuildID==static_cast<DWORD>(BuildTask::Inc)) dwASMToBuild=static_cast<DWORD>(ASMTask::IncVar);
					if(dwBuildID==static_cast<DWORD>(BuildTask::Dec)) dwASMToBuild=static_cast<DWORD>(ASMTask::DecVar);

					// Call hard code builder (leefix-260603-passing in all data now as INC can change to ADD)
//					g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, dwASMToBuild, pP1->m_pStringToken, pP1->m_dwType);
					g_pASMWriter->WriteASMTaskCore(	m_dwLineNumber, dwASMToBuild, pP1->m_pStringToken.get(), pP1->m_pAdditionalOffset.get(), pP1->m_dwType, pP1->m_dwDataOffset, nullptr, nullptr, 0, 0, nullptr, nullptr, 0, 0 );
				}
				else
				{
					// Add or Sub
					DWORD dwASMToBuild=0;
					if(dwBuildID==static_cast<DWORD>(BuildTask::Inc)) dwASMToBuild=static_cast<DWORD>(ASMTask::Add);
					if(dwBuildID==static_cast<DWORD>(BuildTask::Dec)) dwASMToBuild=static_cast<DWORD>(ASMTask::Sub);

					// Call hard code builder
					g_pASMWriter->WriteASMTaskCore(	m_dwLineNumber, dwASMToBuild,
													pP1->m_pStringToken.get(), pP1->m_pAdditionalOffset.get(), pP1->m_dwType, pP1->m_dwDataOffset,
													pP2->m_pStringToken.get(), pP2->m_pAdditionalOffset.get(), pP2->m_dwType, pP2->m_dwDataOffset,
													pP1->m_pStringToken.get(), pP1->m_pAdditionalOffset.get(), pP1->m_dwType, pP1->m_dwDataOffset);
				}
			}
			break;

		case static_cast<DWORD>(BuildTask::IncAdd):
		case static_cast<DWORD>(BuildTask::DecAdd):
			{
				// Inc or Dec
				DWORD dwMathSymbol=0;
				if(dwBuildID==static_cast<DWORD>(BuildTask::IncAdd)) dwMathSymbol=4;
				if(dwBuildID==static_cast<DWORD>(BuildTask::DecAdd)) dwMathSymbol=5;

				// If pP2 is empty, assume 1
				std::unique_ptr<CResultData> pDynValue;
				CResultData* pValue = pP2;
				if(pValue==nullptr)
				{
					pDynValue = std::make_unique<CResultData>();
					pDynValue->m_pStringToken = std::make_unique<CStr>(const_cast<LPSTR>("1"));
					pDynValue->m_dwType = pP1->m_dwType % 100;
					pValue = pDynValue.get();
				}

				// Push Params for Add to stack (reverse order)
				g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::Push), pValue);
				g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::Push), pP1);

				// Internal Math Call to DLL
// get INC A,5.0 to parse as an INTEGER command, not a FLOAT (FFF) command...
				DWORD dwUseNewInstruction=g_pInstructionTable->DetermineInternalCommandCode(dwMathSymbol, pP1->m_dwType);
				if(dwUseNewInstruction==0)
				{
					// Command not yet implemented
					g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::Unknown), nullptr);
					pDynValue.reset();
					return true;
				}
				CInstructionTableEntry* pRef = g_pInstructionTable->GetRef(dwUseNewInstruction);
				LPSTR pMathDLL=pRef->GetDLL()->GetStr();
				LPSTR pMathCommand=pRef->GetDecoratedName()->GetStr();
				g_pASMWriter->WriteASMCall(m_dwLineNumber, pMathDLL, pMathCommand);

				// Copy RAX (holding call result) to temp var used to hold return value
				if(pP1) g_pASMWriter->WriteASMTaskP2(m_dwLineNumber, static_cast<DWORD>(ASMTask::Assign), pP1, nullptr);

				// Pop params after calc
				g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::PopRbx), nullptr);
				g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::PopRbx), nullptr);
				
				// Free usages (unique_ptr auto-cleanup)
				pDynValue.reset();
			}
			break;

		case static_cast<DWORD>(BuildTask::CopyUdt):
			{
				// P1 = P2 using UDT values (use a memcpy to perform the copy) P3 is Size
				g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::Push), pP3);

				// Variable Or Array
				if ( pP2->m_dwType==1101 )
					g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::Push), pP2);
				else
					g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::PushAddress), pP2);

				if ( pP1->m_dwType==1101 )
					g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::Push), pP1);
				else
					g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::PushAddress), pP1);

				// Call MEMCPY equivilant
				g_pASMWriter->WriteASMCall(m_dwLineNumber, "dbprocore.dll", "?CopyByteMemory@@YAX_K0H@Z");

				// Pop params after calc - leefix - 170403 - stupidy stupidy stupidy
				g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::PopRbx), nullptr);
				g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::PopRbx), nullptr);
				g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::PopRbx), nullptr);

				// After block copy (for values, run through UDT area to recreate strings
				ActOnLocalVars ( pP1 );
			}
			break;

		case static_cast<DWORD>(BuildTask::UserFunctionExit):

			// Not in function so ignore command
			if(g_pUserFunctionWithin==nullptr)
				break;

			// Establish Return Param
			CStr* pReturnData = nullptr;
			DWORD dwReturnDataType = 0;
			if(pP1)
			{
				pReturnData = pP1->m_pStringToken.get();
				dwReturnDataType = pP1->m_dwType;
			}

			// check if returned string is global (leefix - 210703 - CODE FROM ENDFUNCTION!)
			bool bDuplicateReturnString=false;
			CResultData* pResultData = nullptr;
			if(pP1)
			{
				pResultData = pP1;
				if ( dwReturnDataType==3 || dwReturnDataType==103 )
				{
					DWORD dwArr=0;
					if ( dwReturnDataType==103 ) dwArr=1;
					CVarTable* pVar = g_pVarTable->FindVariable ( "", pReturnData->GetStr()+1, dwArr );
					if ( pVar )
					{
						// return is a global var or array string, must create a duplicate string to send back
						bDuplicateReturnString=true;
					}
				}
			}

			// Free any allocated dynamic memory (strings and arrays)
			g_pUserFunctionWithin->ActOnLocalVars(DBMPLACEMENT_BOTTOM, pReturnData);

			// If user function returns value, store in RAX/RDX/XMM0
			if(pP1)
			{
				// Direct or 'Duplicate copy of string'
				if ( bDuplicateReturnString )
				{
					// Source global string array
					g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::Push), pResultData);

					// Blank String - no thing to free
					CStr pNull("0");
					g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::Push), &pNull, 7);

					// Put new string address in RAX for return passing
					g_pASMWriter->WriteASMCall(m_dwLineNumber, "dbprocore.dll", "?EquateSS@@YA_K_K0@Z");
				}
				else
				{
					// direct copy of var ref to outside RAX
					g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::AssignToRax), pP1);
				}
			}

			/* does not account for global return strings (see abive replacement code)
			// Free any allocated dynamic memory (strings and arrays)
			g_pUserFunctionWithin->ActOnLocalVars(DBMPLACEMENT_BOTTOM, pReturnData);

			// If user function returns value, store in RAX/RDX/XMM0
			if(pP1)
			{
				g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::AssignToRax), pP1);
			}
			*/

			// Restore RSP from RBP Register
			g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::MovRspRbp), nullptr, 0);

			// Restore RBP Register
			g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::PopRbp), nullptr, 0);

			// RETurn
			g_pASMWriter->WriteASMTaskCoreP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::PureReturn), nullptr, 0);
			break;
	}

	// Complete
	return true;
}


