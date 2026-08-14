#include "TaskEmitter.h"
#include "ASMWriter.h"
#include "Str.h"
#include "DebugInfo.h"

DWORD CTaskEmitter::DetermineASMCall(DWORD dwASMCodeAsAByte, DWORD dwTypeValue) const noexcept
{
    // Strings (type 3), relative-addresses to string elements (203), address-of
    // results (107: DWORD POINTER / RELATIVE ADDRESS) and full-width pointer
    // values (1002: the runtime array-pointer API) are heap pointers: every
    // memory load/store of such a value must move 8 bytes on x64. Map to the
    // REX.W variants directly so dword/byte/word types keep their exact
    // 32-bit byte streams.
    if (dwTypeValue == 3 || dwTypeValue == 203 || dwTypeValue == 1002 || dwTypeValue == 107)
    {
        switch (dwASMCodeAsAByte)
        {
            case static_cast<DWORD>(ASMOp::MOVEAXMEM1):    return static_cast<DWORD>(ASMOp::MOVEAXMEM8);
            case static_cast<DWORD>(ASMOp::MOVMEMEAX1):    return static_cast<DWORD>(ASMOp::MOVMEMEAX8);
            case static_cast<DWORD>(ASMOp::MOVEAXEBP1):    return static_cast<DWORD>(ASMOp::MOVEAXEBP8);
            case static_cast<DWORD>(ASMOp::MOVEBPEAX1):    return static_cast<DWORD>(ASMOp::MOVEBPEAX8);
            case static_cast<DWORD>(ASMOp::MOVEAXECXOFF1): return static_cast<DWORD>(ASMOp::MOVEAXECXOFF8);
            case static_cast<DWORD>(ASMOp::MOVECXOFFEAX1): return static_cast<DWORD>(ASMOp::MOVECXOFFEAX8);
            case static_cast<DWORD>(ASMOp::MOVEAXECXREL1): return static_cast<DWORD>(ASMOp::MOVEAXECXREL8);
            case static_cast<DWORD>(ASMOp::MOVEAXEAXREL1): return static_cast<DWORD>(ASMOp::MOVEAXEAXREL8);
            default: break;
        }
    }

    // First Determine SizeCode From Type
    DWORD dwAddressSizeCode = 0;
    switch (dwTypeValue)
    {
        case 4:  dwAddressSizeCode = 0; break;  // BYTE
        case 5:  dwAddressSizeCode = 0; break;  // BYTE
        case 6:  dwAddressSizeCode = 1; break;  // WORD
        case 8:  dwAddressSizeCode = 3; break;  // DWORDx2
        case 9:  dwAddressSizeCode = 3; break;  // DWORDx2
        default: dwAddressSizeCode = 2; break;  // DWORD (default)
    }

    return dwASMCodeAsAByte + dwAddressSizeCode;
}

