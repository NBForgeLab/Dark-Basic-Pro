#include "TaskEmitter.h"
#include "ASMWriter.h"
#include "Str.h"
#include "DebugInfo.h"
#include "TargetABI.h"

#include <string>

// External Globals
extern CDebugInfo g_DebugInfo;

DWORD CTaskEmitter::DetermineASMCall(DWORD dwASMCodeAsAByte, DWORD dwTypeValue) const noexcept
{
    // First Determine SizeCode From Type (0: 1 byte, 1: 2 bytes, 2: 4 bytes, 3: 8 bytes)
    DWORD dwAddressSizeCode = 0;
    switch (dwTypeValue)
    {
        case 4:  dwAddressSizeCode = 0; break;  // BYTE (1 byte)
        case 5:  dwAddressSizeCode = 0; break;  // BOOL (1 byte)
        case 6:  dwAddressSizeCode = 1; break;  // WORD (2 bytes)
        case 3:  dwAddressSizeCode = 2; break;  // FLOAT (4 bytes)
        case 7:  dwAddressSizeCode = 2; break;  // DWORD (4 bytes)
        case 8:  dwAddressSizeCode = 3; break;  // DOUBLE (8 bytes)
        case 9:  dwAddressSizeCode = 3; break;  // INT64 (8 bytes)
        case 2:  dwAddressSizeCode = 2; break;  // STRING handle/ptr (8-byte pointer)
        case 1001: dwAddressSizeCode = 2; break; // POINTER / REF (8-byte pointer)
        default: dwAddressSizeCode = 2; break;
    }

    return dwASMCodeAsAByte + dwAddressSizeCode;
}

DWORD CTaskEmitter::DetermineASMCallForREL(DWORD dwASMCodeAsAByte, DWORD dwTypeValue) const noexcept
{
	DWORD dwAddressSizeCode=0;
	switch(dwTypeValue)
	{
		case 104 :	dwAddressSizeCode=0;	break;	// RELATIVE ADDRESS TO A BYTE
		case 105 :	dwAddressSizeCode=0;	break;	// RELATIVE ADDRESS TO A BOOL
		case 106 :	dwAddressSizeCode=1;	break;	// RELATIVE ADDRESS TO A WORD
		case 103 :
		case 107 :	dwAddressSizeCode=2;	break;	// RELATIVE ADDRESS TO A DWORD / FLOAT
		case 108 :	dwAddressSizeCode=3;	break;	// RELATIVE ADDRESS TO A DOUBLE (8 bytes)
		case 109 :	dwAddressSizeCode=3;	break;	// RELATIVE ADDRESS TO A INT64 (8 bytes)
		case 102 :	dwAddressSizeCode=2;	break;	// RELATIVE ADDRESS TO A STRING PTR
		default:	dwAddressSizeCode=2;	break;
	}
	return dwASMCodeAsAByte+dwAddressSizeCode;
}

DWORD CTaskEmitter::DetermineParamMode(CStr* pP, DWORD dwPType, DWORD dwPOffset) const noexcept
{
	if(pP)
	{
		if(pP->GetChar(0)=='@')
		{
			if(pP->GetChar(1)==':')
			{
				if(dwPType==1001)
					return static_cast<DWORD>(ParamMode::Rbp);
				else
				{
					if((dwPType>100 && dwPType<=199) || dwPType==1101)
						return static_cast<DWORD>(ParamMode::RbpArr);
					else
					{
						if(dwPType>200 && dwPType<=299)
							return static_cast<DWORD>(ParamMode::RbpRel);
						else
						{
							if(dwPOffset>0)
								return static_cast<DWORD>(ParamMode::RbpOff);
							else
								return static_cast<DWORD>(ParamMode::Rbp);
						}
					}
				}
			}
			else
			{
				if(dwPType==1001)
					return static_cast<DWORD>(ParamMode::Mem);
				else
				{
					if((dwPType>100 && dwPType<=199) || dwPType==1101)
						return static_cast<DWORD>(ParamMode::MemArr);
					else
					{
						if(dwPType>200 && dwPType<=299)
							return static_cast<DWORD>(ParamMode::MemRel);
						else
						{
							if(dwPOffset>0)
								return static_cast<DWORD>(ParamMode::MemOff);
							else
								return static_cast<DWORD>(ParamMode::Mem);
						}
					}
				}
			}
		}
		else
			return static_cast<DWORD>(ParamMode::Imm);
	}
	return static_cast<DWORD>(ParamMode::None);
}

