# Versioned Runtime Bundle and ABI Contract Design

## Status

Approved direction. This specification defines the runtime compatibility boundary for the Dark Basic Pro compiler and the FPS Creator Classic validation target.

## Problem

The compiler currently discovers runtime DLLs relative to the compiler executable and packages referenced DLLs into each generated executable. The compiler bootstrap and those DLLs are not versioned or validated as one product.

This permits a mixed deployment: a newly built compiler can package an older `DBProCore.dll`. The generated executable then resolves bootstrap functions with `GetProcAddress` and may call a null pointer. The observed FPS Creator Map Editor crash is this exact failure:

- `FPSC-MapEditor.exe` raised execute access violation `0xC0000005` at `EIP=0`.
- The return address was consistently `0x004053E8` in two dumps.
- The preceding instruction called the null `g_CORE_PassStructurePatterns` pointer.
- The FPS Creator copy of `DBProCore.dll` does not export `?PassStructurePatterns@@YAXPAXK@Z`.
- The current Dark Basic Pro build of `DBProCore.dll` does export it.

This is a toolchain/runtime ABI mismatch, not a Dark Basic source error.

## Goals

1. Build and distribute the compiler, executable bootstrap, official runtime DLLs, effects, and metadata as one reproducible Win32 toolchain bundle.
2. Prevent the compiler from producing an executable when its runtime cannot satisfy the program's required capabilities.
3. Prevent every runtime API call through an unresolved function pointer.
4. Preserve safe legacy compatibility without limiting modern compiler or runtime features.
5. Validate FPS Creator Map Editor, Game, and Screens through actual startup and IPC behavior, not compilation alone.
6. Make compatibility failures deterministic, structured, and actionable.

## Non-goals

- Replacing the DBPro runtime with SDL, Direct3D 11/12, or another engine in this change.
- Rewriting all plugins or changing Dark Basic source projects without evidence.
- Promising compatibility for arbitrary third-party binary plugins.
- Introducing source maps or native debug symbols; those are valuable but independent work.
- Silently emulating missing modern runtime features on old DLLs.

## Chosen Approach

Use a modern runtime bundle by default and introduce explicit ABI/capability validation. Legacy runtimes may be selected explicitly, but only when they implement every capability required by the generated program. Missing required capabilities stop compilation. Missing optional APIs never result in an unchecked call.

This combines strict correctness with bounded legacy compatibility. It does not reduce modern compiler functionality: a legacy runtime can only compile programs within its demonstrated capability set.

## Runtime Component Classification

### Official DBPro components

Official components whose source and Visual Studio projects are present in this repository are built from the same commit and configuration as the compiler. Examples include `DBProCore.dll`, Setup, Basic2D, Basic3D, Image, Text, System, Input, Camera, Sound, Sprites, and Vectors.

### Legacy or third-party components

FPSC-specific and third-party components such as `EnhancementsFREE.dll`, `LightMapper.dll`, physics plugins, and model converters are not replaced automatically. Each is retained only after export, architecture, dependency, and smoke-test validation. When source is available, incompatibilities are fixed at that plugin boundary under a failing test. When source is unavailable, compatibility is provided only through a bounded adapter that does not weaken the official core contract.

### Unused components

Components not referenced by a program are neither required nor packaged.

## Bundle Layout

The build produces a versioned directory independently of any consumer project:

```text
DarkBasicProToolchain/
  bin/DBPCompiler.exe
  templates/DarkEXE.exe
  runtime/runtime-manifest.json
  runtime/plugins/
  runtime/plugins-user/
  runtime/plugins-licensed/
  runtime/effects/
```

The bundle is assembled into a staging directory and atomically published only after validation passes. FPS Creator does not own or silently override the official runtime.

## Runtime Selection

The compiler resolves its runtime in this order:

1. An explicit `--runtime-root <path>` argument.
2. The versioned runtime shipped with the compiler.

No implicit fallback searches project directories, `PATH`, unrelated DBPro installations, or temporary directories. The selected canonical path is reported in structured build output.

An explicitly selected legacy layout without a manifest is probed and classified as `legacy-unversioned`; it is never treated as equivalent to the official bundle.

## Runtime Manifest

`runtime-manifest.json` is generated from build artifacts rather than maintained as a handwritten inventory. It contains:

- manifest schema version;
- toolchain and ABI versions;
- target architecture;
- build configuration and Git commit;
- component relative paths and SHA-256 hashes;
- component roles;
- declared capabilities;
- required companion components.

Example:

```json
{
  "schemaVersion": 1,
  "toolchainVersion": "2.0.0",
  "abiVersion": 2,
  "architecture": "x86",
  "buildId": "git-<commit>",
  "components": {
    "core": {
      "path": "plugins/DBProCore.dll",
      "sha256": "<generated>",
      "capabilities": [
        "core.data-statements.v1",
        "core.structure-patterns.v1"
      ]
    }
  }
}
```

The manifest is evidence and input to validation, not the sole authority. The validator also inspects the actual PE architecture, exports, and hashes.

## Capability Model

Capabilities name runtime behavior, not DLL implementation details. They are versioned independently, for example:

- `core.bootstrap.v1`
- `core.data-statements.v1`
- `core.structure-patterns.v1`
- `core.runtime-errors.v1`

