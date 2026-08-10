# x64 dbp_plugin_core & GlobStruct Alignment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Integrate `Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core` into CMake as `dbp_plugin_core` and audit `GlobStruct` 64-bit alignment and pointer traits.

**Architecture:** Create `Core/CMakeLists.txt`, update `cmake/DBPPlugins.cmake`, and write alignment traits unit tests in `test_plugin_build_matrix.cpp`.

**Tech Stack:** C++17, CMake 4.2+, GoogleTest, MSVC v145.

---

### Task 1: Integrate `dbp_plugin_core` into CMake and Add GlobStruct Alignment Tests

**Files:**
- Create: `Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core/CMakeLists.txt`
- Modify: `cmake/DBPPlugins.cmake`
- Modify: `tests/test_plugin_build_matrix.cpp`

- [ ] **Step 1: Write failing unit tests for GlobStruct pointer alignment and traits**

```cpp
TEST(PluginBuildMatrixTest, ValidatesGlobStructAlignmentAndPointerTraits) {
    EXPECT_EQ(alignof(GlobStruct), alignof(uintptr_t));
    EXPECT_EQ(sizeof(GlobStruct::CreateDeleteString), sizeof(uintptr_t));
    EXPECT_EQ(sizeof(GlobStruct::g_pVariableSpace), sizeof(uintptr_t));
    EXPECT_EQ(sizeof(GlobStruct::g_GFX), sizeof(uintptr_t));
}

TEST(PluginBuildMatrixTest, ValidatesGlobChecklistStructPointerSafety) {
    EXPECT_EQ(alignof(GlobChecklistStruct), alignof(uintptr_t));
    EXPECT_EQ(sizeof(GlobChecklistStruct::string), sizeof(uintptr_t));
}
```

- [ ] **Step 2: Run test to verify build/pass failure**

- [ ] **Step 3: Create `Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core/CMakeLists.txt` and update `cmake/DBPPlugins.cmake`**

- [ ] **Step 4: Build and run test suite to verify PASS**

Run: `cmake --build --preset windows-x86-debug --target dbp_plugin_core dbp_tests`
Run: `.\out\build\windows-x86-debug\bin\Debug\dbp_tests.exe --gtest_filter=PluginBuildMatrixTest.*`

- [ ] **Step 5: Commit**

```bash
git add cmake/DBPPlugins.cmake "Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core/CMakeLists.txt" tests/test_plugin_build_matrix.cpp
git commit -m "feat(x64): integrate SDK engine Core plugin into CMake and verify GlobStruct 64-bit alignment traits"
```
