#include "ASMWriterx64.h"

CASMWriterx64::CASMWriterx64() = default;

namespace {
constexpr uint8_t GetRegIndex(X64Register reg) noexcept {
    switch (reg) {
        case X64Register::RAX: return 0;
        case X64Register::RCX: return 1;
        case X64Register::RDX: return 2;
        case X64Register::RBX: return 3;
        case X64Register::RSP: return 4;
        case X64Register::RBP: return 5;
        case X64Register::RSI: return 6;
        case X64Register::RDI: return 7;
        case X64Register::R8:  return 8;
        case X64Register::R9:  return 9;
        case X64Register::R10: return 10;
        case X64Register::R11: return 11;
        case X64Register::R12: return 12;
        case X64Register::R13: return 13;
        case X64Register::R14: return 14;
        case X64Register::R15: return 15;
        default: return 0;
    }
}
}

void CASMWriterx64::EmitByte(uint8_t b) {
    m_codeBuffer.push_back(b);
}

void CASMWriterx64::EmitDword(uint32_t dw) {
    m_codeBuffer.push_back(static_cast<uint8_t>(dw & 0xFF));
    m_codeBuffer.push_back(static_cast<uint8_t>((dw >> 8) & 0xFF));
    m_codeBuffer.push_back(static_cast<uint8_t>((dw >> 16) & 0xFF));
    m_codeBuffer.push_back(static_cast<uint8_t>((dw >> 24) & 0xFF));
}

void CASMWriterx64::EmitQword(uint64_t qw) {
    EmitDword(static_cast<uint32_t>(qw & 0xFFFFFFFFULL));
    EmitDword(static_cast<uint32_t>((qw >> 32) & 0xFFFFFFFFULL));
}

void CASMWriterx64::EmitMovRegImm64(X64Register reg, uint64_t val) {
    const uint8_t idx = GetRegIndex(reg);
    uint8_t rex = 0x48; // REX.W
    if (idx >= 8) rex |= 0x01; // REX.B
    EmitByte(rex);
    EmitByte(static_cast<uint8_t>(0xB8 + (idx & 7)));
    EmitQword(val);
}

void CASMWriterx64::EmitPushReg(X64Register reg) {
    const uint8_t idx = GetRegIndex(reg);
    if (idx >= 8) EmitByte(0x41); // REX.B
    EmitByte(static_cast<uint8_t>(0x50 + (idx & 7)));
}

void CASMWriterx64::EmitPopReg(X64Register reg) {
    const uint8_t idx = GetRegIndex(reg);
    if (idx >= 8) EmitByte(0x41); // REX.B
    EmitByte(static_cast<uint8_t>(0x58 + (idx & 7)));
}

void CASMWriterx64::EmitSubRegImm32(X64Register reg, uint32_t val) {
    const uint8_t idx = GetRegIndex(reg);
    uint8_t rex = 0x48; // REX.W
    if (idx >= 8) rex |= 0x01; // REX.B
    EmitByte(rex);
    EmitByte(0x81); // SUB r/m64, imm32
    EmitByte(static_cast<uint8_t>(0xE8 | (idx & 7)));
    EmitDword(val);
}

void CASMWriterx64::EmitAddRegImm32(X64Register reg, uint32_t val) {
    const uint8_t idx = GetRegIndex(reg);
    uint8_t rex = 0x48; // REX.W
    if (idx >= 8) rex |= 0x01; // REX.B
    EmitByte(rex);
    EmitByte(0x81); // ADD r/m64, imm32
    EmitByte(static_cast<uint8_t>(0xC0 | (idx & 7)));
    EmitDword(val);
}

void CASMWriterx64::EmitRet() {
    EmitByte(0xC3);
}

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
