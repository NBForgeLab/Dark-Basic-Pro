# Headless Application Publisher

`dbp-publish.exe` creates a runnable DarkBASIC application tuple from a
precompiled Windows x86 host and an explicit asset manifest. It uses the same
transactional publisher as `DBPCompiler.exe`; new publications contain an
executable, one authenticated DBPAK v2 archive, and a 96-byte `.dbpakref`
descriptor. They do not emit a PCK file or a loose `Files` directory.

The build deploys `dbp-publish.exe` beside `DBPCompiler.exe`.

## Command line

```text
dbp-publish publish <manifest.json> --package-key-file <32-byte-file> [--json]
dbp-publish validate <manifest.json> [--json]
dbp-publish --help
dbp-publish --version
```

`validate` checks the manifest contract, host/asset file existence and type,
asset identities, package paths, collisions, and configured limits. It does
not inspect PE semantics, require the output directory to exist, read a key,
or publish output. The `publish` command additionally validates the existing
non-reparse output directory, output aliases, pinned host identity, and key
before staging. Raw key bytes are deliberately not accepted as a command-line
value.

All paths are Unicode Windows paths. Relative paths in a manifest are resolved
from the directory containing that manifest, not from the process working
directory.

## Manifest schema version 1

```json
{
  "schemaVersion": 1,
  "hostExecutable": "runtime/host.exe",
  "outputExecutable": "dist/My Game.exe",
  "mode": "application",
  "assets": [
    {
      "source": "media/level one.fpm",
      "destination": "mapbank/level one.fpm",
      "compress": true
    }
  ]
}
```

Root fields:

| Field | Required | Contract |
|---|---|---|
| `schemaVersion` | Yes | Unsigned integer `1`. |
| `hostExecutable` | Yes | Existing regular non-reparse host file. The publisher treats its contents as opaque; use the intended Windows x86 runtime host. |
| `outputExecutable` | Yes | Destination executable path. Its parent must be an existing non-reparse directory when `publish` runs. |
| `mode` | No | `application` by default, or `installer`. |
| `assets` | Yes | Array of asset objects; an empty array is valid. |

Asset fields:

| Field | Required | Contract |
|---|---|---|
| `source` | Yes | Existing regular source file. |
| `destination` | Yes | Canonical relative UTF-8 path inside the DBPAK. |
| `compress` | No | Boolean; defaults to `true`. |

The parser is strict: duplicate keys, unknown fields, invalid UTF-8, excessive
nesting, unsupported schema versions, missing files, reparse-point inputs,
absolute package destinations, traversal, and case-insensitive or
Unicode-normalization destination collisions are rejected. Source identity is
captured while reading the manifest and checked again from a pinned file
handle before staging, so changing an asset between validation and publication
fails closed.

## Key-file requirements

The key file must contain exactly 32 cryptographically random binary bytes.
Its Windows DACL may grant read access only to the file owner, LocalSystem,
and the built-in Administrators group. It may be stricter, as in the
owner-only example below. Keep it outside source control, do not reuse it
across trust boundaries, and supply it only through `--package-key-file`.

This PowerShell example creates a suitable file for the current user:

```powershell
$keyPath = 'D:\secure-build-keys\game-release.key'
$key = [byte[]]::new(32)
$rng = [Security.Cryptography.RandomNumberGenerator]::Create()
try {
    $rng.GetBytes($key)
} finally {
    $rng.Dispose()
}
[IO.File]::WriteAllBytes($keyPath, $key)
[Array]::Clear($key, 0, $key.Length)

$owner = [Security.Principal.WindowsIdentity]::GetCurrent().User
$acl = [Security.AccessControl.FileSecurity]::new()
$acl.SetOwner($owner)
$acl.SetAccessRuleProtection($true, $false)
$acl.AddAccessRule(
    [Security.AccessControl.FileSystemAccessRule]::new(
        $owner,
        [Security.AccessControl.FileSystemRights]::FullControl,
        [Security.AccessControl.AccessControlType]::Allow))
Set-Acl -LiteralPath $keyPath -AclObject $acl
```

## Automation output

Use `--json` in scripts and CI. The tool writes exactly one compact JSON object
followed by a newline to stdout and keeps stderr empty. This is an NDJSON
contract, so future commands may emit more than one event without changing
the framing.

Successful validation:

```json
{"assetCount":1,"schemaVersion":1,"status":"ok","type":"validation"}
```

Successful publication:

```json
{"descriptor":"D:\\game\\dist\\My Game.dbpakref","executable":"D:\\game\\dist\\My Game.exe","package":"D:\\game\\dist\\data-<package-id>.dbpak","status":"ok","type":"result"}
```

Failure:

```json
{"code":"publication_failed","committed":false,"message":"...","phase":"executable","type":"error"}
```

`committed` is always present on error events. `phase` is present when the
transaction can identify `package`, `executable`, `descriptor`, or `cleanup`.
Messages never include key bytes and key-file validation errors do not disclose
the key path.

## Exit codes

| Code | Meaning |
|---:|---|
| `0` | Command completed successfully. |
| `2` | Invalid command line or invalid/unsafe manifest contract. |
| `3` | Manifest/key input is missing, unreadable, the wrong size, or has an unsafe DACL. |
| `4` | Package construction, authentication, compression, cryptography, or publication input failed. |
| `5` | Transactional publication or cleanup failed. Inspect `committed` before retrying. |
| `7` | Internal invariant, arithmetic, randomness, or unexpected-data failure. |

Do not infer transaction state from the exit code alone. In particular, exit
code `5` with `"committed":true` means the new executable/package/descriptor
tuple is already authoritative and only post-commit cleanup failed.

## Atomic publication and recovery

Publication is serialized per output tuple. The package is immutable and
named by a cryptographically random package identifier. The executable and
descriptor are staged and replaced transactionally. Any failure before
descriptor commit
restores the previous runnable tuple or removes a partial first publication.
The descriptor is the commit point. Cleanup runs afterward, and a cleanup
failure is reported distinctly without claiming that the committed build
failed.

For a CI build:

```powershell
$publisher = 'D:\toolchain\Compiler\dbp-publish.exe'
$manifest = 'D:\workspace\game\publisher.json'
$key = 'D:\secure-build-keys\game-release.key'

& $publisher validate $manifest --json
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $publisher publish $manifest --package-key-file $key --json
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
```

Treat the JSON line and the process exit code as one result. Build into a
dedicated output directory, retain the three-file tuple together, and never
select a `.dbpak` by directory enumeration; the `.dbpakref` descriptor names
the authoritative archive.
