# Versioned Authenticated Package v2 Design

Date: 2026-07-28  
Status: Approved direction; design review required before implementation planning  
Scope: Dark Basic Pro compiler/runtime asset packaging and legacy PCK read compatibility

## 1. Decision Summary

Dark Basic Pro will replace new legacy PCK production with a versioned, sidecar
package format named DBPAK v2. New packages use authenticated encryption,
streaming I/O, explicit little-endian serialization, strict validation, and an
explicit key-provider boundary.

The runtime will retain a separately isolated, read-only legacy PCK adapter so
that existing programs remain usable. No new build will emit the legacy PCK
footer or use the legacy additive transform.

The default output is a sidecar `data.dbpak` next to the executable. DBPAK v2 is
not appended to the Portable Executable. This preserves a conventional PE
layout, avoids the operational problems caused by large executable overlays,
and makes package replacement and signing independent from executable signing.
A single-file distribution, when required, is an installer or deployment-layer
concern rather than a second package layout.

This work is the first modernization tranche. The instruction-table collision,
VFS test-path portability, CI/conformance coverage, and Release configuration
remain required follow-up tranches. Broad rewrites of `CStatement`,
`CASMWriter`, or the diagnostic AST sidecar are not part of this design.

## 2. Verified Current State

The existing package implementation:

- appends package data to an executable and identifies it with a 16-byte footer;
- stores four native 32-bit values in that footer: key, validity value
  `12345678`, package kind, and original executable size;
- uses a hard-coded key value (`12321`) at current writer call sites;
- applies a sparse additive byte transform rather than authenticated
  encryption;
- computes its sampling span as `file_size / 1024`, which is zero for files
  smaller than 1024 bytes and can therefore create a non-progressing loop;
- has no authentication tag, key separation, version negotiation, or robust
  malformed-input contract.

Repository inspection found no standalone `.pck` files. It did find embedded
legacy PCK overlays:

- one present executable in this repository:
  `Install/Help/examples/multiplayer/mp.exe`;
- four tracked examples in the related FPS Creator Classic repository:
  `FPSC-MapEditor.exe`, `FPSC-Screens.exe`, `ConvertAllWAVFiles.exe`, and
  `RecordfSoundSample.exe`;
- additional locally built FPS Creator executables.

Every detected legacy overlay uses key `0`. These files are compatibility
evidence, not evidence that the legacy transform provides meaningful security.
The reader nevertheless retains safe support for structurally valid nonzero-key
legacy packages so that unknown user projects are not needlessly abandoned.

## 3. Goals

The implementation must:

1. Detect tampering and wrong keys before exposing plaintext.
2. Remove hard-coded cryptographic keys and make key acquisition explicit.
3. Stream large files without loading an archive or entry wholly into memory.
4. Parse all persisted integers explicitly as little-endian fixed-width values.
5. Fail closed on malformed, truncated, overlapping, oversized, or ambiguous
   packages.
6. Prevent package paths from escaping the virtual mount or extraction root.
7. Preserve read compatibility with valid legacy embedded PCK programs.
8. Produce DBPAK v2 only for new compiler outputs.
9. Keep the executable a conventional PE without an appended package overlay.
10. Make every security-sensitive behavior executable as a deterministic test.

## 4. Threat Model and Non-Goals

### 4.1 Protected against

DBPAK v2 is designed to resist:

- accidental corruption;
- undetected modification, substitution, truncation, or reordering of package
  metadata and payloads;
- casual extraction without the configured key;
- path traversal and resource-exhaustion inputs presented to the reader;
- nonce reuse caused by deterministic or process-local pseudo-random values;
- accidental disclosure of keys in the package, build log, or command-line
  value.

### 4.2 Explicit limitations

An offline game executable that can decrypt its own assets cannot guarantee
secrecy from a determined owner of the machine. A key embedded in or
recoverable by the shipped client is obfuscation, regardless of the cipher used.
This design does not claim DRM, copy protection, anti-debugging, or protection
against a privileged local attacker.

