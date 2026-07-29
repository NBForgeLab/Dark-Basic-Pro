# Phase 16: Authenticated DBPAK v2 Packages

## Shipped architecture

The CMake-built compiler now emits three files for every application:

- the normal Win32 executable;
- an immutable `data-<package-id>.dbpak` archive;
- a fixed-size `<application>.dbpakref` descriptor.

The descriptor is the publication commit point. It identifies the exact
package, key identifier, and runtime mode. The runtime resolves it relative to
the executable directory, never the process working directory, and fails
closed when the descriptor, package, or embedded key identifiers disagree.
The executable remains a valid PE image; DBPAK bytes and legacy footers are not
appended to it.

The compiler builds and customizes the executable in a private sibling file.
It publishes the verified immutable package first, atomically replaces the
executable, and replaces the descriptor last. During that commit window the
new executable carries one bounded fallback resource for the descriptor's
previous key, so an interrupted rebuild leaves the previous descriptor and
package runnable. A later successful build removes recovery backups.

## Format and cryptography

DBPAK v2 uses:

- AES-256-GCM through Windows CNG for the manifest and every payload;
- HKDF-SHA-256 to derive domain-separated manifest and per-entry keys;
- SHA-256 plaintext digests in the authenticated manifest;
- cryptographically random package identifiers, key identifiers, and nonces;
- Zstandard 1.5.7, pinned by CMake, when compression reduces entry size;
- strict little-endian, versioned headers with checked offsets and sizes.

Each normal build generates a fresh 32-byte master key. The key is stored in a
versioned PE `RCDATA` resource and is never written to the descriptor or logs.
Reproducible key selection is available only through
`--package-key-file <path>`, which requires an exact 32-byte binary file. Raw
keys are deliberately not accepted on the command line. On Windows the file
DACL must restrict read access to its owner and privileged system
administrators; broadly readable key files are rejected without logging their
path.

This protects assets against accidental disclosure, undetected archive
tampering, and package substitution. It is not DRM: a determined user who
controls the machine can extract a key from a runnable offline executable.
Publisher authenticity still requires normal executable/installer code
signing and a secure release process.

## Runtime and installer behavior

`DarkEXE` authenticates the descriptor, PE key resource, encrypted manifest,
and requested payloads before exposing them through the read-only VFS. DLL
exports are resolved through the in-memory PE loader. No `_virtual.pck` is
created and the runtime does not scan the current directory for an arbitrary
replacement package.

Installer mode authenticates every package entry before creating any published
application directory. It writes into a private staging directory, converts
the installed descriptor to application mode, and atomically publishes the
completed directory. Materialized media is created through Win32 handles that
reject reparse points and verify the final normalized handle remains beneath
the staging root. The root and every traversed parent-directory handle deny
write/delete sharing until the destination handle is created, closing the
check-to-use junction race. The executable, package, descriptor, and media
all use this same handle-verified path. Legacy installer images remain
read-only and are not rewritten.

## Validation limits

Default reader/writer limits are:

| Resource | Limit |
|---|---:|
| Entries | 100,000 |
| Canonical UTF-8 path | 1,024 bytes |
| Plaintext manifest | 256 MiB |
| Plaintext per entry | 8 GiB |
| Total plaintext | 64 GiB |
| Archive | 64 GiB |

Persisted paths must be normalized UTF-8 using `/`. Absolute, drive, UNC,
device, traversal, control-character, reserved-device, duplicate, and
case-colliding paths are rejected.

## Legacy compatibility and scope

Historical embedded or same-directory `.pck` files are supported only by the
bounded, read-only `LegacyPckReader`. It validates the old footer and every
record before mounting. It implements the historical additive transform
locally but never executes `compress.dll`; plugin-compressed legacy packages
are rejected. The compiler no longer contains a legacy PCK writer or
`CEncryptor`.

The current implementation remains Win32/x86. It does not claim x64 support,
DRM, guaranteed antivirus reputation, or completed FPS Creator integration.
The tracked historical executables are compatibility fixtures, not newly
produced outputs.
