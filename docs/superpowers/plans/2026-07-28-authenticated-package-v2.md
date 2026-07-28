# Authenticated DBPAK v2 Implementation Plan

> **Execution rule:** Use the Superpowers `executing-plans`,
> `test-driven-development`, `systematic-debugging`, and
> `verification-before-completion` skills. For every behavior below, observe a
> failing test before changing production code, make the smallest coherent
> implementation pass, then refactor while the focused test remains green.

**Goal:** Replace legacy PCK production with a versioned, authenticated,
sidecar DBPAK v2 while preserving strictly validated read-only compatibility
with existing embedded PCK programs.

**Architecture:** Add an independently testable `dbp_package` library under
`DBProShared/Package`. It owns canonical serialization, path policy, CNG
cryptography, Zstandard compression, package reading/writing, legacy PCK
parsing, descriptors, and PE key resources. Refactor VFS registration around
read-only data sources, then integrate the library at the two existing seams:
`CFileBuilder`/`CASMWriter` for production and `DarkEXE`/`CFileReader` for
runtime compatibility.

**Toolchain:** C++17-compatible public ABI, MSVC 2022 x86, CMake 3.25 presets,
GoogleTest, Windows CNG (`bcrypt`), Windows resource APIs, and Zstandard v1.5.7
pinned to commit `f8745da6ff1ad1e7bab384bd1f9d742439278e99`.

**Design:** `docs/superpowers/specs/2026-07-28-versioned-authenticated-package-v2-design.md`

## Invariants for Every Task

- Do not edit or regenerate unrelated user files.
- Do not use native struct serialization or unchecked pointer casts.
- Do not expose unauthenticated plaintext to the caller.
- Do not log keys, key-file contents, plaintext, tags, or nonce buffers.
- Keep new package code independent of compiler globals and UI.
- Run `git diff --check` before every task commit.
- Commit only the files named by the current task.
- If a focused failure is not the expected RED failure, stop and apply
  `systematic-debugging` before implementing.

## Task 1: Isolate the Build and Establish Canonical Byte Serialization

**Files**

- Create: `DBProShared/Package/CMakeLists.txt`
- Create: `DBProShared/Package/include/dbp/package/PackageError.h`
- Create: `DBProShared/Package/include/dbp/package/ByteCodec.h`
- Create: `DBProShared/Package/src/ByteCodec.cpp`
- Create: `tests/test_package_byte_codec.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**RED**

Add tests that require:

- exact little-endian encodings for `uint16_t`, `uint32_t`, and `uint64_t`;
- a checked reader that rejects truncation without advancing its cursor;
- checked addition and multiplication that reject overflow;
- byte slices that cannot outlive their owning input in test APIs.

Run:

```powershell
cmake --build --preset windows-x86-debug --target dbp_tests
ctest --preset windows-x86-debug -R PackageByteCodec --output-on-failure
```

Expected RED: the new codec API is missing or the first encoding assertion
fails.

**GREEN**

Implement:

- `PackageErrorCode` and `PackageResult<T>`;
- `ByteWriter` with explicit fixed-width little-endian append operations;
- `ByteReader` over an owned or caller-lifetime byte view with checked reads;
- `CheckedAdd` and `CheckedMultiply`.

No Windows headers belong in the public codec header.

**REFACTOR AND VERIFY**

Run the focused test, then:

```powershell
ctest --preset windows-x86-debug -R "PackageByteCodec|Structural" --output-on-failure
git diff --check
```

Commit:

```text
feat(package): add checked canonical byte codec
```

## Task 2: Enforce One Canonical Virtual Path Policy

**Files**

- Create: `DBProShared/Package/include/dbp/package/PackagePath.h`
- Create: `DBProShared/Package/src/PackagePath.cpp`
- Create: `tests/test_package_path.cpp`
- Modify: `DBProShared/Package/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**RED**

Table-drive accepted paths, Unicode UTF-8 paths, and every rejected category
from the design: absolute, drive, UNC, device, `.`/`..`, empty components,
backslashes in persisted names, NUL/control characters, invalid UTF-8, reserved
Windows names, trailing space/period, normalized duplicates, and ordinal
case-insensitive collisions.

Expected RED: unsafe inputs are accepted.

**GREEN**

Implement `NormalizePackageInputPath` and `ValidatePersistedPackagePath`.
Normalize input separators before persistence, require valid UTF-8, and use
`CompareStringOrdinal(..., TRUE)` for Windows collision checks while retaining
bytewise UTF-8 sort order in the manifest.

