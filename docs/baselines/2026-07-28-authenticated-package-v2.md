# Authenticated DBPAK v2 Verification Baseline

Evidence captured: 2026-07-29
Branch: `test-25`
Supported build architecture: Windows x86

## Outcome

New compiler outputs use a sidecar DBPAK v2 archive and a fixed 96-byte
runtime descriptor. The executable contains the matching key resource but no
appended package overlay and no legacy PCK footer. DarkEXE resolves the
descriptor relative to its executable, mounts authenticated entries through
the VFS, and retains only a bounded read-only adapter for tracked historical
PCK executables.

The package contract uses explicit little-endian serialization, AES-256-GCM,
HKDF-SHA-256, Windows CNG randomness, Zstandard 1.5.7, canonical UTF-8 virtual
paths, checked limits, and fail-closed publication. The active CMake compiler
and runtime no longer build the legacy PCK writer, `Encryptor.cpp`,
`FileReader.cpp`, or require `compress.dll`.

## Reproducible build and test gates

The three supported verification configurations were configured from fresh
preset state and built with Visual Studio 2022 Win32:

```powershell
cmake --preset windows-x86-debug --fresh
cmake --build --preset windows-x86-debug
ctest --preset windows-x86-debug --output-on-failure

cmake --preset windows-x86-release --fresh
cmake --build --preset windows-x86-release
ctest --preset windows-x86-release --output-on-failure

cmake --preset windows-x86-asan --fresh
cmake --build --preset windows-x86-asan
ctest --preset windows-x86-asan --output-on-failure
```

All three CTest gates passed. CTest intentionally runs `dbp_tests` once as an
aggregate so process-global compiler state is exercised and CMake cannot
truncate the large GoogleTest discovery output. The Release executable
reported:

```text
[==========] 398 tests from 80 test suites ran.
[  PASSED  ] 398 tests.
```

The final incremental runs after the bounded MemoryPE ordinal fix passed in
Release, Debug, and AddressSanitizer. The ASan preset uses
`halt_on_error=1:detect_leaks=0`; no sanitizer error was reported.

The PowerShell CI contract and its real Release pipeline were verified with:

```powershell
$result = Invoke-Pester -Path tests/run-local-ci.Tests.ps1 -PassThru
if ($result.FailedCount -ne 0) { exit 1 }
```

Result: 8 passed, 0 failed. The real pipeline rebuilt Release, ran all C++
tests, and ran language conformance. Both local CI and GitHub Actions inspect
Pester `FailedCount`; they do not infer success from PowerShell's process exit
state.

Language conformance was also run directly with the built Release compiler:

```powershell
$env:DBP_CONFORMANCE_COMPILER = `
  (Resolve-Path out/build/windows-x86-release/bin/Release/DBPCompiler.exe)
$env:DBP_CONFORMANCE_RUNTIME_ROOT = (Resolve-Path Install/Compiler)
Invoke-Pester -Path tests/conformance/run-conformance.Tests.ps1 -PassThru
```

Result: 12 passed, 0 failed, including the two expected compile-failure
diagnostics and an interrupted-rebuild transaction test. That test performs a
successful build, proves a stop after package publication leaves both the
previous executable SHA-256 and descriptor unchanged, injects a second stop
after atomic executable replacement, proves the unchanged previous descriptor
still launches, completes a later rebuild, and verifies recovery backups are
removed.

## Keyed end-to-end evidence

A minimal project containing `end` was compiled with an exact 32-byte binary
key file:

```powershell
DBPCompiler.exe --json `
  --runtime-root "<repo>\Install\Compiler" `
  --package-key-file test.key `
  --output app.exe project.dbpro
```

The compiler exited 0. The application was then launched with the repository
root as its working directory, rather than its output directory, and exited 0.
This verifies executable-relative descriptor resolution.

Generated evidence:

| File | Size | SHA-256 |
|---|---:|---|
| `app.exe` | 803840 | `3E41D9F9391AD9FDF395BD35C39E2C9FFCFDBA71EEE8BC7135F45DCD6730BBB7` |
| `app.dbpakref` | 96 | `F29144CE3264D44D762DBE802C3A074FB3ACCABBBEAEFE671425B129F420E3B4` |
| `data-ecc6ee951beb8db4fe9a7e8bc76ac79c.dbpak` | 147261 | `9E1A644482BF0FCFA51BE6A5EA2FD2407547DD754836FCE96B74993A148A69C1` |

The descriptor magic is `DBPREF2`, the archive magic is `DBPPAK2`, and the
descriptor package ID, key ID, and immutable filename match the archive
header. PE inspection found five sections, an executable size of 803840 bytes,
and the last section ending at byte 803840. Therefore no appended PE overlay
exists. The historical decimal validity marker `12345678` was absent from the
legacy footer position, and the compiler emitted no `.pck` file.

