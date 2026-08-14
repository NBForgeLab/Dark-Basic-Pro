# Plan — Wave 16: Native int64 <-> float cast family

## Mission

Convert every remaining `Cast*ToR` / `CastRto*` runtime DLL call in the
instruction table to emitter-native SSE2/REG64 instructions (TDD).

## Context

Waves 8/15 made the int<->float and int<->int64 boundaries native. The R
(int64) <-> float/double boundary still round-trips through dbprocore.dll:
`?CastFtoR@@YA_JM@Z`, `?CastOtoR@@YA_JN@Z`, `?CastRtoL@@YAK_J@Z`,
`?CastRtoF@@YAK_J@Z`, `?CastRtoB@@YAK_J@Z`, `?CastRtoW@@YAK_J@Z`,
`?CastRtoD@@YAK_J@Z`, `?CastRtoO@@YAN_J@Z`, `?CastBtoR@@YA_JE@Z`,
`?CastWtoR@@YA_JG@Z`.

The key encoding fact: 64-bit CVT instructions need the legacy F2/F3 prefix
**before** REX.W (`F3 48 0F 2C C0`), while the current emitter writes REX.W
first. A new `RexWAfterPrefix` expansion solves this generically.

## Steps

- [x] Design doc: `docs/superpowers/specs/2026-08-11-x64-cast-family-design.md`
- [x] RED: `tests/test_x64_cast_family.cpp` — unit byte tests for the four
      REX.W CVT opcodes; task-level tests for F→R, O→R, R→L, R→F, R→O;
      end-to-end `dim r as int64` + `dim f as float`/`dim d as double`
      assignments compiled with MakeStatements; guards that no `Cast*toR`/
      `CastRto*` DLL reference appears
- [x] GREEN:
      - `ASMWriter.h`: ASMOps 297-300, ASMTasks 609-613, `ASMMAXCOUNT=301`
      - `ASMWriter.cpp`: 4 DefineASM rows, `RexWAfterPrefix` branch, 5
        emission blocks
      - `ICodeGenerator.h`: `RexWAfterPrefix` expansion
      - `InstructionTable.h`: BuildTasks 1109-1113
      - `InstructionTable.cpp`: flip the 9 DLL rows to AddBuildCommand;
        `CastBTOR`/`CastYTOR`/`CastWTOR` share `CastDwordToInt64`;
        `CastRTOL`/`CastRTOD`/`CastRTOB`/`CastRTOY`/`CastRTOW` share
        `CastInt64ToLower`
      - `MathOp.cpp`: 5 BuildTask→ASMTask mappings
- [x] Full test suite green (no regressions; `AsmMaxCountValue` characterization
      test updated to 301)
- [x] Full build + ctest
- [x] Docs: design TDD discoveries + docs/17 wave entry + plan checkboxes

## Acceptance

- [x] `a(int64)=b(float)` / `a(int64)=b(double)` / `b(float)=a(int64)` /
      `b(double)=a(int64)` / `a(int)=b(int64)` compile with zero DLL cast
      references.
- [x] All 4 new CVT opcodes encode with the legacy prefix before REX.W.
- [x] No existing cast semantics change (truncating CVTT*, zero/sign-extension
      rules preserved per the design doc).

## TDD discoveries

1. **Prefix order**: the emitter wrote REX.W before legacy prefixes; the
   64-bit CVT forms need F2/F3 first → `RexWAfterPrefix` expansion.
2. **DBPro's double type is named `"double float"`** — `dim d as double`
   silently declares an integer, so the compiled double tests routed through
   the int→int64 path until corrected.
3. **`ASMMAXCOUNT` must grow** with each opcode; the characterization test
   pinned 300 and was bumped to 301.
