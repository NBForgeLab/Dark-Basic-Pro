# Executable Preparation Pipeline Recovery

## Status

Approved for implementation on 2026-08-01 under the standing authorization to
modernize the compiler without waiting for intermediate approval. This is a
correctness prerequisite for the target-neutral PE32/PE32+ image model.

## Problem

`CASMWriter::PrepareEXE` currently delegates to
`CPEBuilder::BuildEXEPackage`, but that function only validates three hard-coded
PE32 constants and a non-empty filename before returning `true`. It does not
prepare or package an executable.

Repository history shows that the former implementation also:

1. copied newly emitted machine code into `CEXEBlock`;
2. resolved machine-code references;
3. materialized DLL, command, string, DATA, dynamic-variable, and structure
   metadata;
4. validated the selected runtime bundle;
5. updated variable-space and DATA-space sizes;
6. ran the program through the debugger path or produced a standalone package;
7. released the transient machine-code buffer on every terminal path.

The extraction replaced this orchestration with a success-shaped stub. Existing
unit tests assert the stub rather than executable-preparation behavior, so a
large functional regression can pass the complete suite.

Building PE32+ traits on top of this contract would make the architecture more
polished while leaving its central operation incorrect.

## Goals

- Restore the complete executable-preparation behavior before extending PE
  formats.
- Represent the preparation workflow as an explicit, testable state machine.
- Make stage ordering, conditional execution, failure propagation, and cleanup
  deterministic.
- Keep `CASMWriter` focused on code emission rather than workflow control.
- Keep `CPEBuilder` focused on materializing executable data tables; it must not
  pretend that filename validation builds an executable.
- Preserve the active PE32 behavior and existing debug/standalone semantics.
- Create a clean seam for the subsequent PE32/PE32+ image configuration model.
- Add regression tests that fail against the current success-shaped stub.

## Non-goals

- Emitting PE32+ images or x64 machine code in this recovery phase.
- Redesigning the runtime or plug-in ABI.
- Rewriting every legacy detail of debugger launch, media discovery, icon
  conversion, or package publication at once.
- Changing language behavior or standalone package contents.
- Making the existing host-side `CEXEBlock` pointer arrays part of the target
  ABI.

## Considered approaches

### 1. Add PE traits to the current `CPEBuilder`

This is the smallest change, but it preserves the false
`BuildEXEPackage` contract and makes target selection depend on an operation
that does not build anything. Rejected.

### 2. Restore the historical monolithic `PrepareEXE`

This recovers behavior quickly, but restores a very large function with global
state, repeated cleanup, numeric stage errors, and mixed debugger/packager
responsibilities. It is useful as behavioral evidence, not as the desired
architecture. Rejected as the final design.

### 3. Explicit pipeline with a production adapter

The selected approach separates policy from legacy mechanisms. A small pipeline
coordinates named stages through an interface. A production adapter connects
those stages to `CASMWriter`, `CPEBuilder`, `CEXEBlock`, the debugger, and the
standalone packager. Tests exercise the pipeline using a deterministic in-memory
service implementation without mocking Windows APIs.

This approach restores behavior while providing the boundary needed for later
PE32/PE32+ selection.

## Architecture

### Preparation request

`ExecutablePreparationRequest` is an immutable value containing:

- the non-owning output filename;
- whether the request is for the main program or a mini-program;
- whether new machine code was emitted;
- the selected output mode (`debug` or `standalone`).

The caller decides the output mode explicitly. The pipeline does not read a
global debug flag.

### Named stages and errors

`ExecutablePreparationStage` names every externally visible stage:

- request validation;
- machine-code update;
- reference update;
- DLL data;
- command data;
- string data;
- DATA data;
- dynamic data;
- structure patterns;
- runtime validation;
- space-size finalization;
- debug execution;
- standalone packaging.

`ExecutablePreparationError` contains the failing stage and a stable diagnostic
message. Callers no longer report opaque errors such as `PrepareEXE : 6`.

Because the project uses C++17, the result is a small project-owned result type
rather than `std::expected`.

### Pipeline service boundary

`IExecutablePreparationServices` exposes one operation per stage. It contains
no Windows types and no compiler globals. The pipeline owns no compiler data and
performs no allocation beyond its result value.

The pipeline algorithm is:

1. validate the request;
2. when new code exists, execute all eight materialization stages in order;
3. validate the runtime bundle;
4. finalize variable-space and DATA-space sizes;
5. execute exactly one output path;
6. return the first failure with its named stage.

