#include <gtest/gtest.h>
#include "ASMWriter.h"
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <vector>

TEST(X64IntensiveVerificationTest, EmitsAllGprMoveAndArithmeticCombinations) {
    const std::vector<X64Register> allGprs = {
        X64Register::RAX, X64Register::RCX, X64Register::RDX, X64Register::RBX,
        X64Register::RSP, X64Register::RBP, X64Register::RSI, X64Register::RDI,
        X64Register::R8,  X64Register::R9,  X64Register::R10, X64Register::R11,
        X64Register::R12, X64Register::R13, X64Register::R14, X64Register::R15
    };

    for (size_t i = 0; i < allGprs.size(); ++i) {
        CASMWriter writer;
        writer.EmitMovRegImm64(allGprs[i], 0x123456789ABCDEF0ULL + i);
        const auto& code = writer.GetCodeBuffer();
        ASSERT_EQ(code.size(), 10U) << "Failed for register index " << i;
        EXPECT_EQ(code[0] & 0xFE, 0x48) << "REX.W prefix missing for register " << i;
        EXPECT_EQ(code[1] & 0xF8, 0xB8) << "MOV opcode missing for register " << i;
    }
}

TEST(X64IntensiveVerificationTest, EmitsPushPopRoundtripForAllGprs) {
    const std::vector<X64Register> allGprs = {
        X64Register::RAX, X64Register::RCX, X64Register::RDX, X64Register::RBX,
        X64Register::RSP, X64Register::RBP, X64Register::RSI, X64Register::RDI,
        X64Register::R8,  X64Register::R9,  X64Register::R10, X64Register::R11,
        X64Register::R12, X64Register::R13, X64Register::R14, X64Register::R15
    };

    for (const auto& reg : allGprs) {
        CASMWriter writerPush;
        writerPush.EmitPushReg(reg);
        EXPECT_GT(writerPush.GetCodeSize(), 0U);

        CASMWriter writerPop;
        writerPop.EmitPopReg(reg);
        EXPECT_GT(writerPop.GetCodeSize(), 0U);
    }
}

TEST(X64IntensiveVerificationTest, EmitsAllSseRegistersWithMovAddMul) {
    const std::vector<XMMRegister> allXmms = {
        XMMRegister::XMM0, XMMRegister::XMM1, XMMRegister::XMM2, XMMRegister::XMM3,
        XMMRegister::XMM4, XMMRegister::XMM5, XMMRegister::XMM6, XMMRegister::XMM7,
        XMMRegister::XMM8, XMMRegister::XMM9, XMMRegister::XMM10, XMMRegister::XMM11,
        XMMRegister::XMM12, XMMRegister::XMM13, XMMRegister::XMM14, XMMRegister::XMM15
    };

    for (size_t i = 0; i < allXmms.size(); ++i) {
        const auto src = allXmms[i];
        const auto dst = allXmms[(i + 1) % allXmms.size()];

        CASMWriter writer;
        writer.EmitMovss(dst, src);
        writer.EmitAddss(dst, src);
        writer.EmitMulss(dst, src);

        EXPECT_GT(writer.GetCodeSize(), 0U);
    }
}

TEST(X64IntensiveVerificationTest, ValidatesWindowsX64CallingConventionContract) {
    EXPECT_EQ(CASMWriter::GetArgumentRegister(0), X64Register::RCX);
    EXPECT_EQ(CASMWriter::GetArgumentRegister(1), X64Register::RDX);
    EXPECT_EQ(CASMWriter::GetArgumentRegister(2), X64Register::R8);
    EXPECT_EQ(CASMWriter::GetArgumentRegister(3), X64Register::R9);
    EXPECT_EQ(CASMWriter::GetArgumentRegister(4), X64Register::None);

    EXPECT_EQ(CASMWriter::GetShadowSpaceSize(), 32U);

    EXPECT_EQ(CASMWriter::AlignStackFrame(0U), 32U);
    EXPECT_EQ(CASMWriter::AlignStackFrame(8U), 32U);
    EXPECT_EQ(CASMWriter::AlignStackFrame(16U), 32U);
    EXPECT_EQ(CASMWriter::AlignStackFrame(24U), 32U);
    EXPECT_EQ(CASMWriter::AlignStackFrame(32U), 32U);
    EXPECT_EQ(CASMWriter::AlignStackFrame(36U), 48U);
    EXPECT_EQ(CASMWriter::AlignStackFrame(48U), 48U);
    EXPECT_EQ(CASMWriter::AlignStackFrame(60U), 64U);
}

TEST(X64IntensiveVerificationTest, EmitsAndExecutesFunctionCallingConventionJit) {
    CASMWriter writer;
    
    // Windows x64 function that computes: (RCX + RDX) * 2
    // LEA RAX, [RCX + RDX] (48 8D 04 11)
    // ADD RAX, RAX         (48 01 C0)
    // RET                  (C3)
    writer.EmitByte(0x48);
    writer.EmitByte(0x8D);
    writer.EmitByte(0x04);
    writer.EmitByte(0x11);
    
    writer.EmitByte(0x48);
    writer.EmitByte(0x01);
    writer.EmitByte(0xC0);

    writer.EmitRet();

    const auto& code = writer.GetCodeBuffer();
    ASSERT_EQ(code.size(), 8U);

#if defined(_WIN64) || defined(__x86_64__)
    void* pExecMem = VirtualAlloc(nullptr, code.size(), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    ASSERT_NE(pExecMem, nullptr);

    std::memcpy(pExecMem, code.data(), code.size());

    using X64TwoArgFunc = int64_t (*)(int64_t, int64_t);
    auto fn = reinterpret_cast<X64TwoArgFunc>(pExecMem);

    int64_t res = fn(15, 25);
    EXPECT_EQ(res, 80); // (15 + 25) * 2 = 80

    VirtualFree(pExecMem, 0, MEM_RELEASE);
#else
    GTEST_SKIP() << "Direct x64 machine code execution requires a native 64-bit process environment.";
#endif
}
