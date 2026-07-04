# Phase 7: Inline Assembly Removal

## 🎯 Goal
Remove all inline assembly (`__asm`) blocks from C++ source files. This is a strict prerequisite for building the project as a 64-bit application under MSVC (which forbids inline assembly) and eliminates fragile, compiler-specific CPU hacks.

---

## 🔍 Identified Inline Assembly Blocks & Replacements

### 1. Dynamic DLL Function Dispatch (`CSystemC.cpp`)
* **Location**: `Call` function in [CSystemC.cpp](file:///d:/GitHub-repo/Dark-Basic-Pro/Dark%20Basic%20Public%20Shared/Dark%20Basic%20Pro%20SDK/Shared/System/CSystemC.cpp#L162).
* **Problem**: Uses `__asm` to push an arbitrary number of parameters onto the stack at runtime and call a DLL function. This assumes 32-bit stack layout and causes crashes on x64.
* **Modern Replacement**:
  * Integrate the **libffi** (Foreign Function Interface) library.
  * `libffi` handles dynamic calls, argument layout, and return values safely across different platforms (x86, x64, ARM).

```cpp
#include <ffi.h>

bool CallModern(HINSTANCE hDLLModule, char* DecoratedName, DWORD* pDataAddress, int paramnum, DWORD* ReturnData) {
    void (*fpAddress)() = (void(*)())GetProcAddress(hDLLModule, DecoratedName);
    if (!fpAddress) return false;

    ffi_cif cif;
    ffi_type* args[MAX_PARAMS];
    void* values[MAX_PARAMS];

    for (int i = 0; i < paramnum; i++) {
        args[i] = &ffi_type_uint32;
        values[i] = &pDataAddress[i];
    }

    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, paramnum, &ffi_type_uint32, args) == FFI_OK) {
        ffi_arg result;
        ffi_call(&cif, fpAddress, &result, values);
        *ReturnData = static_cast<DWORD>(result);
        return true;
    }
    return false;
}
```

### 2. Float to Integer Conversion (`LMGlobal.h`)
* **Location**: `FtoI` function in [LMGlobal.h](file:///d:/GitHub-repo/Dark-Basic-Pro/Dark%20Basic%20Public%20Shared/Official%20Plugins/DarkLIGHTS/LightMapper/LMGlobal.h#L15).
* **Problem**: Uses x87 instructions `fld` and `fistp` to speed up floating-point truncation.
* **Modern Replacement**:
  * Replace with `static_cast<int>(f)` or `std::lrintf` from `<cmath>`.
  * Modern compilers automatically translate this to highly optimized SSE2 instructions (like `cvttss2si`) rendering inline assembly obsolete.

### 3. Stack Integrity Verification (`PhysErrors.h`)
* **Location**: [PhysErrors.h](file:///d:/GitHub-repo/Dark-Basic-Pro/Dark%20Basic%20Public%20Shared/Official%20Plugins/3D%20Cloth%20&%20Particles/Code/Common/PhysErrors.h#L20).
* **Problem**: Uses `__asm mov dwCurrentESP, EBP` to manually assert stack pointer offsets.
* **Modern Replacement**:
  * Remove this manual check and enable MSVC's standard buffer checks via compiler flag `/GS`, or compile with **AddressSanitizer (ASan)**.

---

## 🚀 Benefits
* **x64 Compatibility**: Resolves MSVC compile errors, preparing the codebase for 64-bit builds.
* **Standard Conformance**: Code becomes standard C++, easily compileable on any major compiler (MSVC, GCC, Clang).
