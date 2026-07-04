# Final Phase: x64 Architecture & Modern Graphics

## 🎯 Goal
Upgrade the entire engine, compiler, and plugins to support native **64-bit (x64)** execution and replace legacy DirectX 9 rendering. This is the final step, executed once all prior restructuring and cleanup phases have established a stable C++17 foundation.

---

## 🏗️ 64-bit Architecture Upgrade (x64)

The 64-bit transition requires three concurrent efforts:

```
                     ┌──────────────────────────────┐
                     │     64-bit Modernization     │
                     └──────────────┬───────────────┘
                                    │
         ┌──────────────────────────┼──────────────────────────┐
         ▼                          ▼                          ▼
┌──────────────────┐       ┌──────────────────┐       ┌──────────────────┐
│ Write x64 JIT    │       │ Recompile DLL    │       │ Clean Pointer    │
│  CASMWriterx64   │       │ Plugins as x64   │       │ Casts to x64     │
└──────────────────┘       └──────────────────┘       └──────────────────┘
```

### 1. Implement the x64 JIT Assembler (`CASMWriterx64`)
Create a new compiler backend implementing `ICodeGenerator` to output x86-64 machine instructions:
* **Register Selection**: Transition from 32-bit registers (EAX, ESP, EBP, EDI) to 64-bit registers (RAX, RSP, RBP, RDI, and new R8-R15 registers).
* **Microsoft x64 Calling Convention**:
  * Adhere to the single Windows x64 calling convention. The first 4 arguments are passed in registers (`RCX`, `RDX`, `R8`, `R9`) instead of pushed onto the stack.
  * Allocate 32 bytes of shadow space (register spill space) on the stack before making calls.
  * Keep the stack pointer `RSP` aligned to 16-byte boundaries before function calls to avoid AVX/DirectX library crashes.

### 2. Recompile DLL Plugins as x64
* A 64-bit runner process cannot load 32-bit DLLs. Therefore, all game engine DLLs (Graphics, Setup, Sound, Inputs) must be built targeting the **x64** configuration.
* **Proprietary DLLs**: For old closed-source plugins without source code, we must replace them with modern open-source alternatives (e.g., replacing old physics DLLs with a modern 64-bit compilation of Bullet Physics).

### 3. Audit Pointer Casting
* Verify all pointer casts in the codebase use `uintptr_t` or `DWORD_PTR` instead of `DWORD` or `unsigned long` to prevent pointer truncation from 8 bytes to 4 bytes, which causes immediate segmentation faults.

---

## 🎨 Modern Graphics API Upgrade

DarkBasic Pro is built around DirectX 9 (released in 2004). Modern GPUs and operating systems do not support native DirectX 9 optimally, which leads to driver bottlenecks and compatibility issues.

### Modern Graphics Migration Options:
1. **DXVK Translation Layer**:
   * A zero-code-change solution. Drop the **DXVK** wrapper DLL into the game folder. It translates Direct3D 9 commands to **Vulkan** instructions at runtime, drastically improving performance and compatibility on modern hardware.
2. **DirectX 11/12 Port**:
   * Refactor the graphics rendering DLLs (`Basic3D.dll`, `Camera3D.dll`, `Image.dll`) to target DirectX 11 or a cross-platform graphics wrapper (like bgfx).
   * This enables modern visual effects, shaders, and multi-threaded rendering pipelines.

---

## 🚀 Benefits
* **Break the 4GB Memory Limit**: 64-bit games can access more than 4GB of system RAM, enabling larger texture packages and highly detailed 3D scenes.
* **Flawless Windows 11 Compatibility**: Native x64 executables run with maximum stability without relying on legacy Wow64 subsystems or OS compatibility options.
* **High Performance**: Native SSE/AVX hardware optimization on 64-bit CPUs improves game framerates.