## Authentication and compatibility evidence

The following focused Release test run passed four of four:

```powershell
$filter = @(
    'PackageReaderTest.RejectsStructuralDamageBeforeKeyLookup'
    'PackageReaderTest.RejectsManifestCiphertextTagAndWrongKeyUniformly'
    'PackageReaderTest.RejectsPayloadTamperingWithoutPublishingPlaintext'
    'LegacyPckReaderTest.OpensTrackedHistoricalExecutableWhenPresent'
) -join ':'

dbp_tests.exe --gtest_filter=$filter
```

This covers mutated structural/header data, manifest ciphertext/tag changes,
payload bit changes with no plaintext publication, and the tracked
`Install/Help/examples/multiplayer/mp.exe` legacy overlay.

A second four-test run passed descriptor round-trip, exact PE key-resource
readback, authenticate-all-before-publish installer behavior, and publishing
nothing when any payload authentication fails. Package format and reader tests
also enforce reserved fields, bounds, padding, trailing-byte, overlap,
canonical-path, wrong-key, and truncation rejection.

Additional review gates reject broader-than-owner key-file DACLs, authenticate
every payload through the production reader before package publication,
reject real child-directory and staging-root junction materialization escapes,
hold the traversed directory handles across destination creation, route the
installed executable/package/descriptor/media through the same safe writer,
cover valid and malformed legacy sidecars without extraction artifacts, and
preserve the previous executable/package/descriptor tuple across an
interrupted rebuild.

The hosted workflow contains a separate required ClangCL/libFuzzer corpus-smoke
job. Its seed generator creates valid and truncated DBPAK v2 and legacy PCK
inputs, then each fuzzer executes 128 bounded corpus runs. These Clang targets
are intentionally separate from the normal MSVC builds.

The sibling FPS Creator Classic checkout was available at commit
`233211868c14` on branch `tets-13`, but had 14 dirty status entries, including
modified compiler/game binaries, deleted support binaries, modified settings
and compiler temporary files, and an untracked document. The final local CI
run nevertheless passed all four DarkBASIC compatibility-matrix cases and all
598 FPS Creator unit tests from 67 suites. This is useful compatibility smoke
evidence, but it is not promoted to a reproducible clean-checkout baseline:
the sibling worktree remains contaminated by unrelated user work. No FPS
Creator compatibility-completion claim is made here.

## Remaining limitations

- The supported build remains Win32 x86; this baseline does not claim x64.
- A client that can decrypt offline assets contains a recoverable key. DBPAK
  v2 provides integrity, bounded parsing, and extraction resistance, not DRM
  or secrecy from the machine owner.
- Historical PCK support is read-only and intentionally narrow. New builds
  never emit PCK, but tracked old executables can still be mounted.
- The libFuzzer targets require ClangCL and remain separate from normal MSVC
  builds. Hosted CI runs their bounded corpus smoke in its own required job;
  local machines without ClangCL can still run the Debug, Release, and ASan
  matrix.
- Legacy icon/input sources still produce pre-existing compiler warnings.
  This verification is not a repository-wide zero-warning claim.

## Headless publisher extension

Evidence captured: 2026-07-29
Branch: `test-26`

The shared `ApplicationPublisher` now serves both `DBPCompiler.exe` and the
deployed `dbp-publish.exe` command-line tool. The publisher accepts a strict
versioned JSON manifest and an owner-only 32-byte key file, emits stable NDJSON
when requested, and preserves the same transactional EXE + DBPAK + descriptor
contract as compiler builds.

The final Release local CI run rebuilt from a fresh configure state and passed
all six phases without skips:

| Gate | Result |
|---|---:|
| DarkBASIC C++ tests | 434 passed |
| Language conformance | 12 passed |
| Publisher process security | 7 passed |
| Golden FPS project compatibility | 4 passed |
| FPS Creator C++ tests | 598 passed |

The publisher process suite uses paths containing spaces and covers valid
publication from an unrelated working directory, unsafe key DACLs, path
traversal and collisions, missing and post-snapshot-mutated assets,
pre-commit rollback boundaries, committed cleanup failure reporting, absence
of PCK/loose/staging artifacts, and corrupted payload rejection through the
production authenticated package reader.

Current CTest runs also passed in Release, Debug, and MSVC AddressSanitizer
configurations. The build presets explicitly build the publisher and its
authenticated-reader process probe, and the compiler bundle has an explicit
target dependency and post-build deployment contract for `dbp-publish.exe`.
The operational command, schema, exit-code, NDJSON, ACL, and recovery
contracts are documented in
[Headless Application Publisher](../17_headless_application_publisher.md).
