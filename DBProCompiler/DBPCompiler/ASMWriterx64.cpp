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

void CASMWriterx64::EmitCmpRegReg(X64Register reg1, X64Register reg2) {
    const uint8_t idx1 = GetRegIndex(reg1);
    const uint8_t idx2 = GetRegIndex(reg2);
    uint8_t rex = 0x48; // REX.W
    if (idx2 >= 8) rex |= 0x04; // REX.R
    if (idx1 >= 8) rex |= 0x01; // REX.B
    EmitByte(rex);
    EmitByte(0x39); // CMP r/m64, r64
    EmitByte(static_cast<uint8_t>(0xC0 | ((idx2 & 7) << 3) | (idx1 & 7)));
}

void CASMWriterx64::EmitTestRegReg(X64Register reg1, X64Register reg2) {
    const uint8_t idx1 = GetRegIndex(reg1);
    const uint8_t idx2 = GetRegIndex(reg2);
    uint8_t rex = 0x48; // REX.W
    if (idx2 >= 8) rex |= 0x04; // REX.R
    if (idx1 >= 8) rex |= 0x01; // REX.B
    EmitByte(rex);
    EmitByte(0x85); // TEST r/m64, r64
    EmitByte(static_cast<uint8_t>(0xC0 | ((idx2 & 7) << 3) | (idx1 & 7)));
}

void CASMWriterx64::EmitJmpRel32(int32_t relOffset) {
    EmitByte(0xE9);
    EmitDword(static_cast<uint32_t>(relOffset));
}

void CASMWriterx64::EmitJneRel32(int32_t relOffset) {
    EmitByte(0x0F);
    EmitByte(0x85);
    EmitDword(static_cast<uint32_t>(relOffset));
}

void CASMWriterx64::EmitJeRel32(int32_t relOffset) {
    EmitByte(0x0F);
    EmitByte(0x84);
    EmitDword(static_cast<uint32_t>(relOffset));
}

void CASMWriterx64::EmitCallReg(X64Register reg) {
    const uint8_t idx = GetRegIndex(reg);
    if (idx >= 8) EmitByte(0x41); // REX.B
    EmitByte(0xFF);
    EmitByte(static_cast<uint8_t>(0xD0 | (idx & 7)));
}

void CASMWriterx64::EmitNop() {
    EmitByte(0x90);
}

namespace {
constexpr uint8_t GetXmmIndex(XMMRegister reg) noexcept {
    return static_cast<uint8_t>(reg);
}
}

void CASMWriterx64::EmitMovss(XMMRegister dst, XMMRegister src) {
    const uint8_t dstIdx = GetXmmIndex(dst);
    const uint8_t srcIdx = GetXmmIndex(src);
    EmitByte(0xF3);
    uint8_t rex = 0x40;
    if (dstIdx >= 8) rex |= 0x04; // REX.R
    if (srcIdx >= 8) rex |= 0x01; // REX.B
    if (rex != 0x40) EmitByte(rex);
    EmitByte(0x0F);
    EmitByte(0x10);
    EmitByte(static_cast<uint8_t>(0xC0 | ((dstIdx & 7) << 3) | (srcIdx & 7)));
}

void CASMWriterx64::EmitAddss(XMMRegister dst, XMMRegister src) {
    const uint8_t dstIdx = GetXmmIndex(dst);
    const uint8_t srcIdx = GetXmmIndex(src);
    EmitByte(0xF3);
    uint8_t rex = 0x40;
    if (dstIdx >= 8) rex |= 0x04; // REX.R
    if (srcIdx >= 8) rex |= 0x01; // REX.B
    if (rex != 0x40) EmitByte(rex);
    EmitByte(0x0F);
    EmitByte(0x58);
    EmitByte(static_cast<uint8_t>(0xC0 | ((dstIdx & 7) << 3) | (srcIdx & 7)));
}

void CASMWriterx64::EmitMulss(XMMRegister dst, XMMRegister src) {
    const uint8_t dstIdx = GetXmmIndex(dst);
    const uint8_t srcIdx = GetXmmIndex(src);
    EmitByte(0xF3);
    uint8_t rex = 0x40;
    if (dstIdx >= 8) rex |= 0x04; // REX.R
    if (srcIdx >= 8) rex |= 0x01; // REX.B
    if (rex != 0x40) EmitByte(rex);
    EmitByte(0x0F);
    EmitByte(0x59);
    EmitByte(static_cast<uint8_t>(0xC0 | ((dstIdx & 7) << 3) | (srcIdx & 7)));
}
