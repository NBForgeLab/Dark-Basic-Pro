#include <gtest/gtest.h>
#include "ASMWriter.h"

TEST(X64AssemblerTest, AssignsRegistersForFirstFourArguments) {
    EXPECT_EQ(CASMWriter::GetArgumentRegister(0), X64Register::RCX);
    EXPECT_EQ(CASMWriter::GetArgumentRegister(1), X64Register::RDX);
    EXPECT_EQ(CASMWriter::GetArgumentRegister(2), X64Register::R8);
    EXPECT_EQ(CASMWriter::GetArgumentRegister(3), X64Register::R9);
    EXPECT_EQ(CASMWriter::GetArgumentRegister(4), X64Register::None);
}

TEST(X64AssemblerTest, CalculatesShadowSpaceAndStackAlignment) {
    EXPECT_EQ(CASMWriter::GetShadowSpaceSize(), 32U);
    EXPECT_EQ(CASMWriter::AlignStackFrame(0U), 32U);
    EXPECT_EQ(CASMWriter::AlignStackFrame(24U), 32U);
    EXPECT_EQ(CASMWriter::AlignStackFrame(32U), 32U);
    EXPECT_EQ(CASMWriter::AlignStackFrame(36U), 48U);
}

TEST(X64AssemblerTest, InstantiatesAndReportsArraySafetyFlags) {
    CASMWriter writer;
    writer.SetDefaultCompileFlags(true);
    EXPECT_TRUE(writer.GetArrayCheckFlag());

    writer.SetArrayCheckFlag(false);
    EXPECT_FALSE(writer.GetArrayCheckFlag());
}

TEST(X64AssemblerTest, EmitsMovRegImm64Opcode) {
    CASMWriter writer;
    writer.EmitMovRegImm64(X64Register::RAX, 0x1122334455667788ULL);
    const auto& buf = writer.GetCodeBuffer();
    ASSERT_EQ(buf.size(), 10U);
    EXPECT_EQ(buf[0], 0x48); // REX.W
    EXPECT_EQ(buf[1], 0xB8); // MOV RAX, imm64
    EXPECT_EQ(buf[2], 0x88); // Little-endian imm64
    EXPECT_EQ(buf[9], 0x11);
}

TEST(X64AssemblerTest, EmitsPushAndPopOpcodes) {
    CASMWriter writer;
    writer.EmitPushReg(X64Register::RAX);
    writer.EmitPushReg(X64Register::R8);
    writer.EmitPopReg(X64Register::RAX);
    writer.EmitPopReg(X64Register::R8);
    const auto& buf = writer.GetCodeBuffer();
    ASSERT_EQ(buf.size(), 6U);
    EXPECT_EQ(buf[0], 0x50);       // PUSH RAX
    EXPECT_EQ(buf[1], 0x41);       // REX.B (for R8)
    EXPECT_EQ(buf[2], 0x50);       // PUSH R8
    EXPECT_EQ(buf[3], 0x58);       // POP RAX
    EXPECT_EQ(buf[4], 0x41);       // REX.B (for R8)
    EXPECT_EQ(buf[5], 0x58);       // POP R8
}

TEST(X64AssemblerTest, EmitsSubAndAddRspStackAlignment) {
    CASMWriter writer;
    writer.EmitSubRegImm32(X64Register::RSP, 32);
    writer.EmitAddRegImm32(X64Register::RSP, 32);
    writer.EmitRet();
    const auto& buf = writer.GetCodeBuffer();
    ASSERT_EQ(buf.size(), 15U);
    // SUB RSP, 32 (48 81 EC 20 00 00 00)
    EXPECT_EQ(buf[0], 0x48);
    EXPECT_EQ(buf[1], 0x81);
    EXPECT_EQ(buf[2], 0xEC);
    EXPECT_EQ(buf[3], 0x20);
    // ADD RSP, 32 (48 81 C4 20 00 00 00)
    EXPECT_EQ(buf[7], 0x48);
    EXPECT_EQ(buf[8], 0x81);
    EXPECT_EQ(buf[9], 0xC4);
    EXPECT_EQ(buf[10], 0x20);
    // RET (C3)
    EXPECT_EQ(buf[14], 0xC3);
}

