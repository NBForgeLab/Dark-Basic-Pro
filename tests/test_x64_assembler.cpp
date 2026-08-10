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
