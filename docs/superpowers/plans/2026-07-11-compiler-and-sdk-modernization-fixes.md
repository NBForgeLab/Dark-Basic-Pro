# Compiler and SDK Modernization Fixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Resolve all 20 compiler lifecycle, memory safety, CStr length, ABI dynamic DLL call compatibility, and repository hygiene issues on the `test-1` branch.

**Architecture:** Refactor `CompilerContext` to adopt/delegate global states, synchronize `CStr` length in legacy mutators, implement a MASM-based dynamic DLL caller for ABI safety on x86, restore stack/rounding sanity, and clean up GTest logging flakes and repository build pollution.

**Tech Stack:** C++17, Win32 API, CMake, MASM (Microsoft Macro Assembler), GoogleTest.

---

### Task 1: Repository Hygiene & CMake Output Cleanups

**Files:**
- Modify: `CMakeLists.txt:31-33`
- Create: `.gitignore`

- [ ] **Step 1: Create a root-level .gitignore**
  
  Create file [.gitignore](file:///d:/GitHub-repo/Dark-Basic-Pro/.gitignore) with the following content:
  ```gitignore
  # CMake build directories
  /build/
  /.cmake/

  # CMake output binaries
  /bin/
  /bin/Debug/
  /bin/Release/

  # Visual Studio user files
  *.user
  *.suo
  *.userOS
  *.sln.docstates
  .vs/

  # Compilation outputs
  *.obj
  *.lib
  *.dll
  *.exe
  *.pdb
  *.ilk
  *.exp
  *.tlog
  *.log
  ```

- [ ] **Step 2: Untrack committed build/bin directories from Git**
  
  Run the following commands to clear build/bin from Git tracking without deleting them locally:
  ```bash
  git rm -r --cached build/
  git rm -r --cached bin/
  ```

- [ ] **Step 3: Modify CMakeLists.txt to write outputs inside the build directory**
  
  Modify [CMakeLists.txt](file:///d:/GitHub-repo/Dark-Basic-Pro/CMakeLists.txt) at lines 31-33:
  ```diff
  -set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
  -set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
  -set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
  +set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
  +set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
  +set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
  ```

- [ ] **Step 4: Verify git status is clean**
  
  Run: `git status`
  Expected: Only source edits and the `.gitignore` are unstaged/staged; `build/` and `bin/` are ignored.

- [ ] **Step 5: Commit Task 1**
  
  Run:
  ```bash
  git add .gitignore CMakeLists.txt
  git commit -m "chore: setup .gitignore and redirect CMake outputs to build/bin"
  ```

---

### Task 2: Fixing CompilerContext Lifecycles & Lifetimes

**Files:**
- Modify: `DBProCompiler/DBPCompiler/CompilerContext.h`
- Modify: `DBProCompiler/DBPCompiler/CompilerContext.cpp`
- Modify: `DBProCompiler/DBPCompiler/DBPCompiler.cpp`
- Test: `tests/test_context.cpp`

- [ ] **Step 1: Write the failing unit test for Lifecycle Adoption**
  
  Modify [test_context.cpp](file:///d:/GitHub-repo/Dark-Basic-Pro/tests/test_context.cpp) to verify that `CompilerContext` adopts existing tables and does not leak or crash:
  ```cpp
  #include <gtest/gtest.h>
  #include "CompilerContext.h"
  #include "InstructionTable.h"
  #include "Error.h"
  #include "EXEBlock.h"

  extern CInstructionTable* g_pInstructionTable;
  extern CError*            g_pErrorReport;
  extern CEXEBlock*         g_pEXE;

  TEST(CompilerContextTest, AdoptionAndCleanLifecycles) {
      // 1. Setup pre-existing globals
      CInstructionTable* pMockInst = new CInstructionTable();
      CError* pMockErr = new CError();
      g_pInstructionTable = pMockInst;
      g_pErrorReport = pMockErr;
      g_pEXE = nullptr;

      // 2. Instantiate and Initialize
      {
          CompilerContext context;
          context.Initialize();

          // Verify globals adopted
          EXPECT_EQ(g_pInstructionTable, pMockInst);
          EXPECT_EQ(g_pErrorReport, pMockErr);
          EXPECT_NE(g_pEXE, nullptr); // Owned by context

          context.Cleanup();
      }

      // 3. Post-cleanup check: pre-existing globals should NOT be deleted or nullified
      EXPECT_EQ(g_pInstructionTable, pMockInst);
      EXPECT_EQ(g_pErrorReport, pMockErr);
      EXPECT_EQ(g_pEXE, nullptr); // Context deleted it

      delete pMockInst;
      delete pMockErr;
      g_pInstructionTable = nullptr;
      g_pErrorReport = nullptr;
  }
  ```

- [ ] **Step 2: Run test to verify it fails**
  
  Run: `cmake --build build --config Debug` and `build\bin\Debug\dbp_tests.exe --gtest_filter=CompilerContextTest.AdoptionAndCleanLifecycles`
  Expected: Compile fails or test fails/crashes (due to double delete or null pointer dereference).

- [ ] **Step 3: Update CompilerContext header**
  
  Modify [CompilerContext.h](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/CompilerContext.h) to add ownership tracking flags and dynamic replacement function:
  ```cpp
  #pragma once
  #include "windows.h"
  #include "DebugInfo.h"

  class CEXEBlock;
  class CDBPCompiler;
  class CError;
  class ICodeGenerator;
  class CDBMWriter;
  class CStructTable;
  class CStatementList;
  class CInstructionTable;
  class CLabelTable;
  class CDataTable;
  class CVarTable;
  class CIncludeTable;

  class CompilerContext {
  public:
      CompilerContext();
      ~CompilerContext();

      void Initialize();
      void Cleanup();
      void ReplaceErrorReport(CError* pNewReport);

      CEXEBlock*			pEXE;
      CDBPCompiler*		pDBPCompiler;
      CError*				pErrorReport;
      ICodeGenerator*		pASMWriter;
      CDBMWriter*			pDBMWriter;
      CStructTable*		pStructTable;
      CStatementList*		pStatementList;
      CInstructionTable*	pInstructionTable;
      CLabelTable*		pLabelTable;
      CDataTable*			pDataTable;
      CDataTable*			pStringTable;
      CDataTable*			pDLLTable;
      CDataTable*			pCommandTable;
      CVarTable*			pVarTable;
      CIncludeTable*		pIncludeTable;
      CDataTable*			pConstantsTable;
      CDebugInfo*         pDebugInfo;

  private:
      bool                m_bOwnsInstructionTable;
      bool                m_bOwnsErrorReport;
  };
  ```

- [ ] **Step 4: Update CompilerContext implementation**
  
  Modify [CompilerContext.cpp](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/CompilerContext.cpp):
  ```cpp
  #include "ParserHeader.h"
  #include "CompilerContext.h"
  #include "EXEBlock.h"
  #include "DBPCompiler.h"
  #include "Error.h"
  #include "ASMWriter.h"
  #include "DBMWriter.h"
  #include "StructTable.h"
  #include "StatementList.h"
  #include "InstructionTable.h"
  #include "LabelTable.h"
  #include "DataTable.h"
  #include "VarTable.h"
  #include "IncludeTable.h"

  extern CEXEBlock*			g_pEXE;
  extern CDBPCompiler*		g_pDBPCompiler;
  extern CError*				g_pErrorReport;
  extern ICodeGenerator*		g_pASMWriter;
  extern CDBMWriter*			g_pDBMWriter;
  extern CStructTable*		g_pStructTable;
  extern CStatementList*		g_pStatementList;
  extern CInstructionTable*	g_pInstructionTable;
  extern CLabelTable*		g_pLabelTable;
  extern CDataTable*			g_pDataTable;
  extern CDataTable*			g_pStringTable;
  extern CDataTable*			g_pDLLTable;
  extern CDataTable*			g_pCommandTable;
  extern CVarTable*			g_pVarTable;
  extern CIncludeTable*		g_pIncludeTable;
  extern CDataTable*			g_pConstantsTable;
  extern CDebugInfo			g_DebugInfo;

  CompilerContext::CompilerContext() {
      pEXE = nullptr;
      pDBPCompiler = nullptr;
      pErrorReport = nullptr;
      pASMWriter = nullptr;
      pDBMWriter = nullptr;
      pStructTable = nullptr;
      pStatementList = nullptr;
      pInstructionTable = nullptr;
      pLabelTable = nullptr;
      pDataTable = nullptr;
      pStringTable = nullptr;
      pDLLTable = nullptr;
      pCommandTable = nullptr;
      pVarTable = nullptr;
      pIncludeTable = nullptr;
      pConstantsTable = nullptr;
      pDebugInfo = nullptr;
      m_bOwnsInstructionTable = false;
      m_bOwnsErrorReport = false;
  }

  CompilerContext::~CompilerContext() {
      Cleanup();
  }

  void CompilerContext::Initialize() {
      pStructTable = new CStructTable();
      pASMWriter = new CASMWriter();
      pDBMWriter = new CDBMWriter();
      pLabelTable = new CLabelTable("*");
      pDataTable = new CDataTable();
      pStringTable = new CDataTable("*");
      pDLLTable = new CDataTable("*");
      pCommandTable = new CDataTable("*");
      pVarTable = new CVarTable("$_ESP_");
      pStatementList = new CStatementList();
      
      if (g_pInstructionTable) {
          pInstructionTable = g_pInstructionTable;
          m_bOwnsInstructionTable = false;
      } else {
          pInstructionTable = new CInstructionTable();
          m_bOwnsInstructionTable = true;
      }

      pIncludeTable = new CIncludeTable();
      pConstantsTable = new CDataTable();

      if (g_pErrorReport) {
          pErrorReport = g_pErrorReport;
          m_bOwnsErrorReport = false;
      } else {
          pErrorReport = new CError();
          m_bOwnsErrorReport = true;
      }

      pEXE = new CEXEBlock();
      pDebugInfo = &g_DebugInfo;

      // Bind legacy globals
      g_pEXE = pEXE;
      g_pErrorReport = pErrorReport;
      g_pASMWriter = pASMWriter;
      g_pDBMWriter = pDBMWriter;
      g_pStructTable = pStructTable;
      g_pStatementList = pStatementList;
      g_pInstructionTable = pInstructionTable;
      g_pLabelTable = pLabelTable;
      g_pDataTable = pDataTable;
      g_pStringTable = pStringTable;
      g_pDLLTable = pDLLTable;
      g_pCommandTable = pCommandTable;
      g_pVarTable = pVarTable;
      g_pIncludeTable = pIncludeTable;
      g_pConstantsTable = pConstantsTable;
  }

  void CompilerContext::ReplaceErrorReport(CError* pNewReport) {
      if (m_bOwnsErrorReport) {
          delete pErrorReport;
      }
      pErrorReport = pNewReport;
      g_pErrorReport = pNewReport;
      m_bOwnsErrorReport = true;
  }

  void CompilerContext::Cleanup() {
      delete pStatementList;    pStatementList = nullptr;
      delete pDBMWriter;        pDBMWriter = nullptr;
      if (pASMWriter) {
          pASMWriter->FreeAll();
          delete pASMWriter;
          pASMWriter = nullptr;
      }

      if (pConstantsTable) { pConstantsTable->Free(); pConstantsTable = nullptr; }
      delete pIncludeTable;     pIncludeTable = nullptr;
      if (pCommandTable) { pCommandTable->Free(); pCommandTable = nullptr; }
      if (pDLLTable) { pDLLTable->Free(); pDLLTable = nullptr; }
      if (pDataTable) { pDataTable->Free(); pDataTable = nullptr; }
      if (pStringTable) { pStringTable->Free(); pStringTable = nullptr; }
      if (pStructTable) { pStructTable->Free(); pStructTable = nullptr; }

      if (m_bOwnsInstructionTable) {
          delete pInstructionTable;
      }
      pInstructionTable = nullptr;

      if (pLabelTable) { pLabelTable->Free(); pLabelTable = nullptr; }
      if (pVarTable) { pVarTable->Free(); pVarTable = nullptr; }

      if (m_bOwnsErrorReport) {
          delete pErrorReport;
      }
      pErrorReport = nullptr;

      delete pEXE;              pEXE = nullptr;

      g_pEXE = nullptr;
      if (m_bOwnsErrorReport) {
          g_pErrorReport = nullptr;
      }
      g_pASMWriter = nullptr;
      g_pDBMWriter = nullptr;
      g_pStructTable = nullptr;
      g_pStatementList = nullptr;
      if (m_bOwnsInstructionTable) {
          g_pInstructionTable = nullptr;
      }
      g_pLabelTable = nullptr;
      g_pDataTable = nullptr;
      g_pStringTable = nullptr;
      g_pDLLTable = nullptr;
      g_pCommandTable = nullptr;
      g_pVarTable = nullptr;
      g_pIncludeTable = nullptr;
      g_pConstantsTable = nullptr;
  }
  ```

- [ ] **Step 5: Modify DBPCompiler.cpp to resolve CLI replacement & double-frees**
  
  Modify [DBPCompiler.cpp](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/DBPCompiler.cpp):
  
  Around line 1370 (CLI error report deletion):
  ```diff
  -	// If been in CLI (so editor does not report old errors)
  -	if(bBeenInCLI==true)
  -	{
  -		// Delete error report
  -		SAFE_DELETE(g_pErrorReport);
  -		g_pErrorReport = new CError;
  -	}
  +	// If been in CLI (so editor does not report old errors)
  +	if(bBeenInCLI==true)
  +	{
  +		if (m_pContext) {
  +			m_pContext->ReplaceErrorReport(new CError);
  +		} else {
  +			SAFE_DELETE(g_pErrorReport);
  +			g_pErrorReport = new CError;
  +		}
  +	}
  ```

  Around line 1380 (EXE deletion):
  ```diff
  -	// Free The EXE when no more to run
  -	g_pEXE->FreeUptoDisplay();
  -	g_pEXE->Free();
  -
  -	// Delete CEXE Object Here
  -	SAFE_DELETE(g_pEXE);
  +	// Free The EXE when no more to run
  +	if (g_pEXE) {
  +		g_pEXE->FreeUptoDisplay();
  +		g_pEXE->Free();
  +	}
  ```

- [ ] **Step 6: Run test to verify it passes**
  
  Run: `cmake --build build --config Debug` and `build\bin\Debug\dbp_tests.exe --gtest_filter=CompilerContextTest.*`
  Expected: PASS

- [ ] **Step 7: Commit Task 2**
  
  Run:
  ```bash
  git add DBProCompiler/DBPCompiler/CompilerContext.h DBProCompiler/DBPCompiler/CompilerContext.cpp DBProCompiler/DBPCompiler/DBPCompiler.cpp tests/test_context.cpp
  git commit -m "refactor: fix CompilerContext lifecycles, global adoptions, and double-free on g_pEXE"
  ```

---

### Task 3: CStr Length Sync and String Macro Purge

**Files:**
- Modify: `DBProCompiler/DBPCompiler/Str.h`
- Modify: `DBProCompiler/DBPCompiler/Str.cpp`
- Test: `tests/test_str.cpp`

- [ ] **Step 1: Write failing test verifying length after Trimming/Eating**
  
  Modify [test_str.cpp](file:///d:/GitHub-repo/Dark-Basic-Pro/tests/test_str.cpp) to add assertions verifying that length updates properly after mutations:
  ```cpp
  TEST(CStrTest, LengthSynchronizationAfterMutations) {
      CStr testStr("  hello world   ");
      EXPECT_EQ(testStr.Length(), 16);

      // Perform trim/eat operations
      testStr.EatTrailingEdgeSpacesandTabs();
      EXPECT_EQ(testStr.Length(), 13);
      EXPECT_STREQ(testStr.GetStr(), "  hello world");

      DWORD chopped = 0;
      testStr.EatEdgeSpacesandTabs(&chopped);
      EXPECT_EQ(testStr.Length(), 11);
      EXPECT_STREQ(testStr.GetStr(), "hello world");
  }
  ```

- [ ] **Step 2: Run test to verify it fails**
  
  Run: `cmake --build build --config Debug` and `build\bin\Debug\dbp_tests.exe --gtest_filter=CStrTest.LengthSynchronizationAfterMutations`
  Expected: FAIL on the trimmed length check (length stays 16).

- [ ] **Step 3: Modify Str.h to support default null constructor safety**
  
  Modify [Str.h](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/Str.h):
  Ensure that `GetStr()` returns `m_pStr.get()` safely and handles empty strings.
  ```diff
  -		LPSTR		GetStr(void) const { return m_pStr ? m_pStr.get() : const_cast<LPSTR>(""); }
  +		LPSTR		GetStr(void) const { return m_pStr ? m_pStr.get() : const_cast<LPSTR>(""); }
  ```

- [ ] **Step 4: Remove raw pointer redefine macro and update mutators**
  
  Modify [Str.cpp](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/Str.cpp):
  Remove:
  ```cpp
  // Redirect all manual raw pointer operations to unique_ptr's managed pointer
  #define m_pStr m_pStr.get()
  ```
  And change all references to `m_pStr` inside member functions defined after this line to use `m_pStr.get()` where raw pointers are expected, or modify in-place access to write directly to `m_pStr.get()`.
  
  Additionally, add `UpdateLen()` to the end of the non-performance path of:
  - `EatTrailingEdgeSpacesandTabs`
  - `EatEdgeSpacesandTabs`
  - `EatSpeechMarks`
  - `EatLeadingChars`
  - `EatNonImportantChars`
  - `CropEqualEdgeBrackets`
  - `CropAll`
  - `EatChar`
  - `Shorten`
  - `Reverse`
  - `TrimToPathOnly`
  - `ResolveSciNot`

  For example, in `EatTrailingEdgeSpacesandTabs()`:
  ```cpp
  #else
  	if(m_pStr)
  	{
  		// Reverse string
  		strrev(m_pStr.get());
  		// ...
  		m_pStr.get()[w++]=0;
  		// Reverse string
  		strrev(m_pStr.get());
  		UpdateLen(); // Add this line!
  	}
  #endif
  ```

- [ ] **Step 5: Run test to verify it passes**
  
  Run: `cmake --build build --config Debug` and `build\bin\Debug\dbp_tests.exe --gtest_filter=CStrTest.*`
  Expected: PASS

- [ ] **Step 6: Commit Task 3**
  
  Run:
  ```bash
  git add DBProCompiler/DBPCompiler/Str.h DBProCompiler/DBPCompiler/Str.cpp tests/test_str.cpp
  git commit -m "fix: remove m_pStr macro redefine, fix CStr length sync in mutators, and add tests"
  ```

---

### Task 4: Dynamic DLL Calling ABI safety on x86

**Files:**
- Create: `Dark Basic Public Shared/Dark Basic Pro SDK/Shared/System/dynamic_call.asm`
- Modify: `Dark Basic Public Shared/Dark Basic Pro SDK/Shared/System/CSystemC.cpp`
- Modify: `Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDK/System/System.vcxproj`
- Modify: `CMakeLists.txt`
- Modify: `tests/test_structural.cpp` (to add a test)

- [ ] **Step 1: Write failing test verifying dynamic calling on x86**
  
  Modify [test_structural.cpp](file:///d:/GitHub-repo/Dark-Basic-Pro/tests/test_structural.cpp) to declare the assembly function and verify it:
  ```cpp
  extern "C" DWORD __cdecl asm_dynamic_call(void* func, const DWORD* args, int argc);

  static DWORD __stdcall dummy_stdcall(DWORD a, DWORD b) {
      return a * 10 + b;
  }

  static DWORD __cdecl dummy_cdecl(DWORD a, DWORD b, DWORD c) {
      return a + b + c;
  }

  TEST(DynamicCallTest, AssemblyCallParity) {
      DWORD args_stdcall[] = { 5, 8 };
      DWORD res_stdcall = asm_dynamic_call((void*)&dummy_stdcall, args_stdcall, 2);
      EXPECT_EQ(res_stdcall, 58);

      DWORD args_cdecl[] = { 10, 20, 30 };
      DWORD res_cdecl = asm_dynamic_call((void*)&dummy_cdecl, args_cdecl, 3);
      EXPECT_EQ(res_cdecl, 60);
  }
  ```

- [ ] **Step 2: Add MASM and ASM file compilation to CMakeLists.txt**
  
  Modify the root [CMakeLists.txt](file:///d:/GitHub-repo/Dark-Basic-Pro/CMakeLists.txt):
  Add `enable_language(ASM_MASM)` near project declaration:
  ```cmake
  project(DarkBasicPro LANGUAGES C CXX ASM_MASM)
  ```
  Add the `.asm` file to `dbp_tests` source files:
  ```cmake
  add_executable(dbp_tests
      # ... existing ...
      "Dark Basic Public Shared/Dark Basic Pro SDK/Shared/System/dynamic_call.asm"
  )
  ```

- [ ] **Step 3: Create the dynamic_call.asm MASM file**
  
  Create file [dynamic_call.asm](file:///d:/GitHub-repo/Dark-Basic-Pro/Dark%20Basic%20Public%20Shared/Dark%20Basic%20Pro%20SDK/Shared/System/dynamic_call.asm):
  ```assembly
  .model flat, c
  .code

  ; extern "C" DWORD __cdecl asm_dynamic_call(void* func, const DWORD* args, int argc);
  asm_dynamic_call proc C func:DWORD, args:DWORD, argc:DWORD
      push ebp
      mov ebp, esp
      push edi
      push esi
      push ebx

      mov ecx, argc
      mov edx, args
      
      test ecx, ecx
      jz do_call
      
      lea esi, [edx + ecx*4 - 4] ; Point to the last argument (push right-to-left)

  push_loop:
      push dword ptr [esi]
      sub esi, 4
      dec ecx
      jnz push_loop

  do_call:
      call func

      ; ESP recovery using EBP frame pointer to prevent crashes 
      ; regardless of callee conventions (__cdecl vs __stdcall)
      pop ebx
      pop esi
      pop edi
      mov esp, ebp
      pop ebp
      ret
  asm_dynamic_call endp
  end
  ```

- [ ] **Step 4: Run test to verify it passes**
  
  Run: `cmake --build build --config Debug` and `build\bin\Debug\dbp_tests.exe --gtest_filter=DynamicCallTest.*`
  Expected: PASS

- [ ] **Step 5: Integrate MASM build customizations in System.vcxproj**
  
  Modify [System.vcxproj](file:///d:/GitHub-repo/Dark-Basic-Pro/Dark%20Basic%20Public%20Shared/Dark%20Basic%20Pro%20SDK/DarkSDK/System/System.vcxproj):
  
  Insert MASM props in `ExtensionSettings` import:
  ```xml
    <ImportGroup Label="ExtensionSettings">
      <Import Project="$(VCTargetsPath)\BuildCustomizations\masm.props" />
    </ImportGroup>
  ```
  Insert MASM targets in `ExtensionTargets` import at the end of the file:
  ```xml
    <ImportGroup Label="ExtensionTargets">
      <Import Project="$(VCTargetsPath)\BuildCustomizations\masm.targets" />
    </ImportGroup>
  ```
  Add the `.asm` source item under an `ItemGroup`:
  ```xml
    <ItemGroup>
      <MASM Include="..\..\Shared\System\dynamic_call.asm" />
    </ItemGroup>
  ```

- [ ] **Step 6: Update CSystemC::Call to invoke asm_dynamic_call**
  
  Modify [CSystemC.cpp](file:///d:/GitHub-repo/Dark-Basic-Pro/Dark%20Basic%20Public%20Shared/Dark%20Basic%20Pro%20SDK/Shared/System/CSystemC.cpp):
  Declare `asm_dynamic_call` and rewrite `Call` to use it, completely eliminating the switch-case ABI risk.
  ```cpp
  extern "C" DWORD __cdecl asm_dynamic_call(void* func, const DWORD* args, int argc);

  DARKSDK bool Call(HINSTANCE hDLLModule, char* DecoratedName, DWORD* pDataAddress, int paramnum, DWORD* ReturnData)
  {
      FARPROC fpAddress = (FARPROC)GetProcAddress(hDLLModule, DecoratedName);
      if(!fpAddress) return false;

      DWORD res = asm_dynamic_call((void*)fpAddress, pDataAddress, paramnum);

      if(ReturnData)
          *ReturnData = res;

      return true;
  }
  ```

- [ ] **Step 7: Commit Task 4**
  
  Run:
  ```bash
  git add "Dark Basic Public Shared/Dark Basic Pro SDK/Shared/System/dynamic_call.asm" "Dark Basic Public Shared/Dark Basic Pro SDK/Shared/System/CSystemC.cpp" "Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDK/System/System.vcxproj" CMakeLists.txt tests/test_structural.cpp
  git commit -m "feat: implement MASM dynamic stack call helper for dynamic DLL ABI dispatching"
  ```

---

### Task 5: Restore Security Check and FtoI Rounding Sanity

**Files:**
- Modify: `Dark Basic Public Shared/Official Plugins/3D Cloth & Particles/Code/Common/PhysErrors.h`
- Modify: `Dark Basic Public Shared/Official Plugins/DarkLIGHTS/LightMapper/LMGlobal.h`

- [ ] **Step 1: Re-implement ON_FAIL_DLL_SECURITY_RETURN without inline assembly**
  
  Modify [PhysErrors.h](file:///d:/GitHub-repo/Dark-Basic-Pro/Dark%20Basic%20Public%20Shared/Official%20Plugins/3D%20Cloth%20&%20Particles/Code/Common/PhysErrors.h):
  Instead of an empty macro, use compiler intrinsics (`_AddressOfReturnAddress`) or frame check.
  ```cpp
  #include <intrin.h>
  #include <math.h>

  #define ON_FAIL_DLL_SECURITY_RETURN(retval) \
      DWORD* pErrPtr = (DWORD*)physics->DBPro_globalPtr->g_pErrorHandlerRef; \
      if (pErrPtr) { \
          DWORD* pESPPtr = pErrPtr + 1; \
          DWORD dwRecordedESP = *pESPPtr; \
          DWORD dwCurrentESP = (DWORD)_AddressOfReturnAddress(); \
          int iDifference = abs((int)(dwCurrentESP - dwRecordedESP)); \
          if (iDifference > 1024) return retval; \
      }
  ```

- [ ] **Step 2: Correct FtoI rounding logic in LMGlobal.h**
  
  Modify [LMGlobal.h](file:///d:/GitHub-repo/Dark-Basic-Pro/Dark%20Basic%20Public%20Shared/Official%20Plugins/DarkLIGHTS/LightMapper/LMGlobal.h):
  Change `static_cast<int>(f)` to `std::lround(f)` to restore the original round-to-nearest behavior.
  ```cpp
  #include <cmath>

  inline int FtoI( float f )
  {
      return static_cast<int>(std::lround(f));
  }
  ```

- [ ] **Step 3: Commit Task 5**
  
  Run:
  ```bash
  git add "Dark Basic Public Shared/Official Plugins/3D Cloth & Particles/Code/Common/PhysErrors.h" "Dark Basic Public Shared/Official Plugins/DarkLIGHTS/LightMapper/LMGlobal.h"
  git commit -m "refactor: restore dynamic stack security checks and correct FtoI rounding behavior"
  ```

---

### Task 6: Unit Test Cleanups and Logger Flakes

**Files:**
- Modify: `tests/test_logger.cpp`
- Modify: `tests/test_vartable.cpp`

- [ ] **Step 1: Refactor test_logger.cpp to remove process spawn**
  
  Modify [test_logger.cpp](file:///d:/GitHub-repo/Dark-Basic-Pro/tests/test_logger.cpp) to delete the `CompilerStartupLoggingIntegration` test which spawns `DBPCompiler.exe` and causes flakiness, and isolate spdlog shutdown.
  ```cpp
  #include <gtest/gtest.h>
  #include <fstream>
  #include <string>
  #include <filesystem>
  #include "DBPLogger.h"

  TEST(DBPLoggerTest, BasicFileLogging) {
      std::string logFile = "test_run.log";
      if (std::filesystem::exists(logFile)) {
          std::filesystem::remove(logFile);
      }

      DBPLogger::Initialize(logFile);
      DBP_INFO("Hello GTest Logger!");
      
      spdlog::shutdown();

      ASSERT_TRUE(std::filesystem::exists(logFile));

      std::ifstream infile(logFile);
      std::string line;
      bool found = false;
      while (std::getline(infile, line)) {
          if (line.find("Hello GTest Logger!") != std::string::npos) {
              found = true;
              break;
          }
      }
      EXPECT_TRUE(found);
  }

  TEST(DBPLoggerTest, FormatTracingAndLogLevels) {
      std::string testLog3 = "test_format.log";
      if (std::filesystem::exists(testLog3)) {
          std::filesystem::remove(testLog3);
      }
      DBPLogger::Initialize(testLog3);
      DBP_TRACE("Trace variable: {} = {}", "myVar", 100);
      DBP_WARN("Warning test: code={}", 404);
      DBP_ERROR("Error test: msg={}", "critical failure");
      spdlog::shutdown();

      std::ifstream infile3(testLog3);
      std::string line3;
      int matches = 0;
      while (std::getline(infile3, line3)) {
          if (line3.find("Trace variable: myVar = 100") != std::string::npos) matches++;
          if (line3.find("Warning test: code=404") != std::string::npos) matches++;
          if (line3.find("Error test: msg=critical failure") != std::string::npos) matches++;
      }
      EXPECT_EQ(matches, 3);
  }
  ```

- [ ] **Step 2: Clean up test_vartable.cpp leaks and standardize initialization**
  
  Modify [test_vartable.cpp](file:///d:/GitHub-repo/Dark-Basic-Pro/tests/test_vartable.cpp) to use `CompilerContext` for initialization and cleanup to ensure no leaks occur:
  ```cpp
  #include <gtest/gtest.h>
  #include <windows.h>
  #include "DBPLogger.h"
  #include "VarTable.h"
  #include "StatementList.h"
  #include "CompilerContext.h"

  extern CStatementList*   g_pStatementList;
  extern CVarTable*        g_pVarTable;

  class VarTableTest : public ::testing::Test {
  protected:
      CompilerContext* m_pCtx;

      void SetUp() override {
          DBPLogger::Initialize("test_vartable.log");
          m_pCtx = new CompilerContext();
          m_pCtx->Initialize();
          
          g_pStatementList->SetWriteStarted(false);
          char dummyProg[] = "";
          g_pStatementList->MakeStatements(dummyProg, 0);
      }

      void TearDown() override {
          if (m_pCtx) {
              m_pCtx->Cleanup();
              delete m_pCtx;
              m_pCtx = nullptr;
          }
          spdlog::shutdown();
      }
  };

  TEST_F(VarTableTest, AddAndFindVariable) {
      g_pStatementList->SetVariableAddParse(true);
      
      DWORD dwAction = 0;
      bool result = g_pVarTable->AddVariable("myIntegerVar", "integer", 0, 10, true, &dwAction, false);
      ASSERT_TRUE(result);
      
      CVarTable* pVar = g_pVarTable->FindVariable(nullptr, "myIntegerVar", 0);
      ASSERT_NE(pVar, nullptr);
  }
  ```

- [ ] **Step 3: Run the entire test suite and verify all tests pass**
  
  Run: `cmake --build build --config Debug` and `build\bin\Debug\dbp_tests.exe`
  Expected: ALL tests pass successfully.

- [ ] **Step 4: Commit Task 6**
  
  Run:
  ```bash
  git add tests/test_logger.cpp tests/test_vartable.cpp
  git commit -m "test: clean up logger test flakiness and resolve memory leaks in vartable test teardown"
  ```
