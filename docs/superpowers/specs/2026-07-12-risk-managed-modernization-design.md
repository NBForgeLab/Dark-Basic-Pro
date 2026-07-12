# Risk-Managed DarkBASIC Professional Modernization Design

## Status and Decision

This document is the governing design for the active DarkBASIC Professional modernization effort. The project will be modernized incrementally in one repository and will retain one production compiler/runtime path at every release point.

Compatibility means source and observable behavioral compatibility for existing `.dbpro` and `.dba` projects, especially FPS Creator Classic. It does not require preserving obsolete internal ABIs, 32-bit DLL binaries, DirectX 9, manual PE loading, Windows API hooks, serialized pointers, or unsafe implementation details.

The clean-break vNext alternative remains documented separately in `docs/future_vnext_clean_architecture.md` for possible future use.

## Product Goals

1. Preserve the ability to build and run representative DarkBASIC Professional and FPS Creator Classic projects throughout the migration.
2. Support Windows 11 and native x64.
3. Support Unicode end to end, including Arabic paths, source files, diagnostics, titles, and asset names.
4. Replace the legacy compiler internals with explicit, testable stages without silently changing language semantics.
5. Replace DirectX 9 with DirectX 11 first; evaluate DirectX 12 only after the DX11 migration is stable and a measured requirement justifies the added complexity.
6. Replace obsolete or 32-bit-only image, audio, input, physics, and asset-import dependencies with maintained x64-capable alternatives behind project-owned interfaces.
7. Make every migration stage independently reviewable, reversible, testable, and accurately documented.

## Compatibility Contract

The project preserves where practical:

- `.dbpro` project format and `.dba` source format.
- DarkBASIC command names, parameter behavior, return values, side effects, errors, and documented quirks.
- Expression precedence, conversions, scopes, suffixes, arrays, user-defined types, control flow, and include behavior.
- Asset and project settings needed by existing projects.
- Observable behavior required by FPS Creator Classic.

The project may intentionally break:

- Binary compatibility with 32-bit plugins.
- Internal compiler/runtime ABI and data layouts.
- Pointer-sized serialization and undocumented reliance on memory addresses.
- DirectX 9, D3DX, DirectInput, and DirectPlay integration.
- Unsafe packaging, temporary-file extraction, manual PE loading, and Windows API hooking.

Every intentional behavioral difference requires a compatibility classification, a regression or conformance test, documentation, and a migration note.

## Migration Model

There is always one user-facing production path. Low-risk changes are made directly after characterization tests. High-risk replacements are developed beside the legacy implementation only within tests or an explicitly disabled experimental target.

```text
production source -> approved implementation -> production artifact

conformance input -> legacy reference result
                 -> candidate result
                 -> normalized differential comparison
```

After the candidate proves parity, it replaces the production implementation and the superseded path is removed. Permanent dual compilers, renderers, or runtimes are prohibited.

### Directly Modernized Areas

- CMake, CTest, presets, and continuous integration.
- Compiler warnings and deterministic build configuration.
- CLI exit codes and structured output.
- Logging separation.
- Unicode conversion boundaries.
- RAII, ownership, resource cleanup, and pointer-size-safe types.
- Repository artifact hygiene.
- Dependency pinning and reproducibility.

### Differentially Replaced Areas

- Lexer, parser, AST, semantic analysis, and type system.
- Code generation and native x64 backend.
- Runtime command dispatch and plugin API.
- DirectX 11 renderer.
- Asset importing, packaging, physics, and animation.

## Testing Architecture

### Characterization Tests

Before changing a legacy component, tests capture behavior relied upon by existing projects. Characterization covers case-insensitive names, implicit declarations, suffixes, overflow, numeric conversions, scopes, includes, command discovery, errors, and resource lifetimes.

Characterization describes existing behavior; it does not automatically declare every legacy bug desirable. A discovered bug must be classified before its behavior changes.

### TDD Regression Tests

Every bug fix and behavioral change follows strict red-green-refactor:

1. Add one minimal test reproducing the defect.
2. Run it and confirm it fails for the expected reason.
3. Make the smallest coherent production change that satisfies the behavior.
4. Run the focused test and the complete relevant suite.
5. Refactor only while tests remain green.

Production code is not written before the failing test. Documentation-only and generated-file changes are the only routine exceptions.

### Language Conformance Corpus

Small `.dba` programs test exactly one language behavior and declare expected compilation status, diagnostics, runtime output, exit code, and artifacts.