AES-GCM authenticates data to a holder of the symmetric key; it does not prove
publisher identity to a party that also possesses that key. Publisher
authenticity is provided by signed executables, signed installers, and release
provenance, not by inventing a custom signature scheme inside DBPAK v2.

The design does not preserve legacy PCK writing, reproduce the old security
claims, or append a new proprietary overlay to PE files.

## 5. Considered Approaches

### 5.1 Repair the legacy transform

Fixing the zero-span loop and changing constants would be small, but the format
would still lack authentication, sound key management, safe serialization, and
versioning. Rejected because it preserves the architectural defect.

### 5.2 Versioned DBPAK v2 with legacy read-only compatibility

This separates modern production from compatibility, permits strict parsing,
and allows a controlled migration without invalidating old programs. Selected.

### 5.3 Adopt a general-purpose archive unchanged

ZIP or a similar container would improve tooling and compression but would not
by itself define authenticated metadata, key acquisition, path policy, or
runtime/VFS integration. A general archive library may supply compression
primitives, but it is not the security or compatibility boundary. Rejected as
the persisted contract.

## 6. Architecture

The implementation is divided into narrow components:

- `package::ByteReader` and `package::ByteWriter`: checked, explicit
  little-endian serialization with no native-struct casts.
- `package::CryptoProvider`: AES-256-GCM, HMAC-SHA-256/HKDF-SHA-256,
  SHA-256, secure random bytes, and constant-time comparisons. The Windows
  implementation uses CNG and checks every `NTSTATUS`.
- `package::KeyProvider`: resolves an opaque 16-byte key identifier to a
  32-byte master key. Package code never knows where a deployment stores keys.
- `package::PackageWriter`: validates an input manifest, streams compression
  and encryption, and publishes a completed archive atomically.
- `package::PackageReader`: validates the entire header and encrypted manifest
  before exposing an entry; entry authentication completes before returned data
  is committed to a caller-visible destination.
- `package::LegacyPckReader`: isolated read-only parser for the old footer and
  payload layout. It is not used by `PackageWriter`.
- `vfs::PackageMount`: presents normalized read-only virtual paths and chooses
  the DBPAK v2 reader from an explicit runtime descriptor.

These components return values and typed errors. They do not display UI, mutate
global compiler state, or terminate the process.

## 7. DBPAK v2 Persisted Format

### 7.1 General rules

- File extension: `.dbpak`.
- Byte order: little-endian.
- Integer encoding: fixed width; no `size_t`, compiler packing, bit fields, or
  native structure dumps.
- Sizes and offsets: unsigned 64-bit values, validated with checked arithmetic
  before conversion to platform types.
- Text: valid UTF-8 without a byte-order mark or embedded NUL.
- Physical package name:
  `data-<32-lowercase-hex-package-id>.dbpak`. The immutable identifier in the
  name makes descriptor publication crash-consistent and prevents an old
  descriptor from silently opening newly replaced bytes.
- Format version: major `2`, minor `0`.
- Unknown major versions are rejected. A higher minor version is accepted only
  when all set flags and algorithms are understood.
- Reserved bytes and reserved flag bits must be zero. This makes extensions
  intentional and prevents ambiguous interpretation.

### 7.2 Fixed 160-byte file header

Fields are serialized in this exact order:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | Magic: ASCII `DBPPAK2` followed by NUL |
| 8 | 2 | Major version: `2` |
| 10 | 2 | Minor version: `0` |
| 12 | 4 | Header size: `160` |
| 16 | 4 | Flags |
| 20 | 4 | Entry count |
| 24 | 8 | Manifest offset |
| 32 | 8 | Manifest ciphertext size |
| 40 | 8 | Payload-region offset |
| 48 | 8 | Payload-region size |
| 56 | 16 | Random package identifier |
| 72 | 16 | Opaque key identifier |
| 88 | 12 | Manifest AES-GCM nonce |
| 100 | 16 | Manifest AES-GCM tag |
| 116 | 32 | SHA-256 of plaintext manifest |
| 148 | 12 | Reserved, all zero |

