#include "TaskEmitter.h"
#include "ASMWriter.h"
#include "Str.h"
#include "DebugInfo.h"
#include "TargetABI.h"

#include <string>

// External Globals
extern CDebugInfo g_DebugInfo;

namespace
{
// UDT array variables carry struct-index type values that
// IsPointerOrHandleType does not cover; the '&' token marker is the reliable
// indicator that the slot holds a 64-bit array handle.
bool IsArrayHandleToken(const CStr* pP) noexcept
{
	if (!pP || !pP->GetStr()) return false;
	const std::string_view p(pP->GetStr());
	if (p.empty() || p.front() != '@') return false;
	const bool isRbp = (p.size() > 1 && p[1] == ':');
	return isRbp ? (p.size() > 2 && p[2] == '&') : (p.size() > 1 && p[1] == '&');
}
} // namespace

uint32_t CTaskEmitter::DetermineASMCall(uint32_t dwASMCodeAsAByte, uint32_t dwTypeValue) const noexcept
{
    // First Determine SizeCode From Type (0: 1 byte, 1: 2 bytes, 2: 4 bytes)
    uint32_t dwAddressSizeCode = 0;
    const auto type = static_cast<DBPType>(dwTypeValue);
    switch (type)
    {
        case DBPType::Boolean:
        case DBPType::Byte:
            dwAddressSizeCode = 0; break;  // 1 byte
        case DBPType::Word:
            dwAddressSizeCode = 1; break;  // 2 bytes
        case DBPType::Integer:
        case DBPType::Float:
        case DBPType::String:
        case DBPType::Dword:
        case DBPType::UserDefinedPtr:
        case DBPType::DoubleFloat:
        case DBPType::DoubleInteger:
        default:
            dwAddressSizeCode = 2; break;  // 4 bytes default
    }

    return dwASMCodeAsAByte + dwAddressSizeCode;
}

uint32_t CTaskEmitter::DetermineASMCallForREL(uint32_t dwASMCodeAsAByte, uint32_t dwTypeValue) const noexcept
{
	uint32_t dwAddressSizeCode = 0;
	const auto type = static_cast<DBPType>(dwTypeValue);
	switch (type)
	{
		case DBPType::BooleanArray:
		case DBPType::ByteArray:
			dwAddressSizeCode = 0; break; // RELATIVE ADDRESS TO A BYTE / BOOL
		case DBPType::WordArray:
			dwAddressSizeCode = 1; break; // RELATIVE ADDRESS TO A WORD
		case DBPType::IntegerArray:
		case DBPType::FloatArray:
		case DBPType::StringArray:
		case DBPType::DwordArray:
		case DBPType::DoubleFloatArray:
		case DBPType::DoubleIntegerArray:
		default:
			dwAddressSizeCode = 2; break; // RELATIVE ADDRESS TO A uint32_t / FLOAT / STRING PTR
	}

	return dwASMCodeAsAByte + dwAddressSizeCode;
}

uint32_t CTaskEmitter::DetermineASMCallWide(uint32_t dwASMCodeAsAByte, uint32_t dwTypeValue) const noexcept
{
	// Pointer/handle values (strings, UDT pointers, array handles) are 64-bit
	// on x64; the 1/2/4-byte families would truncate them.
	if (IsPointerOrHandleType(dwTypeValue))
	{
		switch (dwASMCodeAsAByte)
		{
			case static_cast<uint32_t>(ASMOp::MOVRAXMEM1):    return static_cast<uint32_t>(ASMOp::MOVRAXMEM8);
			case static_cast<uint32_t>(ASMOp::MOVMEMRAX1):    return static_cast<uint32_t>(ASMOp::MOVMEMRAX8);
			case static_cast<uint32_t>(ASMOp::MOVRAXRCXOFF1): return static_cast<uint32_t>(ASMOp::MOVRAXRCXOFF8);
			case static_cast<uint32_t>(ASMOp::MOVRCXOFFRAX1): return static_cast<uint32_t>(ASMOp::MOVRCXOFFRAX8);
			case static_cast<uint32_t>(ASMOp::MOVRAXRBP1):    return static_cast<uint32_t>(ASMOp::MOVRAXRBP8);
			case static_cast<uint32_t>(ASMOp::MOVRBPRAX1):    return static_cast<uint32_t>(ASMOp::MOVRBPRAX8);
			case static_cast<uint32_t>(ASMOp::MOVRAXRAXREL1): return static_cast<uint32_t>(ASMOp::MOVRAXRAXREL8);
			case static_cast<uint32_t>(ASMOp::MOVRAXRCXREL1): return static_cast<uint32_t>(ASMOp::MOVRAXRCXREL8);
			default: break;
		}
	}
	return DetermineASMCall(dwASMCodeAsAByte, dwTypeValue);
}