```text
tests/conformance/
  expressions/
  variables/
  arrays/
  functions/
  types/
  strings/
  control_flow/
  includes/
  errors/
  runtime_commands/
```

### Golden Projects

The golden suite includes FPS Creator Classic, official examples under `Install/Projects`, representative plugins, old games where licensing permits, and projects containing Arabic and other non-ASCII paths and content.

The suite records build status, diagnostics, runtime output, artifacts, structured logs, performance budgets, and visual results where applicable. Renderer comparison uses documented tolerances and image metrics rather than requiring byte-identical screenshots.

### Differential, Metamorphic, and Fuzz Testing

Differential tests compare tokens, ASTs, symbol tables, typed IR, diagnostics, runtime output, or rendering at the appropriate abstraction level. They do not require identical machine code.

Metamorphic tests cover case changes, whitespace/comments, Unicode project relocation, independent declaration ordering, and deterministic repeated compilation.

Fuzz targets cover source decoding, lexer, parser, project reader, diagnostic source mapping, package/archive reader, maintained image decoders, and model-import boundaries. Fuzzing checks memory safety, bounded allocation, termination, recursion depth, malformed UTF-8, and malformed assets.

## Quality Gates

A stage cannot be merged until it has:

1. Clean configure from an empty build directory.
2. Clean supported-architecture build.
3. Registered and passing CTest tests.
4. Passing relevant conformance and golden-project tests.
5. Passing sanitizer configuration where supported.
6. Passing static analysis for changed modern code.
7. No new compiler warnings; new targets use warnings as errors.
8. Clean `git diff --check`.
9. Documented compatibility impact and intentional differences.
10. A verified disable or rollback mechanism while the legacy reference remains.

CI uses reproducible CMake Presets and tests MSVC and clang-cl where compatible. New code targets C++23. Legacy targets keep only the minimum compatibility flags they need; legacy flags never leak into dependencies or modern targets.

Dependencies use immutable versions and integrity verification. Binary build products, `.sbr` files, PDBs, caches, and generated temporary files are excluded from normal source commits and published as release artifacts when required.

## Phased Delivery

### Phase 0: Establish the Baseline

- Record toolchain, SDK, architecture, configuration, and dependency requirements.
- Build the current x86 product from a clean checkout.
- Run the current test executable and record known warnings and failures.
- Build representative DBPro examples and FPS Creator Classic.
- Create an initial compatibility manifest and golden-project inventory.
- Record the exact baseline commit and preserve machine-readable results.

No large refactor begins until this evidence exists.

### Phase 1: Build and Test Foundation

- Fix clean parallel builds and PDB ownership correctly.
- Enable CTest and register every automated test.
- Add shared configure, build, test, and sanitizer presets.
- Isolate legacy compiler options from GoogleTest, spdlog, and modern targets.
- Add deterministic CI for the supported baseline.
- Add static-analysis entry points.
- Remove tracked generated artifacts and define release-artifact handling.
- Pin dependencies with immutable revisions and hashes.

### Phase 2: Remove Unsafe Experiments and Repair CLI

- Remove VFS kernel hooks and manual MemoryPE loading from production.
- Restore safe file extraction temporarily behind RAII and bounded parsing until a tested package service replaces it.
- Remove the partial assignment AST fast path from production.
- Keep the legacy parser production path until an integrated replacement proves parity.
- Define a versioned JSON diagnostic/status schema.
- Guarantee JSON-only stdout in JSON mode, logs on stderr or file, correct Windows argument parsing, and reliable nonzero failure exit codes.

### Phase 3: Complete Unicode Boundaries

- Adopt UTF-8 for compiler/runtime text and modern source representation.
- Use UTF-16 only at Windows boundaries.
- Centralize checked conversions with explicit errors.
- Use wide process entry/argument APIs and `std::filesystem::path`.
- Define legacy source-encoding detection and migration behavior.
- Test Arabic, non-BMP, combining characters, invalid encodings, and Unicode assets.

### Phase 4: Memory Safety and Ownership

- Introduce RAII wrappers for Windows handles, modules, mappings, and resources.
- Replace fixed buffers and unchecked arithmetic with bounded containers and spans.
- Express ownership with values and smart pointers.
- Remove mismatched allocation/deallocation and ownership-obscuring `const_cast` usage.
- Make binary readers validate every size, offset, integer operation, and allocation.