Defined flags are:

- bit 0: manifest encrypted;
- bit 1: entry payloads encrypted;
- bit 2: at least one entry uses compression.

For v2.0, bits 0 and 1 are mandatory and all other unknown bits are rejected.
There is no unauthenticated production profile. Bit 2 is descriptive and must
match the manifest.

The manifest begins at offset 160. The payload region begins at the next
16-byte-aligned offset after the encrypted manifest. Alignment padding must be
zero and is included in structural validation. No bytes may follow the declared
payload region.

### 7.3 Manifest plaintext

The decrypted manifest starts with this 32-byte header:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | Magic: ASCII `DBPMAN2` followed by NUL |
| 8 | 2 | Major version: `2` |
| 10 | 2 | Minor version: `0` |
| 12 | 4 | Record count |
| 16 | 8 | Record-array size |
| 24 | 8 | UTF-8 string-table size |

The header is followed by exactly `record_count` fixed 112-byte records and
then by the string table. `record-array size` must therefore equal
`record_count * 112`. The manifest has no trailing data.

Each record contains:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | Path offset into string table |
| 8 | 4 | Path byte length |
| 12 | 4 | Entry flags |
| 16 | 2 | Compression algorithm |
| 18 | 2 | Encryption algorithm |
| 20 | 4 | Reserved, zero |
| 24 | 8 | Plaintext size |
| 32 | 8 | Stored ciphertext size |
| 40 | 8 | Offset relative to payload region |
| 48 | 32 | SHA-256 of plaintext |
| 80 | 12 | Entry AES-GCM nonce |
| 92 | 16 | Entry AES-GCM tag |
| 108 | 4 | Reserved, zero |

Entry flag bit 0 denotes an executable/runtime resource that must never be
materialized with executable permission by the package layer. No other v2.0
entry flags are defined.

Compression algorithm values are:

- `0`: none;
- `1`: Zstandard.

Encryption algorithm value `1` means AES-256-GCM. No other encryption value is
valid in v2.0.

The writer attempts Zstandard compression with the project-pinned dependency
and a deterministic production level of 3. It records Zstandard only when the
compressed representation is smaller than the original; otherwise it records
`none`. Compression occurs before encryption. Reader output is bounded by the
declared plaintext size and the configured limit, not by claims inside the
compressed stream.

Records are sorted by normalized UTF-8 path bytes. Duplicate paths and Windows
case-fold collisions are rejected. Payload extents are in the same order,
non-overlapping, and contiguous except for zero alignment bytes.

## 8. Cryptographic Contract

### 8.1 Algorithms and parameters

- Authenticated encryption: AES-256-GCM.
- Authentication tag: 128 bits.
- Nonce: 96 random bits.
- Hash: SHA-256.
- Key derivation: HKDF-SHA-256 as defined by RFC 5869.
- Random source on Windows:
  `BCryptGenRandom` with `BCRYPT_USE_SYSTEM_PREFERRED_RNG`.

The implementation uses Windows CNG rather than custom cryptographic
primitives. A portable backend may be introduced behind `CryptoProvider`
without changing the persisted format.

### 8.2 Key hierarchy

The package stores only `key_id`; it never stores a key. `KeyProvider` supplies
the corresponding 32-byte master key.

For each package:

1. HKDF-Extract uses `package_id` as salt and the master key as input keying
   material.
2. The manifest key is HKDF-Expand with ASCII info
   `DBP-PAK-v2/manifest`.
3. Each entry key is HKDF-Expand with ASCII info `DBP-PAK-v2/entry/`
   followed by the entry's 32-byte normalized-path SHA-256.

Derived keys are held in zeroing buffers and never logged. Copying key-owning
objects is disabled.

