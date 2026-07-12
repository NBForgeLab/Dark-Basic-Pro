# Runtime ABI Safety Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prevent the compiler and generated executable bootstrap from accepting or invoking an incompatible DBPro runtime, including the FPS Creator `PassStructurePatterns` crash.

**Architecture:** Introduce small immutable runtime-contract types, a PE export inspector, a deterministic runtime-root resolver, and a typed core API resolver. Validate program requirements against verified runtime capabilities before packaging, then repeat required-pointer checks during executable startup as defense in depth.

**Tech Stack:** C++17, Win32 PE32, GoogleTest, CMake/CTest, MSVC Win32, AddressSanitizer, existing structured compiler diagnostics.

---

## File Structure

- Create `DBProCompiler/DBPCompiler/RuntimeContract.h`: capability, component, requirement, and diagnostic value types.
- Create `DBProCompiler/DBPCompiler/PeExportInspector.h/.cpp`: read-only PE32 architecture and export inspection.
- Create `DBProCompiler/DBPCompiler/RuntimeBundleResolver.h/.cpp`: canonical runtime selection and capability verification.
- Create `DBProCompiler/DBPCompiler/CoreRuntimeApi.h/.cpp`: typed resolution and validation of DBProCore bootstrap functions.
- Modify `DBProCompiler/DBPCompiler/DBPCompiler.h/.cpp`: own selected runtime and expose validated paths.
- Modify `DBProCompiler/DBPCompiler/ASMWriter.cpp`: package only from the resolved runtime bundle.
- Modify `DBProCompiler/DBPCompiler/EXEBlock.cpp`: resolve through `CoreRuntimeApi` and never invoke null pointers.
- Modify `DBProCompiler/DBPCompiler/Main.cpp`: parse `--runtime-root` and report selection errors.
- Modify CMake and Visual Studio project files to compile the new production files.
- Add focused tests in `tests/test_pe_export_inspector.cpp`, `tests/test_runtime_bundle.cpp`, and `tests/test_core_runtime_api.cpp`.

## Task 1: Runtime Contract Value Types

**Files:**
- Create: `DBProCompiler/DBPCompiler/RuntimeContract.h`
- Create: `tests/test_runtime_bundle.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing capability-set tests**

```cpp
#include <gtest/gtest.h>
#include "RuntimeContract.h"

TEST(RuntimeContractTest, ReportsOnlyMissingRequiredCapabilities) {
    RuntimeCapabilities available{
        RuntimeCapability::CoreBootstrapV1,
        RuntimeCapability::CoreDataStatementsV1};
    ProgramRuntimeRequirements required{
        RuntimeCapability::CoreBootstrapV1,
        RuntimeCapability::CoreStructurePatternsV1};

    EXPECT_EQ(MissingCapabilities(available, required),
              RuntimeCapabilities{RuntimeCapability::CoreStructurePatternsV1});
}

TEST(RuntimeContractTest, StructurePatternsAreRequiredOnlyForNonEmptyMetadata) {
    EXPECT_FALSE(DeriveProgramRuntimeRequirements(0).contains(
        RuntimeCapability::CoreStructurePatternsV1));
    EXPECT_TRUE(DeriveProgramRuntimeRequirements(1).contains(
        RuntimeCapability::CoreStructurePatternsV1));
}
```

- [ ] **Step 2: Run the focused test and verify RED**

Run:

```powershell
cmake --build build --config Debug --target dbp_tests
ctest --test-dir build -C Debug -R RuntimeContractTest --output-on-failure
```

Expected: compile failure because `RuntimeContract.h` and its types do not exist.

- [ ] **Step 3: Implement immutable contract types**

Define:

```cpp
enum class RuntimeCapability {
    CoreBootstrapV1,
    CoreDataStatementsV1,
    CoreStructurePatternsV1,
    CoreRuntimeErrorsV1
};

using RuntimeCapabilities = std::set<RuntimeCapability>;
using ProgramRuntimeRequirements = RuntimeCapabilities;

RuntimeCapabilities MissingCapabilities(
    const RuntimeCapabilities& available,
    const ProgramRuntimeRequirements& required);