Changes remain small and behaviorally characterized; this phase is not an automated mass rewrite.

### Phase 5: Compiler Session Isolation

- Replace global mutable compiler state with an explicitly owned `CompilerSession`.
- Pass dependencies explicitly instead of rebinding globals through a context facade.
- Give every resource one owner and one lifecycle.
- Support clean repeated compilations and ensure failure state cannot contaminate the next session.
- Prepare, but do not prematurely promise, safe parallel compilation.

### Phase 6: Source Management and Diagnostics

- Add immutable source buffers, `SourceId`, byte offsets, source spans, line indices, and include/source maps.
- Calculate Unicode-aware display columns separately from byte offsets.
- Separate diagnostic data from terminal formatting, JSON serialization, logging, and editor transport.
- Give diagnostic kinds stable identifiers and document the JSON schema.

### Phase 7: Integrated Frontend Replacement

- Specify and test lexical grammar.
- Build a complete parser producing an AST.
- Implement symbols, scopes, semantic analysis, type checking, and typed AST or language IR.
- Compare the candidate and legacy frontend across the conformance and golden corpora.
- Switch production only as a coherent frontend, never as syntax-specific fast paths.

### Phase 8: Native x64 Backend

- Eliminate pointers stored in `DWORD`, pointer/integer truncation, x86 inline assembly, implicit calling conventions, undocumented packing, and serialized pointers.
- Document language and runtime data layouts and calling conventions.
- Prefer an LLVM ahead-of-time backend unless a later evidence-based prototype demonstrates a lower-risk maintainable alternative.
- Use x86 only as a temporary differential reference; x64 becomes the sole product after parity.

### Phase 9: Runtime and Plugins

- Define versioned x64-safe runtime interfaces with explicit ownership, errors, capabilities, and ABI negotiation.
- Preserve DarkBASIC command names and observable semantics while replacing implementations.
- Rebuild source-available plugins for x64 or replace their functionality with maintained libraries.
- Use an out-of-process adapter only as a temporary, isolated fallback for indispensable binary-only plugins.
- Prohibit manual PE mapping and writable-executable module images.

### Phase 10: DirectX 11 Renderer

```text
characterize DX9 behavior
  -> project-owned renderer interface
  -> DirectX 11 candidate
  -> visual and behavioral parity tests
  -> DirectX 11 production path
  -> remove DirectX 9
  -> evaluate DirectX 12 only from measured requirements
```

The renderer interface defines coordinate conventions, resource lifetime, state behavior, device loss, resizing, color spaces, and synchronization. DX12 is not selected merely because it is newer.

### Phase 11: Assets, Images, Audio, and Input

- Put each external library behind a project-owned interface.
- Use Assimp where its supported formats and license fit, followed by conversion to a documented internal asset format.
- Normalize transforms, units, coordinates, materials, skeletons, and animations during import.
- Use maintained bounded image decoding with explicit pixel formats and sRGB/linear rules.
- Use XAudio2 for mixing and a maintained decoding layer such as Media Foundation or suitable codec libraries.
- Replace DirectInput with a unified keyboard, mouse, and controller service supporting state/events, hot-plugging, mapping, and dead zones.

## Compatibility Manifest

Every DarkBASIC command progressively receives a manifest entry:

```text
command name
parameter and return types
error behavior
side effects
thread expectations
resource ownership
legacy quirks
relevant tests
modern implementation status
intentional differences and migration notes
```

Differences are classified as unintended regressions, documented legacy bug fixes, required x64/security changes, or previously undefined behavior assigned a new specification.

## Commit and Review Discipline

Each implementation commit addresses one responsibility, includes its focused test, builds independently, documents compatibility impact, and remains revertible. Formatting-only changes are separate from behavioral changes. Generated binaries and unrelated cleanup are excluded.

Preferred progression:

```text
test: characterize legacy string comparison
refactor: isolate string comparison service
fix: preserve case-insensitive Unicode comparison
```

Commit messages and documentation must describe implemented reality. Terms such as “complete modernization” are not used for partial scaffolding or experimental paths.

## Definition of Done

A phase is complete only when its requirements and compatibility impact are documented; every production behavior has a test observed failing before implementation; focused and complete suites pass; clean build and CTest pass; golden projects pass; sanitizers and applicable analysis pass; no new warnings or whitespace errors remain; intentional differences have migration notes; and any fully replaced legacy path has been deleted.

Passing unit tests alone is not completion evidence.