The build interface accepts a path or an injected provider, not a raw key value
on the command line. The reference command-line provider reads exactly 32 bytes
from an access-controlled key file, rejects broader-than-owner permissions when
the platform can evaluate them, does not echo the path or contents in normal
logs, and wipes its buffer after use. CI creates the file from its secret store
for the duration of the job and removes it through the CI platform's secret
cleanup mechanism.

Runtime distribution of the master key is a deployment policy. If a product
chooses to ship a recoverable client-side key, documentation must describe the
result as tamper detection and extraction resistance, not absolute secrecy.

### 8.3 Nonces and authenticated data

`package_id`, the manifest nonce, and every entry nonce are generated from the
system RNG. The writer tracks all nonces used under a derived key and treats a
duplicate as a fatal generation failure. Because each entry has a separate
derived key, nonce domains are also cryptographically separated.

Manifest additional authenticated data is the complete 160-byte serialized
header with bytes 100 through 115 (the manifest tag) replaced by zero.

Entry additional authenticated data is:

1. ASCII `DBP-PAK-v2/entry`;
2. `package_id`;
3. the normalized path length and bytes;
4. entry flags;
5. compression and encryption identifiers;
6. plaintext size, stored size, and relative payload offset;
7. plaintext SHA-256;
8. the entry nonce.

All integers in additional authenticated data use the persisted little-endian
encoding.

The reader authenticates the manifest before parsing records. It authenticates
an entry before publishing its plaintext. A GCM failure, SHA-256 mismatch, or
decompression size mismatch returns an error and discards temporary plaintext.

## 9. Validation and Resource Limits

The reader applies checked addition and multiplication before allocation,
seeking, or narrowing a value. It validates file size and every declared extent
before requesting a key or decrypting data.

Default limits are:

- 100,000 entries;
- 1,024 UTF-8 bytes per normalized path;
- 256 MiB plaintext manifest;
- 8 GiB plaintext per entry;
- 64 GiB total plaintext;
- 64 GiB archive size.

Applications may lower these limits through `PackageReaderOptions`; increasing
them requires an explicit option and does not bypass checked arithmetic.
Streaming uses bounded buffers, with a default of 1 MiB.

Validation rejects:

- truncated headers, manifests, tags, or payloads;
- invalid UTF-8, embedded NUL, empty paths, and malformed normalization;
- offsets outside the file or payload region;
- integer overflow, overlaps, gaps containing nonzero bytes, and trailing data;
- duplicate names and case-insensitive collisions;
- unsupported flags, algorithms, or versions;
- nonzero reserved fields;
- mismatched entry counts, compression flags, sizes, hashes, or tags;
- an archive or expanded output exceeding configured limits.

No caller receives partially authenticated plaintext. File materialization is
written to a private temporary file in the target directory, flushed, closed,
and atomically renamed only after authentication and all policy checks succeed.
Failure removes that exact temporary file.

## 10. Path Policy

Writer input paths are normalized once to UTF-8 with `/` separators. The writer
and reader reject:

- absolute, drive-qualified, UNC, or device paths;
- leading or trailing `/`;
- empty components, `.` components, or `..` components;
- backslashes in persisted paths;
- control characters, NUL, and invalid UTF-8;
- names that normalize to a duplicate or collide under invariant Windows
  case-folding;
- Windows reserved device names and components ending in a space or period.

The VFS normally serves entries without extraction. When materialization is
explicitly requested, each component is joined beneath a previously
canonicalized root, reparse-point traversal is disabled, and the final handle
is verified to remain beneath that root. Validation is performed on paths and
opened handles; textual prefix comparison alone is insufficient.

## 11. Legacy PCK Compatibility

`LegacyPckReader` recognizes the existing 16-byte footer only after validating:

- the file is at least 16 bytes;
- the validity value is exactly `12345678`;
- the original executable size is no larger than `file_size - 16`;
- all package offsets and lengths remain inside the overlay;
- entry counts, strings, and allocations obey the same configurable limits as
  DBPAK v2;
