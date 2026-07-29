# Modernization Roadmap & Guide
This guide details the structural and code modernization process for the **DarkBasic Pro** engine to make the legacy codebase more flexible, maintainable, extensible, and easier to debug, while keeping the engine targeting 32-bit (x86) in the initial phases, and leaving the 64-bit transition as the final step.

---

## 🗺️ General Roadmap

```
[ Phase 1: CMake Build System ]
               │
               ▼
[ Phase 2: spdlog Logging System ]
               │
               ▼
[ Phase 3: GTest Automated Testing ]
               │
               ▼
[ Phase 4: Memory Safety & STL Modernization ]
               │
               ▼
[ Phase 5: State Encapsulation (CompilerContext) ]
               │
               ▼
[ Phase 6: Backend Abstraction (ICodeGenerator) ]
               │
               ▼
[ Phase 7: Inline Assembly Removal (__asm) ]
               │
               ▼
[ Phase 8: Structural Cleanup (PluginRegistry) ]
               │
               ▼
[ Phase 9: Unicode Support & Character Encoding ]
               │
               ▼
[ Phase 10: Modern Windows & DirectX APIs ]
               │
               ▼
[ Phase 11: AST and Multi-Pass Compiler ]
               │
               ▼
[ Phase 12: Modern Compiler Error Diagnostics ]
               │
               ▼
[ Phase 13: Virtual File System (VFS) & AV Protection ]
               │
               ▼
[ Phase 14: Job System & Task-based Concurrency ]
               │
               ▼
[ Phase 15: x64 Architecture & Modern Graphics ]
               │
               ▼
[ Phase 16: Modern Asset Protection & VFS PAK Containers ]
```

---

## 📁 Detailed Modernization Phase Documents

Each phase of the modernization process is detailed in its own markdown document:

1. **[Phase 1: Unified CMake Build System](file:///d:/GitHub-repo/Dark-Basic-Pro/docs/01_cmake_build_system.md)**
   * Migrating Visual Studio projects to central CMake and resolving dependencies.
2. **[Phase 2: Logging and Diagnostics](file:///d:/GitHub-repo/Dark-Basic-Pro/docs/02_logging_and_diagnostics.md)**
   * Integrating `spdlog` for compiler tracing and error logging.
3. **[Phase 3: Automated Testing Environment](file:///d:/GitHub-repo/Dark-Basic-Pro/docs/03_automated_testing.md)**
   * Integrating `Google Test` to build a safety net against regressions.
4. **[Phase 4: Memory Safety and Modern C++](file:///d:/GitHub-repo/Dark-Basic-Pro/docs/04_memory_safety_and_modern_cpp.md)**
   * Replacing custom `CStr`, raw pointers, and manual heap allocations with STL and smart pointers.
5. **[Phase 5: State Encapsulation](file:///d:/GitHub-repo/Dark-Basic-Pro/docs/05_state_encapsulation.md)**
   * Grouping compiler tables and globals in `CompilerContext`.
6. **[Phase 6: Backend Abstraction](file:///d:/GitHub-repo/Dark-Basic-Pro/docs/06_backend_abstraction.md)**
   * Introducing `ICodeGenerator` to isolate parser from assembler.
7. **[Phase 7: Inline Assembly Removal](file:///d:/GitHub-repo/Dark-Basic-Pro/docs/07_inline_assembly_removal.md)**
   * Removing `__asm` blocks and replacing with `libffi` and standard C++.
8. **[Phase 8: Structural Cleanup](file:///d:/GitHub-repo/Dark-Basic-Pro/docs/08_structural_cleanup.md)**
   * Replacing magic numbers with `enum class` and implementing `PluginRegistry`.
9. **[Phase 9: Unicode Transition](file:///d:/GitHub-repo/Dark-Basic-Pro/docs/09_unicode_transition.md)**
   * Moving to standard UTF-8/UTF-16 and wide Win32 APIs for non-English support.
10. **[Phase 10: Modern Win32 APIs](file:///d:/GitHub-repo/Dark-Basic-Pro/docs/10_modern_win32_apis.md)**
    * Discarding DirectPlay, DirectInput, and D3DX9 in favor of modern equivalents.
11. **[Phase 11: AST Compiler Architecture](file:///d:/GitHub-repo/Dark-Basic-Pro/docs/11_ast_compiler_architecture.md)**
    * Restructuring compiler frontend with Abstract Syntax Tree.
12. **[Phase 12: Compiler Diagnostics](file:///d:/GitHub-repo/Dark-Basic-Pro/docs/12_compiler_diagnostics.md)**
    * Creating rich diagnostic outputs with source highlighting and helpers.
13. **[Phase 13: Virtual File System (VFS)](13_vfs_and_antivirus.md)**
    * Shipped read-only VFS mounting with exact paths, owned streams, and no temporary PCK extraction.
14. **[Phase 14: Modern Threading & Job System](file:///d:/GitHub-repo/Dark-Basic-Pro/docs/14_modern_threading_job_system.md)**
    * Task-based concurrency using standard thread pools.
15. **[Phase 15: x64 and Future Tech](file:///d:/GitHub-repo/Dark-Basic-Pro/docs/15_x64_and_future_tech.md)**
    * Compiler x86-64 backend rewrite, DLL rebuilds, and upgrading to DirectX 11/Vulkan.
16. **[Phase 16: Authenticated DBPAK v2 Packages](16_modern_asset_protection_and_vfs_pak_roadmap.md)**
    * Shipped AES-256-GCM/HKDF-SHA-256 packages, explicit runtime descriptors, PE key resources, and read-only legacy compatibility.
    * [Verification baseline and reproducible evidence](baselines/2026-07-28-authenticated-package-v2.md).
17. **[Headless Application Publisher](17_headless_application_publisher.md)**
    * Publish a precompiled host and explicit assets as an authenticated, transactional EXE + DBPAK + descriptor tuple from scripts or CI.

---

## 🛠️ How to Start?
To start implementing this roadmap, navigate to **[Phase 1: CMake Build System](file:///d:/GitHub-repo/Dark-Basic-Pro/docs/01_cmake_build_system.md)** to configure a robust build environment, then follow the numbered steps in order.