DWORD CTaskEmitter::DetermineASMCallForREL(DWORD dwASMCodeAsAByte, DWORD dwTypeValue) const noexcept
{
	// String arrays (103) hold 8-byte heap pointers: the element value access
	// (and only that) must move a full QWORD on x64. Address-of results (107,
	// DWORD POINTER / RELATIVE ADDRESS) carry the full 8-byte address the same
	// way (wave 14).
	if (dwTypeValue == 103 || dwTypeValue == 107)
	{
		switch (dwASMCodeAsAByte)
		{
			case static_cast<DWORD>(ASMOp::MOVECXEAXOFF1): return static_cast<DWORD>(ASMOp::MOVECXEAXOFF8);
			case static_cast<DWORD>(ASMOp::MOVEAXECX1):    return static_cast<DWORD>(ASMOp::MOVEAXECX8);
			case static_cast<DWORD>(ASMOp::MOVEAXOFFECX1): return static_cast<DWORD>(ASMOp::MOVEAXOFFECX8);
			default: break;
		}
	}

	DWORD dwAddressSizeCode=0;
	switch(dwTypeValue)
	{
		case 104 :	dwAddressSizeCode=0;	break;	// RELATIVE ADDRESS TO A BYTE
		case 105 :	dwAddressSizeCode=0;	break;	// RELATIVE ADDRESS TO A BYTE
		case 106 :	dwAddressSizeCode=1;	break;	// RELATIVE ADDRESS TO A WORD
		case 101 :	
		case 102 :	
		case 107 :	dwAddressSizeCode=2;	break;	// RELATIVE ADDRESS TO A DWORD
		case 108 :	dwAddressSizeCode=3;	break;	// RELATIVE ADDRESS TO A DWORDx2
		case 109 :	dwAddressSizeCode=3;	break;	// RELATIVE ADDRESS TO A DWORDx2
		default:	dwAddressSizeCode=2;	break;	// RELATIVE ADDRESS TO A DWORD
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
					return static_cast<DWORD>(ParamMode::Ebp);
				else
				{
					if((dwPType>100 && dwPType<=199) || dwPType==1101)
						return static_cast<DWORD>(ParamMode::EbpArr);
					else
					{
						if(dwPType>200 && dwPType<=299)
							return static_cast<DWORD>(ParamMode::EbpRel);
						else
						{
							if(dwPOffset>0)
								return static_cast<DWORD>(ParamMode::EbpOff);
							else
								return static_cast<DWORD>(ParamMode::Ebp);
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

void CTaskEmitter::WriteASMARRtoEAX(CASMWriter* pASMWriter, DWORD dwMode, CStr* pP, CStr* pOffset, DWORD dwPType, DWORD dwPOffset) const
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
		pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::CMPEAX4), "0");
		pASMWriter->WriteASMLeapMarkerJump(static_cast<DWORD>(ASMOp::JE), 1);
	}

	pASMWriter->CalculateArrayOffsetInEBX(pOffset);
	// Runtime ref table stores 8-byte element addresses on x64 (scale ×8).
	pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXSIB8), "");

	switch(dwPType-100)
	{
		case 1001:
					pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::ADDEAX4), pOffset1Str->GetStr());
					break;

		case 8:
					pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDXMM0EAX), pOffset1Str->GetStr());
					break;

		case 9:
					// int64 element: single 8-byte move (wave 8b).
					pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXEAXOFF8), pOffset1Str->GetStr());
					pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXECX8), "");
					break;

		default:
					dwCorrectASMCode1=pASMWriter->DetermineASMCallForREL(static_cast<DWORD>(ASMOp::MOVECXEAXOFF1),dwPType);
					dwCorrectASMCode2=pASMWriter->DetermineASMCallForREL(static_cast<DWORD>(ASMOp::MOVEAXECX1),dwPType);
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

		char leapstring[32];
		itoa ( iLeapSize, leapstring, 10 );
		pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::JMP), leapstring);

		pASMWriter->WriteASMLeapMarkerEnd(1);
		pASMWriter->WriteASMLeapMarkerEnd(2);
		pASMWriter->WriteASMLeapMarkerEnd(3);

		pASMWriter->WriteASMLine2(static_cast<DWORD>(ASMOp::MOVMEMIMM4), "@$_ERR_", "118");
		pASMWriter->WriteASMTaskCoreP2(pASMWriter->m_dwLineNumber, static_cast<DWORD>(ASMTask::RuntimeErrorHook), NULL, 0, NULL, 0);
	}
}