TEST(X64AssemblerTest, EmitsCmpAndTestRegRegOpcodes) {
    CASMWriter writer;
    writer.EmitCmpRegReg(X64Register::RAX, X64Register::RCX);
    writer.EmitTestRegReg(X64Register::R8, X64Register::R9);
    const auto& buf = writer.GetCodeBuffer();
    ASSERT_EQ(buf.size(), 6U);
    // CMP RAX, RCX (48 39 C8)
    EXPECT_EQ(buf[0], 0x48);
    EXPECT_EQ(buf[1], 0x39);
    EXPECT_EQ(buf[2], 0xC8);
    // TEST R8, R9 (4D 85 C8)
    EXPECT_EQ(buf[3], 0x4D);
    EXPECT_EQ(buf[4], 0x85);
    EXPECT_EQ(buf[5], 0xC8);
}

TEST(X64AssemblerTest, EmitsControlFlowJumpAndCallOpcodes) {
    CASMWriter writer;
    writer.EmitJmpRel32(16);
    writer.EmitJneRel32(32);
    writer.EmitJeRel32(-8);
    writer.EmitCallReg(X64Register::RAX);
    writer.EmitCallReg(X64Register::R8);
    writer.EmitNop();
    const auto& buf = writer.GetCodeBuffer();
    ASSERT_EQ(buf.size(), 23U);
    // JMP rel32 (E9 10 00 00 00)
    EXPECT_EQ(buf[0], 0xE9);
    EXPECT_EQ(buf[1], 0x10);
    // JNE rel32 (0F 85 20 00 00 00)
    EXPECT_EQ(buf[5], 0x0F);
    EXPECT_EQ(buf[6], 0x85);
    EXPECT_EQ(buf[7], 0x20);
    // JE rel32 (0F 84 F8 FF FF FF)
    EXPECT_EQ(buf[11], 0x0F);
    EXPECT_EQ(buf[12], 0x84);
    EXPECT_EQ(buf[13], 0xF8);
    EXPECT_EQ(buf[14], 0xFF);
    // CALL RAX (FF D0)
    EXPECT_EQ(buf[17], 0xFF);
    EXPECT_EQ(buf[18], 0xD0);
    // CALL R8 (41 FF D0)
    EXPECT_EQ(buf[19], 0x41);
    EXPECT_EQ(buf[20], 0xFF);
    EXPECT_EQ(buf[21], 0xD0);
    // NOP (90)
    EXPECT_EQ(buf[22], 0x90);
}

TEST(X64AssemblerTest, EmitsSseScalarFloatOperations) {
    CASMWriter writer;
    writer.EmitMovss(XMMRegister::XMM0, XMMRegister::XMM1);
    writer.EmitAddss(XMMRegister::XMM2, XMMRegister::XMM3);
    writer.EmitMulss(XMMRegister::XMM8, XMMRegister::XMM9);
    const auto& buf = writer.GetCodeBuffer();
    ASSERT_EQ(buf.size(), 13U);
    // MOVSS XMM0, XMM1 (F3 0F 10 C1)
    EXPECT_EQ(buf[0], 0xF3);
    EXPECT_EQ(buf[1], 0x0F);
    EXPECT_EQ(buf[2], 0x10);
    EXPECT_EQ(buf[3], 0xC1);
    // ADDSS XMM2, XMM3 (F3 0F 58 D3)
    EXPECT_EQ(buf[4], 0xF3);
    EXPECT_EQ(buf[5], 0x0F);
    EXPECT_EQ(buf[6], 0x58);
    EXPECT_EQ(buf[7], 0xD3);
    // MULSS XMM8, XMM9 (F3 45 0F 59 C1)
    EXPECT_EQ(buf[8], 0xF3);
    EXPECT_EQ(buf[9], 0x45); // REX.R | REX.B
    EXPECT_EQ(buf[10], 0x0F);
    EXPECT_EQ(buf[11], 0x59);
    EXPECT_EQ(buf[12], 0xC1);
}
