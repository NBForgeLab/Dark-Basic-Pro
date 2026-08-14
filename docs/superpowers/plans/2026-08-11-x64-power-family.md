# Plan — Wave 20: Remaining Power family native (PowerBBB/YYY/WWW/DDD/OOO/RRR)

## Mission

Replace the 6 remaining dbprocore.dll `^` rows — byte/boolean/word/dword/
double/int64 — with the emitter-built exp/log sequence from wave 17, covering
the source/target width cases the wave-17 block did not yet handle — TDD.

## Context

Wave 17 made PowerLLL (int) and PowerFFF (float) native via
`x^y = exp(y·log(x))` in double precision using CRT exp/log (msvcrt.dll)
resolved through the command table. The remaining 6 rows are the same `^`
operator on the other DB numeric types. DLL semantics verified in
DBDLLCore.cpp:

| Row | C++ implementation | Semantics to preserve |
| :-- | :-- | :-- |
| `PowerBBB` | `(unsigned char)pow((long double)a,(long double)b)` | byte sources **unsigned** (0-255); result truncates to byte |
| `PowerYYY` | shares `?PowerBBB@@YAKKK@Z` | identical to B (Y rows share the B entry) |
| `PowerWWW` | `(WORD)pow((long double)a,(long double)b)` | word sources unsigned (0-65535); result truncates to word |
| `PowerDDD` | `(DWORD)pow((double)a,(double)b)` | dword sources; result truncates to dword |
| `PowerOOO` | `double result = (float)pow(a,b)` | **float round-trip** before the double store |
| `PowerRRR` | `(LONGLONG)pow((double)a,(double)b)` | int64 sources → double via REX.W CVTSI2SD; result truncates to int64 via REX.W CVTTSD2SI |

## Steps

- [x] Design doc: `docs/superpowers/specs/2026-08-11-x64-power-family-design.md`
- [x] RED: extend `tests/test_x64_power.cpp` — task-level Power with
      byte/word/dword/double/int64 operands; compiled `b=b^b`, `w=w^w`,
      `d=d^d`, `dd=dd^dd`, `r=r^r`; no `PowerBBB/YYY/WWW/DDD/OOO/RRR`
      references; MOVZX/REX.W byte assertions
- [x] GREEN: extend the `ASMTask::Power` block in `ASMWriter.cpp` —
      MOVZX (wave-19 opcodes) for byte/word sources, `CVTSI2SDXMM0RAX` for
      int64 sources, `CVTTSD2SIRAXXMM0` + 8-byte store for int64 targets,
      CVTSD2SS+CVTSS2SD float round-trip for the double target
- [x] GREEN: flip the 6 rows to `AddBuildCommand` reusing
      `BuildTask::Power` (wave-17 mapping already exists)
- [x] Full test suite green (no regressions)
- [x] Full build + ctest
- [x] Docs: design TDD discoveries + docs/17 wave entry + plan checkboxes

## Acceptance

- [x] `b=b^b`, `w=w^w`, `d=d^d`, `dd=dd^dd`, `r=r^r` compile with zero
      dbprocore Power references.
- [x] Byte/word sources zero-extend (MOVZX) before the double conversion —
      unsigned semantics match `(unsigned char)/(WORD)` + `(long double)`.
- [x] int64 operands use the REX.W CVTSI2SD/CVTTSD2SI forms (full width).
- [x] The double result round-trips through float precision (matches
      `(float)pow` in PowerOOO).
- [x] No existing cast/math semantics change.

## TDD discoveries

1. **One task, one block** — all six rows reuse the wave-17
   `BuildTask::Power`/`ASMTask::Power`; no new enums were needed, only
   widening of the existing block's source/result ladder.
2. **A `widenToDouble` lambda** collapsed the five-way source ladder
   (float/byte/word/int64/int) into one place instead of the wave-17
   duplicated P1/P2 blocks.
3. **PowerOOO is `(float)pow`**, not plain `pow` — the float round-trip
   (CVTSD2SS+CVTSS2SD) is an observable semantic that must stay explicit.
4. **First lambda signature fix** — `WriteASMXtoEAX` takes `CStr*`, so the
   helper's params were `CStr*`, not `LPSTR` (caught by the compiler).
