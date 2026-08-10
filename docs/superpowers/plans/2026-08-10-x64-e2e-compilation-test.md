# x64 End-to-End JIT Compilation & Native Execution Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create an automated integration test suite `X64E2ECompilationTest` in `tests/test_e2e_x64_compilation.cpp` to verify `CASMWriterx64` JIT opcode emission, direct executable memory execution (`PAGE_EXECUTE_READWRITE`), and PE32+ (0x020B/AMD64) header integrity.

**Architecture:** Add `test_e2e_x64_compilation.cpp` to `tests/CMakeLists.txt`, test JIT code emission, JIT memory execution, and PE32+ 64-bit headers.

**Tech Stack:** C++17/C++20, GoogleTest, Windows VirtualProtect API, `CASMWriterx64`, `CPEBuilder`.

---

### Task 1: Create `X64E2ECompilationTest` Suite and Integrate into CMake

**Files:**
- Create: `tests/test_e2e_x64_compilation.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write `tests/test_e2e_x64_compilation.cpp`**
- [ ] **Step 2: Add `test_e2e_x64_compilation.cpp` to `tests/CMakeLists.txt`**
- [ ] **Step 3: Rebuild `dbp_tests`**
- [ ] **Step 4: Run `X64E2ECompilationTest` tests and full suite**
- [ ] **Step 5: Commit**

```bash
git add tests/test_e2e_x64_compilation.cpp tests/CMakeLists.txt
git commit -m "test(x64): add X64E2ECompilationTest suite for JIT memory execution and PE32+ validation"
```