DWORD CTaskEmitter::CalculateTaskPassOffset(DWORD dwPassNumber, DWORD dwBaseOffset) const noexcept
{
    return dwPassNumber * dwBaseOffset;
}

void CTaskEmitter::WriteASMARRtoRAX(CASMWriter* pASMWriter, [[maybe_unused]] DWORD dwMode, [[maybe_unused]] CStr* pP, [[maybe_unused]] CStr* pOffset, DWORD dwPType, DWORD dwPOffset) const
{
	if (!pASMWriter) return;

	DWORD dwCorrectASMCode1=0;
	DWORD dwCorrectASMCode2=0;

	CStr offset1Str("");
	CStr* pOffset1Str = &offset1Str;
	pOffset1Str->SetNumericText( dwPOffset );
	CStr offset2Str("");
	CStr* pOffset2Str = &offset2Str;
	pOffset2Str->SetNumericText( dwPOffset + 4 );

	if(pASMWriter->GetArrayCheckFlag())
	{
		pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::CMPRAX4), "0");
		pASMWriter->WriteASMLeapMarkerJump(static_cast<DWORD>(ASMOp::JE), 1);
	}

	pASMWriter->CalculateArrayOffsetInRBX(pOffset);
	pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXSIB), "");

	switch(dwPType-100)
	{
		case 1001:
					pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::ADDRAX4), pOffset1Str->GetStr());
					break;

		case 8:
					pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVXMM0RAX8), pOffset1Str->GetStr());
					break;

		case 9:
					pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRCXRAXOFF4), pOffset2Str->GetStr());
					pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRDXRCX4), "");
					pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRCXRAXOFF4), pOffset1Str->GetStr());
					pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXRCX4), "");
					break;

		default:
					dwCorrectASMCode1=pASMWriter->DetermineASMCallForREL(static_cast<DWORD>(ASMOp::MOVRCXRAXOFF1),dwPType);
					dwCorrectASMCode2=pASMWriter->DetermineASMCallForREL(static_cast<DWORD>(ASMOp::MOVRAXRCX1),dwPType);
					pASMWriter->WriteASMLine(dwCorrectASMCode1, pOffset1Str->GetStr());
					pASMWriter->WriteASMLine(dwCorrectASMCode2, "");
					break;
	}

	if(pASMWriter->GetArrayCheckFlag())
	{
		int iLeapSize = 10;
		if(g_DebugInfo.DebugModeOn())
			iLeapSize+=51;
		else
			iLeapSize+=26;

		const std::string leapString = std::to_string(iLeapSize);
		pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::JMP), const_cast<LPSTR>(leapString.c_str()));

		pASMWriter->WriteASMLeapMarkerEnd(1);
		pASMWriter->WriteASMLeapMarkerEnd(2);
		pASMWriter->WriteASMLeapMarkerEnd(3);

		pASMWriter->WriteASMLine2(static_cast<DWORD>(ASMOp::MOVMEMIMM4), "@$_ERR_", "118");
		pASMWriter->WriteASMTaskCoreP2(pASMWriter->m_dwLineNumber, static_cast<DWORD>(ASMTask::RuntimeErrorHook), nullptr, 0, nullptr, 0);
	}
}