ProgramRuntimeRequirements DeriveProgramRuntimeRequirements(
    std::uint32_t structurePatternCount);
```

Always require bootstrap, data-statements, and runtime-errors. Add structure-patterns only when `structurePatternCount > 0`.

- [ ] **Step 4: Run all contract tests and verify GREEN**

Expected: both `RuntimeContractTest` cases pass.

- [ ] **Step 5: Commit**

```powershell
git add DBProCompiler/DBPCompiler/RuntimeContract.h tests/test_runtime_bundle.cpp tests/CMakeLists.txt
git commit -m "feat: define DBPro runtime capability contract"
```

## Task 2: PE Export Inspection Without Loading Plugin Code

**Files:**
- Create: `DBProCompiler/DBPCompiler/PeExportInspector.h`
- Create: `DBProCompiler/DBPCompiler/PeExportInspector.cpp`
- Create: `tests/test_pe_export_inspector.cpp`
- Modify: `DBProCompiler/DBPCompiler/CMakeLists.txt`
- Modify: `DBProCompiler/DBPCompiler/DBPCompiler.vcxproj`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing tests against the two observed DBProCore generations**

```cpp
TEST(PeExportInspectorTest, DetectsModernStructurePatternsExport) {
    const auto result = PeExportInspector::Inspect(ModernCoreFixture());
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().machine, PeMachine::X86);
    EXPECT_TRUE(result.value().exports.contains(
        "?PassStructurePatterns@@YAXPAXK@Z"));
}

TEST(PeExportInspectorTest, IdentifiesLegacyCoreWithoutStructurePatterns) {
    const auto result = PeExportInspector::Inspect(LegacyCoreFixture());
    ASSERT_TRUE(result);
    EXPECT_FALSE(result.value().exports.contains(
        "?PassStructurePatterns@@YAXPAXK@Z"));
}

TEST(PeExportInspectorTest, RejectsTruncatedPeFile) {
    TemporaryBinary fixture("MZ");
    const auto result = PeExportInspector::Inspect(fixture.path());
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, PeInspectionErrorCode::MalformedImage);
}
```

Fixtures copy known test inputs into a per-test temporary directory. They never load or execute DLL code.

- [ ] **Step 2: Run and verify RED**

Expected: compile failure because `PeExportInspector` is undefined.

- [ ] **Step 3: Implement bounds-checked PE parsing**

Read the file into `std::vector<std::byte>` and validate, in order:

1. DOS header and `e_lfanew` bounds.
2. NT signature and `IMAGE_FILE_HEADER`.
3. PE32 optional header and x86 machine.
4. section table bounds.
5. export-directory RVA mapping.
6. export name-table, ordinal-table, and string bounds.

Return `PeImageInfo { PeMachine machine; std::set<std::string> exports; }`. Never use `LoadLibrary`, `DONT_RESOLVE_DLL_REFERENCES`, or plugin `DllMain` for inspection.

- [ ] **Step 4: Run focused tests and ASan**

```powershell
ctest --test-dir build -C Debug -R PeExportInspectorTest --output-on-failure
cmake --build build-asan --config Debug --target dbp_tests
ctest --test-dir build-asan -C Debug -R PeExportInspectorTest --output-on-failure
```

Expected: all valid, legacy, malformed, and truncation cases pass without sanitizer findings.

- [ ] **Step 5: Commit**

```powershell
git add DBProCompiler/DBPCompiler/PeExportInspector.* DBProCompiler/DBPCompiler/CMakeLists.txt DBProCompiler/DBPCompiler/DBPCompiler.vcxproj tests/test_pe_export_inspector.cpp tests/CMakeLists.txt
git commit -m "feat: inspect DBPro runtime PE exports safely"
```

## Task 3: Deterministic Runtime Bundle Resolution

**Files:**
- Create: `DBProCompiler/DBPCompiler/RuntimeBundleResolver.h`
- Create: `DBProCompiler/DBPCompiler/RuntimeBundleResolver.cpp`
- Modify: `tests/test_runtime_bundle.cpp`
- Modify build project files.

- [ ] **Step 1: Write failing path and capability tests**

```cpp
TEST(RuntimeBundleResolverTest, ExplicitRootWinsAndIsCanonical) {
    RuntimeSelection selection{explicitRoot, compilerDirectory};
    const auto result = RuntimeBundleResolver::Resolve(selection, requirements);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().root, std::filesystem::weakly_canonical(explicitRoot));
}

