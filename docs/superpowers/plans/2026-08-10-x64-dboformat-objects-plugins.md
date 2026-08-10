# DBOFormat and Objects 3D Plugins CMake Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Integrate `DBOFormat` and `Objects` plugins into CMake build matrix (`dbp_plugin_dboformat`, `dbp_plugin_objects`) and verify native 64-bit/32-bit compilation and ABI alignment.

**Architecture:** Create CMakeLists.txt for `DBOFormat` and `Objects`, update `cmake/DBPPlugins.cmake`, and verify with `test_plugin_build_matrix.cpp`.

**Tech Stack:** C++17/C++20, CMake, DirectX SDK headers (D3D9, D3DX9), GoogleTest.

---

### Task 1: Create CMakeLists.txt for DBOFormat and Objects Plugins and Update CMake/Tests

**Files:**
- Create: `Dark Basic Public Shared/Dark Basic Pro SDK/Shared/DBOFormat/CMakeLists.txt`
- Create: `Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Objects/CMakeLists.txt`
- Modify: `cmake/DBPPlugins.cmake`
- Modify: `tests/test_plugin_build_matrix.cpp`

- [ ] **Step 1: Create `DBOFormat/CMakeLists.txt` and `Objects/CMakeLists.txt`**
- [ ] **Step 2: Register plugin subdirectories in `cmake/DBPPlugins.cmake`**
- [ ] **Step 3: Update `tests/test_plugin_build_matrix.cpp` with unit tests**
- [ ] **Step 4: Rebuild and verify tests under Win32 and x64 presets**
- [ ] **Step 5: Commit**

```bash
git add "Dark Basic Public Shared/Dark Basic Pro SDK/Shared/" cmake/DBPPlugins.cmake tests/test_plugin_build_matrix.cpp
git commit -m "feat(plugins): integrate DBOFormat and Objects 3D plugins into CMake build matrix"
```