The compiler derives a `ProgramRuntimeRequirements` value from generated program metadata. A capability can be:

- always required by the executable bootstrap;
- required only when the program uses the associated feature;
- optional and safely omitted.

`core.structure-patterns.v1` is required when structure pattern metadata is non-empty. An old core may only omit this call when the program has no such requirement. The compiler must not silently discard metadata.

## Compile-time Validation

A focused `RuntimeContract` subsystem validates the selected bundle before executable packaging:

1. Parse and validate manifest schema when present.
2. Canonicalize paths and reject components outside the selected runtime root.
3. Verify required files, Win32 architecture, hashes, and PE exports.
4. Derive the program's required capabilities.
5. Compare requirements with verified runtime capabilities.
6. Return structured diagnostics and prevent packaging on failure.

Representative diagnostics:

- `DBP3001`: runtime manifest is invalid.
- `DBP3002`: runtime component is missing or outside the runtime root.
- `DBP3003`: runtime component architecture is incompatible.
- `DBP3004`: required runtime capability is unavailable.
- `DBP3005`: runtime component hash does not match the manifest.
- `DBP3006`: runtime components belong to incompatible ABI generations.

Diagnostics include the project, runtime root, component, capability/export, expected value, actual value, and remediation.

## Runtime API Resolution

Raw global function pointers are replaced behind a typed `CoreRuntimeApi` boundary. Resolution returns an explicit success/error result. Required functions are validated before initialization; capability-dependent functions are invoked only when the corresponding requirement is present; optional functions are null-safe by construction.

The first compatibility implementation continues to resolve legacy decorated exports but centralizes their names in one resolver. A subsequent ABI evolution adds an unmangled versioned entry point:

```cpp
extern "C" DBP_API bool DBP_GetRuntimeApi(
    std::uint32_t requestedVersion,
    DBPRuntimeApi* outApi);
```

The bootstrap prefers this versioned table and falls back to the legacy resolver only for explicitly supported legacy bundles. Consumers do not call `GetProcAddress` throughout initialization code.

Runtime validation remains defense in depth. If a packaged DLL is altered after compilation, startup reports a controlled incompatibility error and exits with a nonzero code rather than executing a null address.

## Packaging

Executable packaging consumes only the validated `ResolvedRuntimeBundle`. It cannot reconstruct plugin paths independently. Every packaged component records its source bundle identity in the build report. A missing referenced plugin is an error unless that plugin is explicitly classified as optional by the command metadata.

The FPS Creator integration uses a separate staging bundle. The existing FPSC plugin snapshot remains untouched until the staged bundle passes all validation.

## Testing Strategy

Implementation follows red-green-refactor TDD.

### Unit tests

- valid and invalid manifest parsing;
- canonical runtime-root containment;
- x86 versus incompatible PE architecture;
- export and hash verification;
- capability requirement derivation;
- required, conditional, and optional API resolution;
- deterministic structured diagnostics.

Small fixture DLLs provide known export sets, including a legacy core without `PassStructurePatterns` and a modern core with it.

### Integration tests

- reject a structure-using program with the legacy core;
- permit a program without structure metadata only when all remaining capabilities exist;
- compile and start a minimal program with the official bundle;
- modify or replace a packaged runtime component and verify controlled startup failure;
- verify that explicit runtime selection cannot escape its root or mix components.

### FPS Creator validation

- compile Map Editor, Game, and Screens with the staged official bundle;
- start Map Editor and require completion of the editor IPC handshake;
- verify Map Editor remains running through initialization;
- load and save a representative map;
- start Test Game and verify the Game/Screens lifecycle;
- inspect Windows event logs and crash-dump locations for new faults;
- compare visible behavior with the known reference build.

Compilation success alone is not an acceptance criterion.

## Migration and Rollout

1. Add contract types, PE inspection, and tests without changing default packaging.
2. Add typed legacy core resolution and eliminate unchecked bootstrap calls.
3. Generate and validate the official runtime manifest.
4. Add explicit runtime-root selection and make packaging consume the resolved bundle.
5. Build the official Win32 runtime components from one commit into staging.
6. Inventory and validate FPSC third-party plugins.
7. Run minimal executable and FPSC end-to-end validation.
8. Promote the staged runtime as the compiler default only after all gates pass.
9. Retain the old FPSC bundle as a rollback artifact until promotion is accepted.

## Acceptance Criteria

- A mixed compiler/core pair is rejected before EXE packaging with a structured diagnostic.
- No bootstrap path can call an unresolved runtime function pointer.
- Official compiler and runtime artifacts identify the same build and ABI generation.
- Legacy runtime support cannot suppress a required program capability.
- Map Editor completes its IPC handshake and remains running after startup.
- FPSC Game and Screens complete their expected startup lifecycle.
- Existing compiler unit tests and sanitizer tests remain green.
- Runtime contract unit, integration, tamper, and FPSC end-to-end tests pass.

## Future Work

- Native source/line maps and PDB/MAP production for generated Dark Basic code.
- Progressive graphics/audio modernization behind the stable runtime API.
- Stronger artifact provenance or signing for distributed toolchain releases.
- Deprecation policy and telemetry for legacy capability fallback.
