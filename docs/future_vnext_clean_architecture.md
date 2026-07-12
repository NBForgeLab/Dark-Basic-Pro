# DarkBASIC Professional vNext Clean Architecture

## Status

This document records a possible future clean-break architecture for DarkBASIC Professional. It is intentionally deferred. The active modernization strategy remains an incremental migration that preserves compatibility with existing DarkBASIC Professional and FPS Creator Classic source projects wherever practical.

The clean-break design remains useful as a long-term reference for boundaries, testing standards, and the desired destination of individual subsystems.

## Motivation

The legacy repository combines the compiler, executable packer, runtime, editor integration, DirectX 9-era libraries, plugins, and third-party dependencies. Incremental modernization must temporarily support global state, 32-bit assumptions, legacy project formats, and DLL-based commands. A clean architecture would remove those constraints and make safety and maintainability primary design requirements.

## Proposed Target

- Windows 11 x64.
- C++23 for new native code.
- UTF-8 internally and UTF-16 only at Windows API boundaries.
- LLVM ahead-of-time compilation to native Windows x64 object files.
- A versioned native runtime with explicit interfaces.
- Direct3D 12 graphics, XAudio2 audio, and modern Windows input APIs.
- CMake Presets, Ninja, CTest, reproducible dependency management, and continuous integration.
- AddressSanitizer-enabled development builds and warnings-as-errors for new code.

## Repository Shape

```text
legacy/                         Frozen behavioral reference
src/
  compiler/
    source/
    lexer/
    parser/
    ast/
    semantics/
    ir/
    llvm/
    diagnostics/
  runtime/
    core/
    platform/windows/
    graphics/d3d12/
    audio/xaudio2/
    input/
    assets/
  cli/
tests/
  unit/
  integration/
  conformance/
  end_to_end/
```

Each component owns one responsibility and communicates through documented, versioned interfaces. Legacy headers, global variables, and platform APIs do not cross into the new compiler core.

## Compiler Pipeline

```text
UTF-8 source
  -> immutable source manager
  -> lexer
  -> parser
  -> typed abstract syntax tree
  -> semantic analysis
  -> language intermediate representation
  -> LLVM IR
  -> native Windows x64 object files
  -> linker
  -> executable
```

The language IR separates DarkBASIC semantics from LLVM implementation details. This permits semantic tests, alternate backends, and controlled LLVM upgrades without coupling the parser to machine code generation.

The first production backend is ahead-of-time compilation. LLVM ORC JIT may later support editor previews and interactive workflows, but it is not required for the initial product.

## Runtime Architecture

The runtime exposes typed interfaces for windowing, rendering, audio, input, assets, timing, and diagnostics. It does not discover commands by scanning arbitrary DLL exports and does not load PE images manually.

Assets use a documented archive format with normalized full paths, explicit ownership, integrity checks, bounded parsing, and stream-based access. Filesystem virtualization is implemented through injected interfaces rather than process-wide Windows API hooks.

If plugins are reintroduced, they use a versioned ABI with capability negotiation. Untrusted plugins should run out of process. Manual PE mapping and writable-executable image pages are prohibited.

## Windows and Media Stack

- Direct3D 12 with the debug layer and explicit resource lifetime management.
- XAudio2 for low-latency mixing and Media Foundation or a maintained codec library for decoding.
- Modern Windows input APIs behind a testable input service.
- Assimp or narrowly selected maintained importers for offline asset conversion.
- Maintained image libraries with explicit color-space and ownership rules.
- No dependency on the retired DirectX SDK, D3DX, DirectInput, DirectPlay, or 32-bit-only middleware.

Third-party dependencies must be pinned to immutable versions, carry license records, receive security updates, and be isolated behind project-owned interfaces.

## Error Handling and Safety

- RAII for every resource.
- Smart pointers express ownership; non-owning references use references, spans, or observers.
- `std::expected`-style results for expected operational failures.
- Exceptions are reserved for failures that cannot be handled locally.
- No mutable global state in compiler or runtime services.
- No unchecked pointer arithmetic when reading source, archives, images, or executable formats.
- No permanent read-write-execute memory mappings.
- Structured diagnostics use immutable source spans and source maps.

## Build and Quality Gates

- Shared configure, build, and test presets.
- CTest registration for every automated test executable.
- MSVC and clang-cl builds in continuous integration.
- `/W4 /WX` or equivalent for new targets.
- AddressSanitizer builds and parser/archive fuzzing.
- Unit, integration, language conformance, and end-to-end compiler tests.
- Process-level CLI tests validate stdout, stderr, JSON schemas, and exit codes.
- Reproducible dependency acquisition with immutable revisions and hashes.

All production changes follow red-green-refactor TDD. A regression test must fail for the expected reason before its implementation is changed.

## Suggested Delivery Order

1. Establish an isolated build, test, sanitizer, and CI foundation.
2. Write a versioned language specification and conformance corpus.
3. Implement source management, lexer, parser, AST, and diagnostics.
4. Implement symbols, scopes, type checking, and constant evaluation.
5. Add a small reference evaluator used only as a semantic test oracle.
6. Implement the language IR and LLVM x64 AOT backend.
7. Implement the core Windows runtime and asset interfaces.
8. Add graphics, audio, input, and maintained asset import pipelines.
9. Add a documented CLI/editor protocol.
10. Migrate valuable examples and remove the legacy product from the build.

## Relationship to the Active Incremental Strategy

The active strategy preserves old project source compatibility and modernizes the existing compiler and runtime in controlled, test-backed stages. The following principles from vNext should still guide that work:

- Create explicit subsystem boundaries before replacing internals.
- Characterize legacy behavior with tests before changing it.
- Replace unsafe global mechanisms with injected services.
- Introduce x64-safe types and ownership before enabling x64 output.
- Keep modern and legacy backends selectable until parity is demonstrated.
- Replace third-party libraries behind stable project-owned interfaces.
- Remove a legacy path only after conformance and end-to-end tests prove the replacement.

The vNext option should be reconsidered if compatibility costs prevent safe x64 support, modern rendering, reliable Unicode behavior, or maintainable language evolution.
