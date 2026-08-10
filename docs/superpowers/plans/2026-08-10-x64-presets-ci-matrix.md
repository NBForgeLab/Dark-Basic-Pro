# x64 CMake Presets & CI Matrix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add official `windows-x64-debug` and `windows-x64-release` presets to `CMakePresets.json` and verify 100% test pass rate under native 64-bit builds.

**Architecture:** Update `CMakePresets.json` with x64 configure, build, and test presets, then configure, build, and run tests via CTest.

**Tech Stack:** CMake 4.2+, GoogleTest, MSVC v145 (Visual Studio 2026).

---

### Task 1: Update `CMakePresets.json` and Verify Native x64 Build Matrix

**Files:**
- Modify: `CMakePresets.json`

- [ ] **Step 1: Add `windows-x64-base`, `windows-x64-debug`, `windows-x64-release` presets to `CMakePresets.json`**

- [ ] **Step 2: Configure with preset `windows-x64-debug`**

Run: `cmake --preset windows-x64-debug`

- [ ] **Step 3: Build targets with preset `windows-x64-debug`**

Run: `cmake --build --preset windows-x64-debug`

- [ ] **Step 4: Execute tests with preset `windows-x64-debug`**

Run: `ctest --preset windows-x64-debug` or `.\out\build\windows-x64-debug\bin\Debug\dbp_tests.exe`

- [ ] **Step 5: Commit**

```bash
git add CMakePresets.json
git commit -m "feat(x64): add official windows-x64-debug and windows-x64-release CMake presets"
```
