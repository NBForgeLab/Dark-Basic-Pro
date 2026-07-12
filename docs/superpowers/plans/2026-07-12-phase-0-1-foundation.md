# Phase 0–1 Modernization Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Establish a reproducible x86 compatibility baseline and a clean, parallel, CTest-driven build foundation that can safely support the later Unicode, x64, compiler, runtime, and renderer migrations.

**Architecture:** Keep the current compiler and runtime behavior unchanged. Introduce target-scoped build policies, deterministic dependency declarations, shared CMake presets, registered tests, baseline evidence, and CI quality gates. Legacy compatibility flags apply only to legacy project targets; third-party and modern test infrastructure remain isolated.

**Tech Stack:** CMake 3.25+, Visual Studio 2022/MSVC x86, clang-cl where supported, CTest, GoogleTest, spdlog, PowerShell, GitHub Actions.

---

## Scope Boundaries

This plan does not modify CLI behavior, Unicode behavior, compiler semantics, AST selection, VFS, MemoryPE, runtime commands, rendering, or plugin loading. Those belong to later phases after the foundation is verified.

Tracked legacy redistributables under `Install/` and prebuilt third-party libraries are not removed indiscriminately. Phase 1 removes only proven build artifacts accidentally introduced by this modernization branch and records a later classification task for required redistributables.

Existing untracked user files and the untracked `Dark Basic Professional TEMP/` directory are not modified.

## File Structure

- Create `cmake/ProjectOptions.cmake`: target-scoped warning, compatibility, and sanitizer helpers.
- Create `CMakePresets.json`: shared configure, build, and test workflows.
- Create `docs/baselines/2026-07-12-x86-baseline.md`: reproducible baseline evidence and known limitations.
- Create `tests/CMakeLists.txt`: owns the test executable and CTest discovery.
- Create `.github/workflows/windows-x86.yml`: clean configure/build/test quality gate.
- Modify `CMakeLists.txt`: orchestration only; enable testing, declare dependencies, and add subdirectories.
- Modify `DBProCompiler/DBPCompiler/CMakeLists.txt`: apply project-owned target policies explicitly.
- Modify `DBProCompiler/DBPCompilerEXE/CMakeLists.txt`: apply project-owned target policies explicitly.
- Modify `.gitignore`: cover generated CMake, test, sanitizer, and IDE artifacts without hiding source assets.
- Remove the three tracked `.sbr` files introduced by commit `d6c63f0` after verifying they are compiler-generated browser databases.

## Task 1: Record the Existing Baseline

**Files:**
- Create: `docs/baselines/2026-07-12-x86-baseline.md`

- [ ] **Step 1: Record repository and toolchain identity**

Run:

```powershell
git rev-parse HEAD
git status --short --branch
cmake --version
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationVersion
```

Expected: exact commit, branch, CMake version, Visual Studio version, and the pre-existing untracked paths are captured without modifying them.

- [ ] **Step 2: Reproduce the current clean parallel-build failure**

Run:

```powershell
cmake --fresh -S . -B out/baseline-vs2022 -G "Visual Studio 17 2022" -A Win32
cmake --build out/baseline-vs2022 --config Debug --target dbp_tests --parallel 8
```

Expected RED: build fails with the existing shared-PDB `C1041` error, or the baseline document records the different observed failure exactly. This command is the regression test for Task 3.

- [ ] **Step 3: Confirm current CTest discovery failure**

Run:

```powershell
ctest --test-dir out/baseline-vs2022 -C Debug --show-only
```

Expected RED: `No tests were found` or zero discovered tests. This command is the regression test for Task 2.

- [ ] **Step 4: Capture the serial test baseline**

Build with compilation serialization only for baseline capture:

```powershell
cmake --build out/baseline-vs2022 --config Debug --target dbp_tests -- /m:1 /p:CL_MPCount=1
& .\out\baseline-vs2022\bin\Debug\dbp_tests.exe --gtest_color=no
```

Expected: record the exact build status, test count, passed/failed tests, duration, and warnings. Do not describe failures as new regressions.

- [ ] **Step 5: Inventory golden projects without executing external writes**

Run:

```powershell
rg --files Install -g '*.dbpro' -g '*.dba'
Test-Path '..\FPS-Creator-Classic'
git -C '..\FPS-Creator-Classic' rev-parse HEAD 2>$null
```

Expected: the baseline document lists official example projects and whether the sibling FPS Creator Classic checkout is available, including its commit when present.

- [ ] **Step 6: Write and self-check the baseline document**

The document must include commands, exit results, known warnings, test evidence, golden-project inventory, and explicit gaps. It must not claim FPS Creator compatibility until that build has actually been executed.

Run:

```powershell
rg -n 'TBD|TODO|PLACEHOLDER' docs/baselines/2026-07-12-x86-baseline.md
git diff --check -- docs/baselines/2026-07-12-x86-baseline.md
```

Expected: no placeholders and no whitespace errors.

- [ ] **Step 7: Commit the baseline evidence**

```powershell
git add docs/baselines/2026-07-12-x86-baseline.md
git commit -m "docs: record x86 modernization baseline"
```

## Task 2: Register Tests with CTest

**Files:**
- Create: `tests/CMakeLists.txt`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Preserve the RED evidence**

Run the Task 1 discovery command again and confirm zero tests before editing CMake.

- [ ] **Step 2: Move test target ownership into `tests/CMakeLists.txt`**

Use CMake's GoogleTest integration rather than a hand-maintained `add_test` list:

```cmake
add_executable(dbp_tests
    test_main.cpp
    test_logger.cpp
    test_vartable.cpp
    test_context.cpp
    test_str.cpp
    test_structural.cpp
    test_unicode.cpp
    test_raw_input.cpp
    test_ast.cpp
    test_diagnostics.cpp
    test_vfs.cpp
    test_cli.cpp
    "${PROJECT_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Input/CInputC.cpp"
    "${PROJECT_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Input/Controller.cpp"
    "${PROJECT_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/System/dynamic_call.asm"
)

target_link_libraries(dbp_tests PRIVATE
    dbp_compiler_lib
    GTest::gtest
    spdlog::spdlog
    xinput
)

target_compile_definitions(dbp_tests PRIVATE DBP_TESTS_COMPILATION)
target_include_directories(dbp_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/DBProCompiler/DBPCompiler"
    "${PROJECT_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Input"
)

if(MSVC)
    target_link_options(dbp_tests PRIVATE /SAFESEH:NO)
endif()

include(GoogleTest)
gtest_discover_tests(dbp_tests
    DISCOVERY_TIMEOUT 30
    PROPERTIES TIMEOUT 60
)
```

In the root file, add `include(CTest)` and `add_subdirectory(tests)` only when `BUILD_TESTING` is enabled.

- [ ] **Step 3: Verify GREEN test discovery**

Run:

```powershell
cmake --fresh -S . -B out/phase1-vs2022 -G "Visual Studio 17 2022" -A Win32 -DBUILD_TESTING=ON
cmake --build out/phase1-vs2022 --config Debug --target dbp_tests -- /m:1 /p:CL_MPCount=1
ctest --test-dir out/phase1-vs2022 -C Debug --show-only
ctest --test-dir out/phase1-vs2022 -C Debug --output-on-failure
```

Expected GREEN: CTest discovers all GoogleTest cases and reports zero failures.

- [ ] **Step 4: Commit test registration**

```powershell
git add CMakeLists.txt tests/CMakeLists.txt
git commit -m "test: register compiler suite with CTest"
```

## Task 3: Make Parallel MSVC Builds Reliable

**Files:**
- Create: `cmake/ProjectOptions.cmake`
- Modify: `CMakeLists.txt`
- Modify: `DBProCompiler/DBPCompiler/CMakeLists.txt`
- Modify: `DBProCompiler/DBPCompilerEXE/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Confirm the focused RED build**

Run:

```powershell
cmake --build out/phase1-vs2022 --config Debug --target dbp_tests --clean-first --parallel 8
```

Expected RED before the fix: `C1041` reports concurrent access to the target compilation PDB.

- [ ] **Step 2: Add target-scoped project helpers**

Create helpers with no directory-global compiler options:

```cmake
function(dbp_enable_parallel_msvc target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /FS)
    endif()
endfunction()