uint32_t CTaskEmitter::DetermineParamMode(const CStr* pP, uint32_t dwPType, uint32_t dwPOffset, const CStr* pPIndex) const noexcept
{
	return (pP && pP->GetStr()) ? DetermineParamMode(std::string_view(pP->GetStr()), dwPType, dwPOffset, pPIndex) : static_cast<uint32_t>(ParamMode::None);
}

uint32_t CTaskEmitter::DetermineParamMode(std::string_view p, uint32_t dwPType, uint32_t dwPOffset, const CStr* pPIndex) const noexcept
{
	if (p.empty())
		return static_cast<uint32_t>(ParamMode::None);

	if (p.front() == '@')
	{
		const bool isRbp = (p.size() > 1 && p[1] == ':');
		const bool isArrayHandle = isRbp ? (p.size() > 2 && p[2] == '&') : (p.size() > 1 && p[1] == '&');
		const auto type = static_cast<DBPType>(dwPType);

		// Array tokens split by access shape: an element access carries its
		// linearized index in the additional-offset token and must dereference
		// through the direct-layout element data; a missing index token means
		// the whole 64-bit handle is the operand (DIM/UNDIM/push-address).
		if (isArrayHandle || IsArrayType(type))
		{
			if (pPIndex)
				return isRbp ? static_cast<uint32_t>(ParamMode::RbpArr) : static_cast<uint32_t>(ParamMode::MemArr);
			return isRbp ? static_cast<uint32_t>(ParamMode::Rbp) : static_cast<uint32_t>(ParamMode::Mem);
		}

		if (type == DBPType::UserDefinedPtr)
			return isRbp ? static_cast<uint32_t>(ParamMode::Rbp) : static_cast<uint32_t>(ParamMode::Mem);

		if (dwPType > 200 && dwPType <= 299)
			return isRbp ? static_cast<uint32_t>(ParamMode::RbpRel) : static_cast<uint32_t>(ParamMode::MemRel);

		if (dwPOffset > 0)
			return isRbp ? static_cast<uint32_t>(ParamMode::RbpOff) : static_cast<uint32_t>(ParamMode::MemOff);

		return isRbp ? static_cast<uint32_t>(ParamMode::Rbp) : static_cast<uint32_t>(ParamMode::Mem);
	}

	return static_cast<uint32_t>(ParamMode::Imm);
}

uint32_t CTaskEmitter::CalculateTaskPassOffset(uint32_t dwPassNumber, uint32_t dwBaseOffset) const noexcept
{
    return dwPassNumber * dwBaseOffset;
}

