# Advanced 3D Engine Plugins (Objects, Light, SpecialEffects, Vectors, Transforms) CMake Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Integrate 5 advanced 3D engine SDK plugins (`Objects`, `Light`, `SpecialEffects`, `Vectors`, `Transforms`) into CMake as native 64-bit/32-bit plugins (`dbp_plugin_*`) and add unit tests in `test_plugin_build_matrix.cpp`.

**Architecture:** Create CMakeLists.txt for each plugin directory, register them in `cmake/DBPPlugins.cmake`, update `test_plugin_build_matrix.cpp`.

**Tech Stack:** C++17/C++20, CMake, DirectX SDK headers (D3D9, D3DX9), GoogleTest.

---

### Task 1: Create CMakeLists.txt for 5 Advanced 3D Plugins and Update CMake/Tests

**Files:**
- Create: `Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Objects/CMakeLists.txt`
- Create: `Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Light/CMakeLists.txt`
- Create: `Dark Basic Public Shared/Dark Basic Pro SDK/Shared/SpecialEffects/CMakeLists.txt`
- Create: `Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Vectors/CMakeLists.txt`
- Create: `Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Transforms/CMakeLists.txt`
- Modify: `cmake/DBPPlugins.cmake`
- Modify: `tests/test_plugin_build_matrix.cpp`

- [ ] **Step 1: Create `Objects/CMakeLists.txt`, `Light/CMakeLists.txt`, `SpecialEffects/CMakeLists.txt`, `Vectors/CMakeLists.txt`, `Transforms/CMakeLists.txt`**
- [ ] **Step 2: Register 5 plugin subdirectories in `cmake/DBPPlugins.cmake`**
- [ ] **Step 3: Update `tests/test_plugin_build_matrix.cpp` with unit tests**
- [ ] **Step 4: Rebuild and verify tests under Win32 and x64 presets**
- [ ] **Step 5: Commit**

```bash
git add "Dark Basic Public Shared/Dark Basic Pro SDK/Shared/" cmake/DBPPlugins.cmake tests/test_plugin_build_matrix.cpp
git commit -m "feat(plugins): integrate Objects, Light, SpecialEffects, Vectors, Transforms 3D plugins into CMake build matrix"
```