**REFACTOR AND VERIFY**

Run:

```powershell
ctest --preset windows-x86-debug -R PackagePath --output-on-failure
```

Commit:

```text
feat(package): enforce canonical safe package paths
```

## Task 3: Add a Narrow CNG Cryptography Provider

**Files**

- Create: `DBProShared/Package/include/dbp/package/CryptoProvider.h`
- Create: `DBProShared/Package/include/dbp/package/SecureBuffer.h`
- Create: `DBProShared/Package/src/CngCryptoProvider.cpp`
- Create: `DBProShared/Package/src/SecureBuffer.cpp`
- Create: `tests/test_package_crypto.cpp`
- Modify: `DBProShared/Package/CMakeLists.txt`

**RED**

Add known-answer tests for:

- SHA-256;
- HMAC-SHA-256;
- RFC 5869 HKDF-SHA-256 test case 1;
- NIST AES-256-GCM vector with a 96-bit nonce and 128-bit tag;
- wrong-tag rejection;
- requested random byte length and a deterministic fake-provider seam;
- move-only secure buffers that zero memory through an injected observer.

Expected RED: vectors do not exist or do not match.

**GREEN**

Use only CNG primitives. Query and verify that the selected GCM provider
supports a 16-byte tag. Check every `NTSTATUS`, use RAII for algorithm/key/hash
handles, perform constant-time comparisons, and zero secret buffers with
`SecureZeroMemory`.

Implement HMAC-based HKDF rather than a custom hash construction. Use
`BCryptGenRandom(nullptr, ..., BCRYPT_USE_SYSTEM_PREFERRED_RNG)`.

**REFACTOR AND VERIFY**

Run:

```powershell
ctest --preset windows-x86-debug -R PackageCrypto --output-on-failure
```

Commit:

```text
feat(package): add authenticated CNG crypto provider
```

## Task 4: Freeze and Validate the DBPAK v2 Header and Manifest

**Files**

- Create: `DBProShared/Package/include/dbp/package/PackageFormat.h`
- Create: `DBProShared/Package/src/PackageFormat.cpp`
- Create: `tests/test_package_format.cpp`
- Modify: `DBProShared/Package/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**RED**

Add golden-vector tests that assert:

- the file header is exactly 160 bytes;
- the manifest header is exactly 32 bytes;
- each manifest record is exactly 112 bytes;
- every field lands at its specified byte offset;
- parsing rejects unknown versions/flags/algorithms, nonzero reserved fields,
  mismatched counts, overflow, out-of-bounds regions, overlaps, nonzero padding,
  duplicate/colliding paths, and trailing bytes;
- manifest AAD is the complete header with only the tag field zeroed;
- entry AAD is stable and includes every field listed in the design.

Expected RED: serialization or validation is unavailable.

**GREEN**

Implement immutable value types for header/manifest/record data and
`Serialize*`/`Parse*` functions using `ByteCodec` only. Parsing accepts explicit
`PackageLimits`.

**REFACTOR AND VERIFY**

Run:

```powershell
ctest --preset windows-x86-debug -R PackageFormat --output-on-failure
```

Commit:

```text
feat(package): define validated DBPAK v2 format
```

## Task 5: Pin and Wrap Zstandard Streaming Compression

**Files**

- Modify: `CMakeLists.txt`
- Modify: `DBProShared/Package/CMakeLists.txt`
- Create: `DBProShared/Package/include/dbp/package/CompressionCodec.h`
- Create: `DBProShared/Package/src/ZstdCompressionCodec.cpp`
- Create: `tests/test_package_compression.cpp`
- Modify: `tests/CMakeLists.txt`

**RED**

Test incompressible and compressible data, deterministic level-3 output,
multi-buffer streaming, truncated frames, corrupted frames, declared-size
mismatches, and expansion-limit rejection.

Expected RED: compression codec is absent.

**GREEN**

Fetch the official Zstandard v1.5.7 commit with static-library programs, shared
library, and upstream tests disabled. Wrap `ZSTD_CStream`/`ZSTD_DStream` in
RAII. Select compression only when it reduces stored size. Enforce the caller's
plaintext limit during decompression.

**REFACTOR AND VERIFY**

Run:

```powershell
ctest --preset windows-x86-debug -R PackageCompression --output-on-failure
```

Commit:

```text
feat(package): add pinned streaming zstd codec
```

## Task 6: Implement Key Providers and the Package Key Hierarchy

**Files**

- Create: `DBProShared/Package/include/dbp/package/KeyProvider.h`
- Create: `DBProShared/Package/src/KeyProvider.cpp`
- Create: `tests/test_package_keys.cpp`
- Modify: `DBProShared/Package/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**RED**

