# Phase 9: Unicode Transition Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Transition the compiler to support Unicode (UTF-8 / UTF-16) by enabling target-specific `UNICODE` and `_UNICODE` preprocessor definitions and refactoring Win32 boundary API calls (`CreateFile`, `CreateProcess`, `MessageBox`) to use wide-character (`W`) API suffixes via the `TextConvert` utility.

**Architecture:** Enable the `UNICODE` and `_UNICODE` compile definitions in target build configurations. Convert ANSI string inputs (`char*` / `CStr`) to UTF-16 wide strings using `TextConvert::UTF8ToUTF16` when calling system APIs. Ensure legacy C files explicitly invoke ANSI functions (`CreateFileA`) to retain self-contained backward compatibility.

**Tech Stack:** C++17, Win32 API, CMake, GoogleTest.

---

### Task 1: Enable UNICODE compile definitions and refactor C files

**Files:**
- Modify: `DBProCompiler/DBPCompiler/CMakeLists.txt`
- Modify: `DBProCompiler/DBPCompilerEXE/CMakeLists.txt`
- Modify: `DBProCompiler/DBPCompiler/icons/DIB.C`
- Modify: `DBProCompiler/DBPCompiler/icons/ICONS.C`

- [ ] **Step 1: Update DBPCompiler and DarkEXE compile definitions in CMake**

  Modify [DBProCompiler/DBPCompiler/CMakeLists.txt](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/CMakeLists.txt):
  Add `UNICODE` and `_UNICODE` compile definitions:
  ```cmake
  # Preprocessor definitions
  add_compile_definitions(
      WIN32
      _WINDOWS
      NOGETDXVERSIONCODE
      ALWAYSCOMPILEMODE
      UNICODE
      _UNICODE
  )
  ```

  Modify [DBProCompiler/DBPCompilerEXE/CMakeLists.txt](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompilerEXE/CMakeLists.txt):
  Add `UNICODE` and `_UNICODE` compile definitions:
  ```cmake
  # Preprocessor definitions
  add_compile_definitions(
      WIN32
      _WINDOWS
      DARKEXE
      UNICODE
      _UNICODE
  )
  ```

- [ ] **Step 2: Explicitly target CreateFileA in legacy C files**

  Modify [DBProCompiler/DBPCompiler/icons/DIB.C](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/icons/DIB.C):
  Replace `CreateFile` on lines 437 and 551 with `CreateFileA`.
  
  Modify [DBProCompiler/DBPCompiler/icons/ICONS.C](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/icons/ICONS.C):
  Replace `CreateFile` on lines 157 and 848 with `CreateFileA`.

- [ ] **Step 3: Try to compile and verify compilation error reports**

  Run: `cmake --build build --config Debug`
  Expected: Compiler fails with errors indicating `char*` to `LPCWSTR` conversion mismatches on `CreateFile`, `CreateProcess`, and `MessageBox` calls in C++ files.

- [ ] **Step 4: Commit Task 1**

  Run:
  ```bash
  git add DBProCompiler/DBPCompiler/CMakeLists.txt DBProCompiler/DBPCompilerEXE/CMakeLists.txt DBProCompiler/DBPCompiler/icons/DIB.C DBProCompiler/DBPCompiler/icons/ICONS.C
  git commit -m "build: enable UNICODE definitions and fix legacy C API calls"
  ```

---

### Task 2: Refactor Main.cpp and DBPCompiler.cpp Win32 API calls

**Files:**
- Modify: `DBProCompiler/DBPCompiler/Main.cpp`
- Modify: `DBProCompiler/DBPCompiler/DBPCompiler.cpp`

- [ ] **Step 1: Refactor Main.cpp Win32 boundary API calls**

  Modify [Main.cpp](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/Main.cpp):
  1. Include `"TextConvert.h"` at the top.
  2. Refactor `CreateFile` at line 388:
     ```cpp
     HANDLE hFile = CreateFileW(L"DBProAssistNet.exe", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
     ```
  3. Refactor `CreateProcess` at line 452:
     ```cpp
     std::wstring wFullLine = TextConvert::UTF8ToUTF16(pFullLine);
     if(CreateProcessW(	NULL, &wFullLine[0],
     					NULL, NULL, false,
     					NORMAL_PRIORITY_CLASS,
     					NULL, NULL,	&si, &pi))
     ```
  4. Refactor `MessageBox` at line 564:
     Change `wsprintf` to `sprintf_s` and convert output message to UTF-16:
     ```cpp
     char line[512];
     #ifdef DEMOPROTECTEDMODE
      sprintf_s( line, "Demo installation folder and compiler versions are incompatible at '%s'", g_ActualCompilerFilename );
     #else
      sprintf_s( line, "Full installation folder and compiler versions are incompatible at '%s'", g_ActualCompilerFilename );
     #endif
     MessageBoxW(NULL, TextConvert::UTF8ToUTF16(line).c_str(), L"Compiler Error", MB_OK);
     ```
  5. Refactor `CreateProcess` at line 717:
     ```cpp
     wchar_t wFullLine[_MAX_PATH] = L"DarkEXE.exe";
     if(CreateProcessW(	NULL, wFullLine,
     					NULL, NULL, false,
     					NORMAL_PRIORITY_CLASS,
     					NULL, NULL,	&si, &pi))
     ```