- normalized entry names pass the v2 path policy before entering the VFS.

The legacy additive transform is implemented locally in this reader and is not
presented as encryption. For a nonzero legacy key, its sampling span is
`max(1, file_size / 1024)`, with checked progress and bounds. This preserves the
intended behavior for valid historical files while guaranteeing termination.
Files smaller than 1024 bytes could not have been successfully produced by the
old nonzero-key writer, so accepting the safe intended transform does not
exclude a valid historical output.

Legacy parsing never writes or modifies the source executable. The old
`CEncryptor` and legacy writer are removed from active build paths after all
callers move to `PackageWriter`. There is no compatibility switch that emits a
new legacy package.

Tests use both minimal deterministic fixtures and the tracked legacy executables
identified in Section 2. Source test-data locations are supplied by CMake as
normalized fixture roots; no developer-specific drive path is allowed.

## 12. Compiler, Runtime, and VFS Integration

The compiler emits:

- the normal executable;
- an immutable `data-<package-id>.dbpak`;
- a small versioned runtime descriptor naming the package, expected package
  identifier, and opaque key identifier.

The descriptor path is resolved relative to the executable, not the process
working directory. It never contains a key. The package is written, flushed,
authenticated through a verification read, and renamed to its unique final
name first. The descriptor is then written and atomically replaced as the
commit point. A crash before descriptor replacement leaves the previous
descriptor/package pair active; an unreferenced new package is safe to remove
on a later successful build. A package/descriptor identifier mismatch is a hard
failure rather than an implicit fallback.

`PackageMount` receives an explicit descriptor and a `KeyProvider`. It does not
scan arbitrary current-directory files. New executables mount DBPAK v2 only.
The legacy startup path invokes `LegacyPckReader` only when the old footer is
present on the executing legacy image.

Compiler and runtime changes land as one compatibility tranche: the compiler
does not switch its output until the matching runtime reader and VFS mount are
available and tested. Existing legacy executables continue through the
read-only adapter.

## 13. Error Model and Observability

Public operations return `PackageResult<T>` containing either a value or a
`PackageError` with:

- stable `PackageErrorCode`;
- operation name;
- package-relative path when safe to disclose;
- byte offset when relevant;
- nested platform status without secret material.

At minimum, error codes distinguish invalid format, unsupported version,
unsupported algorithm, limit exceeded, unsafe path, missing key, wrong
key/authentication failure, integrity failure, compression failure, I/O
failure, and atomic-publication failure.

Authentication and wrong-key failures intentionally share the same external
message. Logs never contain key bytes, plaintext contents, nonce/tag buffers,
or raw secret-provider errors. Diagnostic detail is sufficient to identify the
operation and file offset without becoming an oracle.

## 14. TDD and Verification Strategy

Every production behavior starts with a failing test. The first implementation
sequence is:

1. checked byte reader/writer and canonical format vectors;
2. path normalization and collision policy;
3. structural header/manifest validation;
4. safe legacy footer/parser compatibility;
5. CNG provider and published AES-GCM/HKDF known-answer vectors;
6. package key hierarchy and nonce uniqueness;
7. streaming writer/reader round trips;
8. tamper, truncation, wrong-key, and decompression-bomb rejection;
9. atomic output and cleanup behavior;
10. VFS mount and compiler/runtime integration;
11. removal of active legacy writing and hard-coded keys.

The test suite must include:

- empty, one-byte, 1023-byte, 1024-byte, and multi-buffer entries;
- Unicode names and every rejected path category;
- reordered, duplicated, overlapping, overflowing, and truncated records;
- mutation of every authenticated header and record field;
- payload, nonce, tag, hash, padding, and trailing-byte mutations;
- wrong key, missing key, and unknown key identifier;
- incompressible and highly compressible payloads;
- declared-size and decompression-limit violations;
- simulated short reads/writes and publication failures;
- deterministic golden serialization vectors;
- concurrent readers and repeated writer invocations;
- all tracked legacy embedded examples, plus nonzero-key and sub-1024-byte
  termination fixtures;
