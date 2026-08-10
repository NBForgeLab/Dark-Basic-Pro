# x64 Backend CASMWriterx64 Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement `CASMWriterx64` backend generator foundation complying with Microsoft x64 Calling Convention and 64-bit ABI.

**Architecture:** Implement `ICodeGenerator` interface in `CASMWriterx64` with register allocation helper (`RCX`, `RDX`, `R8`, `R9`), 32-byte shadow space, and 16-byte stack alignment rules.

**Tech Stack:** C++17, GoogleTest, MSVC v145.

---

### Task 1: Create `CASMWriterx64` Interface and Microsoft x64 ABI Helper

**Files:**
- Create: `DBProCompiler/DBPCompiler/ASMWriterx64.h`
- Create: `DBProCompiler/DBPCompiler/ASMWriterx64.cpp`
- Modify: `DBProCompiler/DBPCompiler/CMakeLists.txt`
- Test: `tests/test_x64_assembler.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing unit tests for x64 ABI register assignment and shadow space calculation**

```cpp
#include <gtest/gtest.h>
#include "ASMWriterx64.h"

TEST(X64AssemblerTest, AssignsRegistersForFirstFourArguments) {
    EXPECT_EQ(CASMWriterx64::GetArgumentRegister(0), X64Register::RCX);
    EXPECT_EQ(CASMWriterx64::GetArgumentRegister(1), X64Register::RDX);
    EXPECT_EQ(CASMWriterx64::GetArgumentRegister(2), X64Register::R8);
    EXPECT_EQ(CASMWriterx64::GetArgumentRegister(3), X64Register::R9);
}

TEST(X64AssemblerTest, CalculatesShadowSpaceAndStackAlignment) {
    EXPECT_EQ(CASMWriterx64::GetShadowSpaceSize(), 32U);
    EXPECT_EQ(CASMWriterx64::AlignStackFrame(24U), 32U);
    EXPECT_EQ(CASMWriterx64::AlignStackFrame(32U), 32U);
    EXPECT_EQ(CASMWriterx64::AlignStackFrame(36U), 48U);
}
```

- [ ] **Step 2: Add test to `tests/CMakeLists.txt` and run to verify build failure**

- [ ] **Step 3: Implement `CASMWriterx64.h` and `ASMWriterx64.cpp`**

- [ ] **Step 4: Build and run test to verify PASS**

Run: `cmake --build --preset windows-x86-debug --target dbp_tests`
Run: `.\out\build\windows-x86-debug\bin\Debug\dbp_tests.exe --gtest_filter=X64AssemblerTest.*`

- [ ] **Step 5: Commit**

```bash
git add DBProCompiler/DBPCompiler/ASMWriterx64.h DBProCompiler/DBPCompiler/ASMWriterx64.cpp DBProCompiler/DBPCompiler/CMakeLists.txt tests/test_x64_assembler.cpp tests/CMakeLists.txt
git commit -m "feat(x64): add CASMWriterx64 foundation and Microsoft x64 ABI helper"
```