void CTaskEmitter::WriteASMXtoEAX(CASMWriter* pASMWriter, DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset) const
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
							pASMWriter->WriteASMLine2IMM(static_cast<DWORD>(ASMOp::MOVMEMIMM4), pTemp2Str->GetStr(), pDWORD2Str->GetStr(),2);							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDXMM0MEM), pTemp1Str->GetStr());
							break;

					case 9:
							// int64 literal: low dword to TEMPA, high dword to TEMPB
							// (adjacent 4-byte temps, same contract the double path
							// uses), then one 8-byte load into RAX (wave 8b).
							pTemp1Str->SetText("@$_TEMPA_");
							pTemp2Str->SetText("@$_TEMPB_");
							pASMWriter->WriteASMLine2IMM(static_cast<DWORD>(ASMOp::MOVMEMIMM4), pTemp1Str->GetStr(), pDWORD1Str->GetStr(), 2);
							pASMWriter->WriteASMLine2IMM(static_cast<DWORD>(ASMOp::MOVMEMIMM4), pTemp2Str->GetStr(), pDWORD2Str->GetStr(), 2);
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXMEM8), pTemp1Str->GetStr());
							break;

				case 3:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXIMM4), pP->GetStr());
							break;

				case 20:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXIMM4), pP->GetStr());
							break;

				default:
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXIMM1),dwPType);
							dwIMMSize=dwCorrectASMCode-static_cast<DWORD>(ASMOp::MOVEAXIMM1);
							pASMWriter->WriteASMLine2IMM(dwCorrectASMCode, NULL, pDWORD1Str->GetStr(), dwIMMSize);
							break;
			}
			break;
			
		case static_cast<DWORD>(ParamMode::Mem):
			pDoubleStr->SetText("+");
			pDoubleStr->AddText(pP->GetStr());
			switch(dwPType)
			{
				case 8:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDXMM0MEM), pP->GetStr());
							break;

				case 9:
							// int64: single 8-byte load into RAX (wave 8b).
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXMEM8), pP->GetStr());
							break;

				default:
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXMEM1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, pP->GetStr());
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::MemOff):
			pOffset1Str->SetDWORDNumericText(dwPOffset);
			pOffset2Str->SetDWORDNumericText(dwPOffset+4);
			pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXIMM4), pP->GetStr());
			switch(dwPType)
			{
				case 8:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDXMM0ECXOFF), pOffset1Str->GetStr());
							break;

				case 9:
							// int64: single 8-byte load into RAX (wave 8b).
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXECXOFF8), pOffset1Str->GetStr());
							break;
	
				default:
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXECXOFF1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, pOffset1Str->GetStr());
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::Ebp):
			pDoubleStr->SetText((pP->GetStr()+2));
			iOffset=(int)pDoubleStr->GetValue();
			iOffset+=4;
			pDoubleStr->SetNumericText(iOffset);
			switch(dwPType)
			{
				case 8:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDXMM0EBP), (pP->GetStr()+2));
							break;

				case 9:
							// int64: single 8-byte load into RAX (wave 8b).
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXEBP8), (pP->GetStr()+2));
							break;

				default:
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXEBP1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, (pP->GetStr()+2));	
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::EbpOff):
			pOffset2Str->SetText((pP->GetStr()+2));
			iOffset=(int)pOffset2Str->GetValue();
			pOffset1Str->SetNumericText( iOffset + dwPOffset );
			iOffset+=4;
			pOffset2Str->SetNumericText( iOffset + dwPOffset );
			switch(dwPType)
			{
				case 8:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDXMM0EBP), pOffset1Str->GetStr());
							break;

				case 9:
							// int64: single 8-byte load into RAX (wave 8b).
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXEBP8), pOffset1Str->GetStr());
							break;

				default:
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXEBP1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, pOffset1Str->GetStr());	
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::MemArr):
			// The array pointer lives in an 8-byte address slot on x64.
			pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXMEM8), pP->GetStr());
			WriteASMARRtoEAX(pASMWriter, dwMode, pP, pPIndex, dwPType, dwPOffset);
			break;

		case static_cast<DWORD>(ParamMode::EbpArr):
			// The array pointer lives in an 8-byte address slot on x64.
			pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXEBP8), (pP->GetStr()+2));
			WriteASMARRtoEAX(pASMWriter, dwMode, pP, pPIndex, dwPType, dwPOffset);
			break;

		case static_cast<DWORD>(ParamMode::MemRel):
			pDoubleStr->SetText("+");
			pDoubleStr->AddText(pP->GetStr());
			switch(dwPType)
			{
				case 8: break;
				case 9: break;
				default:
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXMEM1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, pP->GetStr());
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXEAXREL1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, "");
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::EbpRel):
			switch(dwPType)
			{
				case 8: break;
				case 9: break;
				default:
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXEBP1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, (pP->GetStr()+2));
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXEAXREL1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, "");
							break;
			}
			break;
	}
}

void CTaskEmitter::WriteASMEAXtoARR(CASMWriter* pASMWriter, DWORD dwMode, CStr* pP, CStr* pOffset, DWORD dwPType, DWORD dwPOffset) const
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
		pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::CMPEAX4), "0");
		pASMWriter->WriteASMLeapMarkerJump(static_cast<DWORD>(ASMOp::JE), 1);
	}

	pASMWriter->CalculateArrayOffsetInEBX(pOffset);
	// Runtime ref table stores 8-byte element addresses on x64 (scale ×8).
	pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXSIB8), "");

	switch(dwPType-100)
	{
		case 1001:
					pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::ADDEAXECX4), "");
					break;

		case 8:
					pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDEAXXMM0), pOffset1Str->GetStr());
					break;

		case 9:
					// int64 element: single 8-byte store (wave 8b).
					pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXOFFECX8), pOffset1Str->GetStr());
					break;

		default:
					dwCorrectASMCode=pASMWriter->DetermineASMCallForREL(static_cast<DWORD>(ASMOp::MOVEAXOFFECX1),dwPType);
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

		char leapstring[32];
		itoa ( iLeapSize, leapstring, 10 );
		pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::JMP), leapstring);

		pASMWriter->WriteASMLeapMarkerEnd(1);
		pASMWriter->WriteASMLeapMarkerEnd(2);
		pASMWriter->WriteASMLeapMarkerEnd(3);

		pASMWriter->WriteASMLine2(static_cast<DWORD>(ASMOp::MOVMEMIMM4), "@$_ERR_", "118");
		pASMWriter->WriteASMTaskCoreP2(pASMWriter->m_dwLineNumber, static_cast<DWORD>(ASMTask::RuntimeErrorHook), NULL, 0, NULL, 0);
	}
}