- [ ] **Step 2: Refactor DBPCompiler.cpp Win32 boundary API calls**

  Modify [DBPCompiler.cpp](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/DBPCompiler.cpp):
  1. Include `"TextConvert.h"` at the top.
  2. Refactor `CreateFile` at line 315:
     ```cpp
     HANDLE hFile = CreateFileW(TextConvert::UTF8ToUTF16(pDBAFilename).c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
     ```
  3. Refactor `CreateFile` at line 512:
     ```cpp
     HANDLE hFile = CreateFileW(TextConvert::UTF8ToUTF16(pFullSourceDump).c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
     ```
  4. Refactor `CreateFile` at line 1835:
     ```cpp
     HANDLE hFile = CreateFileW(TextConvert::UTF8ToUTF16(pFilename).c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
     ```
  5. Refactor `MessageBoxA` at line 1982, 1987, 1994 to `MessageBoxW`:
     ```cpp
     MessageBoxW(0, TextConvert::UTF8ToUTF16(textfiles).c_str(), L"Thread Count", 64);
     ```
     ```cpp
     void(*func)(const char *) = [](const char *text)->void{MessageBoxW(0, TextConvert::UTF8ToUTF16(text).c_str(), L"Worker", 64);};
     ```
  6. Refactor `CreateFile` at line 2055:
     ```cpp
     HANDLE hFile = CreateFileW( L"_temp.temp", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
     ```
  7. Refactor `MessageBoxA` at line 2144:
     ```cpp
     MessageBoxW(NULL, TextConvert::UTF8ToUTF16(missing).c_str(), TextConvert::UTF8ToUTF16(g_pDBPCompiler->GetWordString(9)).c_str(), MB_OK|MB_ICONERROR);
     ```

- [ ] **Step 3: Commit Task 2**

  Run:
  ```bash
  git add DBProCompiler/DBPCompiler/Main.cpp DBProCompiler/DBPCompiler/DBPCompiler.cpp
  git commit -m "refactor: convert Main.cpp and DBPCompiler.cpp Win32 boundaries to Unicode"
  ```

---

### Task 3: Refactor remaining compiler files

**Files:**
- Modify: `DBProCompiler/DBPCompiler/EXEBlock.cpp`
- Modify: `DBProCompiler/DBPCompiler/Error.cpp`
- Modify: `DBProCompiler/DBPCompiler/DBMWriter.cpp`
- Modify: `DBProCompiler/DBPCompiler/InstructionTable.cpp`
- Modify: `DBProCompiler/DBPCompiler/FileBuilder.cpp`

- [ ] **Step 1: Refactor EXEBlock.cpp CreateFile calls**

  Modify [EXEBlock.cpp](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/EXEBlock.cpp):
  1. Include `"TextConvert.h"` at the top.
  2. Refactor `CreateFile` at line 349:
     ```cpp
     HANDLE hFile = CreateFileW(TextConvert::UTF8ToUTF16(lpFilename).c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
     ```
  3. Refactor `CreateFile` at line 522:
     ```cpp
     HANDLE hFile = CreateFileW(TextConvert::UTF8ToUTF16(lpFilename).c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
     ```

- [ ] **Step 2: Refactor Error.cpp CreateFile calls**

  Modify [Error.cpp](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/Error.cpp):
  1. Include `"TextConvert.h"` at the top.
  2. Refactor `CreateFile` at line 194:
     ```cpp
     HANDLE hFile = CreateFileW(TextConvert::UTF8ToUTF16(g_pDBPCompiler->GetInternalFile(PATH_TEMPERRORFILE)).c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
     ```

- [ ] **Step 3: Refactor DBMWriter.cpp CreateFile calls**

  Modify [DBMWriter.cpp](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/DBMWriter.cpp):
  1. Include `"TextConvert.h"` at the top.
  2. Refactor `CreateFile` at line 298:
     ```cpp
     HANDLE hFile = CreateFileW(TextConvert::UTF8ToUTF16(g_pDBPCompiler->GetInternalFile(PATH_TEMPDBMFILE)).c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
     ```

- [ ] **Step 4: Refactor InstructionTable.cpp CreateFile calls**

  Modify [InstructionTable.cpp](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/InstructionTable.cpp):
  1. Include `"TextConvert.h"` at the top.
  2. Refactor `CreateFile` at line 86:
     ```cpp
     HANDLE hReadFile = CreateFileW(TextConvert::UTF8ToUTF16(pFilename).c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
     ```

- [ ] **Step 5: Refactor FileBuilder.cpp CreateFile calls**

  Modify [FileBuilder.cpp](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/FileBuilder.cpp):
  1. Include `"TextConvert.h"` at the top.
  2. Refactor all 10 occurrences of `CreateFile` to `CreateFileW` wrapping the filename string argument in `TextConvert::UTF8ToUTF16(...).c_str()` (e.g. lines 239, 314, 374, 428, 458, 504, 1094, 1205, 1228, 1247, 1306, 1352).

- [ ] **Step 6: Run full test suite and E2E compiler verification**

  1. Rebuild tests: `cmake --build build --config Debug`
  2. Run unit tests: `build\bin\Debug\dbp_tests.exe`
  3. Run E2E script: `powershell -ExecutionPolicy Bypass -File "C:\Users\batah\.gemini\antigravity\brain\787b428b-629a-4104-b6c5-4399a2033171/scratch/verify_compiler.ps1"`
  Expected: ALL 20 tests pass and E2E compilation completes successfully outputting `demo_test.exe`.

- [ ] **Step 7: Commit Task 3**

  Run:
  ```bash
  git add DBProCompiler/DBPCompiler/EXEBlock.cpp DBProCompiler/DBPCompiler/Error.cpp DBProCompiler/DBPCompiler/DBMWriter.cpp DBProCompiler/DBPCompiler/InstructionTable.cpp DBProCompiler/DBPCompiler/FileBuilder.cpp
  git commit -m "refactor: complete Win32 boundary Unicode transition for remaining compiler files"
  ```