void CTaskEmitter::WriteASMXtoRAX(CASMWriter* pASMWriter, DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset) const
{
	if (!pASMWriter) return;

	DWORD dwCorrectASMCode = 0;
	int iOffset = 0;
	DWORD dwDWORDRep=0;
	DWORD dwExtraDWORD=0;
	DWORD dwIMMSize=0;

	CStr doubleStr, dword1Str, dword2Str, offset1Str, offset2Str, temp1Str, temp2Str;
	CStr* pDoubleStr = &doubleStr;
	CStr* pDWORD1Str = &dword1Str;
	CStr* pDWORD2Str = &dword2Str;
	CStr* pOffset1Str = &offset1Str;
	CStr* pOffset2Str = &offset2Str;
	CStr* pTemp1Str = &temp1Str;
	CStr* pTemp2Str = &temp2Str;

	switch(dwMode)
	{
		case static_cast<DWORD>(ParamMode::Imm):
			dwExtraDWORD=0;
			dwDWORDRep = pP->GetDWORDRepresentation(dwPType, &dwExtraDWORD);
			pDWORD1Str->SetDWORDNumericText(dwDWORDRep);
			pDWORD2Str->SetDWORDNumericText(dwExtraDWORD);
			switch(dwPType)
			{
				case 8:
							pTemp1Str->SetText("@$_TEMPA_");
							pTemp2Str->SetText("@$_TEMPB_");
							pASMWriter->WriteASMLine2IMM(static_cast<DWORD>(ASMOp::MOVMEMIMM4), pTemp1Str->GetStr(), pDWORD1Str->GetStr(),2);
							pASMWriter->WriteASMLine2IMM(static_cast<DWORD>(ASMOp::MOVMEMIMM4), pTemp2Str->GetStr(), pDWORD2Str->GetStr(),2);
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVXMM0MEM8), pTemp1Str->GetStr());
							break;

				case 9:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRDXIMM4), pDWORD2Str->GetStr());
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXIMM4), pDWORD1Str->GetStr());
							break;

				case 3:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXIMM4), pP->GetStr());
							break;

				case 20:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXIMM4), pP->GetStr());
							break;

				default:
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVRAXIMM1),dwPType);
							dwIMMSize=dwCorrectASMCode-static_cast<DWORD>(ASMOp::MOVRAXIMM1);
							pASMWriter->WriteASMLine2IMM(dwCorrectASMCode, nullptr, pDWORD1Str->GetStr(), dwIMMSize);
							break;
			}
			break;
			
		case static_cast<DWORD>(ParamMode::Mem):
			pDoubleStr->SetText("+");
			pDoubleStr->AddText(pP->GetStr());
			switch(dwPType)
			{
				case 8:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVXMM0MEM8), pP->GetStr());
							break;

				case 9:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXMEM4), pDoubleStr->GetStr());
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRDXRAX4), "");
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXMEM4), pP->GetStr());
							break;

				default:
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVRAXMEM1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, pP->GetStr());
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::MemOff):
			pOffset1Str->SetDWORDNumericText(dwPOffset);
			pOffset2Str->SetDWORDNumericText(dwPOffset+4);
			pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRCXIMM4), pP->GetStr());
			switch(dwPType)
			{
				case 8:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVXMM0RCXOFF8), pOffset1Str->GetStr());
							break;

				case 9:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXRCXOFF4), pOffset2Str->GetStr());
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRDXRAX4), "");
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXRCXOFF4), pOffset1Str->GetStr());
							break;
	
				default:
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVRAXRCXOFF1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, pOffset1Str->GetStr());
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::Rbp):
			pDoubleStr->SetText((pP->GetStr()+2));
			iOffset=(int)pDoubleStr->GetValue();
			iOffset+=4;
			pDoubleStr->SetNumericText(iOffset);
			switch(dwPType)
			{
				case 8:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVXMM0RBP8), (pP->GetStr()+2));
							break;

				case 9:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXRBP4), pDoubleStr->GetStr());
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRDXRAX4), "");
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXRBP4), (pP->GetStr()+2));
							break;

				default:
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVRAXRBP1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, (pP->GetStr()+2));	
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::RbpOff):
			pOffset2Str->SetText((pP->GetStr()+2));
			iOffset=(int)pOffset2Str->GetValue();
			pOffset1Str->SetNumericText( iOffset + dwPOffset );
			iOffset+=4;
			pOffset2Str->SetNumericText( iOffset + dwPOffset );
			switch(dwPType)
			{
				case 8:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVXMM0RBP8), pOffset1Str->GetStr());
							break;

				case 9:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXRBP4), pOffset2Str->GetStr());
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRDXRAX4), "");
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXRBP4), pOffset1Str->GetStr());
							break;

				default:
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVRAXRBP1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, pOffset1Str->GetStr());	
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::MemArr):
			pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXMEM4), pP->GetStr());
			WriteASMARRtoRAX(pASMWriter, dwMode, pP, pPIndex, dwPType, dwPOffset);
			break;

		case static_cast<DWORD>(ParamMode::RbpArr):
			pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXRBP4), (pP->GetStr()+2));
			WriteASMARRtoRAX(pASMWriter, dwMode, pP, pPIndex, dwPType, dwPOffset);
			break;

		case static_cast<DWORD>(ParamMode::MemRel):
			pDoubleStr->SetText("+");
			pDoubleStr->AddText(pP->GetStr());
			switch(dwPType)
			{
				case 8: break;
				case 9: break;
				default:
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVRAXMEM1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, pP->GetStr());
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVRAXRAXREL1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, "");
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::RbpRel):
			switch(dwPType)
			{
				case 8: break;
				case 9: break;
				default:
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVRAXRBP1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, (pP->GetStr()+2));
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVRAXRAXREL1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, "");
							break;
			}
			break;
	}
}

