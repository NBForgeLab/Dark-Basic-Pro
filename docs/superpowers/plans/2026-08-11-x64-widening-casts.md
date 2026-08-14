# Plan — Wave 19: Native widening casts (Cast*toL/W/D/F/O from byte/word)

## Mission

Replace the 16 dbprocore.dll rows whose sources are byte/boolean/word
(B/Y/W → L/W/D/F/O) with emitter-native zero-extension stores — TDD.

## Context

Verified in DBDLLCore.cpp: all 16 rows are pure value conversions. B/Y
(`unsigned char` in the mangled signatures `?CastBtoL@@YAKE@Z` — the Y rows
share the same DLL entries) and W (`unsigned short`, `?CastWtoL@@YAKG@Z`)
extend as **unsigned**; W→B/Y are plain truncations. The emitter's generic
load (`WriteASMXtoEAX`) uses `MOV AL/AX`, which preserves the upper bits of
EAX — so the widening rows need an explicit zero-extension, unlike wave 18's
truncating stores which only read the low bits.

## Steps

- [x] Design doc: `docs/superpowers/specs/2026-08-11-x64-widening-casts-design.md`
- [x] RED: `tests/test_x64_widening_casts.cpp` — task-level
      `WriteASMTaskCore(CastWiden)` (byte→int, byte→word, word→dword) and
      `CastWidenToFloat` (byte→float, word→double); compiled `a=b`, `a=w`,
      `d=b`, `f#=b`, `dbl=w`, `w=b`, `b=w`; no `CastBto*`/`CastWto*`/`CastYto*`
      references (except the W→B/Y truncation rows which reuse `CastToNarrow`)
- [x] GREEN:
      - `ASMWriter.h`: `MOVZXEAXAL = 301`, `MOVZXEAXAX = 302`;
        `CastWiden = 616`, `CastWidenToFloat = 617`
      - `ASMWriter.cpp`: two DefineASM entries (`0F B6 C0` / `0F B7 C0`) and
        two emission blocks (load at source width, MOVZX, then width store /
        CVTSI2SS-CVTSI2SD)
      - `InstructionTable.h`: BuildTasks 1116/1117
      - `InstructionTable.cpp`: flip the 16 rows to AddBuildCommand
        (W→B/Y reuse BuildTask::CastToNarrow)
      - `MathOp.cpp`: two BuildTask→ASMTask mappings
- [x] Full test suite green (no regressions)
- [x] Full build + ctest
- [x] Docs: design TDD discoveries + docs/17 wave entry + plan checkboxes

## Acceptance

- [x] `a=b`, `a=w`, `d=b`, `f#=b`, `dbl=w`, `w=b`, `b=w` compile with zero
      dbprocore cast references for the widening rows.
- [x] Byte/word sources zero-extend through MOVZX before any wider store or
      CVT* conversion (upper bits never garbage).
- [x] W→B/Y keep the wave-18 truncation semantics via `CastToNarrow`.
- [x] No existing cast semantics change (unsigned extension matches the C++
      casts in DBDLLCore.cpp).

## TDD discoveries

1. **The register-form MOVZX suffices** — the source-width load
   (`WriteASMXtoEAX`) always precedes the extension, so `MOVZX EAX,AL/AX`
   covers every addressing mode with one opcode pair; no per-mode memory
   MOVZX forms were needed.
2. **`ASMMAXCOUNT` had to grow** — the descriptor array was sized 301, so
   the new opcodes (301/302) hit out-of-bounds and emitted nothing; bumped
   to 303 and updated the pinned `AsmMaxCountValue` enum test.
3. **Wave-8/16 float/double paths reused verbatim** — `CastWidenToFloat`
   rides the existing CVTSI2SS spill-through-`@$_TEMPA_` contract and the
   MOVSD direct-store path; no new opcodes beyond the two MOVZX forms.
