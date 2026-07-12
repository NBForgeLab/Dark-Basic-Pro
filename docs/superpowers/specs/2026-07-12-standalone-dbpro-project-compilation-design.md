# Standalone DBPro Project Compilation Design

## Status

Approved on 2026-07-12. This specification defines
the implementation boundary for making `DBPCompiler.exe <project.dbpro>` the
authoritative standalone build path while preserving legacy DBPro project
compatibility.

## Problem Statement

The current compiler reads the `final source` field from a `.dbpro` file and
assumes an editor has already concatenated `main` and the ordered `includeN`
entries into that file. Consequently, invoking the compiler directly can build
stale or incomplete source. FPS Creator Classic exposes this defect because its
active targets share `_Temp.dbsource` and normally depend on Synergy Editor to
regenerate it before each build.

A separate defect exists in the experimental AST path. Simple assignments emit
machine code during parsing, before `CASMWriter::CreateASMHeader()` initializes
the machine-code buffer. This violates stage ordering and causes an access
violation on valid FPS Creator source such as `gloadreportstate=0`.

The solution must remove both hidden prerequisites. It must not require changes
to FPS Creator's `.dbpro` or `.dba` files.

## Goals

- Make `DBPCompiler.exe [options] <project.dbpro>` a complete standalone build.
- Preserve source order: `main`, followed by contiguous `include1..includeN`.
- Resolve source and output paths relative to the project file, independent of
  the process current working directory.
- Compile from an owned in-memory source representation.
- Retain `final source` as an explicit compatibility/artifact facility, not an
  implicit prerequisite for normal builds.
- Separate project loading, source assembly, parsing, semantic processing, and
  code generation into observable stages.
- Prevent code emission before a code-generation session is initialized.
- Produce actionable human-readable and JSON diagnostics.
- Prove compatibility by building the three active FPS Creator DBPro targets.

## Non-Goals

- Redesigning the `.dbpro` format.
- Rewriting the complete legacy parser or x86 backend.
- Changing DBPro language semantics, source encodings, or include ordering.
- Converting FPS Creator source files to a new format.
- Making the compiler depend on Synergy Editor libraries or behavior at runtime.
- Introducing a permanent second compiler pipeline.

## Architectural Decision

Project preparation becomes a first-class compiler front-end stage. A `.dbpro`
manifest is parsed into a typed model, its ordered source set is validated and
assembled into owned memory, and that immutable compilation input is passed to
the existing language pipeline. The compiler no longer discovers its primary
input by reading a mutable shared `_Temp.dbsource` file.

The compiler continues to accept a direct `.dba` source path. Both inputs
converge on the same `CompilationInput` abstraction before parsing.

```text
.dbpro -> ProjectManifestReader -> SourceAssembler --+
                                                     +-> CompilationInput
.dba   -> DirectSourceLoader ------------------------+          |
                                                                v
                                      parse -> analyze -> initialize backend -> emit
```

## Components

### ProjectManifest

A value type containing:

- absolute normalized project path;
- project directory;
- required main source path;
- ordered include paths;
- optional final-source artifact path;
- executable path and existing compiler options needed downstream.

It owns strings and paths. It does not retain pointers into the raw project
buffer.

### ProjectManifestReader

Responsibilities:

- parse legacy `key=value` project data without changing its observable
  case-insensitive key behavior;
- require a non-empty `main` field for standalone project assembly;
- discover `include1..includeN` in numeric order;
- reject gaps, duplicate indices, duplicate `main`, and malformed include keys;
- preserve source file spelling for diagnostics while also producing normalized
  absolute paths;
- expose typed errors rather than message boxes.

Unknown fields remain allowed for compatibility.

### SourceAssembler

Responsibilities:

- read `main` and every include exactly once in manifest order;
- enforce configurable resource limits before allocating the combined buffer;
- preserve source bytes rather than silently transcoding legacy ANSI files;
- add one line boundary only when adjacent files otherwise have no newline;
- record a source map from combined byte/line positions to original files and
  lines;
- return an immutable owned buffer and source map;
- report missing, unreadable, oversized, or conflicting files with their
  manifest key and resolved path.

Source assembly is deterministic and has no dependency on process current
directory.

### CompilationInput

An owned value representing either a direct source file or an assembled project.
It contains:

- immutable source bytes;
- logical input name;
- project directory;
- source map;
- project/output metadata.

The parser consumes this object and never borrows a buffer whose owner can be
destroyed during compilation.

### FinalSourceArtifactWriter

Writing `final source` is optional and occurs only when compatibility output or
diagnostics request it. It writes to a sibling temporary file, flushes and
closes it, then atomically replaces the destination. A failed write never
leaves a partially written `_Temp.dbsource`.

The default CLI build compiles from memory even when this artifact is enabled.
Different project builds therefore cannot accidentally consume one another's
shared temporary source.

### Compiler Pipeline Boundary

The build pipeline has explicit ordered stages:

1. load and validate input;
2. assemble immutable source;
3. parse;
4. perform symbol and semantic validation;
5. initialize code-generation session and machine-code storage;
6. emit code;
7. link/package the executable;
8. publish outputs atomically where practical.

Parsing may construct AST/IR but may not call `ICodeGenerator`. The backend is
unavailable to parsing APIs by construction. Code generation accepts a parsed
representation only after initialization succeeds.

## AST Crash Correction

The assignment fast path in `CStatement::DoAssignment` must stop instantiating
`CodeGenVisitor` during parsing. It will produce a parsed assignment node owned
by the program representation. Emission occurs later through the normal backend
stage after `CreateASMHeader()` succeeds.