Test:

- a memory provider resolving only the requested key ID;
- a binary key file accepting exactly 32 bytes and rejecting all other sizes;
- missing and unreadable key files;
- manifest and per-entry HKDF domain separation;
- distinct normalized paths producing distinct entry keys;
- key-owning types being non-copyable.

Expected RED: key resolution/derivation is unavailable.

**GREEN**

Implement `KeyProvider`, `MemoryKeyProvider`, `FileKeyProvider`, and
`PackageKeyDeriver`. Keep path I/O outside the crypto provider. Errors must not
include key contents.

**REFACTOR AND VERIFY**

Run:

```powershell
ctest --preset windows-x86-debug -R PackageKeys --output-on-failure
```

Commit:

```text
feat(package): add explicit key providers and derivation
```

## Task 7: Build DBPAK v2 with Streaming and Crash-Consistent Publication

**Files**

- Create: `DBProShared/Package/include/dbp/package/PackageWriter.h`
- Create: `DBProShared/Package/src/PackageWriter.cpp`
- Create: `DBProShared/Package/src/AtomicFilePublisher.cpp`
- Create: `tests/test_package_writer.cpp`
- Modify: `DBProShared/Package/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**RED**

Test:

- empty, 1, 1023, 1024, and multi-megabyte entries;
- input path collision rejection before output creation;
- random package IDs/nonces and duplicate-nonce failure using a fake RNG;
- compression before encryption;
- header/manifest/entry tags matching independently computed values;
- no caller-visible output after simulated short read/write, flush, RNG,
  compression, encryption, or rename failure;
- immutable `data-<package-id>.dbpak` naming.

Expected RED: package creation fails or leaves an invalid file.

**GREEN**

Implement two-pass entry staging:

1. validate all input metadata;
2. stream each source through optional Zstandard into a private staging file;
3. stream staged bytes through chained CNG GCM calls into package payload;
4. build and encrypt the manifest;
5. write the final header, flush, close, verify-read the package, and rename the
   exact temporary file to the immutable final name.

Use a 1 MiB bounded buffer. Treat any duplicate nonce as a fatal RNG failure.

**REFACTOR AND VERIFY**

Run:

```powershell
ctest --preset windows-x86-debug -R PackageWriter --output-on-failure
```

Commit:

```text
feat(package): write authenticated DBPAK v2 archives
```

## Task 8: Read and Authenticate DBPAK v2 Before Publication

**Files**

- Create: `DBProShared/Package/include/dbp/package/PackageReader.h`
- Create: `DBProShared/Package/src/PackageReader.cpp`
- Create: `tests/test_package_reader.cpp`
- Modify: `DBProShared/Package/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**RED**

Use valid writer output, then mutate every header/record field, payload byte,
nonce, tag, hash, padding byte, and file ending. Test missing/wrong keys,
truncation at every structural boundary, configured limits, concurrent reader
instances, and no partially published destination.

Expected RED: at least one mutation is accepted or plaintext is visible on
failure.

**GREEN**

Open and structurally validate the file before key lookup. Authenticate the
manifest before parsing it. For entry materialization, decrypt to a private
temporary backing store, authenticate, decompress with output limits, verify
the plaintext SHA-256, then publish the read-only source or destination.

Wrong-key and authentication failures share one external error code/message.

**REFACTOR AND VERIFY**

Run:

```powershell
ctest --preset windows-x86-debug -R "PackageReader|PackageWriter" --output-on-failure
```

Commit:

```text
feat(package): read DBPAK v2 with fail-closed authentication
```

## Task 9: Quarantine a Bounds-Checked Legacy PCK Reader

**Files**

