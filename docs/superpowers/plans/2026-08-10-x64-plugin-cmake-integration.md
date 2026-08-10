# x64 Engine Plugin CMake Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Integrate SDK engine plugins into the CMake build system for dual-target x86/x64 compilation and verification.

**Architecture:** Create `cmake/DBPPlugins.cmake` rules and `Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Input/CMakeLists.txt` defining `dbp_plugin_input`, verified via `test_plugin_build_matrix.cpp`.

**Tech Stack:** C++17, CMake 4.2+, GoogleTest, MSVC v145, DirectInput8.

---

### Task 1: Create CMake Plugin Framework and Integrate `dbp_plugin_input`

**Files:**
- Create: `cmake/DBPPlugins.cmake`
- Create: `Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Input/CMakeLists.txt`
- Modify: `CMakeLists.txt`
- Create: `tests/test_plugin_build_matrix.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing unit tests for plugin build matrix & ABI verification**

```cpp
#include <gtest/gtest.h>
#include "TargetABI.h"

TEST(PluginBuildMatrixTest, VerifiesInputPluginAbiTargetTraits) {
    EXPECT_GE(sizeof(dbp::abi::ActiveTargetAbi::address_size), 4U);
}
```

- [ ] **Step 2: Add test to `tests/CMakeLists.txt` and run to verify build**

- [ ] **Step 3: Create `cmake/DBPPlugins.cmake` and `Input/CMakeLists.txt`**

- [ ] **Step 4: Build target `dbp_plugin_input` and `dbp_tests` and run tests**

Run: `cmake --build --preset windows-x86-debug --target dbp_plugin_input dbp_tests`
Run: `.\out\build\windows-x86-debug\bin\Debug\dbp_tests.exe --gtest_filter=PluginBuildMatrixTest.*`

- [ ] **Step 5: Commit**

```bash
git add cmake/DBPPlugins.cmake "Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Input/CMakeLists.txt" CMakeLists.txt tests/test_plugin_build_matrix.cpp tests/CMakeLists.txt
git commit -m "feat(x64): integrate SDK input plugin into CMake build graph for dual x86/x64 targets"
```
