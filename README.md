# Dark-Basic-Pro Engine & Compiler

[![Build & Test Status](https://img.shields.io/badge/tests-CMake%20%2B%20CTest-brightgreen.svg)]()
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-17%20%2F%2020-blue.svg)]()
[![Target](https://img.shields.io/badge/active%20target-Windows%20PE32%20%2F%20x86-lightgrey.svg)]()
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

A modernizing, open-source game development language, compilation engine, and toolchain suite for Windows. The active executable target remains PE32/x86 while the compiler architecture is being prepared for a deliberate PE32+/x64 port.

---

## 📚 Table of Contents

- [Overview \& Architectural Highlights](#-overview--architectural-highlights)
- [Prerequisites \& Environment Setup](#-prerequisites--environment-setup)
- [Building the Project](#-building-the-project)
  - [Available CMake Presets](#available-cmake-presets)
  - [Build Targets](#build-targets)
- [Testing \& Quality Assurance](#-testing--quality-assurance)
  - [1. C++ Unit \& Integration Test Suite (`dbp_tests.exe`)](#1-c-unit--integration-test-suite-dbp_testsexe)
  - [2. DarkBASIC Language Conformance Tests (`run-conformance.Tests.ps1`)](#2-darkbasic-language-conformance-tests-run-conformancetestsps1)
  - [3. Code Coverage \& Sanitizers](#3-code-coverage--sanitizers)
- [Automation \& Utility Scripts (`/scripts`)](#-automation--utility-scripts-scripts)
- [Packaging \& Installation (CPack)](#-packaging--installation-cpack)
- [Repository Structure](#-repository-structure)
- [License \& Attribution](#-license--attribution)

---

## 🚀 Overview & Architectural Highlights

DarkBasic Pro is a complete language implementation that compiles high-level BASIC source code (`.dba`) directly into standalone Windows Portable Executable (`.exe`) binaries with raw x86 machine code emission.

### Target ABI and x64 Roadmap

The compiler now models the generated program's ABI independently from the host process that runs the compiler. `TargetAbi32` is the explicit active target, while `TargetAbi64` provides a testable layout model for the future port. Serialized addresses are decoded through bounded byte operations instead of host-pointer casts, and target-dependent variable layouts derive their address width from the selected ABI.

This foundation does **not** claim complete x64 output support. A production PE32+/x64 target still requires x64 machine-code emission, PE32+ image generation, Windows x64 calling-convention and unwind metadata support, and a compatible 64-bit runtime/plugin ABI. Keeping these concerns explicit prevents a 64-bit compiler host from silently changing the format of 32-bit generated programs.

### Core Extracted Subsystems (God-Class Refactoring)
To ensure long-term maintainability and high code quality, legacy monolithic classes (`CASMWriter`, `CStatement`) have been refactored into focused, decoupled subsystems adhering to modern C++17/C++20 standards:

* **`CMachineCodeBuffer`** (`MachineCodeBuffer.cpp/.h`): Manages raw x86 machine code buffer allocation, low-level byte/DWORD emission, and dynamic 100KB chunk expansion with bounds safety and `[[nodiscard]]`/`noexcept` attributes.
* **`CLeapMarkerManager`** (`LeapMarkerManager.cpp/.h`): Handles forward references, leap-marker resolution, relative offset calculations, and backpatching during assembly emission.
* **`CDebuggerInterface`** (`DebuggerInterface.cpp/.h`): Manages shared-memory IPC (`CreateFileMappingW`) and process communication between compiled targets and internal/external debuggers.
* **`StatementHelper`** (`StatementHelper.cpp/.h`): Pure utility namespace for string manipulation, declaration type separation, and assignment operator checks with zero global state dependencies.
* **`EXEBlock` W^X Security**: Enforces the **W^X (Write XOR Execute)** principle by allocating Machine Code Blocks as `PAGE_READWRITE` and applying `VirtualProtect(PAGE_EXECUTE_READ)` upon completion.
* **AST Pipeline & Optimization**: Full Abstract Syntax Tree (AST) parser, constant folding optimizer, typed IR lowering, and target code generation.
* **Executable Preparation Pipeline**: A fail-fast coordinator owns machine-code finalization, reference serialization, runtime metadata, debugger launch, and standalone publication. Output paths are value-owned, every stage reports a specific failure, and transient code cleanup is guaranteed on every exit path.
* **Pointer-Width-Independent References**: Machine-code fixups retain owned symbolic labels until the PE32 serialization boundary. Host pointers are never truncated into target reference fields, and symbol resolution validates offsets before committing executable metadata.
* **VFS & Authenticated Package V2**: Integrated Virtual File System with sidecar descriptors (`.dbpakref`) and SHA-256 payload verification (`PackageMount`).

---

## 🛠️ Prerequisites & Environment Setup

* **Operating System**: Windows 10 or Windows 11 (x64)
* **Compiler Toolchain**: Visual Studio 2026 (MSVC, default toolset with C++17 / C++20 support)
* **Build System**: CMake 4.2+ & MSBuild or Ninja
* **DirectX SDK**: Microsoft DirectX SDK (August 2007)
* **Testing Tools**: PowerShell 7+ and Pester 5+ (for language conformance tests)

---

## 🔨 Building the Project

### Available CMake Presets

The project includes pre-configured CMake presets in `CMakePresets.json`:

| Preset Name | Configuration | Purpose |
| :--- | :--- | :--- |
| **`windows-x86-debug`** | Debug | Default compatibility baseline with compiler and CTest coverage. |
| **`windows-x86-release`** | Release | Optimized release build with full runtime compiler. |
| **`windows-x86-asan`** | Debug | MSVC AddressSanitizer instrumented build (`DBP_ENABLE_ASAN=ON`). |
| **`windows-x86-ubsan`** | Debug | MSVC UndefinedBehaviorSanitizer instrumented build (`DBP_ENABLE_UBSAN=ON`). |
| **`windows-x86-coverage`** | Debug | Code coverage instrumented build (`DBP_ENABLE_COVERAGE=ON`). |
| **`windows-x86-clang-tidy`**| Debug | Static analysis build with `clang-tidy` integration (`DBP_ENABLE_CLANG_TIDY=ON`). |

#### Building with CMake Presets:

```powershell
# Configure Debug Preset
cmake --preset windows-x86-debug

# Build Targets
cmake --build --preset windows-x86-debug
```

For Release builds:

```powershell
cmake --preset windows-x86-release
cmake --build --preset windows-x86-release
```

#### Manual Build:

```powershell
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

### Build Targets:
* **`DBPCompiler`** (`DBPCompiler.exe`) — Main DarkBasic Pro compiler executable.
* **`DarkEXE`** (`DarkEXE.exe`) — Runtime executable runner stub embedded into output binaries.
* **`DBPDebugger`** (`DBPDebugger.exe`) — Official GUI debugger process.
* **`dbp_tests`** (`dbp_tests.exe`) — GoogleTest unit and integration test suite.

---

## 🧪 Testing & Quality Assurance

The codebase features comprehensive diagnostic gates and multi-layer test suites:

### 1. C++ Unit & Integration Test Suite (`dbp_tests.exe`)
Executes the C++ unit and integration suite covering AST parsing, IR lowering, target-ABI serialization and layout, memory protection, statement handling, and MachineCodeBuffer isolation:

```powershell
# Run via CTest:
ctest -C Debug --output-on-failure

# Or via CMake Test Preset:
ctest --preset windows-x86-debug

# Or execute directly:
.\build\bin\Debug\dbp_tests.exe
```

### 2. DarkBASIC Language Conformance Tests (`run-conformance.Tests.ps1`)
Golden tests that compile real `.dba` language samples using `DBPCompiler.exe` and verify stdout, exit codes, and assertions against expected directives (`REM EXPECT:`):

```powershell
$env:DBP_CONFORMANCE_COMPILER = ".\build\bin\Debug\DBPCompiler.exe"
$env:DBP_CONFORMANCE_RUNTIME_ROOT = ".\Install\Compiler"
Invoke-Pester -Path .\tests\conformance\run-conformance.Tests.ps1
```

### 3. Code Coverage & Sanitizers

Generate coverage reports or run memory/undefined-behavior sanitizers:

```powershell
# AddressSanitizer test run:
cmake --preset windows-x86-asan
cmake --build --preset windows-x86-asan
ctest --preset windows-x86-asan

# Coverage report generation:
cmake --preset windows-x86-coverage
cmake --build --preset windows-x86-coverage
ctest --preset windows-x86-coverage
.\scripts\coverage-report.ps1 -OutputFormat json
```

---

## 📜 Automation & Utility Scripts (`/scripts`)

The repository includes dedicated automation scripts for local CI/CD, coverage generation, and test validation:

| Script Path | Purpose | Example Usage |
| :--- | :--- | :--- |
| **`scripts/run-local-ci.ps1`** | Unified 5-phase CI/CD runner (Compile, C++Tests, Conformance, CompatibilityMatrix, FPSTests). | `.\scripts\run-local-ci.ps1 -Configuration Debug` |
| **`scripts/coverage-report.ps1`** | Aggregates MSVC `.cov` or GCC/Clang `.gcda` files and generates text/JSON code coverage reports. | `.\scripts\coverage-report.ps1 -OutputFormat json` |
| **`tests/conformance/run-conformance.Tests.ps1`** | Pester-based golden test suite for DarkBASIC language conformance. | `Invoke-Pester -Path tests/conformance/run-conformance.Tests.ps1` |
| **`tests/conformance/DirectiveParser.psm1`** | PowerShell module for parsing `REM EXPECT:` directives in conformance `.dba` files. | `Import-Module .\tests\conformance\DirectiveParser.psm1` |
| **`fuzz/run-corpus-smoke.ps1`** | Smoke test runner for validating compiler robustness against fuzzing corpora. | `.\fuzz\run-corpus-smoke.ps1` |

---

## 📦 Packaging & Installation (CPack)

The project configures CPack rules for generating distribution packages:

```powershell
cd build
cpack -C Release -G NSIS
cpack -C Release -G ZIP
```

### Package Components:
* **`Runtime`**: `DBPCompiler.exe`, `DarkEXE.exe`, `DBPDebugger.exe`, and core runtime DLLs.
* **`Data`**: Localized diagnostic files (`lang/`), help files, project templates, and plugins.
* **`Documentation`**: Architectural specs and language reference manuals.

---

## 📂 Repository Structure

```
Dark-Basic-Pro/
├── CMakeLists.txt              # Root CMake build configuration
├── CMakePresets.json           # MSVC build presets (Debug, Release, ASan, Coverage, Clang-Tidy)
├── DBProCompiler/
│   ├── DBPCompiler/            # Compiler core (ASMWriter, MachineCodeBuffer, Statement, AST)
│   ├── DBPCompilerEXE/         # CLI wrapper & runtime package bootstrap
│   └── DBPDebugger/           # Debugger interface application
├── Dark Basic Professional TEMP/# Synergy Editor TGC IDE
├── Dark Basic Public Shared/   # Shared SDK, Core runtime, and dynamic call assembly
├── Install/
│   └── Compiler/               # Tracked installation assets, templates, and runtime plugins
├── docs/                       # Architectural specs, modernization roadmaps, and baselines
├── fuzz/                       # Fuzzing test scripts and corpora
├── scripts/                    # Automation scripts (run-local-ci.ps1, coverage-report.ps1)
└── tests/                      # GoogleTest suite (dbp_tests) & Pester conformance tests
```

---

## 📄 License & Attribution

DarkBasic Pro is open-source software under the MIT License. See [LICENSE](LICENSE) for details. Third-party plug-ins and SDK components included in `Install/Compiler/plugins` are distributed with permission from their respective authors.
