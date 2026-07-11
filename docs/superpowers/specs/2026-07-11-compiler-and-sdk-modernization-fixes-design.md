# Technical Design Spec: Compiler and SDK Modernization Fixes

- **Date**: 2026-07-11
- **Status**: Draft (Proposed)
- **Target Issues**: 10 Bugs, 7 Suggestions, 3 Nits in the `test-1` branch.

---

## 1. Compiler Lifecycle and Ownership (Issues 1, 2, 3, 4)

### Problem Analysis
`CompilerContext` was introduced to encapsulate global state. However, it violates existing object lifecycles:
1. `CompilerContext::Initialize()` allocates a new `CInstructionTable` and `CError` on top of already loaded/initialized globals, leaking the old ones and leaving the compiler with empty instruction and error databases.
2. `CompilerContext::Cleanup()` deletes `pErrorReport` and sets `g_pErrorReport = nullptr`, causing null pointer dereference crashes when the outer `PerformCompileOnProject()` function reports compilation results or errors after `MakeProgram()` returns.
3. `MakeProgram()` deletes `g_pEXE` manually, while `CompilerContext::Cleanup()` also deletes `pEXE` (which points to the same object), leading to a double-free heap corruption.
4. CLI mini-program compilation replaces `g_pErrorReport` with a new `CError` pointer, causing a memory leak of the new instance and a double-free of the old one via `CompilerContext->pErrorReport`.

### Proposed Solution
We will refactor the memory ownership model so that the context behaves as a **State Adapter / Coordinator** rather than a destructive allocator.

1. **Adopt Existing Globals**: `CompilerContext::Initialize()` will check if `g_pInstructionTable` and `g_pErrorReport` are already allocated. If they are, it will adopt them and reference them, rather than re-creating them.
2. **Decouple Error Lifecycle**: The error reporter (`CError`) lifetime is managed at the compiler process/compilation scope. `CompilerContext` will reference `g_pErrorReport` but **not delete it** during `Cleanup()`. The deletion of `g_pErrorReport` will be handled exclusively by the compiler's main entry point/destruction phases.
3. **Single Ownership for `CEXEBlock`**: Remove manual `SAFE_DELETE(g_pEXE)` from `MakeProgram()`. Let `CompilerContext` own the lifetime of the `CEXEBlock` via `std::unique_ptr<CEXEBlock>` (or safely delete it in `Cleanup()` and set `g_pEXE = nullptr` there).
4. **Synchronize CLI Error Upgrades**: Ensure that any CLI error reporter replacement updates both the global `g_pErrorReport` and the active context's `pErrorReport` pointer to maintain pointer consistency.

---

## 2. CStr String Performance and Length (Issues 5, 6, 19)

### Problem Analysis
1. `#undef __AARON_STRPERF__` is set, compiled, and used.
2. `Length()` is changed to return `m_dwLen` directly.
3. The legacy in-place string mutators (e.g. `EatTrailingEdgeSpacesandTabs`, `EatEdgeSpacesandTabs`, `Shorten`, `Reverse`, `EatChar`) modify the underlying character array and null-terminate it, but do NOT update `m_dwLen`.
4. `#define m_pStr m_pStr.get()` is a hazardous macro hack that replaces all tokens in `Str.cpp`, obscuring `std::unique_ptr` APIs and causing potential compilation issues.

### Proposed Solution
1. **Remove Preprocessor Hack**: Eliminate the `#define m_pStr m_pStr.get()` macro entirely. Replace it with explicit `.get()` calls or a private helper `char* data()` / `const char* data() const`.
2. **Mandatory Length Synchronization**: Add calls to `UpdateLen()` at the end of all mutator functions in `Str.cpp` (specifically inside `#else` blocks where the raw buffer is directly modified).
3. **Null Buffer Safety**: Update `GetStr()` to return an empty string safely, and ensure the default constructor allocates a 1-byte empty string buffer to avoid undefined behavior if a caller attempts to write into a default-constructed `CStr`.

---

## 3. Dynamic DLL Call ABI Compatibility (Issues 7, 9, 15)