function(dbp_apply_legacy_cpp_options target)
    target_compile_features(${target} PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(${target} PRIVATE /EHsc /W3 /Zc:forScope-)
    endif()
    dbp_enable_parallel_msvc(${target})
endfunction()

function(dbp_apply_modern_cpp_options target)
    target_compile_features(${target} PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(${target} PRIVATE /EHsc /W4 /permissive- /FS)
    endif()
endfunction()
```

The foundation uses C++20 for new infrastructure because the currently selected GoogleTest baseline and legacy compiler interfaces must first be validated before enabling C++23 across toolchains. The architecture still requires C++23 for later new production targets.

- [ ] **Step 3: Apply helpers only to project-owned targets**

Apply the legacy helper to `dbp_compiler_lib`, `DBPCompiler`, and `DarkEXE`; apply the modern helper to `dbp_tests`. Remove root-level `add_compile_options`. Do not apply project flags to `spdlog` or GoogleTest.

- [ ] **Step 4: Verify GREEN parallel clean build repeatedly**

Run twice from clean target state:

```powershell
cmake --build out/phase1-vs2022 --config Debug --target dbp_tests --clean-first --parallel 8
cmake --build out/phase1-vs2022 --config Debug --target dbp_tests --clean-first --parallel 8
ctest --test-dir out/phase1-vs2022 -C Debug --output-on-failure
```

Expected GREEN: both parallel builds exit zero with no `C1041`; all tests pass.

- [ ] **Step 5: Verify dependency isolation**

Build with diagnostic output and confirm `/Zc:forScope-` is absent from spdlog and GoogleTest compile commands.

```powershell
cmake --build out/phase1-vs2022 --config Debug --target spdlog gtest --verbose
```

Expected: no project legacy compatibility flag appears in third-party compilation.

- [ ] **Step 6: Commit build policy isolation**

```powershell
git add CMakeLists.txt cmake/ProjectOptions.cmake DBProCompiler/DBPCompiler/CMakeLists.txt DBProCompiler/DBPCompilerEXE/CMakeLists.txt tests/CMakeLists.txt
git commit -m "build: isolate target policies and fix parallel PDB access"
```

## Task 4: Pin and Isolate Dependencies

**Files:**
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Record current dependency identities**

Run:

```powershell
git -C out/phase1-vs2022/_deps/spdlog-src rev-parse HEAD
git -C out/phase1-vs2022/_deps/googletest-src rev-parse HEAD 2>$null
```

Expected: immutable source revisions are captured before changing declarations.

- [ ] **Step 2: Select maintained compatible releases from primary sources**

Verify current stable releases and Windows/MSVC requirements using the official spdlog and GoogleTest repositories. Record the selected immutable commit SHA and license in CMake comments. Do not use a floating branch or mutable tag alone.

- [ ] **Step 3: Declare deterministic dependencies**

Use immutable Git commit SHAs with shallow progress disabled, or archive URLs with SHA-256 hashes. Set dependency options before materialization and keep their tests/examples disabled.

```cmake
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
```

- [ ] **Step 4: Verify a fresh dependency build**

Run:

```powershell
cmake --fresh -S . -B out/dependency-verification -G "Visual Studio 17 2022" -A Win32 -DBUILD_TESTING=ON
cmake --build out/dependency-verification --config Debug --target dbp_tests --parallel 8
ctest --test-dir out/dependency-verification -C Debug --output-on-failure
```

Expected: configure, build, and tests succeed using only pinned declarations.

- [ ] **Step 5: Commit dependency reproducibility**

```powershell
git add CMakeLists.txt
git commit -m "build: pin test and logging dependencies"
```

## Task 5: Add Shared CMake Presets

**Files:**
- Create: `CMakePresets.json`
- Modify: `.gitignore`

- [ ] **Step 1: Establish the RED preset command**

Run:

```powershell
cmake --preset windows-x86-debug
```

Expected RED: CMake reports that no preset exists.

- [ ] **Step 2: Add schema-valid shared presets**

Define hidden base presets and user-facing presets for configure, build, and test:

```json
{
  "version": 6,
  "cmakeMinimumRequired": { "major": 3, "minor": 25, "patch": 0 },
  "configurePresets": [
    {
      "name": "windows-x86-base",
      "hidden": true,
      "generator": "Visual Studio 17 2022",
      "architecture": "Win32",
      "binaryDir": "${sourceDir}/out/build/${presetName}",
      "cacheVariables": { "BUILD_TESTING": "ON" }
    },
    {
      "name": "windows-x86-debug",
      "inherits": "windows-x86-base",
      "displayName": "Windows x86 Debug"
    }
  ],
  "buildPresets": [
    {
      "name": "windows-x86-debug",
      "configurePreset": "windows-x86-debug",
      "configuration": "Debug",
      "jobs": 8
    }
  ],
  "testPresets": [
    {
      "name": "windows-x86-debug",
      "configurePreset": "windows-x86-debug",
      "configuration": "Debug",
      "output": { "outputOnFailure": true },
      "execution": { "noTestsAction": "error" }
    }
  ]
}
```

Add `CMakeUserPresets.json` and `/out/` to `.gitignore`. Do not ignore `CMakePresets.json`.

- [ ] **Step 3: Verify GREEN preset workflow**

Run:

```powershell
cmake --preset windows-x86-debug --fresh
cmake --build --preset windows-x86-debug --clean-first
ctest --preset windows-x86-debug
```

Expected GREEN: all commands exit zero and CTest fails if it discovers zero tests.

- [ ] **Step 4: Commit presets**

```powershell
git add CMakePresets.json .gitignore
git commit -m "build: add reproducible Windows x86 presets"
```

## Task 6: Remove Accidental Browser Database Artifacts

**Files:**
- Remove: `Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDK/System/Debug/CError.sbr`
- Remove: `Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDK/System/Debug/DLLMain.sbr`
- Remove: `Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDK/System/Debug/dxdiaginfo.sbr`
- Modify: `.gitignore`

- [ ] **Step 1: Prove the files are generated artifacts**

Run:

```powershell
git log --oneline -- "Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDK/System/Debug/*.sbr"
git check-ignore -v "Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDK/System/Debug/CError.sbr"
```

Expected: the files originate from the modernization branch's debug-artifact commit and have no source consumers.

- [ ] **Step 2: Add explicit ignore coverage**

Add:

```gitignore
# Visual C++ browser and incremental build databases
*.sbr
*.bsc
*.idb
*.ipdb
*.iobj
```

- [ ] **Step 3: Remove only the three confirmed generated files**

Do not remove required prebuilt libraries or redistributables in this task.

- [ ] **Step 4: Verify repository and build behavior**

```powershell
git diff --check
cmake --build --preset windows-x86-debug
ctest --preset windows-x86-debug
```

Expected: build and tests remain green and no `.sbr` file is tracked.

- [ ] **Step 5: Commit artifact cleanup**

```powershell
git add .gitignore
git add -u -- "Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDK/System/Debug"
git commit -m "chore: remove generated Visual C++ browser artifacts"
```

## Task 7: Add Windows x86 CI

**Files:**
- Create: `.github/workflows/windows-x86.yml`

- [ ] **Step 1: Add a workflow syntax validation test**

Use a locally available YAML parser or GitHub workflow linter. If neither exists, parse the YAML with the bundled workspace Python environment and PyYAML before committing. The first validation must fail while the file is absent.

- [ ] **Step 2: Create the quality-gate workflow**

The workflow must:

- trigger on pull requests and pushes to modernization branches;
- use a pinned major version of official checkout/cache actions;
- configure through `windows-x86-debug`;
- build with the build preset;
- run CTest with the test preset;
- run `git diff --check` against the checked-out tree where meaningful;
- upload test logs only on failure;
- avoid committing or publishing binaries.

Use PowerShell with `$ErrorActionPreference = 'Stop'` so failures propagate.

- [ ] **Step 3: Validate workflow structure locally**

Expected: YAML parses, required keys exist, preset names match exactly, and no secret or machine-specific path is embedded.

- [ ] **Step 4: Run the same commands locally**

```powershell
cmake --preset windows-x86-debug --fresh
cmake --build --preset windows-x86-debug --clean-first
ctest --preset windows-x86-debug
git diff --check
```

Expected: zero exit codes.

- [ ] **Step 5: Commit CI**

```powershell
git add .github/workflows/windows-x86.yml
git commit -m "ci: verify clean Windows x86 build and tests"
```

## Task 8: Final Phase 0–1 Verification and Documentation

**Files:**
- Modify: `docs/baselines/2026-07-12-x86-baseline.md`

- [ ] **Step 1: Run a fully fresh preset workflow**

Remove only the verified `out/build/windows-x86-debug` build directory after resolving and confirming its absolute path remains inside the repository, then run:

```powershell
cmake --preset windows-x86-debug
cmake --build --preset windows-x86-debug
ctest --preset windows-x86-debug
```

Expected: clean configure/build/test succeeds with the documented test count.

- [ ] **Step 2: Run quality checks**

```powershell
git diff --check
git status --short
git ls-files | rg '\.(sbr|pdb|obj|ilk|tlog)$'
```

Expected: no whitespace failures, no accidental build products, and only pre-existing explicitly preserved untracked user paths.

- [ ] **Step 3: Update baseline with after-state evidence**

Record exact commands, timestamps, test totals, warnings remaining in legacy code, known limitations, and whether FPS Creator Classic was built. Do not claim x64, Unicode, CLI, VFS, AST, or renderer work is complete.

- [ ] **Step 4: Verify the plan's requirements line by line**

Confirm:

- clean parallel build succeeds twice;
- CTest discovers and passes tests;
- legacy flags do not reach dependencies;
- presets reproduce the workflow;
- generated `.sbr` files are untracked;
- CI matches local commands;
- baseline evidence is accurate.

- [ ] **Step 5: Commit final evidence**

```powershell
git add docs/baselines/2026-07-12-x86-baseline.md
git commit -m "docs: verify modernization foundation"
```

## Deferred Work

The following work is explicitly deferred to the next approved plan:

- Removing production VFS hooks and MemoryPE.
- Removing the partial AST assignment path.
- Correcting CLI JSON and exit behavior.
- End-to-end Unicode conversion.
- Compiler ownership and session isolation.
- x64, LLVM, plugins, DirectX 11, audio, images, input, and Assimp.

This prevents Phase 0–1 commits from combining infrastructure changes with semantic or runtime behavior changes.
