#include <gtest/gtest.h>
#include "ASMWriterx64.h"

TEST(X64AssemblerTest, AssignsRegistersForFirstFourArguments) {
    EXPECT_EQ(CASMWriterx64::GetArgumentRegister(0), X64Register::RCX);
    EXPECT_EQ(CASMWriterx64::GetArgumentRegister(1), X64Register::RDX);
    EXPECT_EQ(CASMWriterx64::GetArgumentRegister(2), X64Register::R8);
    EXPECT_EQ(CASMWriterx64::GetArgumentRegister(3), X64Register::R9);
    EXPECT_EQ(CASMWriterx64::GetArgumentRegister(4), X64Register::None);
}

TEST(X64AssemblerTest, CalculatesShadowSpaceAndStackAlignment) {
    EXPECT_EQ(CASMWriterx64::GetShadowSpaceSize(), 32U);
    EXPECT_EQ(CASMWriterx64::AlignStackFrame(0U), 32U);
    EXPECT_EQ(CASMWriterx64::AlignStackFrame(24U), 32U);
    EXPECT_EQ(CASMWriterx64::AlignStackFrame(32U), 32U);
    EXPECT_EQ(CASMWriterx64::AlignStackFrame(36U), 48U);
}

TEST(X64AssemblerTest, InstantiatesAndReportsArraySafetyFlags) {
    CASMWriterx64 writer;
    writer.SetDefaultCompileFlags(true);
    EXPECT_TRUE(writer.GetArrayCheckFlag());

    writer.SetArrayCheckFlag(false);
    EXPECT_FALSE(writer.GetArrayCheckFlag());
}

TEST(X64AssemblerTest, EmitsMovRegImm64Opcode) {
    CASMWriterx64 writer;
    writer.EmitMovRegImm64(X64Register::RAX, 0x1122334455667788ULL);
    const auto& buf = writer.GetCodeBuffer();
    ASSERT_EQ(buf.size(), 10U);
    EXPECT_EQ(buf[0], 0x48); // REX.W
    EXPECT_EQ(buf[1], 0xB8); // MOV RAX, imm64
    EXPECT_EQ(buf[2], 0x88); // Little-endian imm64
    EXPECT_EQ(buf[9], 0x11);
}

TEST(X64AssemblerTest, EmitsPushAndPopOpcodes) {
    CASMWriterx64 writer;
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
    CASMWriterx64 writer;
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
