# x64 dbp_plugin_camera & dbp_plugin_text Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Integrate `Camera` and `Text` SDK plugins into CMake as `dbp_plugin_camera` and `dbp_plugin_text`.

**Architecture:** Create `Camera/CMakeLists.txt` and `Text/CMakeLists.txt`, update `cmake/DBPPlugins.cmake`, and write unit tests in `test_plugin_build_matrix.cpp`.

**Tech Stack:** C++17, CMake 4.2+, GoogleTest, MSVC v145.

---

### Task 1: Integrate `dbp_plugin_camera` and `dbp_plugin_text` into CMake and Add Unit Tests

**Files:**
- Create: `Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Camera/CMakeLists.txt`
- Create: `Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Text/CMakeLists.txt`
- Modify: `cmake/DBPPlugins.cmake`
- Modify: `tests/test_plugin_build_matrix.cpp`

- [ ] **Step 1: Write failing unit tests for Camera and Text plugin traits**

```cpp
TEST(PluginBuildMatrixTest, ValidatesCameraAndTextPluginTargetTraits) {
    EXPECT_GE(dbp::abi::ActiveTargetAbi::address_size, 4U);
    EXPECT_TRUE(sizeof(GlobStruct::g_Camera3D) == sizeof(HINSTANCE));
    EXPECT_TRUE(sizeof(GlobStruct::g_Text) == sizeof(HINSTANCE));
}
```

- [ ] **Step 2: Run test to verify build/pass failure**

- [ ] **Step 3: Create `Camera/CMakeLists.txt` and `Text/CMakeLists.txt` and update `cmake/DBPPlugins.cmake`**

- [ ] **Step 4: Build and run test suite to verify PASS**

Run: `cmake --build --preset windows-x86-debug --target dbp_plugin_camera dbp_plugin_text dbp_tests`
Run: `.\out\build\windows-x86-debug\bin\Debug\dbp_tests.exe --gtest_filter=PluginBuildMatrixTest.*`

- [ ] **Step 5: Commit**

```bash
git add cmake/DBPPlugins.cmake "Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Camera/CMakeLists.txt" "Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Text/CMakeLists.txt" tests/test_plugin_build_matrix.cpp
git commit -m "feat(x64): integrate SDK engine Camera and Text plugins into CMake"
```