void CTaskEmitter::WriteASMEAXtoX(CASMWriter* pASMWriter, DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset) const
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
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDMEMXMM0), pP->GetStr());
							break;

				case 9:
							// int64: single 8-byte store from RAX (wave 8b).
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVMEMEAX8), pP->GetStr());
							break;
	
				default:
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVMEMEAX1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, pP->GetStr());
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::MemOff):
			pOffset1Str->SetDWORDNumericText(dwPOffset);
			pOffset2Str->SetDWORDNumericText(dwPOffset+4);
			pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXIMM4), pP->GetStr());
			switch(dwPType)
			{
				case 8:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDECXOFFXMM0), pOffset1Str->GetStr());
							break;

				case 9:
							// int64: single 8-byte store from RAX (wave 8b).
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXOFFEAX8), pOffset1Str->GetStr());
							break;
	
				default:
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVECXOFFEAX1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, pOffset1Str->GetStr());
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::Ebp):
			pDoubleStr->SetText((pP->GetStr()+2));
			iOffset=(int)pDoubleStr->GetValue();
			iOffset+=4;
			pDoubleStr->SetNumericText(iOffset);
			switch(dwPType)
			{
				case 8:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDEBPXMM0), (pP->GetStr()+2));
							break;

				case 9:
							// int64: single 8-byte store from RAX (wave 8b).
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVEBPEAX8), (pP->GetStr()+2));
							break;

				default:
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEBPEAX1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, (pP->GetStr()+2));
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::EbpOff):
			pOffset2Str->SetText((pP->GetStr()+2));
			iOffset=(int)pOffset2Str->GetValue();
			pOffset1Str->SetNumericText( iOffset + dwPOffset );
			iOffset+=4;
			pOffset2Str->SetNumericText( iOffset + dwPOffset );
			switch(dwPType)
			{
				case 8:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDEBPXMM0), pOffset1Str->GetStr());
							break;

				case 9:
							// int64: single 8-byte store from RAX (wave 8b).
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVEBPEAX8), pOffset1Str->GetStr());
							break;

				default:
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEBPEAX1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, pOffset1Str->GetStr());
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::MemArr):
			// Value guard: strings and int64 values keep the full 64-bit
			// width (strings hold heap pointers, int64 holds a QWORD).
			if(dwPType==3 || dwPType==103 || dwPType==203 || dwPType==9 || dwPType==109)
				pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXEAX8), "");
			else
				pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXEAX4), "");
			// The array pointer lives in an 8-byte address slot on x64.
			pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXMEM8), pP->GetStr());
			WriteASMEAXtoARR(pASMWriter, dwMode, pP, pPIndex, dwPType, dwPOffset);
			break;

		case static_cast<DWORD>(ParamMode::EbpArr):
			// Value guard: strings and int64 values keep the full 64-bit
			// width (strings hold heap pointers, int64 holds a QWORD).
			if(dwPType==3 || dwPType==103 || dwPType==203 || dwPType==9 || dwPType==109)
				pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXEAX8), "");
			else
				pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXEAX4), "");
			// The array pointer lives in an 8-byte address slot on x64.
			pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXEBP8), (pP->GetStr()+2));
			WriteASMEAXtoARR(pASMWriter, dwMode, pP, pPIndex, dwPType, dwPOffset);
			break;

		case static_cast<DWORD>(ParamMode::Stack):
			switch(dwPType)
			{
				case 8:		
				case 108:
							pTemp1Str->SetText("@$_TEMPA_");
							pTemp2Str->SetText("@$_TEMPB_");
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDMEMXMM0), pTemp1Str->GetStr());
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXMEM4), pTemp2Str->GetStr());
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::PUSHEAX), "");
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXMEM4), pTemp1Str->GetStr());
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::PUSHEAX), "");
							break;

				case 9:		
				case 109:
							// int64: one 8-byte slot (PUSH RAX) for the x64 call ABI
							// (wave 8b — __int64 rides a single integer register).
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::PUSHEAX), "");
							break;

				default:
							pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::PUSHEAX), "");
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::MemRel):
			// Value guard: strings need the full 64-bit pointer in RCX.
			if(dwPType==3)
				pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXEAX8), "");
			else
				pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXEAX4), "");
			pDoubleStr->SetText("+");
			pDoubleStr->AddText(pP->GetStr());
			switch(dwPType)
			{
				case 8: break;
				case 9: break;
				default:
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXMEM1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, pP->GetStr());
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXECXREL1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, "");
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::EbpRel):
			// Value guard: strings need the full 64-bit pointer in RCX.
			if(dwPType==3)
				pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXEAX8), "");
			else
				pASMWriter->WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXEAX4), "");
			switch(dwPType)
			{
				case 8: break;
				case 9: break;
				default:
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXEBP1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, (pP->GetStr()+2));
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXECXREL1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, "");
							break;
			}
			break;
	}
}
