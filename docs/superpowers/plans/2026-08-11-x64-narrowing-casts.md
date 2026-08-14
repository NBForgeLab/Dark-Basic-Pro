# Plan — Wave 18: Native narrowing casts (Cast*toB/W/D)

## Mission

Replace the 13 narrowing-cast dbprocore.dll rows (sources L/F/O/D → targets
byte/word/dword) with emitter-native truncating stores — TDD.

## Context

Verified in DBDLLCore.cpp: all 13 rows are pure value truncations
(`(unsigned char)/(WORD)/(DWORD)` casts) or bit-preserving 4→4 moves (L→D).
No heap/string involvement. The emitter already owns every needed primitive:
`WriteASMEAXtoX` stores at the P3 width (the wave-16 `CastInt64ToLower`
pattern) and the wave-8 `CastFloatToInt`/`CastDoubleToInt` blocks provide the
CVTT* float→int conversion. Two tasks cover all 13 rows.

## Steps

- [x] Design doc: `docs/superpowers/specs/2026-08-11-x64-narrowing-casts-design.md`
- [x] RED: `tests/test_x64_narrowing_casts.cpp` — task-level
      `WriteASMTaskCore(CastToNarrow)` (int→byte, dword→byte) and
      `CastFloatToNarrow` (float→byte, double→byte); compiled `b=a` (byte=int),
      `w=a` (word=int), `d=a` (dword=int), `b=f#`, `b=double`, `b=dword`;
      no `CastLtoB`/`CastFtoB`/`CastOtoB`/`CastDtoB` references
- [x] GREEN:
      - `ASMWriter.h`: `CastToNarrow = 614`, `CastFloatToNarrow = 615`
      - `ASMWriter.cpp`: two emission blocks (store-width truncation;
        CVTT* conversion for float/double sources)
      - `InstructionTable.h`: BuildTasks 1114/1115
      - `InstructionTable.cpp`: flip the 13 DLL rows to AddBuildCommand
      - `MathOp.cpp`: two BuildTask→ASMTask mappings
- [x] Full test suite green (no regressions)
- [x] Full build + ctest
- [x] Docs: design TDD discoveries + docs/17 wave entry + plan checkboxes

## Acceptance

- [x] `b=a` (byte=int), `w=a`, `d=a`, `b=f#`, `b=double`, `b=dword` compile
      with zero dbprocore cast references.
- [x] Float/double→byte/word emits the truncating CVTT* conversion then the
      width store; integer-family sources store directly at the target width.
- [x] No existing cast semantics change (truncation matches the C++ casts).

## TDD discoveries

1. **Two tasks suffice for all 13 rows** — the P3 type drives the store
   width, so `CastToNarrow` serves every L/D→B/Y/W/D combination and
   `CastFloatToNarrow` every F/O→B/Y/W combination with a single emission
   block each.
2. **The wave-8 CVTT* blocks were reusable verbatim** (float→int conversion
   before the width store); no new opcodes were needed.
3. **The compiled `b=f#` and `b=double` paths emit the CVTT* bytes even if
   routed through an intermediate int cast** — the assertions hold either
   way, and the no-DLL guard pins the real requirement.