void CTaskEmitter::WriteASMRAXtoARR(CASMWriter* pASMWriter, [[maybe_unused]] DWORD dwMode, [[maybe_unused]] CStr* pP, [[maybe_unused]] CStr* pOffset, DWORD dwPType, DWORD dwPOffset) const
{
	if (!pASMWriter) return;

	DWORD dwCorrectASMCode=0;

	CStr offset1Str("");
	CStr* pOffset1Str = &offset1Str;
	pOffset1Str->SetNumericText( dwPOffset );
	CStr offset2Str("");
	CStr* pOffset2Str = &offset2Str;
	pOffset2Str->SetNumericText( dwPOffset+4 );

	if(pASMWriter->GetArrayCheckFlag())
	{
		pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::CMPRAX4), "0");
		pASMWriter->WriteASMLeapMarkerJump(static_cast<DWORD>(ASMOp::JE), 1);
	}

	pASMWriter->CalculateArrayOffsetInRBX(pOffset);
	pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXSIB), "");

	switch(dwPType-100)
	{
		case 1001:
					pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::ADDRAXRCX4), "");
					break;

		case 8:
					pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXXMM0), pOffset1Str->GetStr());
					break;

		case 9:
					pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXOFFRCX4), pOffset1Str->GetStr());
					pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRCXRDX4), "");
					pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXOFFRCX4), pOffset2Str->GetStr());
					break;

		default:
					dwCorrectASMCode=pASMWriter->DetermineASMCallForREL(static_cast<DWORD>(ASMOp::MOVRAXOFFRCX1),dwPType);
					pASMWriter->WriteASMLine(dwCorrectASMCode, pOffset1Str->GetStr());
					break;
	}

	if(pASMWriter->GetArrayCheckFlag())
	{
		int iLeapSize = 10;
		if(g_DebugInfo.DebugModeOn())
			iLeapSize+=51;
		else
			iLeapSize+=26;

		const std::string leapString = std::to_string(iLeapSize);
		pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::JMP), const_cast<LPSTR>(leapString.c_str()));

		pASMWriter->WriteASMLeapMarkerEnd(1);
		pASMWriter->WriteASMLeapMarkerEnd(2);
		pASMWriter->WriteASMLeapMarkerEnd(3);

		pASMWriter->WriteASMLine2(static_cast<DWORD>(ASMOp::MOVMEMIMM4), "@$_ERR_", "118");
		pASMWriter->WriteASMTaskCoreP2(pASMWriter->m_dwLineNumber, static_cast<DWORD>(ASMTask::RuntimeErrorHook), nullptr, 0, nullptr, 0);
	}
}