As an incremental compatibility boundary, AST coverage may initially be limited
to constructs that are semantically complete. Unsupported constructs must
continue through the single legacy path; they must not be partially emitted by
both paths. This is one production pipeline with staged modernization, not two
permanent compilers.

The code-generation session will enforce an initialized state. Calling an emit
operation before initialization produces a controlled internal diagnostic in
debug/test builds rather than dereferencing a null machine-code pointer.

## Compatibility Rules

- `main` precedes all `includeN` entries.
- `includeN` indices start at 1 and are contiguous.
- Paths are resolved relative to the directory containing the `.dbpro` file.
- Absolute legacy paths remain accepted.
- Unknown project fields remain ignored by components that do not own them.
- Direct `.dba` compilation remains supported.
- Legacy `#include` directives inside source remain handled by the existing
  source include expansion stage after project assembly.
- Existing `final source` paths remain accepted as optional artifact paths.
- A legacy fallback that consumes a prebuilt `final source` is available only
  through an explicit CLI compatibility option; it is never silently selected
  when manifest assembly fails.

## CLI Behavior

Normal invocation:

```text
DBPCompiler.exe --json path\project.dbpro
```

This loads and assembles the project without Synergy Editor. JSON status events
identify the `project_manifest`, `source_assembly`, `parse`, `codegen`, and
`package` stages.

Compatibility flags are explicit:

- `--emit-final-source`: atomically writes the configured `final source`
  artifact while still compiling from memory.
- `--legacy-final-source`: bypasses manifest assembly and consumes the existing
  `final source`. It exists for controlled comparison and migration only.

Conflicting flags or missing required fields return a nonzero process exit code
and a structured diagnostic.

## Diagnostics and Error Handling

Diagnostics carry a stable code, stage, severity, message, project path, source
path, and original line when available. JSON output must be valid JSON Lines;
logging noise remains on stderr and does not corrupt stdout diagnostics.

No new project-loading or source-assembly failure uses a modal message box in
CLI mode. Exceptions at filesystem boundaries are converted into diagnostics.
Internal invariants fail deterministically with enough context to identify the
stage and operation.

Representative diagnostic codes include:

- `DBP1001`: missing `main` field;
- `DBP1002`: non-contiguous include sequence;
- `DBP1003`: source file not found;
- `DBP1004`: source file could not be read;
- `DBP1005`: assembled source exceeds configured limit;
- `DBP2001`: code emission attempted before backend initialization.

## Testing Strategy

Implementation follows strict red-green-refactor TDD.

### Unit Tests

- parse project keys case-insensitively;
- preserve numeric include order beyond nine entries;
- reject missing main, gaps, duplicate indices, and malformed keys;
- resolve relative, absolute, spaced, and Unicode Windows paths;
- assemble bytes deterministically and insert only required boundaries;
- enforce resource limits and propagate read errors;
- map assembled locations back to original sources;
- atomically publish optional final-source artifacts;
- reject backend emission before initialization.

### Regression Tests

- reproduce the simple assignment crash with `gloadreportstate=0` before the
  correction;
- prove parsing performs no code emission;
- prove backend initialization precedes all emitted instructions;
- prove sequential projects sharing `_Temp.dbsource` cannot consume stale source.

### Integration Tests

- compile a minimal multi-file `.dbpro` without an editor;
- compile a direct `.dba` input;
- compare assembled bytes against a known legacy editor fixture;
- compile FPS Creator `FPSC-Screens.dbpro`;
- compile FPS Creator `FPSC-MapEditor (english).dbpro`;
- compile FPS Creator `FPSC-Game (english).dbpro`;
- verify each executable is freshly produced and the process exits zero;
- run existing compiler unit, conformance, and CTest suites.

FPS Creator tests run serially where they share legacy output paths. Unit tests
remain parallel and isolated in unique temporary directories.

## Implementation Boundaries

New project-front-end code lives in focused files under the compiler target,
not inside an enlarged `Main.cpp` or a monolithic `CDBPCompiler` method. Legacy
APIs are adapted at one boundary and retired incrementally after callers move to
owned types.

Filesystem code uses `std::filesystem::path`, RAII streams/handles, checked size
arithmetic, and explicit byte buffers. New code is warning-clean under the
project's supported MSVC configuration and does not introduce raw owning
pointers.

No FPS Creator source or project file is modified to make tests pass.

## Delivery Sequence

1. Characterization tests for current project parsing and source behavior.
2. Typed manifest reader with unit tests.
3. Deterministic source assembler and source map with unit tests.
4. `CompilationInput` integration for direct DBA and DBPro inputs.
5. Optional atomic final-source artifact writer.
6. Parser/backend stage separation and assignment crash regression test.
7. Minimal end-to-end compiler test.
8. FPS Creator Screens, Map Editor, and Game builds.
9. Full compiler and conformance verification.
10. Remove transitional paths that have no remaining callers, except the
    explicit `--legacy-final-source` compatibility mode.

## Acceptance Criteria

- The three active FPS Creator DBPro projects build through direct compiler CLI
  invocation without launching Synergy Editor or pre-generating
  `_Temp.dbsource`.
- Sequential builds always compile the source declared by the selected project.
- The simple assignment regression no longer crashes and emits valid code.
- Missing or invalid manifest inputs produce deterministic diagnostics and a
  nonzero exit code.
- Existing direct DBA and legacy source-include tests continue to pass.
- All new behavior is covered by tests written before production changes.
- The full supported compiler test suite passes from a clean build.
