# Phase 13: Read-only Virtual File System

## Current behavior

The runtime mounts authenticated DBPAK v2 archives and validated historical
PCK archives through a read-only VFS. Registry entries own their underlying
data sources, opened handles own independent seek cursors, and clearing a mount
does not invalidate an already-open stream.

Package paths are canonical and exact. The VFS does not fall back from a path
to its basename; any compatibility alias must be registered explicitly. A
mounted path rejects write/create/truncate access. Disk access is used only
when no virtual path exists, so a failed authentication cannot silently fall
through to an untrusted disk file.

## Runtime flow

1. `DarkEXE` resolves its sibling `.dbpakref`.
2. It reads the matching versioned key resource from its own PE image.
3. It opens and authenticates the named DBPAK relative to the executable.
4. `PackageMount` registers lazy authenticated data sources.
5. `_virtual.dat`, plugins, and media are opened through VFS-aware file and PE
   loading paths.

There is no temporary `_virtual.pck`, arbitrary current-directory package
selection, or execution of a legacy decompression plugin. Historical
executables without a v2 descriptor use only the bounded, read-only legacy
adapter.

## Security and antivirus expectations

Avoiding a self-extracting PCK and loading packaged DLLs from authenticated
memory removes common dropper-like behavior and reduces temporary I/O. It
cannot guarantee that antivirus products will never report a false positive;
reputation, signatures, compiler output, dependencies, and distribution
channels also affect detection.

Installer mode is the explicit exception to no extraction. It authenticates
all entries first, writes only beneath a private staging directory, and
publishes the completed application directory atomically.

## Concurrency model

The registry and active-handle tables use narrow mutex scopes. Concurrent
opens and reads are supported, each stream tracks its own offset, and source
lifetime is shared rather than borrowed through raw pointers.