TEST(RuntimeBundleResolverTest, RejectsLegacyCoreWhenStructurePatternsRequired) {
    const auto result = RuntimeBundleResolver::Resolve(
        LegacyBundleFixture(),
        DeriveProgramRuntimeRequirements(1));
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, RuntimeErrorCode::MissingCapability);
    EXPECT_EQ(result.error().capability,
              RuntimeCapability::CoreStructurePatternsV1);
}

TEST(RuntimeBundleResolverTest, AcceptsLegacyCoreWhenMissingCapabilityIsUnused) {
    const auto result = RuntimeBundleResolver::Resolve(
        LegacyBundleFixture(),
        DeriveProgramRuntimeRequirements(0));
    ASSERT_TRUE(result);
}

TEST(RuntimeBundleResolverTest, RejectsComponentOutsideRuntimeRoot) {
    const auto result = RuntimeBundleResolver::Resolve(
        EscapingComponentFixture(), requirements);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, RuntimeErrorCode::PathEscapesRoot);
}
```

- [ ] **Step 2: Run and verify RED**

Expected: compile failure because bundle resolution is absent.

- [ ] **Step 3: Implement explicit/default resolution and legacy probing**

Define:

```cpp
struct RuntimeSelection {
    std::optional<std::filesystem::path> explicitRoot;
    std::filesystem::path compilerDirectory;
};

struct ResolvedRuntimeBundle {
    std::filesystem::path root;
    std::filesystem::path pluginsDirectory;
    std::filesystem::path corePath;
    RuntimeCapabilities capabilities;
    bool versioned;
};
```

Default to the `runtime` sibling of the compiler's `bin` directory (`<compiler-directory>/../runtime`), not the project directory. Until the bundle-publication phase creates that directory, preserve the compiler-adjacent legacy layout as a temporary, explicitly reported `legacy-unversioned` migration fallback and derive its capabilities from verified exports. This fallback remains subject to all validation and is removed by the bundle-publication plan. Containment checks use canonical paths and component-wise path comparison, not string prefixes.

- [ ] **Step 4: Verify focused tests and full suite**

Expected: resolver tests and the existing 70 tests pass.

- [ ] **Step 5: Commit**

```powershell
git add DBProCompiler/DBPCompiler/RuntimeBundleResolver.* DBProCompiler/DBPCompiler/CMakeLists.txt DBProCompiler/DBPCompiler/DBPCompiler.vcxproj tests/test_runtime_bundle.cpp
git commit -m "feat: resolve and validate DBPro runtime bundles"
```

## Task 4: Typed Core Runtime API and Null-call Elimination

**Files:**
- Create: `DBProCompiler/DBPCompiler/CoreRuntimeApi.h`
- Create: `DBProCompiler/DBPCompiler/CoreRuntimeApi.cpp`
- Create: `tests/test_core_runtime_api.cpp`
- Modify: `DBProCompiler/DBPCompiler/EXEBlock.cpp`
- Modify build project files and `tests/CMakeLists.txt`.

- [ ] **Step 1: Write failing resolver tests with an injected symbol lookup**

```cpp
TEST(CoreRuntimeApiTest, RejectsMissingRequiredBootstrapFunction) {
    FakeSymbolLookup symbols = ModernCoreSymbols();
    symbols.erase("?PassErrorHandlerPtr@@YAXPAX@Z");
    const auto result = ResolveCoreRuntimeApi(symbols, requirements);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, CoreApiErrorCode::MissingRequiredExport);
}