If a stage fails, later stages are not called.

### Cleanup ownership

Transient machine-code storage belongs to `CASMWriter`. Its production adapter
uses one scope guard around the complete pipeline call, ensuring
`FreeMachineBlock` runs exactly once on success or failure. The pure pipeline
does not own or release caller state.

This avoids both duplicated cleanup and hiding resource ownership inside a
generic coordinator.

### Production adapter

`CASMWriterPreparationServices` is the only layer allowed to bridge the pipeline
to legacy compiler state. Its responsibilities are narrow:

- compute the emitted byte count and call `CASMWriter::UpdateMCB`;
- delegate reference and table materialization to existing focused methods;
- validate the runtime through `CDBPCompiler`;
- update `CEXEBlock` space sizes from `CStatementList`;
- delegate debug execution and standalone packaging to dedicated helpers.

The adapter validates required global collaborators before dereferencing them.
Missing context becomes a named stage failure rather than an access violation.

### Debug and standalone outputs

The historical behavior is recovered behind two focused operations:

- `RunPreparedExecutableInDebugger` owns debugger discovery, launch, metadata
  transfer, main/mini execution, and runtime-error reporting.
- `PackagePreparedExecutable` owns EXB serialization, runtime/plugin/media
  collection, executable staging, resource customization, and atomic package
  publication.

The first implementation preserves behavior while moving the code without
semantic changes. Once characterization tests are green, each helper can be
decomposed further without changing the pipeline contract.

### Relationship to the PE image model

Request validation initially preserves the current PE32 constraints. The next
spec replaces those raw constants with a target-neutral `PeImageConfig` and
strong `Rva`, `ImageBase`, `FileOffset`, and alignment types. The pipeline then
receives a validated image configuration without changing stage orchestration.

## Error handling

- Empty or null output names fail at request validation.
- Missing compiler collaborators fail before any state mutation.
- The first failed stage is returned unchanged.
- Production diagnostics use stable stage names and descriptive messages.
- Debugger and packaging failures remain ordinary failures, not exceptions.
- Cleanup is unconditional and occurs exactly once.
- Pipeline and result operations are `noexcept` where their implementation is
  non-throwing.

## TDD strategy

Every production behavior starts with a failing test.

1. A regression test proves that a new-code request invokes machine-code and
   metadata stages instead of returning after filename validation.
2. A pipeline test verifies the exact new-code stage order.
3. A no-new-code test verifies materialization stages are skipped.
4. Parameterized failure tests verify short-circuiting and error stage identity.
5. Output-mode tests verify exactly one final output operation runs.
6. Request tests reject null and empty filenames before mutation.
7. Adapter tests verify missing global collaborators fail safely.
8. An integration test verifies `CASMWriter::PrepareEXE` releases transient code
   exactly once and preserves the pipeline result.

Tests use real pipeline code and a small recording service. Windows APIs are not
mocked; platform-specific output helpers retain existing integration coverage.

## Migration sequence

1. Add failing regression and pure pipeline tests.
2. Implement request, stage, error, result, and service contracts.
3. Implement the pure coordinator and make its focused tests green.
4. Add the production adapter with collaborator validation.
5. Recover debug and standalone behavior behind focused helper functions.
6. Replace `CPEBuilder::BuildEXEPackage` and `BuildExecutable` with truthful,
   narrowly named operations.
7. Route `CASMWriter::PrepareEXE` through the pipeline and one cleanup guard.
8. Run focused tests, clean build, full GoogleTest, and an end-to-end compiler
   smoke test that produces an executable.

## Quality gates

- Each new behavior has a witnessed RED test before production code.
- Pipeline tests cover every stage and every failure edge.
- No success-shaped stub remains in executable preparation.
- No opaque numeric preparation diagnostic remains.
- Null legacy collaborators cannot cause a crash in the new path.
- Transient machine code is released exactly once on every terminal path.
- Existing PE32 debug and standalone behavior is preserved.
- A compiled smoke program produces a valid PE32 executable.
- Win32 Debug clean build succeeds.
- Full CTest and GoogleTest suites pass.
- `git diff --check` passes and changed files add no warnings.

## Follow-on work

After this recovery is verified, implement the target-neutral PE image model:

1. strong address and offset types;
2. PE32 and PE32+ traits;
3. overflow-safe alignment and layout validation;
4. PE header serialization tests;
5. an x64 compiler-host preset that still targets PE32;
6. native x64 code generation and runtime ABI work.
