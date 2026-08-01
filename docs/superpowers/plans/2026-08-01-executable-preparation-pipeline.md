# Executable Preparation Pipeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore the complete executable-preparation workflow behind a testable stage pipeline and remove the success-shaped `CPEBuilder` build stub.

**Architecture:** A platform-neutral coordinator executes named stages through `IExecutablePreparationServices`. A production adapter connects those stages to `CASMWriter`, focused debug and standalone helpers recover the historical behavior, and `CASMWriter::PrepareEXE` owns exactly one cleanup guard.

**Tech Stack:** C++17, GoogleTest, CMake/MSBuild, Windows PE32 runtime and packaging APIs.

---

### Task 1: Define pipeline contracts with request validation

**Files:**
- Create: `DBProCompiler/DBPCompiler/ExecutablePreparationPipeline.h`
- Create: `DBProCompiler/DBPCompiler/ExecutablePreparationPipeline.cpp`
- Create: `tests/test_executable_preparation_pipeline.cpp`
- Modify: `DBProCompiler/DBPCompiler/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] Write tests that pass null and empty output names and assert failure at `requestValidation` with zero service calls.
- [ ] Run the test build and verify RED because the pipeline header is missing.
- [ ] Define `ExecutableOutputMode { debug, standalone }`, named `ExecutablePreparationStage` values for every stage, immutable `ExecutablePreparationRequest`, and `ExecutablePreparationResult` with explicit boolean conversion.
- [ ] Define `IExecutablePreparationServices` with one non-throwing boolean operation per service-backed stage.
- [ ] Implement only request validation, run the two tests, and verify GREEN.
- [ ] Commit with `feat(compiler): define executable preparation pipeline`.

The required request-test shape is:

```cpp
RecordingPreparationServices services;
const auto result = ExecutablePreparationPipeline{}.Run(
    {nullptr, true, true, ExecutableOutputMode::standalone}, services);
EXPECT_FALSE(result);
EXPECT_EQ(result.failedStage,
          ExecutablePreparationStage::requestValidation);
EXPECT_TRUE(services.calls.empty());
```

### Task 2: Implement ordered orchestration

**Files:**
- Modify: `DBProCompiler/DBPCompiler/ExecutablePreparationPipeline.cpp`
- Modify: `tests/test_executable_preparation_pipeline.cpp`

- [ ] Write a failing test expecting this exact stage order for new code:

```cpp
using Stage = ExecutablePreparationStage;
const std::vector expected{
    Stage::targetValidation, Stage::machineCode, Stage::references,
    Stage::dllData, Stage::commandData, Stage::stringData,
    Stage::dataData, Stage::dynamicData, Stage::structurePatterns,
    Stage::runtimeValidation, Stage::spaceSizes,
    Stage::standalonePackaging};
```

- [ ] Run the single test and verify RED because only validation executes.
- [ ] Implement the exact sequence and stable stage-specific diagnostic strings.
- [ ] Run all pipeline tests and verify GREEN.
- [ ] Refactor repeated failure construction without changing behavior.
- [ ] Commit with `feat(compiler): orchestrate executable preparation stages`.

### Task 3: Conditional execution and failure boundaries

**Files:**
- Modify: `DBProCompiler/DBPCompiler/ExecutablePreparationPipeline.cpp`
- Modify: `tests/test_executable_preparation_pipeline.cpp`

- [ ] Write a failing no-new-code test expecting only target validation, runtime validation, size finalization, and the selected output stage.
- [ ] Add a parameterized test that makes each service-backed stage fail and asserts the call list ends at that stage.
- [ ] Run RED and confirm at least one case continues after failure.
- [ ] Guard materialization with `hasNewCode` and return immediately on the first failed service operation.
- [ ] Add tests proving debug and standalone are mutually exclusive.
- [ ] Run GREEN and commit with `test(compiler): cover preparation failure boundaries`.

### Task 4: Build the production materialization adapter

**Files:**
- Create: `DBProCompiler/DBPCompiler/ASMWriterPreparationServices.h`
- Create: `DBProCompiler/DBPCompiler/ASMWriterPreparationServices.cpp`
- Create: `tests/test_asmwriter_preparation_services.cpp`
- Modify: `DBProCompiler/DBPCompiler/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] Write a failing test that clears compiler globals and verifies target, runtime, and size stages return false without an SEH exception.
- [ ] Run RED because the adapter does not exist.
- [ ] Implement stage delegation:

```cpp
bool UpdateMachineCode() noexcept override {
    return writer_.UpdateMCB(writer_.GetCurrentMCPosition());
}
bool UpdateReferences() noexcept override { return writer_.UpdateMCBRefData(); }
bool UpdateDllData() noexcept override { return writer_.UpdateDLLData(); }
bool UpdateCommandData() noexcept override { return writer_.UpdateCommandData(); }
bool UpdateStringData() noexcept override { return writer_.UpdateStringData(); }
bool UpdateDataData() noexcept override { return writer_.UpdateDataData(); }
bool UpdateDynamicData() noexcept override { return writer_.UpdateDynamicData(); }
bool UpdateStructurePatterns() noexcept override {
    return writer_.UpdateStructurePatternData();
}
```

