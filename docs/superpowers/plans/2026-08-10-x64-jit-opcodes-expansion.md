# CASMWriterx64 JIT Assembler Opcode Expansions Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expand `CASMWriterx64` JIT assembler opcodes to support control flow, comparison (`CMP`, `TEST`, `JMP`, `JNE`, `JE`, `CALL`, `NOP`), and SSE2 floating-point instructions (`MOVSS`, `ADDSS`, `MULSS`) for `XMM0`-`XMM15`.

**Architecture:** Modify `ASMWriterx64.h` and `ASMWriterx64.cpp`, add unit tests in `tests/test_x64_assembler.cpp`.

**Tech Stack:** C++17/C++20, x64 Instruction Encoding, SSE2 ISA, GoogleTest.

---

### Task 1: Expand `CASMWriterx64` Opcodes and Add Tests

**Files:**
- Modify: `DBProCompiler/DBPCompiler/ASMWriterx64.h`
- Modify: `DBProCompiler/DBPCompiler/ASMWriterx64.cpp`
- Modify: `tests/test_x64_assembler.cpp`

- [ ] **Step 1: Declare XMMRegister enum and new emission methods in `ASMWriterx64.h`**
- [ ] **Step 2: Implement opcode byte encoding methods in `ASMWriterx64.cpp`**
- [ ] **Step 3: Add unit tests in `tests/test_x64_assembler.cpp`**
- [ ] **Step 4: Rebuild `dbp_tests` and run tests under both Win32 and x64 presets**
- [ ] **Step 5: Commit**

```bash
git add DBProCompiler/DBPCompiler/ASMWriterx64.h DBProCompiler/DBPCompiler/ASMWriterx64.cpp tests/test_x64_assembler.cpp
git commit -m "feat(x64): expand CASMWriterx64 opcodes with control flow and SSE2 instructions"
```
