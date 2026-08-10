# x64 CASMWriterx64 Opcode Expansion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement x64 binary opcode emission helpers in `CASMWriterx64` (REX prefixes, MOV r64 imm64, PUSH/POP r64, ADD/SUB RSP, RET) with unit tests.

**Architecture:** Add machine code buffer `m_codeBuffer` and emission helper functions in `CASMWriterx64`, verified via `test_x64_assembler.cpp`.

**Tech Stack:** C++17, GoogleTest, MSVC v145.

---

### Task 1: Implement x64 Opcode Emission Helpers and Unit Tests

**Files:**
- Modify: `DBProCompiler/DBPCompiler/ASMWriterx64.h`
- Modify: `DBProCompiler/DBPCompiler/ASMWriterx64.cpp`
- Modify: `tests/test_x64_assembler.cpp`

- [ ] **Step 1: Write failing unit tests for opcode byte generation**

```cpp
TEST(X64AssemblerTest, EmitsMovRegImm64Opcode) {
    CASMWriterx64 writer;
    writer.EmitMovRegImm64(X64Register::RAX, 0x1122334455667788ULL);
    const auto& buf = writer.GetCodeBuffer();
    ASSERT_EQ(buf.size(), 10U);
    EXPECT_EQ(buf[0], 0x48); // REX.W
    EXPECT_EQ(buf[1], 0xB8); // MOV RAX, imm64
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
    EXPECT_EQ(buf[1], 0x41);       // REX.B
    EXPECT_EQ(buf[2], 0x50);       // PUSH R8
    EXPECT_EQ(buf[3], 0x58);       // POP RAX
    EXPECT_EQ(buf[4], 0x41);       // REX.B
    EXPECT_EQ(buf[5], 0x58);       // POP R8
}
```

- [ ] **Step 2: Run test to verify build/pass failure**

- [ ] **Step 3: Implement opcode emission functions in `ASMWriterx64.h` and `ASMWriterx64.cpp`**

- [ ] **Step 4: Build and run test suite to verify PASS**

Run: `cmake --build --preset windows-x86-debug --target dbp_tests`
Run: `.\out\build\windows-x86-debug\bin\Debug\dbp_tests.exe --gtest_filter=X64AssemblerTest.*`

- [ ] **Step 5: Commit**

```bash
git add DBProCompiler/DBPCompiler/ASMWriterx64.h DBProCompiler/DBPCompiler/ASMWriterx64.cpp tests/test_x64_assembler.cpp
git commit -m "feat(x64): add opcode emission helpers for MOV, PUSH, POP, ADD, SUB, and RET instructions"
```