void CTaskEmitter::WriteASMARRtoRAX(CASMWriter* pASMWriter, [[maybe_unused]] uint32_t dwMode, [[maybe_unused]] CStr* pP, [[maybe_unused]] CStr* pOffset, uint32_t dwPType, uint32_t dwPOffset) const
{
	if (!pASMWriter) return;

	uint32_t dwCorrectASMCode1=0;
	uint32_t dwCorrectASMCode2=0;

	CStr offset1Str("");
	CStr* pOffset1Str = &offset1Str;
	pOffset1Str->SetNumericText( dwPOffset );
	CStr offset2Str("");
	CStr* pOffset2Str = &offset2Str;
	pOffset2Str->SetNumericText( dwPOffset + 4 );

	if(pASMWriter->GetArrayCheckFlag())
	{
		pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::TESTRAXRAX), "");
		pASMWriter->WriteASMLeapMarkerJump(static_cast<uint32_t>(ASMOp::JE), 1);
	}

	pASMWriter->CalculateArrayOffsetInRBX(pOffset);

	// Direct-layout array: RAX points at element zero, so the element address
	// is RAX + index * stride. The stride is the runtime header's itemSize at
	// [handle-12] — the real byte size of the element, which no compile-time
	// table can supply for user-defined types.
	pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::PUSHRDX), "");
	pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRDXRAXOFF4), "-12");
	pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::IMULRBXRDX), "");
	pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::POPRDX), "");
	pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::ADDRAXRBX8), "");

	switch(dwPType-100)
	{
		case 1001:
					pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::ADDRAX4), pOffset1Str->GetStr());
					break;

		case 3:
					pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRCXRAXOFF8), pOffset1Str->GetStr());
					pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXRCX8), "");
					break;

		case 8:
					pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVXMM0RAX8), pOffset1Str->GetStr());
					break;

		case 9:
					pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRCXRAXOFF4), pOffset2Str->GetStr());
					pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRDXRCX4), "");
					pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRCXRAXOFF4), pOffset1Str->GetStr());
					pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXRCX4), "");
					break;

		default:
					dwCorrectASMCode1=pASMWriter->DetermineASMCallForREL(static_cast<uint32_t>(ASMOp::MOVRCXRAXOFF1),dwPType);
					dwCorrectASMCode2=pASMWriter->DetermineASMCallForREL(static_cast<uint32_t>(ASMOp::MOVRAXRCX1),dwPType);
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
		pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::JMP), const_cast<LPSTR>(leapString.c_str()));

		pASMWriter->WriteASMLeapMarkerEnd(1);
		pASMWriter->WriteASMLeapMarkerEnd(2);
		pASMWriter->WriteASMLeapMarkerEnd(3);

		pASMWriter->WriteASMLine2(static_cast<uint32_t>(ASMOp::MOVMEMIMM4), "@$_ERR_", "118");
		pASMWriter->WriteASMTaskCoreP2(pASMWriter->m_dwLineNumber, static_cast<uint32_t>(ASMTask::RuntimeErrorHook), nullptr, 0, nullptr, 0);
	}
}

