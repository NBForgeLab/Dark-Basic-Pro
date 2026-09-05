# Elimination of Legacy Relics, Deprecated APIs & Obsolete Platform Code

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** Completely eliminate all remaining legacy relics, deprecated Win32 APIs, dead EAX code, 1980s NetBIOS hooks, broken Windows version discrimination, and fragile process-wide working directory mutations across DarkBasic Pro plugins (System, Enhancements, Core) using modern C++20 and native 64-bit standards with 100% test verification and zero log/crash debris.

**Architecture:** Replace legacy Win32 calls (GlobalMemoryStatus, GetVersionEx, NetBIOS Nb30, LoadLibrary shims for Windows XP) with modern C++20 / Windows SDK equivalents (GlobalMemoryStatusEx, VersionHelpers.h, IPHlpAPI / GetAdaptersAddresses, Direct3D9Ex / native User32 calls). Eliminate dead code (960 lines of commented EAX, unused string tables). Isolate process-wide SetCurrentDirectory mutations in FileBlocks using deterministic filesystem paths.

**Tech Stack:** C++20, MSVC v143/v144 (x64), CMake 4.2, GoogleTest (dbp_tests.exe, FPSTests.exe), Windows SDK 10/11 (iphlpapi.lib, versionhelpers.h).

---

### Task 1: Modernize Memory & OS Version APIs in System Plugin and Core Runtime

**Files:**
- Modify: Dark Basic Public Shared/Dark Basic Pro SDK/Shared/System/CSystemC.cpp
- Modify: Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core/DBDLLCore.cpp
- Test: tests/test_plugin_system_api.cpp

- [x] **Step 1: Write the failing test for System Memory and OS Detection**
- [x] **Step 2: Add test to tests/CMakeLists.txt and verify it compiles**
- [x] **Step 3: Refactor CSystemC.cpp and DBDLLCore.cpp**
- [x] **Step 4: Run test suite to verify success**

---

### Task 2: Eliminate Obsolete OS Probing, Leaks & Double-Free in Enhancements

**Files:**
- Modify: Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDKMore/Enhancements/OSCpu.cpp
- Modify: Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDKMore/Enhancements/CpuUsage.cpp
- Modify: Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDKMore/Enhancements/CpuUsage.h
- Test: tests/test_plugin_enhancements.cpp

- [x] **Step 1: Write test for OS Identification and CPU Info in test_plugin_enhancements.cpp**
- [x] **Step 2: Modernize OSCpu.cpp and CpuUsage.cpp**
- [x] **Step 3: Run test to verify passes**

---

### Task 3: Replace 1980s NetBIOS MAC Address Querying with IP Helper API

**Files:**
- Modify: Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDKMore/Enhancements/OSHardDrive.cpp
- Modify: Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDKMore/Enhancements/CMakeLists.txt
- Test: tests/test_plugin_enhancements.cpp

- [x] **Step 1: Write unit test for Hardware Serial and MAC Generation**
- [x] **Step 2: Replace NetBIOS in OSHardDrive.cpp with GetAdaptersAddresses**
- [x] **Step 3: Run test and verify pass**

---

### Task 4: Remove Dead EAX Code and Commented Ghost Code

**Files:**
- Delete or Clear: Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDKMore/Enhancements/EAX.cpp
- Modify: Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDKMore/Enhancements/CMakeLists.txt
- Modify: Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDKMore/Enhancements/commands.rc
- Modify: Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDKMore/Enhancements/OSDisplay.cpp

- [x] **Step 1: Clean CMakeLists.txt and commands.rc**
- [x] **Step 2: Build project and verify clean compilation**

---

### Task 5: Memory Safety and Working Directory Isolation in FileBlocks

**Files:**
- Modify: Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDKMore/Enhancements/FileBlocks.cpp
- Test: tests/test_plugin_enhancements.cpp

- [x] **Step 1: Write boundary safety test for FileBlocks**
- [x] **Step 2: Modernize FileBlocks.cpp**
- [x] **Step 3: Run test and verify pass**

---

### Task 6: Modernize Internet / Network APIs

**Files:**
- Modify: Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDKMore/Enhancements/OSInternet.cpp

- [x] **Step 1: Modernize OSInternet.cpp**

---

### Task 7: Full Regression Verification and Clean Debris Sweep

**Files:**
- Test: All suites (dbp_tests.exe, FPSTests.exe)

- [x] **Step 1: Run complete dbp_tests.exe suite**
- [x] **Step 2: Run complete FPSTests.exe suite**
- [x] **Step 3: Verify zero .log and zero .dmp files exist**