- proof that a new executable has no legacy `12345678` PCK footer and that its
  DBPAK is a separate file.

Security-sensitive parsers receive property tests and a libFuzzer-compatible
harness on a supported Clang CI job. Corpus seeds contain valid v2, valid
legacy, and representative malformed files. Unit tests run in Debug and
Release; sanitizer jobs run AddressSanitizer and UndefinedBehaviorSanitizer
where supported. The existing conformance suite is a required hosted CI gate.

No test depends on `D:\GitHub-repo`, a developer account, or the current working
directory.

## 15. Migration and Removal Sequence

1. Introduce the package library and tests without changing compiler output.
2. Isolate and test `LegacyPckReader`; route existing reads through it.
3. Add the CNG and key-provider boundaries with known-answer tests.
4. Implement DBPAK v2 writer/reader and malformed-input corpus.
5. Integrate `PackageMount` and the explicit runtime descriptor.
6. Switch compiler and runtime output/input atomically to sidecar DBPAK v2.
7. Delete active legacy writer call sites, hard-coded package keys, and obsolete
   encryptor build entries.
8. Update conformance, CI, the asset-protection roadmap, and migration
   documentation to match the shipped behavior.
9. Verify the related FPS Creator Classic integration against the new runtime
   and all tracked legacy executables before declaring migration complete.

Each step is reviewable and keeps the tree buildable. Compatibility code remains
quarantined under the legacy namespace and may be removed only in a separately
approved breaking release.

## 16. Acceptance Criteria

The tranche is complete only when:

- new compiler output uses sidecar DBPAK v2 and never emits a legacy PCK;
- the executable has no appended DBPAK/PCK overlay;
- AES-256-GCM uses 96-bit nonces and 128-bit tags from Windows CNG;
- no package key is hard-coded, serialized, printed, or accepted as a raw
  command-line value;
- a one-bit mutation of authenticated metadata or payload is rejected before
  plaintext publication;
- wrong and missing keys fail closed;
- malformed and oversized archives cannot overflow arithmetic, escape the VFS,
  allocate beyond limits, hang, or leave a published partial file;
- all detected tracked legacy embedded packages remain readable through the
  read-only adapter;
- new sub-1024-byte fixtures terminate and round-trip correctly;
- no test contains a developer-specific absolute path;
- Debug, Release, sanitizer, fuzz-smoke, unit, integration, and conformance
  gates pass from clean build directories;
- documentation states the offline secrecy limitation without describing
  recoverable client-side keys as DRM.

## 17. Authoritative References

- [NIST SP 800-38D, *Recommendation for Block Cipher Modes of Operation:
  Galois/Counter Mode (GCM) and GMAC*](https://csrc.nist.gov/pubs/sp/800/38/d/final).
- [NIST decision to revise SP 800-38D and remove support for GCM tags shorter
  than 96 bits](https://csrc.nist.gov/News/2024/nist-to-revise-sp-80038d-gcm-and-gmac-modes).
- Microsoft CNG documentation for
  [`BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO`](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/ns-bcrypt-bcrypt_authenticated_cipher_mode_info),
  [`BCryptEncrypt`](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptencrypt),
  [`BCryptDecrypt`](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptdecrypt),
  and
  [`BCryptGenRandom`](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptgenrandom).
- [RFC 5869, *HMAC-based Extract-and-Expand Key Derivation Function
  (HKDF)*](https://www.rfc-editor.org/rfc/rfc5869).
- [OWASP guidance for archive extraction and path
  traversal](https://owasp.org/www-project-web-security-testing-guide/latest/4-Web_Application_Security_Testing/10-Business_Logic_Testing/09-Test_Upload_of_Malicious_Files).