- [ ] Validate required collaborators before every global dereference and expose descriptive failure through the pipeline result.
- [ ] Run adapter and pipeline tests GREEN.
- [ ] Commit with `refactor(compiler): adapt asm writer to preparation pipeline`.

### Task 5: Recover debug execution as a focused component

**Files:**
- Create: `DBProCompiler/DBPCompiler/PreparedExecutableDebugger.h`
- Create: `DBProCompiler/DBPCompiler/PreparedExecutableDebugger.cpp`
- Create: `tests/test_prepared_executable_debugger.cpp`
- Modify: `DBProCompiler/DBPCompiler/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] Write RED characterization tests with a recording service: main mode initializes and runs once; mini mode initializes mini state, optionally runs new code from the mini offset, then resumes the main executable.
- [ ] Implement the platform-neutral debug dispatcher and make those tests GREEN.
- [ ] Recover the debugger branch from `CASMWriter::PrepareEXE` immediately before commit `4d7cc50`, moving it behind the production debug-service interface without semantic changes.
- [ ] Replace temporary owned arrays with `std::unique_ptr` and retain explicit pointer-width conversions.
- [ ] Run debugger, EXEBlock, and pipeline tests.
- [ ] Commit with `refactor(debugger): isolate prepared executable execution`.

### Task 6: Recover standalone packaging as a focused component

**Files:**
- Create: `DBProCompiler/DBPCompiler/PreparedExecutablePackager.h`
- Create: `DBProCompiler/DBPCompiler/PreparedExecutablePackager.cpp`
- Create: `tests/test_prepared_executable_packager.cpp`
- Modify: `DBProCompiler/DBPCompiler/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] Write RED preflight tests for an empty output path, missing runtime bundle, failed executable staging, failed resource customization, and failed publication.
- [ ] Implement a small coordinator over `IStandalonePackagingServices` and verify short-circuiting GREEN.
- [ ] Recover the standalone branch from historical `CASMWriter::PrepareEXE` immediately before commit `4d7cc50` into the production packaging service.
- [ ] Keep `CFileBuilder` responsible for authenticated staging, resource changes, and atomic publication; use `std::filesystem::path` at the new boundary.
- [ ] Run FileBuilder, package, runtime-bundle, and packager tests.
- [ ] Commit with `refactor(packager): isolate prepared executable publication`.

### Task 7: Route CASMWriter and remove false build APIs

**Files:**
- Modify: `DBProCompiler/DBPCompiler/ASMWriter.cpp`
- Modify: `DBProCompiler/DBPCompiler/PEBuilder.h`
- Modify: `DBProCompiler/DBPCompiler/PEBuilder.cpp`
- Modify: `tests/test_pe_builder_generation.cpp`
- Create: `tests/test_asmwriter_prepare_exe.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] Write a RED integration test: emit one instruction, force runtime validation failure, call `PrepareEXE`, and assert machine code reached `CEXEBlock` before the failure and transient code was released.
- [ ] Add a reusable C++17 scope guard and route `PrepareEXE` through the pipeline:

```cpp
const auto cleanup = MakeScopeExit([this] { FreeMachineBlock(); });
CASMWriterPreparationServices services(*this);
const ExecutablePreparationRequest request{
    outputFilename, parsingMainProgram, hasNewCode,
    g_DebugInfo.DebugModeOn()
        ? ExecutableOutputMode::debug
        : ExecutableOutputMode::standalone};
const auto result = ExecutablePreparationPipeline{}.Run(request, services);
if (!result) {
    services.ReportFailure(result);
    return false;
}
return true;
```

- [ ] Run GREEN and verify cleanup on success and every failure edge.
- [ ] Remove `CPEBuilder::BuildExecutable` and both `BuildEXEPackage` overloads; replace stub tests with truthful table and configuration tests.
- [ ] Search for and remove opaque `PrepareEXE : 1` through `: 8` diagnostics.
- [ ] Commit with `fix(compiler): restore executable preparation workflow`.

### Task 8: End-to-end verification and documentation

**Files:**
- Modify: `README.md`

- [ ] Compile an existing minimal `.dba` fixture into a temporary standalone executable.
- [ ] Verify the output exists, contains valid `MZ` and PE32 signatures, executes, and returns the fixture's expected result.
- [ ] Run focused tests:

```powershell
out\build\windows-x86-debug\bin\Debug\dbp_tests.exe --gtest_filter='ExecutablePreparation*:*PreparedExecutable*:ASMWriterPrepareEXE*:PEBuilder*:FileBuilder*'
```

- [ ] Run clean build and full verification:

```powershell
cmake --build --preset windows-x86-debug --clean-first
ctest --preset windows-x86-debug --output-on-failure
out\build\windows-x86-debug\bin\Debug\dbp_tests.exe --gtest_brief=1
git diff --check
```

- [ ] Confirm `rg -n "BuildEXEPackage|BuildExecutable|PrepareEXE' : [1-8]" DBProCompiler/DBPCompiler tests` returns no false build contracts or numeric stage diagnostics.
- [ ] Document the recovered pipeline and the subsequent PE32/PE32+ image-model phase.
- [ ] Commit with `docs(architecture): document executable preparation pipeline`.