TEST(CoreRuntimeApiTest, OmitsUnusedLegacyStructureFunctionSafely) {
    FakeSymbolLookup symbols = LegacyCoreSymbols();
    const auto result = ResolveCoreRuntimeApi(
        symbols, DeriveProgramRuntimeRequirements(0));
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().passStructurePatterns, nullptr);
}

TEST(CoreRuntimeApiTest, RejectsRequiredLegacyStructureFunction) {
    const auto result = ResolveCoreRuntimeApi(
        LegacyCoreSymbols(), DeriveProgramRuntimeRequirements(3));
    ASSERT_FALSE(result);
}
```

- [ ] **Step 2: Run and verify RED**

Expected: compile failure because the typed resolver is absent.

- [ ] **Step 3: Implement the typed API object**

`CoreRuntimeApi` owns typed pointers for command-line, error, escape, breakout, structure-pattern, data-statement, DLL-construction, variable-space, and display bootstrap functions. `ResolveCoreRuntimeApi` accepts a lookup callable so unit tests require no DLL loading.

All required lookups are collected into one diagnostic result. The optional structure-pattern pointer is invoked only through:

```cpp
if (requirements.contains(RuntimeCapability::CoreStructurePatternsV1)) {
    coreApi.passStructurePatterns(patterns, count);
}
```

- [ ] **Step 4: Replace raw startup resolution and calls in `EXEBlock.cpp`**

Remove direct initialization/calls for the covered globals. On resolution failure, set `pReturnError`, set `bResult=false`, and stop before any core bootstrap callback. Do not continue with a partially populated API.

- [ ] **Step 5: Run tests and reproduce the original failure as a controlled error**

Expected:

- resolver unit tests pass;
- a fixture executable with a legacy core and structure metadata exits nonzero with a compatibility message;
- no Windows `BEX`/`0xC0000005` event is produced.

- [ ] **Step 6: Commit**

```powershell
git add DBProCompiler/DBPCompiler/CoreRuntimeApi.* DBProCompiler/DBPCompiler/EXEBlock.cpp DBProCompiler/DBPCompiler/CMakeLists.txt DBProCompiler/DBPCompiler/DBPCompiler.vcxproj tests/test_core_runtime_api.cpp tests/CMakeLists.txt
git commit -m "fix: reject incompatible DBPro core APIs safely"
```

## Task 5: CLI Runtime Selection and Compiler Ownership

**Files:**
- Modify: `DBProCompiler/DBPCompiler/Main.cpp`
- Modify: `DBProCompiler/DBPCompiler/DBPCompiler.h`
- Modify: `DBProCompiler/DBPCompiler/DBPCompiler.cpp`
- Modify: `DBProCompiler/DBPCompiler/ASMWriter.cpp`
- Modify: `tests/test_cli.cpp`
- Modify: `tests/test_runtime_bundle.cpp`

- [ ] **Step 1: Write failing CLI tests**

```cpp
TEST(CliTest, AcceptsOneRuntimeRoot) {
    const auto options = ParseCompilerArguments(
        {"DBPCompiler.exe", "--runtime-root", "D:/runtime", "Game.dbpro"});
    ASSERT_TRUE(options);
    EXPECT_EQ(options.value().runtimeRoot, "D:/runtime");
}