void CTaskEmitter::WriteASMXtoRAX(CASMWriter* pASMWriter, uint32_t dwMode, CStr* pP, CStr* pPIndex, uint32_t dwPType, uint32_t dwPOffset) const
{
	if (!pASMWriter) return;

	uint32_t dwCorrectASMCode = 0;
	int iOffset = 0;
	uint32_t dwDwordRep=0;
	uint32_t dwExtraDword=0;
	uint32_t dwIMMSize=0;

	CStr doubleStr, dword1Str, dword2Str, offset1Str, offset2Str, temp1Str, temp2Str;
	CStr* pDoubleStr = &doubleStr;
	CStr* pdword1Str = &dword1Str;
	CStr* pdword2Str = &dword2Str;
	CStr* pOffset1Str = &offset1Str;
	CStr* pOffset2Str = &offset2Str;
	CStr* pTemp1Str = &temp1Str;
	CStr* pTemp2Str = &temp2Str;

	switch(dwMode)
	{
		case static_cast<uint32_t>(ParamMode::Imm):
			dwExtraDword=0;
			dwDwordRep = pP->GetDWORDRepresentation(dwPType, &dwExtraDword);
			pdword1Str->SetDWORDNumericText(dwDwordRep);
			pdword2Str->SetDWORDNumericText(dwExtraDword);
			switch(dwPType)
			{
				case 8:
							pTemp1Str->SetText("@$_TEMPA_");
							pTemp2Str->SetText("@$_TEMPB_");
							pASMWriter->WriteASMLine2IMM(static_cast<uint32_t>(ASMOp::MOVMEMIMM4), pTemp1Str->GetStr(), pdword1Str->GetStr(),2);
							pASMWriter->WriteASMLine2IMM(static_cast<uint32_t>(ASMOp::MOVMEMIMM4), pTemp2Str->GetStr(), pdword2Str->GetStr(),2);
							pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVXMM0MEM8), pTemp1Str->GetStr());
							break;

				case 9:
							pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRDXIMM4), pdword2Str->GetStr());
							pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXIMM4), pdword1Str->GetStr());
							break;

				case 3:
							pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXIMM8), pP->GetStr());
							break;

				case 20:
							pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXIMM8), pP->GetStr());
							break;

				default:
							dwCorrectASMCode=DetermineASMCall(static_cast<uint32_t>(ASMOp::MOVRAXIMM1),dwPType);
							dwIMMSize=dwCorrectASMCode-static_cast<uint32_t>(ASMOp::MOVRAXIMM1);
							pASMWriter->WriteASMLine2IMM(dwCorrectASMCode, nullptr, pdword1Str->GetStr(), dwIMMSize);
							break;
			}
			break;
			
		case static_cast<uint32_t>(ParamMode::Mem):
			if (IsPointerOrHandleType(dwPType) || IsArrayHandleToken(pP))
			{
				pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXMEM8), pP->GetStr());
			}
			else
			{
				pDoubleStr->SetText("+");
				pDoubleStr->AddText(pP->GetStr());
				switch(dwPType)
				{
					case 8:
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVXMM0MEM8), pP->GetStr());
						break;

					case 9:
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXMEM4), pDoubleStr->GetStr());
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRDXRAX4), "");
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXMEM4), pP->GetStr());
						break;

					default:
						dwCorrectASMCode=DetermineASMCallWide(static_cast<uint32_t>(ASMOp::MOVRAXMEM1),dwPType);
						pASMWriter->WriteASMLine(dwCorrectASMCode, pP->GetStr());
						break;
				}
			}
			break;

		case static_cast<uint32_t>(ParamMode::MemOff):
			pOffset1Str->SetNumericText(dwPOffset);
			pOffset2Str->SetNumericText(dwPOffset+4);
			pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRCXIMM8), pP->GetStr());
			if (IsPointerOrHandleType(dwPType))
			{
				pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXRCXOFF8), pOffset1Str->GetStr());
			}
			else
			{
				switch(dwPType)
				{
					case 8:
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVXMM0RCXOFF8), pOffset1Str->GetStr());
						break;

					case 9:
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXRCXOFF4), pOffset2Str->GetStr());
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRDXRAX4), "");
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXRCXOFF4), pOffset1Str->GetStr());
						break;
		
					default:
						dwCorrectASMCode=DetermineASMCallWide(static_cast<uint32_t>(ASMOp::MOVRAXRCXOFF1),dwPType);
						pASMWriter->WriteASMLine(dwCorrectASMCode, pOffset1Str->GetStr());
						break;
				}
			}
			break;

		case static_cast<uint32_t>(ParamMode::Rbp):
			if (IsPointerOrHandleType(dwPType) || IsArrayHandleToken(pP))
			{
				pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXRBP8), (pP->GetStr()+2));
			}
			else
			{
				pDoubleStr->SetText((pP->GetStr()+2));
				iOffset=(int)pDoubleStr->GetValue();
				iOffset+=4;
				pDoubleStr->SetNumericText(iOffset);
				switch(dwPType)
				{
					case 8:
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVXMM0RBP8), (pP->GetStr()+2));
						break;

					case 9:
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXRBP4), pDoubleStr->GetStr());
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRDXRAX4), "");
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXRBP4), (pP->GetStr()+2));
						break;

					default:
						dwCorrectASMCode=DetermineASMCallWide(static_cast<uint32_t>(ASMOp::MOVRAXRBP1),dwPType);
						pASMWriter->WriteASMLine(dwCorrectASMCode, (pP->GetStr()+2));	
						break;
				}
			}
			break;

		case static_cast<uint32_t>(ParamMode::RbpOff):
			pOffset2Str->SetText((pP->GetStr()+2));
			iOffset=(int)pOffset2Str->GetValue();
			pOffset1Str->SetNumericText( iOffset + dwPOffset );
			iOffset+=4;
			pOffset2Str->SetNumericText( iOffset + dwPOffset );
			if (IsPointerOrHandleType(dwPType))
			{
				pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXRBP8), pOffset1Str->GetStr());
			}
			else
			{
				switch(dwPType)
				{
					case 8:
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVXMM0RBP8), pOffset1Str->GetStr());
						break;

					case 9:
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXRBP4), pOffset2Str->GetStr());
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRDXRAX4), "");
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXRBP4), pOffset1Str->GetStr());
						break;

					default:
						dwCorrectASMCode=DetermineASMCallWide(static_cast<uint32_t>(ASMOp::MOVRAXRBP1),dwPType);
						pASMWriter->WriteASMLine(dwCorrectASMCode, pOffset1Str->GetStr());	
						break;
				}
			}
			break;

		case static_cast<uint32_t>(ParamMode::MemArr):
			pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXMEM8), pP->GetStr());
			WriteASMARRtoRAX(pASMWriter, dwMode, pP, pPIndex, dwPType, dwPOffset);
			break;

		case static_cast<uint32_t>(ParamMode::RbpArr):
			pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXRBP8), (pP->GetStr()+2));
			WriteASMARRtoRAX(pASMWriter, dwMode, pP, pPIndex, dwPType, dwPOffset);
			break;

		case static_cast<uint32_t>(ParamMode::MemRel):
			pDoubleStr->SetText("+");
			pDoubleStr->AddText(pP->GetStr());
			switch(dwPType)
			{
				case 8: break;
				case 9: break;
				default:
							dwCorrectASMCode=DetermineASMCallWide(static_cast<uint32_t>(ASMOp::MOVRAXMEM1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, pP->GetStr());
							dwCorrectASMCode=DetermineASMCallWide(static_cast<uint32_t>(ASMOp::MOVRAXRAXREL1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, "");
							break;
			}
			break;

		case static_cast<uint32_t>(ParamMode::RbpRel):
			switch(dwPType)
			{
				case 8: break;
				case 9: break;
				default:
							dwCorrectASMCode=DetermineASMCallWide(static_cast<uint32_t>(ASMOp::MOVRAXRBP1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, (pP->GetStr()+2));
							dwCorrectASMCode=DetermineASMCallWide(static_cast<uint32_t>(ASMOp::MOVRAXRAXREL1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, "");
							break;
			}
			break;
	}
}

void CTaskEmitter::WriteASMRAXtoARR(CASMWriter* pASMWriter, [[maybe_unused]] uint32_t dwMode, [[maybe_unused]] CStr* pP, [[maybe_unused]] CStr* pOffset, uint32_t dwPType, uint32_t dwPOffset) const
{
	if (!pASMWriter) return;

	uint32_t dwCorrectASMCode=0;

	CStr offset1Str("");
	CStr* pOffset1Str = &offset1Str;
	pOffset1Str->SetNumericText( dwPOffset );
	CStr offset2Str("");
	CStr* pOffset2Str = &offset2Str;
	pOffset2Str->SetNumericText( dwPOffset+4 );

	if(pASMWriter->GetArrayCheckFlag())
	{
		pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::TESTRAXRAX), "");
		pASMWriter->WriteASMLeapMarkerJump(static_cast<uint32_t>(ASMOp::JE), 1);
	}

	pASMWriter->CalculateArrayOffsetInRBX(pOffset);

	// Direct-layout array: RAX points at element zero, so the element address
	// is RAX + index * stride. The stride is the runtime header's itemSize at
	// [handle-12] — the real byte size of the element, which no compile-time
	// table can supply for user-defined types.
	pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::PUSHRDX), "");
	pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRDXRAXOFF4), "-12");
	pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::IMULRBXRDX), "");
	pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::POPRDX), "");
	pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::ADDRAXRBX8), "");

	switch(dwPType-100)
	{
		case 1001:
					pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::ADDRAXRCX4), "");
					break;

		case 3:
					pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXOFFRCX8), pOffset1Str->GetStr());
					break;

		case 8:
					pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXXMM0), pOffset1Str->GetStr());
					break;

		case 9:
					pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXOFFRCX4), pOffset1Str->GetStr());
					pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRCXRDX4), "");
					pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXOFFRCX4), pOffset2Str->GetStr());
					break;

		default:
					dwCorrectASMCode=pASMWriter->DetermineASMCallForREL(static_cast<uint32_t>(ASMOp::MOVRAXOFFRCX1),dwPType);
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
		pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::JMP), const_cast<LPSTR>(leapString.c_str()));

		pASMWriter->WriteASMLeapMarkerEnd(1);
		pASMWriter->WriteASMLeapMarkerEnd(2);
		pASMWriter->WriteASMLeapMarkerEnd(3);

		pASMWriter->WriteASMLine2(static_cast<uint32_t>(ASMOp::MOVMEMIMM4), "@$_ERR_", "118");
		pASMWriter->WriteASMTaskCoreP2(pASMWriter->m_dwLineNumber, static_cast<uint32_t>(ASMTask::RuntimeErrorHook), nullptr, 0, nullptr, 0);
	}
}

