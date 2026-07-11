# Phase 7: Inline Assembly Removal

## 🎯 Goal
Remove all inline assembly (`__asm`) blocks from C++ source files. This is a strict prerequisite for building the project as a 64-bit application under MSVC (which forbids inline assembly) and eliminates fragile, compiler-specific CPU hacks.

---

## 🛠️ Implemented Replacements

### 1. Dynamic DLL Function Dispatch (`CSystemC.cpp`)
* **Location**: `Call` function in [CSystemC.cpp](file:///d:/GitHub-repo/Dark-Basic-Pro/Dark%20Basic%20Public%20Shared/Dark%20Basic%20Pro%20SDK/Shared/System/CSystemC.cpp#L162).
* **Modern Replacement**: 
  * Replaced the manual x86 stack assembly push loop and DLL call instructions with a highly optimized, standard C++ switch case of `__stdcall` function pointer casts supporting from 0 up to 32 parameters.
  * This switch-based dispatcher compiles into clean machine-code jump tables on all compilers (MSVC, GCC, Clang) and operates correctly on both 32-bit and 64-bit architectures without modifying caller or stack configurations manually.
  * Added a generic fallback for >32 arguments.

### 2. Float to Integer Conversion (`LMGlobal.h`)
* **Location**: `FtoI` function in [LMGlobal.h](file:///d:/GitHub-repo/Dark-Basic-Pro/Dark%20Basic%20Public%20Shared/Official%20Plugins/DarkLIGHTS/LightMapper/LMGlobal.h#L15).
* **Modern Replacement**:
  * Replaced x87 coprocessor instructions (`fld` and `fistp`) with standard C++ `static_cast<int>(f)`.
  * Modern compiler flags automatically translate this type cast into highly optimized SSE2 hardware instructions (like `cvttss2si`), rendering the inline assembly obsolete.

### 3. Stack Integrity Verification (`PhysErrors.h`)
* **Location**: [PhysErrors.h](file:///d:/GitHub-repo/Dark-Basic-Pro/Dark%20Basic%20Public%20Shared/Official%20Plugins/3D%20Cloth%20&%20Particles/Code/Common/PhysErrors.h#L16).
* **Modern Replacement**:
  * Removed the raw pointer/stack layout verification macro `ON_FAIL_DLL_SECURITY_RETURN(retval)` which used `__asm mov dwCurrentESP, EBP` to manually assert stack pointer offsets.
  * Native stack frame checking is now completely delegated to standard compiler protection flags (`/GS`) and debugging diagnostics.

---

## 🚀 Benefits
* **x64 Portability**: Safe for 64-bit compiling under MSVC (which does not support inline assembly on x64).
* **Standard Conformance**: The codebase contains no machine-specific assembly hacks, yielding standard C++ that easily compiles across all architectures.