### Problem Analysis
1. `CSystemC::Call()` was refactored from inline assembly to a C++ `switch-case` casting function pointers to `__stdcall` with up to 32 parameters.
2. In x86, DLL plugins can have either `__cdecl` or `__stdcall` calling conventions. The original inline assembly pushed parameters and restored ESP dynamically (`mov esp, dwStore`), which gracefully survived calling convention differences. Hardcoding `__stdcall` will corrupt the stack for `__cdecl` functions.
3. The `default:` case for `paramnum > 32` calls the target with a fixed 20-parameter `__stdcall` signature, leading to stack overflow or corruption when the function actually expects a different number of parameters.
4. `ON_FAIL_DLL_SECURITY_RETURN` was gutted, removing the stack validation check.
5. `FtoI()` was changed from `fistp` FPU rounding to `static_cast<int>()` truncation, causing numerical divergence in lightmap calculations.

### Proposed Solution
1. **Dynamic Assembly Call Helper (x86)**:
   We will write a dedicated assembly file `dynamic_call.asm` compiled using MASM. This assembly function will dynamically push parameters onto the stack in reverse order, invoke the target address, and then clean up/restore the ESP pointer using the EBP frame pointer. This guarantees absolute ABI compatibility for both `__cdecl` and `__stdcall` calling conventions and supports any number of parameters.
   
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
       lea esi, [edx + ecx*4 - 4] ; Point to the last argument (right-to-left push)
   
   push_loop:
       test ecx, ecx
       jz do_call
       push dword ptr [esi]
       sub esi, 4
       dec ecx
       jmp push_loop
   
   do_call:
       call func
   
       ; Restore ESP from EBP frame pointer to dynamically correct stack imbalances 
       ; caused by calling convention mismatches (__cdecl vs __stdcall)
       pop ebx
       pop esi
       pop edi
       mov esp, ebp
       pop ebp
       ret
   asm_dynamic_call endp
   end
   ```
2. **x64 ABI Call Strategy**: For future 64-bit support, compile `asm_dynamic_call` using x64 MASM complying with the Microsoft x64 calling convention (where the caller always allocates shadow space and cleans up).
3. **Restore Stack Security Check**: Re-implement `ON_FAIL_DLL_SECURITY_RETURN` using compiler intrinsics or standard stack pointer checks that do not require inline assembly.
4. **Restore Proper Rounding**: Change `FtoI` in `LMGlobal.h` to use `std::lround` or `std::nearbyint` to match the original FPU `fistp` rounding behavior, preventing numerical divergence in lightmap generation.

---

## 4. Repository Cleanliness and Build Hygiene (Issue 8)

### Problem Analysis
No `.gitignore` exists at the root. The entire `build/` (CMake cache, FetchContent logs, objects) and `bin/` (compiler executable, libraries) directories are committed, cluttering the history and introducing machine-specific configuration files.

### Proposed Solution
1. Create a comprehensive root-level `.gitignore` file covering `/build/`, `/bin/`, visual studio user files, and build logs.
2. Remove all committed build artifacts from Git track history:
   ```bash
   git rm -r --cached build/
   git rm -r --cached bin/
   ```
3. Configure CMake to write output binaries to `build/bin/` rather than the source root directory `bin/` to separate source code from build outputs.

---

## 5. Unit Testing and Logging (Issues 10, 14, 16, 20)

### Problem Analysis
1. `test_logger.cpp` spawns `DBPCompiler.exe` via `std::system()`, which hangs on GUI/dialog boxes and causes flakes.
2. `test_vartable.cpp` manually creates compiler global objects rather than using `CompilerContext`, fails to pass `"$_ESP_"` to `CVarTable`, and leaks all allocated variable nodes in `TearDown()` because it uses `delete g_pVarTable;` without walking/calling `Free()`.
3. `DBPLogger` initializes log files in the current working directory, causing logs to scatter depending on the launch location.

### Proposed Solution
1. **Refactor Logger Integration Test**: Remove the `std::system("DBPCompiler.exe")` call. Test the logging initialization and file writing directly inside the test process.
2. **Standardize Test Fixtures**: Refactor `test_vartable.cpp` to initialize the environment using `CompilerContext` so that testing environment lifecycles exactly match production. Ensure `TearDown()` walks the variable list or calls `CompilerContext::Cleanup()` to prevent leaks.
3. **Log Directory Standardization**: Configure `DBPLogger` to write its output to `%TEMP%` or a dedicated relative path under the compiler directory.
