#include "ASMWriterx64.h"

CASMWriterx64::CASMWriterx64() = default;

void CASMWriterx64::SetDefaultCompileFlags(bool bArraySafetyFlag) {
    m_bArraySafetyFlag = bArraySafetyFlag;
}

void CASMWriterx64::SetArrayCheckFlag(bool bFlag) {
    m_bArraySafetyFlag = bFlag;
}

bool CASMWriterx64::GetArrayCheckFlag(void) {
    return m_bArraySafetyFlag;
}

void CASMWriterx64::GenerateASMCodes(void) {}
void CASMWriterx64::DefineASM(DWORD, LPSTR, int, int, int, bool) {}

bool CASMWriterx64::CreateASMHeader(void) { return true; }
bool CASMWriterx64::CreateASMMiddle(int, int, int, LPSTR) { return true; }
bool CASMWriterx64::CreateASMMiddleCore(int, int, int, LPSTR, LPSTR, bool, DWORD) { return true; }
bool CASMWriterx64::CheckAndExpandMCBMemory(void) { return true; }
bool CASMWriterx64::CheckAndExpandREFMemory(void) { return true; }

DWORD CASMWriterx64::GetCurrentMCPosition(void) { return 0U; }

bool CASMWriterx64::ReportAnyErrorsToCLI(void) { return false; }
bool CASMWriterx64::PrepareEXE(LPSTR, bool, bool) { return true; }
bool CASMWriterx64::UpdateMCB(DWORD) { return true; }
bool CASMWriterx64::UpdateMCBRefData(void) { return true; }
bool CASMWriterx64::UpdateDLLData(void) { return true; }
bool CASMWriterx64::UpdateCommandData(void) { return true; }
bool CASMWriterx64::UpdateStringData(void) { return true; }
bool CASMWriterx64::UpdateDataData(void) { return true; }
bool CASMWriterx64::UpdateDynamicData(void) { return true; }

void CASMWriterx64::UpdateStructurePatternDataRec(LPSTR, CDeclaration*) {}
bool CASMWriterx64::UpdateStructurePatternData(void) { return true; }

LPSTR CASMWriterx64::MakeVarDataForTransfer(DWORD* dwDataSize) {
    if (dwDataSize) *dwDataSize = 0U;
    return nullptr;
}

LPSTR CASMWriterx64::MakeVarValuesForTransfer(DWORD* dwDataSize) {
    if (dwDataSize) *dwDataSize = 0U;
    return nullptr;
}

void CASMWriterx64::TraverseDecForPattern(DWORD, short, DWORD*, DWORD*, CDeclaration*) {}
void CASMWriterx64::FreeMachineBlock(void) {}
void CASMWriterx64::FreeAll(void) {}

DWORD CASMWriterx64::GetBytePosOfLastInstruction(void) { return 0U; }

DWORD CASMWriterx64::DetermineASMCall(DWORD, DWORD) { return 0U; }
DWORD CASMWriterx64::DetermineASMCallForREL(DWORD, DWORD) { return 0U; }
DWORD CASMWriterx64::DetMode(CStr*, DWORD, DWORD) { return 0U; }

void CASMWriterx64::CalculateArrayOffsetInEBX(CStr*) {}
void CASMWriterx64::WriteASMEAXtoARR(DWORD, CStr*, CStr*, DWORD, DWORD) {}
void CASMWriterx64::WriteASMARRtoEAX(DWORD, CStr*, CStr*, DWORD, DWORD) {}
void CASMWriterx64::WriteASMXtoEAX(DWORD, CStr*, CStr*, DWORD, DWORD) {}
void CASMWriterx64::WriteASMEAXtoX(DWORD, CStr*, CStr*, DWORD, DWORD) {}

bool CASMWriterx64::WriteASMCall(DWORD, LPSTR, LPSTR) { return true; }
bool CASMWriterx64::WriteASMTaskP1(DWORD, DWORD, CResultData*) { return true; }
bool CASMWriterx64::WriteASMTaskP2(DWORD, DWORD, CResultData*, CResultData*) { return true; }
bool CASMWriterx64::WriteASMTaskP3(DWORD, DWORD, CResultData*, CResultData*, CResultData*) { return true; }
bool CASMWriterx64::WriteASMTaskCoreP1(DWORD, DWORD, CStr*, DWORD) { return true; }
bool CASMWriterx64::WriteASMTaskCoreP2(DWORD, DWORD, CStr*, DWORD, CStr*, DWORD) { return true; }
bool CASMWriterx64::WriteASMTaskCore(DWORD, DWORD, CStr*, CStr*, DWORD, DWORD, CStr*, CStr*, DWORD, DWORD) { return true; }
bool CASMWriterx64::WriteASMTaskCore(DWORD, DWORD, CStr*, CStr*, DWORD, DWORD,
                                      CStr*, CStr*, DWORD, DWORD,
                                      CStr*, CStr*, DWORD, DWORD) { return true; }

bool CASMWriterx64::WriteASMLine(DWORD, LPSTR) { return true; }
bool CASMWriterx64::WriteASMLine2(DWORD, LPSTR, LPSTR) { return true; }
bool CASMWriterx64::WriteASMLine1IMM(DWORD, LPSTR, DWORD) { return true; }
bool CASMWriterx64::WriteASMLine2IMM(DWORD, LPSTR, LPSTR, DWORD) { return true; }
bool CASMWriterx64::WriteASMComment(LPSTR, LPSTR, LPSTR, LPSTR) { return true; }

bool CASMWriterx64::WriteASMLeapMarkerTop(void) { return true; }
bool CASMWriterx64::WriteASMLineLeapToTop(DWORD) { return true; }
bool CASMWriterx64::WriteASMLeapMarkerJumpToTop(void) { return true; }

bool CASMWriterx64::WriteASMLeapForwardMarker(void) { return true; }
bool CASMWriterx64::WriteASMLineLeap(DWORD, DWORD) { return true; }
bool CASMWriterx64::WriteASMLeapMarkerJump(DWORD, DWORD) { return true; }
bool CASMWriterx64::WriteASMLeapMarkerJumpNotEqual(DWORD) { return true; }
bool CASMWriterx64::WriteASMLeapMarkerEnd(DWORD) { return true; }
bool CASMWriterx64::WriteASMCheckBreakPointVar(void) { return true; }
bool CASMWriterx64::WriteASMForceEscapeAtCodeBREAK(void) { return true; }
void CASMWriterx64::SetBreakPointValue(void) {}

DWORD CASMWriterx64::AddCommandToTable(LPSTR, LPSTR) { return 0U; }
bool CASMWriterx64::AddProtectionToSelectedDLLs(LPSTR) { return true; }