- Create: `DBProShared/Package/include/dbp/package/LegacyPckReader.h`
- Create: `DBProShared/Package/src/LegacyPckReader.cpp`
- Create: `tests/test_legacy_pck_reader.cpp`
- Modify: `DBProShared/Package/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**RED**

Add deterministic legacy fixtures for:

- key `0`;
- a structurally valid nonzero-key payload;
- 0, 1, 1023, and 1024-byte transform inputs proving termination;
- truncated footer, invalid validity code, invalid EXE size, filename overflow,
  data overflow, duplicate paths, traversal, excessive entry count, and missing
  terminator;
- the tracked `Install/Help/examples/multiplayer/mp.exe` when present.

Pass the fixture root with a CMake compile definition, never an absolute
developer path.

Expected RED: malformed input is accepted or nonzero small input hangs.

**GREEN**

Parse with `ByteCodec`, never native casts. Keep the historical additive
transform private to this reader and use checked `max(1, size / 1024)` progress.
Expose immutable legacy entries through the same reader-facing abstraction as
DBPAK v2. Do not write or extract a PCK.

**REFACTOR AND VERIFY**

Run:

```powershell
ctest --preset windows-x86-debug -R LegacyPckReader --output-on-failure
```

Commit:

```text
feat(package): add safe read-only legacy PCK compatibility
```

## Task 10: Add a Versioned Runtime Descriptor and PE Key Resource

**Files**

- Create: `DBProShared/Package/include/dbp/package/RuntimeDescriptor.h`
- Create: `DBProShared/Package/src/RuntimeDescriptor.cpp`
- Create: `DBProShared/Package/include/dbp/package/ExecutableKeyResource.h`
- Create: `DBProShared/Package/src/ExecutableKeyResource.cpp`
- Create: `tests/test_package_runtime_metadata.cpp`
- Modify: `DBProShared/Package/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**RED**

Test a fixed binary `.dbpakref` descriptor containing magic, version, runtime
mode, package ID, key ID, and package filename. Reject traversal, absolute
names, mismatched IDs, trailing data, and unknown modes.

Copy the test PE to a temporary path and test resource injection/readback for a
fixed 64-byte `DBP_PACKAGE_KEY_V2` resource. Reject missing, duplicate, wrong
version, wrong key ID, wrong size, and malformed resources. Verify the original
test executable remains unchanged.

Expected RED: metadata APIs are absent.

**GREEN**

Implement canonical descriptor serialization and atomic publication. Implement
resource update with `BeginUpdateResourceW`/`UpdateResourceW`/
`EndUpdateResourceW`, and runtime lookup with `FindResourceW`/`LoadResource`.
The resource contains no strings and is injected only after all other PE
resource customization and before signing.

**REFACTOR AND VERIFY**

Run:

```powershell
ctest --preset windows-x86-debug -R PackageRuntimeMetadata --output-on-failure
```

Commit:

```text
feat(package): add runtime descriptor and PE key provider
```

## Task 11: Replace Raw VFS Pointers with Read-Only Data Sources

**Files**

- Modify: `DBProCompiler/DBPCompiler/VFSHooks.h`
- Modify: `DBProCompiler/DBPCompiler/VFSHooks.cpp`
- Create: `DBProCompiler/DBPCompiler/PackageMount.h`
- Create: `DBProCompiler/DBPCompiler/PackageMount.cpp`
- Modify: `DBProCompiler/DBPCompiler/CMakeLists.txt`
- Modify: `DBProCompiler/DBPCompilerEXE/CMakeLists.txt`
- Modify: `tests/test_vfs.cpp`
- Create: `tests/test_package_mount.cpp`
- Modify: `tests/CMakeLists.txt`

**RED**

Test:

- registry ownership after the caller's original buffer is destroyed;
- exact path matching without unsafe basename fallback;
- independent read/seek cursors;
- thread-safe open/close and stable lifetime across registry clear;
- lazy authenticated DBPAK entry loading;
- legacy entry mounting;
- disk fallback only for names not present in the mounted namespace;
- removal of the hard-coded `D:\GitHub-repo` DLL fallback.

Expected RED: raw pointer lifetime/path behavior fails.

**GREEN**

Introduce `IVFSDataSource` and `IVFSReadStream`. Store shared immutable sources
in the registry and shared stream instances per virtual handle. `PackageMount`
registers sources backed by `PackageReader` or `LegacyPckReader`; it does not
extract the archive. Remove basename aliasing unless an explicit alias is
registered.

Protect registry/active-stream state with narrow mutexes and make virtual handle
allocation collision-safe.

**REFACTOR AND VERIFY**

Run:

```powershell
ctest --preset windows-x86-debug -R "VFS|PackageMount" --output-on-failure
```

Commit:

```text
refactor(vfs): mount owned authenticated package sources
```

## Task 12: Switch Compiler Production to DBPAK v2

**Files**

- Modify: `DBProCompiler/DBPCompiler/FileBuilder.h`
- Modify: `DBProCompiler/DBPCompiler/FileBuilder.cpp`
- Modify: `DBProCompiler/DBPCompiler/ASMWriter.cpp`
- Modify: `DBProCompiler/DBPCompiler/CompilerArguments.h`
- Modify: `DBProCompiler/DBPCompiler/CompilerArguments.cpp`
- Modify: `DBProCompiler/DBPCompiler/Main.cpp`
- Modify: `DBProCompiler/DBPCompiler/DBPCompiler.h`
- Modify: `DBProCompiler/DBPCompiler/DBPCompiler.cpp`
- Modify: `tests/test_filebuilder.cpp`
- Modify: `tests/test_cli.cpp`

**RED**

Test:

- `--package-key-file` accepts exactly one path and never accepts a raw key
  option;
- builds without a key file generate a fresh 32-byte master key;
- builds with a key file use its bytes without logging them;
- `CFileBuilder` maps every file-table entry to a normalized package entry;
- package finalization happens after `ChangeEXE`, injects the key resource, then
  publishes the descriptor last;
- application and installer modes are preserved in the descriptor;
- new output has a clean PE end and no `12345678` footer;
- failure in package/resource/descriptor publication fails the build.

Expected RED: output is `.pck` or the legacy footer is appended.

**GREEN**

Replace the stateful `ConstructPCK`/`AddFileToConstruct` flow with a
`PackageBuildSession` owned by `CFileBuilder`. `MakeEXE` stages inputs and
creates the normal stub. Replace `AddPCKToEXE` with `FinalizePackage`, called
after `ChangeEXE` for every media-bearing mode.

Always use DBPAK v2 encryption. Treat legacy `compression` and `encryption`
project booleans as migration warnings: compression selection is automatic and
authenticated encryption is mandatory.

**REFACTOR AND VERIFY**

Run:

```powershell
ctest --preset windows-x86-debug -R "FileBuilder|CompilerArguments|Package" --output-on-failure
```

Commit:

```text
feat(compiler): emit authenticated sidecar packages
```

## Task 13: Switch DarkEXE Startup to Explicit Package Mounting

**Files**

- Modify: `DBProCompiler/DBPCompilerEXE/FileReader.h`
- Modify: `DBProCompiler/DBPCompilerEXE/FileReader.cpp`
- Modify: `DBProCompiler/DBPCompilerEXE/DarkEXE.cpp`
- Modify: `DBProCompiler/DBPCompilerEXE/CMakeLists.txt`
- Create: `tests/test_runtime_package_startup.cpp`
- Modify: `tests/CMakeLists.txt`

**RED**

Test startup decisions:

- valid descriptor + matching DBPAK + matching PE key resource mounts v2;
- descriptor/package/key ID mismatch fails closed;
- descriptor paths resolve relative to the executable, never CWD;
- missing v2 descriptor falls back only to a valid embedded or sidecar legacy
  PCK;
- malformed legacy input fails without creating `_virtual.pck`;
- installer mode materializes only after full authentication and path checks.

Expected RED: runtime scans CWD or creates an extracted PCK.

**GREEN**

Move startup selection into a testable `RuntimePackageBootstrap`. DarkEXE asks
it to mount v2 or legacy sources, then initializes normal execution. Remove the
temporary `_virtual.pck` extraction and CWD preference. Keep legacy behavior
behind `LegacyPckReader` only.

Replace the old global encryption `DWORD` with no-key legacy state for
`CEXEBlock`; DBPAK keys stay inside the package/key-provider boundary.

**REFACTOR AND VERIFY**

Run:

```powershell
ctest --preset windows-x86-debug -R "RuntimePackageStartup|PackageMount|LegacyPckReader" --output-on-failure
```

Commit:

```text
feat(runtime): mount DBPAK v2 with legacy read fallback
```

## Task 14: Remove Legacy Production and Correct Documentation

**Files**