void CTaskEmitter::WriteASMRAXtoX(CASMWriter* pASMWriter, uint32_t dwMode, CStr* pP, CStr* pPIndex, uint32_t dwPType, uint32_t dwPOffset) const
{
	if (!pASMWriter) return;

	uint32_t dwCorrectASMCode=0;
	int iOffset=0;

	CStr doubleStr, offset1Str, offset2Str, temp1Str, temp2Str;
	CStr* pDoubleStr = &doubleStr;
	CStr* pOffset1Str = &offset1Str;
	CStr* pOffset2Str = &offset2Str;
	CStr* pTemp1Str = &temp1Str;
	CStr* pTemp2Str = &temp2Str;

	switch(dwMode)
	{
		case static_cast<uint32_t>(ParamMode::Mem):
			if (IsPointerOrHandleType(dwPType) || IsArrayHandleToken(pP))
			{
				pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVMEMRAX8), pP->GetStr());
			}
			else
			{
				pDoubleStr->SetText("+");
				pDoubleStr->AddText(pP->GetStr());
				switch(dwPType)
				{
					case 8:
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVMEMXMM0), pP->GetStr());
						break;

					case 9:
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVMEMRAX4), pP->GetStr());
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXRDX4), "");
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVMEMRAX4), pDoubleStr->GetStr());
						break;
		
					default:
						dwCorrectASMCode=DetermineASMCallWide(static_cast<uint32_t>(ASMOp::MOVMEMRAX1),dwPType);
						pASMWriter->WriteASMLine(dwCorrectASMCode, pP->GetStr());
						break;
				}
			}
			break;

		case static_cast<uint32_t>(ParamMode::MemOff):
			pOffset1Str->SetNumericText(dwPOffset);
			pOffset2Str->SetNumericText(dwPOffset+4);
			pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRCXIMM8), pP->GetStr());
			if (IsPointerOrHandleType(dwPType))
			{
				pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRCXOFFRAX8), pOffset1Str->GetStr());
			}
			else
			{
				switch(dwPType)
				{
					case 8:
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRCXOFFXMM0), pOffset1Str->GetStr());
						break;

					case 9:
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRCXOFFRAX4), pOffset1Str->GetStr());
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXRDX4), "");
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRCXOFFRAX4), pOffset2Str->GetStr());
						break;
		
					default:
						dwCorrectASMCode=DetermineASMCallWide(static_cast<uint32_t>(ASMOp::MOVRCXOFFRAX1),dwPType);
						pASMWriter->WriteASMLine(dwCorrectASMCode, pOffset1Str->GetStr());
						break;
				}
			}
			break;

		case static_cast<uint32_t>(ParamMode::Rbp):
			if (IsPointerOrHandleType(dwPType) || IsArrayHandleToken(pP))
			{
				pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRBPRAX8), (pP->GetStr()+2));
			}
			else
			{
				pDoubleStr->SetText((pP->GetStr()+2));
				iOffset=(int)pDoubleStr->GetValue();
				iOffset+=4;
				pDoubleStr->SetNumericText(iOffset);
				switch(dwPType)
				{
					case 8:
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRBPXMM0), (pP->GetStr()+2));
						break;

					case 9:
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRBPRAX4), (pP->GetStr()+2));
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXRDX4), "");
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRBPRAX4), pDoubleStr->GetStr());
						break;

					default:
						dwCorrectASMCode=DetermineASMCallWide(static_cast<uint32_t>(ASMOp::MOVRBPRAX1),dwPType);
						pASMWriter->WriteASMLine(dwCorrectASMCode, (pP->GetStr()+2));
						break;
				}
			}
			break;

		case static_cast<uint32_t>(ParamMode::RbpOff):
			pOffset2Str->SetText((pP->GetStr()+2));
			iOffset=(int)pOffset2Str->GetValue();
			pOffset1Str->SetNumericText( iOffset + dwPOffset );
			iOffset+=4;
			pOffset2Str->SetNumericText( iOffset + dwPOffset );
			if (IsPointerOrHandleType(dwPType))
			{
				pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRBPRAX8), pOffset1Str->GetStr());
			}
			else
			{
				switch(dwPType)
				{
					case 8:
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRBPXMM0), pOffset1Str->GetStr());
						break;

					case 9:
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRBPRAX4), pOffset1Str->GetStr());
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXRDX4), "");
						pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRBPRAX4), pOffset2Str->GetStr());
						break;

					default:
						dwCorrectASMCode=DetermineASMCallWide(static_cast<uint32_t>(ASMOp::MOVRBPRAX1),dwPType);
						pASMWriter->WriteASMLine(dwCorrectASMCode, pOffset1Str->GetStr());
						break;
				}
			}
			break;

		case static_cast<uint32_t>(ParamMode::MemArr):
			pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRCXRAX8), "");
			pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXMEM8), pP->GetStr());
			WriteASMRAXtoARR(pASMWriter, dwMode, pP, pPIndex, dwPType, dwPOffset);
			break;

		case static_cast<uint32_t>(ParamMode::RbpArr):
			pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRCXRAX8), "");
			pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXRBP8), (pP->GetStr()+2));
			WriteASMRAXtoARR(pASMWriter, dwMode, pP, pPIndex, dwPType, dwPOffset);
			break;

		case static_cast<uint32_t>(ParamMode::Stack):
			switch(dwPType)
			{
				case 8:		
				case 108:
							pTemp1Str->SetText("@$_TEMPA_");
							pTemp2Str->SetText("@$_TEMPB_");
							pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVMEMXMM0), pTemp1Str->GetStr());
							pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXMEM4), pTemp2Str->GetStr());
							pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::PUSHRAX), "");
							pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRAXMEM4), pTemp1Str->GetStr());
							pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::PUSHRAX), "");
							break;

				case 9:		
				case 109:
							pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::PUSHRDX), "");
							pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::PUSHRAX), "");
							break;

				default:
							pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::PUSHRAX), "");
							break;
			}
			break;

		case static_cast<uint32_t>(ParamMode::MemRel):
			pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRCXRAX4), "");
			pDoubleStr->SetText("+");
			pDoubleStr->AddText(pP->GetStr());
			switch(dwPType)
			{
				case 8: break;
				case 9: break;
				default:
							dwCorrectASMCode=DetermineASMCallWide(static_cast<uint32_t>(ASMOp::MOVRAXMEM1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, pP->GetStr());
							dwCorrectASMCode=DetermineASMCallWide(static_cast<uint32_t>(ASMOp::MOVRAXRCXREL1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, "");
							break;
			}
			break;

		case static_cast<uint32_t>(ParamMode::RbpRel):
			pASMWriter->WriteASMLine(static_cast<uint32_t>(ASMOp::MOVRCXRAX4), "");
			switch(dwPType)
			{
				case 8: break;
				case 9: break;
				default:
							dwCorrectASMCode=DetermineASMCallWide(static_cast<uint32_t>(ASMOp::MOVRAXRBP1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, (pP->GetStr()+2));
							dwCorrectASMCode=DetermineASMCallWide(static_cast<uint32_t>(ASMOp::MOVRAXRCXREL1),dwPType);
							pASMWriter->WriteASMLine(dwCorrectASMCode, "");
							break;
			}
			break;
	}
}
