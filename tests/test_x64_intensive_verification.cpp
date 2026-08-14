#include <gtest/gtest.h>
#include "ASMWriter.h"
#include "PEBuilder.h"
#include "CompilerContext.h"
#include "globstruct.h"

#include <windows.h>
#include <cstdint>
#include <cstring>
#include <vector>

TEST(X64IntensiveVerificationTest, VerifiesAllRegisterMOVImm64EncodingBitPatterns) {
    const std::vector<X64Register> allRegs = {
        X64Register::RAX, X64Register::RCX, X64Register::RDX, X64Register::RBX,
        X64Register::RSP, X64Register::RBP, X64Register::RSI, X64Register::RDI,
        X64Register::R8,  X64Register::R9,  X64Register::R10, X64Register::R11,
        X64Register::R12, X64Register::R13, X64Register::R14, X64Register::R15
    };

    uint64_t testPattern = 0xA1B2C3D4E5F67890ULL;

    for (size_t i = 0; i < allRegs.size(); ++i) {
        CASMWriter writer;
        writer.EmitMovRegImm64(allRegs[i], testPattern);
        const auto& code = writer.GetCodeBuffer();

        ASSERT_EQ(code.size(), 10U) << "Failed MOV imm64 size for register index " << i;

        // Check REX prefix byte
        uint8_t expectedRex = (i >= 8) ? 0x49 : 0x48;
        EXPECT_EQ(code[0], expectedRex) << "Failed REX prefix for register index " << i;

        // Check Opcode byte (0xB8 + (idx & 7))
        uint8_t expectedOpcode = static_cast<uint8_t>(0xB8 + (i & 7));
        EXPECT_EQ(code[1], expectedOpcode) << "Failed opcode for register index " << i;

        // Check 64-bit little-endian immediate value
        uint64_t decodedImm = 0;
        std::memcpy(&decodedImm, code.data() + 2, sizeof(uint64_t));
        EXPECT_EQ(decodedImm, testPattern) << "Failed immediate payload for register index " << i;
    }
}

TEST(X64IntensiveVerificationTest, VerifiesAllRegisterPushPopEncoding) {
    const std::vector<X64Register> allRegs = {
        X64Register::RAX, X64Register::RCX, X64Register::RDX, X64Register::RBX,
        X64Register::RSP, X64Register::RBP, X64Register::RSI, X64Register::RDI,
        X64Register::R8,  X64Register::R9,  X64Register::R10, X64Register::R11,
        X64Register::R12, X64Register::R13, X64Register::R14, X64Register::R15
    };

    for (size_t i = 0; i < allRegs.size(); ++i) {
        CASMWriter writerPush;
        writerPush.EmitPushReg(allRegs[i]);
        const auto& pushCode = writerPush.GetCodeBuffer();

        CASMWriter writerPop;
        writerPop.EmitPopReg(allRegs[i]);
        const auto& popCode = writerPop.GetCodeBuffer();

        if (i < 8) {
            ASSERT_EQ(pushCode.size(), 1U);
            EXPECT_EQ(pushCode[0], static_cast<uint8_t>(0x50 + i));

            ASSERT_EQ(popCode.size(), 1U);
            EXPECT_EQ(popCode[0], static_cast<uint8_t>(0x58 + i));
        } else {
            ASSERT_EQ(pushCode.size(), 2U);
            EXPECT_EQ(pushCode[0], 0x41); // REX.B
            EXPECT_EQ(pushCode[1], static_cast<uint8_t>(0x50 + (i & 7)));

            ASSERT_EQ(popCode.size(), 2U);
            EXPECT_EQ(popCode[0], 0x41); // REX.B
            EXPECT_EQ(popCode[1], static_cast<uint8_t>(0x58 + (i & 7)));
        }
    }
}

