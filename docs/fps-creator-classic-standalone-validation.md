# FPS Creator Classic standalone compiler validation

Validated on 2026-07-13 with the Release Win32 compiler invoked directly with
`--json` and an absolute `.dbpro` path. Synergy Editor was not running and no
pre-existing `_Temp.dbsource` was required.

| Project | Result |
| --- | --- |
| `FPSC-MapEditor (english).dbpro` | Compiled successfully |
| `FPSC-Game (english).dbpro` | Compiled successfully |
| `FPSC-Screens.dbpro` | Compiled successfully |
| `FPSCREATOR.dbpro` | Invalid standalone manifest: it lists only `FPSCREATOR.dba`, which calls labels supplied by omitted source files |

The integration exposed three legacy assumptions now handled by the compiler:

1. Synergy normalized source lines while creating `_Temp.dbsource`. FPSC source
   files contain a mixture of CRLF and LF, so standalone assembly now
   normalizes all source line endings to CRLF before invoking the legacy parser.
2. An empty `media root path` means the project directory. Standalone builds
   now resolve cursor, icon, media, and nested include paths from that directory
   instead of the compiler process directory.
3. Release CLI runs use `info` logging rather than per-instruction `trace`
   logging, keeping JSON output responsive on the large FPSC projects.

Two independent memory-safety defects reached by the larger projects were also
fixed and covered by regression tests: the breakpoint scan read one byte before
the source buffer, and the DBM writer copied two bytes beyond each input line.

Example:

```powershell
DBPCompiler.exe --json `
  --runtime-root "D:\path\to\modern\Compiler" `
  "D:\path\to\FPSC-MapEditor (english).dbpro"
```

## Runtime compatibility

FPSC carries a product-specific DBPro command surface. Replacing its complete
`plugins` tree with a newer generic DBPro tree is not compatible: commands such
as `SET STATIC PORTALS` moved between plugin products over time. Conversely,
compiling against the old core and packaging a new core creates invalid command
references (for example the old decorated `?AbsFF@@YAKM@Z` versus the modern
stable `AbsFF` export).

The compiler therefore applies the selected runtime as a component overlay:

- command metadata for `DBProCore.dll`, the packaged `DBProCore.dll`, and core
  effects come from `--runtime-root`;
- all other official, user, and licensed command DLLs remain sourced from the
  compiler host installation (the FPSC distribution in this validation).

The runtime is resolved and its baseline ABI is validated before instruction
discovery. Programs that emit structure metadata receive the stricter
`CoreStructurePatternsV1` validation before packaging. This preserves FPSC's
language surface without allowing a mixed Core command table/runtime binary.

`FPSCREATOR.dbpro` should not be repaired by guessing an include list. It needs
to be assigned a concrete product target (game, map editor, or screens) and then
declare that target's ordered sources, as the three working project manifests
already do.