TEST(CliTest, RejectsDuplicateOrMissingRuntimeRootValue) {
    EXPECT_FALSE(ParseCompilerArguments(
        {"DBPCompiler.exe", "--runtime-root", "Game.dbpro"}));
    EXPECT_FALSE(ParseCompilerArguments({"DBPCompiler.exe", "--runtime-root",
        "A", "--runtime-root", "B", "Game.dbpro"}));
}
```

- [ ] **Step 2: Run and verify RED**

Expected: argument tests fail because the option is unknown.

- [ ] **Step 3: Parse and store runtime selection**

Extend the existing argument value object rather than adding another global. `CDBPCompiler` owns one `ResolvedRuntimeBundle` for a compilation. Report the canonical selected root and classification (`versioned` or `legacy-unversioned`) in the JSON `initialization` event.

- [ ] **Step 4: Make packaging consume the validated bundle**

Replace independent construction of `plugins`, `plugins-user`, `plugins-licensed`, and `effects` paths in `ASMWriter.cpp` with paths from `ResolvedRuntimeBundle`. Packaging must not fall back to the compiler-adjacent legacy folders after validation.

- [ ] **Step 5: Map runtime failures to structured diagnostics**

Emit `DBP3001`-`DBP3006` with stage `runtime-validation`, component/capability context, and nonzero process exit. No message box is used in CLI operation.

- [ ] **Step 6: Run focused and full tests**

```powershell
ctest --test-dir build -C Debug -R "CliTest|RuntimeBundleResolverTest" --output-on-failure
ctest --test-dir build -C Debug --output-on-failure
```

- [ ] **Step 7: Commit**

```powershell
git add DBProCompiler/DBPCompiler/Main.cpp DBProCompiler/DBPCompiler/DBPCompiler.h DBProCompiler/DBPCompiler/DBPCompiler.cpp DBProCompiler/DBPCompiler/ASMWriter.cpp tests/test_cli.cpp tests/test_runtime_bundle.cpp
git commit -m "feat: select validated DBPro runtime explicitly"
```

## Task 6: Regression Test for the FPSC Crash Boundary

**Files:**
- Create: `tests/test_runtime_compatibility.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `docs/fps-creator-classic-standalone-validation.md`

- [ ] **Step 1: Add the exact regression case**

Build or stage two deterministic core fixtures:

- legacy core: all baseline bootstrap exports, no `PassStructurePatterns`;
- modern core: same exports plus `PassStructurePatterns`.

Test the matrix:

| Program metadata | Legacy core | Modern core |
|---|---:|---:|
| zero structure patterns | accepted | accepted |
| nonzero structure patterns | `DBP3004` | accepted |

- [ ] **Step 2: Verify the regression test fails before final wiring**

Expected: at least one matrix case fails until compiler requirements are passed through packaging and bootstrap resolution.

- [ ] **Step 3: Complete only the missing wiring**

Pass `m_dwUsertypeStringPatternQuantity` into `DeriveProgramRuntimeRequirements`, persist the verified requirements in the EXB/runtime initialization data, and use the same value for compile-time and startup checks.

- [ ] **Step 4: Run Debug and ASan suites**

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
cmake --build build-asan --config Debug
ctest --test-dir build-asan -C Debug --output-on-failure
```

Expected: all tests pass and no sanitizer finding occurs.

- [ ] **Step 5: Run a staged FPSC Map Editor validation**

Compile Map Editor with an explicit isolated runtime root. First confirm the old FPSC root fails with `DBP3004` before EXE output replacement. Then compile against the current official runtime and launch through `FPSCreator.exe`. Acceptance requires the Map Editor IPC handshake and absence of a new Application Error event.

- [ ] **Step 6: Document evidence and commit**

```powershell
git add tests/test_runtime_compatibility.cpp tests/CMakeLists.txt docs/fps-creator-classic-standalone-validation.md
git commit -m "test: cover DBPro runtime ABI compatibility"
```

## Task 7: Phase Verification and Next-plan Gate

**Files:**
- Modify only documentation if verification discovers a documentation mismatch.

- [ ] **Step 1: Run repository verification**

```powershell
git diff --check
cmake --build build --config Release --target DBPCompiler
ctest --test-dir build -C Debug --output-on-failure
ctest --test-dir build-asan -C Debug --output-on-failure
```

- [ ] **Step 2: Run CLI compatibility checks**

Verify direct `.dba`, `.dbpro`, `--emit-final-source`, `--legacy-final-source`, argument conflicts, invalid runtime root, legacy runtime rejection, and modern runtime success.

- [ ] **Step 3: Record remaining scope**

Create the subsequent plan for generated `runtime-manifest.json`, atomic bundle publication, official DLL build orchestration, third-party FPSC plugin inventory, and complete Game/Screens lifecycle tests. Do not fold those independent deployment changes into this safety phase.

- [ ] **Step 4: Request code review and commit any documentation correction**

Expected: no unresolved critical or important review findings before beginning the bundle-publication phase.
