# Core Utility Plugins (Bitmap, File, FTP, Matrix, Setup, Memblocks) CMake Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Integrate 6 additional core engine SDK plugins (`Bitmap`, `File`, `FTP`, `Matrix`, `Setup`, `Memblocks`) into CMake as native 64-bit/32-bit plugins (`dbp_plugin_*`) and add unit tests in `test_plugin_build_matrix.cpp`.

**Architecture:** Create CMakeLists.txt for each plugin directory, add them to `Shared/CMakeLists.txt`, update `test_plugin_build_matrix.cpp`.

**Tech Stack:** C++17/C++20, CMake, DirectX SDK headers, GoogleTest.

---

### Task 1: Create CMakeLists.txt for 6 Plugins and Update Shared/CMakeLists.txt and Tests

**Files:**
- Create: `Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Bitmap/CMakeLists.txt`
- Create: `Dark Basic Public Shared/Dark Basic Pro SDK/Shared/File/CMakeLists.txt`
- Create: `Dark Basic Public Shared/Dark Basic Pro SDK/Shared/FTP/CMakeLists.txt`
- Create: `Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Matrix/CMakeLists.txt`
- Create: `Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Setup/CMakeLists.txt`
- Create: `Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Memblocks/CMakeLists.txt`
- Modify: `Dark Basic Public Shared/Dark Basic Pro SDK/Shared/CMakeLists.txt`
- Modify: `tests/test_plugin_build_matrix.cpp`

- [ ] **Step 1: Create `Bitmap/CMakeLists.txt`, `File/CMakeLists.txt`, `FTP/CMakeLists.txt`, `Matrix/CMakeLists.txt`, `Setup/CMakeLists.txt`, `Memblocks/CMakeLists.txt`**
- [ ] **Step 2: Update `Shared/CMakeLists.txt` to include subdirectories**
- [ ] **Step 3: Update `tests/test_plugin_build_matrix.cpp` with unit tests**
- [ ] **Step 4: Rebuild and verify tests under Win32 and x64 presets**
- [ ] **Step 5: Commit**

```bash
git add "Dark Basic Public Shared/Dark Basic Pro SDK/Shared/" tests/test_plugin_build_matrix.cpp
git commit -m "feat(plugins): integrate Bitmap, File, FTP, Matrix, Setup, Memblocks plugins into CMake build matrix"
```