- Delete: `DBProCompiler/DBPCompiler/Encryptor.cpp`
- Delete: `DBProCompiler/DBPCompiler/Encryptor.h`
- Modify: `DBProCompiler/DBPCompiler/CMakeLists.txt`
- Modify: `DBProCompiler/DBPCompiler/FileBuilder.h`
- Modify: `DBProCompiler/DBPCompiler/FileBuilder.cpp`
- Modify: `DBProCompiler/DBPCompilerEXE/FileReader.h`
- Modify: `DBProCompiler/DBPCompilerEXE/FileReader.cpp`
- Modify: `docs/16_modern_asset_protection_and_vfs_pak_roadmap.md`
- Modify: `docs/13_vfs_and_antivirus.md`
- Modify: `docs/README.md`

**RED**

Add or retain source-policy tests that fail when active production code contains
`CEncryptor`, `12321`, `AddPCKToEXE`, raw PCK footer writes, or a developer
absolute path. Do not apply this rule to the isolated compatibility fixture
constants or migration documentation.

Expected RED: the current legacy writer symbols are found.

**GREEN**

Delete active legacy production and update documentation to describe shipped
behavior, limits, key-provider policy, migration, and honest offline threat
model. Do not claim unsupported x64, DRM, signing, or completed FPS Creator
integration.

**REFACTOR AND VERIFY**

Run:

```powershell
rg -n "12321|AddPCKToEXE|CEncryptor" DBProCompiler
ctest --preset windows-x86-debug -R "ModernCppSafety|Package|VFS" --output-on-failure
```

The `rg` command must return no active compiler/runtime production matches.

Commit:

```text
refactor(package): remove legacy PCK production
```

## Task 15: Add Release, Conformance, and Parser Hardening Gates

**Files**

- Modify: `CMakePresets.json`
- Modify: `.github/workflows/windows-x86.yml`
- Modify: `cmake/ProjectOptions.cmake`
- Create: `fuzz/package_reader_fuzzer.cpp`
- Create: `fuzz/legacy_pck_reader_fuzzer.cpp`
- Create: `fuzz/CMakeLists.txt`
- Modify: `CMakeLists.txt`
- Modify: `scripts/run-local-ci.ps1`
- Modify: `tests/run-local-ci.Tests.ps1`

**RED**

Add Pester expectations for:

- Debug, Release, and ASan preset coverage;
- hosted conformance invocation;
- no hard-coded repository path;
- optional Clang/libFuzzer targets guarded by `DBP_BUILD_FUZZERS`;
- corpus smoke execution on valid and malformed seeds.

Expected RED: Release/conformance/fuzz gates are absent.

**GREEN**

Add a Windows x86 Release preset and CI matrix entry. Run conformance after the
compiler build. Add optional Clang fuzz targets without making MSVC builds
depend on libFuzzer. Keep all dependency revisions immutable.

**REFACTOR AND VERIFY**

Run:

```powershell
Invoke-Pester tests/run-local-ci.Tests.ps1
cmake --preset windows-x86-release --fresh
cmake --build --preset windows-x86-release
ctest --preset windows-x86-release
```

Commit:

```text
ci: gate package hardening and release conformance
```

## Task 16: Full Verification and Compatibility Evidence

**Files**

- Create: `docs/baselines/2026-07-28-authenticated-package-v2.md`
- Modify only if evidence requires correction:
  `docs/superpowers/specs/2026-07-28-versioned-authenticated-package-v2-design.md`

Run from clean build directories:

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

& ./tests/conformance/run-conformance.Tests.ps1
git diff --check
git status --short
```

Also:

1. Compile a representative `.dbpro` project with a 32-byte test key file.
2. Prove the EXE has neither a legacy footer nor an appended package overlay.
3. Parse its descriptor and open every DBPAK entry.
4. Flip one bit in the header, manifest, and payload copies and record that each
   fails before plaintext publication.
5. Read the tracked legacy multiplayer executable through
   `LegacyPckReader`.
6. If the sibling FPS Creator Classic checkout is buildable, execute its
   documented integration build. Otherwise record the exact external blocker
   without claiming compatibility completion.

Write the baseline with exact commands, configurations, test totals, hashes of
the generated evidence files, and remaining limitations. Never describe a
recoverable PE resource key as secret from the local machine owner.

Commit:

```text
docs: record authenticated package v2 verification
```

## Final Review

After all tasks:

1. Apply `requesting-code-review` to the complete diff.
2. Address only verified findings using `receiving-code-review` and TDD.
3. Re-run Task 16 after every review-driven production change.
4. Apply `verification-before-completion`.
5. Use `finishing-a-development-branch` to present merge, PR, or retention
   choices without deleting user work.

