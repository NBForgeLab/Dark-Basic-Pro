#include <gtest/gtest.h>
#include "ASMWriterx64.h"
#include "PEBuilder.h"
#include "CompilerContext.h"

#include <windows.h>
#include <cstdint>
#include <cstring>

TEST(X64E2ECompilationTest, EmitsAndExecutesX64MachineCodeStreamDirectly) {
    CASMWriterx64 writer;
    
    // Emit a 64-bit function returning 42:
    // MOV RAX, 42 (10 bytes: REX.W 0x48 0xB8 + 64-bit imm)
    // RET        (1 byte: 0xC3)
    writer.EmitMovRegImm64(X64Register::RAX, 42ULL);
    writer.EmitRet();

    const auto& code = writer.GetCodeBuffer();
    ASSERT_EQ(code.size(), 11U);
    EXPECT_EQ(code[0], 0x48);
    EXPECT_EQ(code[1], 0xB8);
    EXPECT_EQ(code[10], 0xC3);

    // Allocate executable memory in the current process space
#if defined(_WIN64) || defined(__x86_64__)
    void* pExecMem = VirtualAlloc(nullptr, code.size(), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    ASSERT_NE(pExecMem, nullptr);

    std::memcpy(pExecMem, code.data(), code.size());

    // Cast executable buffer pointer to a C function pointer and execute it
    using X64JitFunc = int64_t (*)();
    auto jitFunction = reinterpret_cast<X64JitFunc>(pExecMem);

    int64_t result = jitFunction();
    EXPECT_EQ(result, 42);

    VirtualFree(pExecMem, 0, MEM_RELEASE);
#else
    GTEST_SKIP() << "Direct x64 machine code execution requires a native 64-bit process environment.";
#endif
}

TEST(X64E2ECompilationTest, ValidatesPE32PlusHeader64BitIntegrity) {
    CPEBuilder builder;
    
    // Magic 0x020B is PE32+ (64-bit executable header format)
    EXPECT_EQ(CPEBuilder::GetPeMagic(true), 0x020BU);
    EXPECT_EQ(CPEBuilder::GetPeMagic(false), 0x010BU);

    // Verify 64-bit PE header requirements (ImageBase, SectionAlignment, FileAlignment)
    EXPECT_TRUE(builder.ValidatePE64HeaderRequirements(0x0000000140000000ULL, 4096U, 512U));
    
    // Reject invalid 64-bit header alignments
    EXPECT_FALSE(builder.ValidatePE64HeaderRequirements(0x0000000140000000ULL, 512U, 4096U));
    EXPECT_FALSE(builder.ValidatePE64HeaderRequirements(0x0000000140000000ULL, 0U, 512U));
}

TEST(X64E2ECompilationTest, VerifiesX64CompilerHostBootstrapping) {
    CompilerContext context;
    context.Initialize();

    CASMWriterx64 x64Writer;
    x64Writer.EmitPushReg(X64Register::RBP);
    x64Writer.EmitPopReg(X64Register::RBP);
    x64Writer.EmitRet();

    EXPECT_EQ(x64Writer.GetCodeSize(), 3U);
}