void CTaskEmitter::WriteASMRAXtoX(CASMWriter* pASMWriter, DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset) const
{
	if (!pASMWriter) return;

	DWORD dwCorrectASMCode=0;
	int iOffset=0;

	CStr doubleStr, offset1Str, offset2Str, temp1Str, temp2Str;
	CStr* pDoubleStr = &doubleStr;
	CStr* pOffset1Str = &offset1Str;
	CStr* pOffset2Str = &offset2Str;
	CStr* pTemp1Str = &temp1Str;
	CStr* pTemp2Str = &temp2Str;

	switch(dwMode)
	{
		case static_cast<DWORD>(ParamMode::Mem):
			pDoubleStr->SetText("+");
			pDoubleStr->AddText(pP->GetStr());
			switch(dwPType)
			{
				case 8:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVMEMXMM0), pP->GetStr());
							break;

				case 9:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVMEMRAX4), pP->GetStr());
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXRDX4), "");
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVMEMRAX4), pDoubleStr->GetStr());
							break;
	
				default:
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVMEMRAX1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, pP->GetStr());
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::MemOff):
			pOffset1Str->SetDWORDNumericText(dwPOffset);
			pOffset2Str->SetDWORDNumericText(dwPOffset+4);
			pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRCXIMM4), pP->GetStr());
			switch(dwPType)
			{
				case 8:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRCXOFFXMM0), pOffset1Str->GetStr());
							break;

				case 9:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRCXOFFRAX4), pOffset1Str->GetStr());
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXRDX4), "");
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRCXOFFRAX4), pOffset2Str->GetStr());
							break;
	
				default:
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVRCXOFFRAX1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, pOffset1Str->GetStr());
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::Rbp):
			pDoubleStr->SetText((pP->GetStr()+2));
			iOffset=(int)pDoubleStr->GetValue();
			iOffset+=4;
			pDoubleStr->SetNumericText(iOffset);
			switch(dwPType)
			{
				case 8:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRBPXMM0), (pP->GetStr()+2));
							break;

				case 9:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRBPRAX4), (pP->GetStr()+2));
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXRDX4), "");
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRBPRAX4), pDoubleStr->GetStr());
							break;

				default:
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVRBPRAX1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, (pP->GetStr()+2));
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::RbpOff):
			pOffset2Str->SetText((pP->GetStr()+2));
			iOffset=(int)pOffset2Str->GetValue();
			pOffset1Str->SetNumericText( iOffset + dwPOffset );
			iOffset+=4;
			pOffset2Str->SetNumericText( iOffset + dwPOffset );
			switch(dwPType)
			{
				case 8:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRBPXMM0), pOffset1Str->GetStr());
							break;

				case 9:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRBPRAX4), pOffset1Str->GetStr());
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXRDX4), "");
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRBPRAX4), pOffset2Str->GetStr());
							break;

				default:
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVRBPRAX1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, pOffset1Str->GetStr());
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::MemArr):
			pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRCXRAX4), "");
			pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXMEM4), pP->GetStr());
			WriteASMRAXtoARR(pASMWriter, dwMode, pP, pPIndex, dwPType, dwPOffset);
			break;

		case static_cast<DWORD>(ParamMode::RbpArr):
			pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRCXRAX4), "");
			pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXRBP4), (pP->GetStr()+2));
			WriteASMRAXtoARR(pASMWriter, dwMode, pP, pPIndex, dwPType, dwPOffset);
			break;

		case static_cast<DWORD>(ParamMode::Stack):
			switch(dwPType)
			{
				case 8:		
				case 108:
							pTemp1Str->SetText("@$_TEMPA_");
							pTemp2Str->SetText("@$_TEMPB_");
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVMEMXMM0), pTemp1Str->GetStr());
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXMEM4), pTemp2Str->GetStr());
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::PUSHRAX), "");
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXMEM4), pTemp1Str->GetStr());
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::PUSHRAX), "");
							break;

				case 9:		
				case 109:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::PUSHRDX), "");
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::PUSHRAX), "");
							break;

				default:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::PUSHRAX), "");
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::MemRel):
			pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRCXRAX4), "");
			pDoubleStr->SetText("+");
			pDoubleStr->AddText(pP->GetStr());
			switch(dwPType)
			{
				case 8: break;
				case 9: break;
				default:
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVRAXMEM1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, pP->GetStr());
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVRAXRCXREL1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, "");
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::RbpRel):
			pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVRCXRAX4), "");
			switch(dwPType)
			{
				case 8: break;
				case 9: break;
				default:
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVRAXRBP1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, (pP->GetStr()+2));
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVRAXRCXREL1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, "");
							break;
			}
			break;
	}
}
