# x64 Prerequisites & Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Establish pointer safety, clean CMake target platform options, and TargetABI expansion for full x64 transition.

**Architecture:** Refactor hardcoded 32-bit pointer casts to `uintptr_t`/`size_t`, remove `FATAL_ERROR` on 64-bit CMake configuration, expand `TargetABI` to model both x86 and x64 targets cleanly.

**Tech Stack:** C++17, CMake 4.2+, GoogleTest, MSVC v145.

---

### Task 1: Refactor Pointer Storage and Casts in `EXEBlock` & `MemoryPE`

**Files:**
- Modify: `DBProCompiler/DBPCompiler/EXEBlock.cpp`
- Modify: `DBProCompiler/DBPCompiler/MemoryPE.cpp`
- Test: `tests/test_x64_pointer_safety.cpp`

- [ ] **Step 1: Write failing/extended pointer safety tests for 64-bit simulated memory**

```cpp
TEST(PointerSafetyTest, VerifyPointerToUintptrConversions) {
    uint64_t highAddr = 0x7FFF000012345678ULL;
    void* ptr = reinterpret_cast<void*>(static_cast<uintptr_t>(highAddr));
    EXPECT_NE(ptr, nullptr);
}
```

- [ ] **Step 2: Run test to verify passes/fails**

Run: `ctest --test-dir out/build/windows-x86-debug -R PointerSafetyTest --output-on-failure`

- [ ] **Step 3: Audit and replace any 32-bit DWORD casts of pointers in EXEBlock.cpp and MemoryPE.cpp**

- [ ] **Step 4: Run test suite to verify no regressions**

Run: `ctest --test-dir out/build/windows-x86-debug --output-on-failure`

- [ ] **Step 5: Commit**

```bash
git add DBProCompiler/DBPCompiler/EXEBlock.cpp DBProCompiler/DBPCompiler/MemoryPE.cpp tests/test_x64_pointer_safety.cpp
git commit -m "refactor(x64): audit and convert pointer casts to uintptr_t for x64 readiness"
```

---

### Task 2: Update `CMakeLists.txt` for Dual-Target Architecture Support

**Files:**
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Replace hardcoded FORCE Win32 platform assignment with configurable default**

```cmake
# Allow Win32 by default, but permit x64 build configurations
if(NOT CMAKE_GENERATOR_PLATFORM)
    set(CMAKE_GENERATOR_PLATFORM Win32 CACHE STRING "Target Platform" FORCE)
endif()
```

- [ ] **Step 2: Remove CMAKE_SIZEOF_VOID_P FATAL_ERROR block and allow x64 builds**

- [ ] **Step 3: Reconfigure and run CMake test build on Win32**

Run: `cmake -B out/build/windows-x86-debug -S .`

- [ ] **Step 4: Run CTest test suite**

Run: `ctest --test-dir out/build/windows-x86-debug --output-on-failure`

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt
git commit -m "build(cmake): allow configurable x64 target platform builds"
```