TEST(X64IntensiveVerificationTest, VerifiesStackSubAddImmediateBoundaries) {
    const std::vector<uint32_t> testOffsets = { 32U, 64U, 256U, 4096U };

    for (uint32_t offset : testOffsets) {
        CASMWriter writer;
        writer.EmitSubRegImm32(X64Register::RSP, offset);
        writer.EmitAddRegImm32(X64Register::RSP, offset);
        const auto& code = writer.GetCodeBuffer();

        ASSERT_EQ(code.size(), 14U);

        // SUB RSP, imm32 (48 81 EC <imm32>)
        EXPECT_EQ(code[0], 0x48);
        EXPECT_EQ(code[1], 0x81);
        EXPECT_EQ(code[2], 0xEC);
        uint32_t subImm = 0;
        std::memcpy(&subImm, code.data() + 3, sizeof(uint32_t));
        EXPECT_EQ(subImm, offset);

        // ADD RSP, imm32 (48 81 C4 <imm32>)
        EXPECT_EQ(code[7], 0x48);
        EXPECT_EQ(code[8], 0x81);
        EXPECT_EQ(code[9], 0xC4);
        uint32_t addImm = 0;
        std::memcpy(&addImm, code.data() + 10, sizeof(uint32_t));
        EXPECT_EQ(addImm, offset);
    }
}

TEST(X64IntensiveVerificationTest, VerifiesMSx64AbiCallingConventionRules) {
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

TEST(X64IntensiveVerificationTest, VerifiesPE32Plus64BitHeaderIntegrityAndAlignments) {
    CPEBuilder builder;
    
    EXPECT_EQ(CPEBuilder::GetPeMagic(true), 0x020BU);
    EXPECT_EQ(CPEBuilder::GetPeMagic(false), 0x010BU);

    // Valid PE32+ header alignments
    EXPECT_TRUE(builder.ValidatePE64HeaderRequirements(0x0000000140000000ULL, 4096U, 512U));
    EXPECT_TRUE(builder.ValidatePE64HeaderRequirements(0x0000000140000000ULL, 4096U, 4096U));

    // Invalid PE32+ header alignments
    EXPECT_FALSE(builder.ValidatePE64HeaderRequirements(0x0000000140000000ULL, 512U, 4096U));
    EXPECT_FALSE(builder.ValidatePE64HeaderRequirements(0x0000000140000000ULL, 0U, 512U));
    EXPECT_FALSE(builder.ValidatePE64HeaderRequirements(0x0000000140000000ULL, 4096U, 0U));
}

TEST(X64IntensiveVerificationTest, VerifiesDirectJITExecutionWithStackFrameAndRegisters) {
    CASMWriter writer;
    
    // Construct a compound x64 JIT routine:
    // SUB RSP, 32
    // PUSH R8
    // MOV RAX, 100
    // MOV R8, 200
    // POP R8
    // ADD RSP, 32
    // RET
    writer.EmitSubRegImm32(X64Register::RSP, 32U);
    writer.EmitPushReg(X64Register::R8);
    writer.EmitMovRegImm64(X64Register::RAX, 100ULL);
    writer.EmitMovRegImm64(X64Register::R8, 200ULL);
    writer.EmitPopReg(X64Register::R8);
    writer.EmitAddRegImm32(X64Register::RSP, 32U);
    writer.EmitRet();

#if defined(_WIN64) || defined(__x86_64__)
    const auto& code = writer.GetCodeBuffer();
    void* pExecMem = VirtualAlloc(nullptr, code.size(), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    ASSERT_NE(pExecMem, nullptr);

    std::memcpy(pExecMem, code.data(), code.size());

    using X64JitFunc = int64_t (*)();
    auto jitFunction = reinterpret_cast<X64JitFunc>(pExecMem);

    int64_t result = jitFunction();
    EXPECT_EQ(result, 100);

    VirtualFree(pExecMem, 0, MEM_RELEASE);
#else
    GTEST_SKIP() << "Direct x64 machine code execution requires a native 64-bit process environment.";
#endif
}

TEST(X64IntensiveVerificationTest, VerifiesHighMemoryPointerSafetyAndGlobStructAlignment) {
#if defined(_WIN64) || defined(__x86_64__)
    const uint64_t highMemAddress = 0x0000000280000000ULL;
    void* pHighMemPtr = reinterpret_cast<void*>(static_cast<uintptr_t>(highMemAddress));

    uintptr_t convertedAddress = reinterpret_cast<uintptr_t>(pHighMemPtr);
    EXPECT_EQ(convertedAddress, highMemAddress);
#endif

    EXPECT_GE(alignof(GlobStruct), alignof(uintptr_t));
    EXPECT_EQ(alignof(GlobStruct) % alignof(uintptr_t), 0U);
}
