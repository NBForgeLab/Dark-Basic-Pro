# Wave 15 — Address Arithmetic (`x = &b + n`) at Full int64 Width — Plan

Date: 2026-08-11

## Goal

`&b + n` (and `&b - n`, `*`, `/`, comparisons) must compute at **int64 width**
in the emitter so the full 8-byte address survives the math — replacing the
DWORD 4-byte mode with native REG64 instructions and no DLL cast calls.

## Steps

- [x] Design doc: `docs/superpowers/specs/2026-08-11-x64-address-math-design.md`
- [x] RED: `tests/test_x64_ptrmath.cpp` (8 tests) — compile-level red for the
      new ASMTask values, then runtime red for the 4-byte ADD and DLL casts.
- [x] Implement:
  - `MathOp.cpp` `DoValue`: `107`-operand math → type-mode 9 (int64), placed
    **after** the DWORD-forcing `%100` rule.
  - `InstructionTable.h/.cpp`: `BuildTask::CastIntToInt64` (1107) /
    `CastDwordToInt64` (1108); flip `CastLToR`/`CastDTOR` to `AddBuildCommand`.
  - `ASMWriter.h/.cpp`: `ASMTask::CastIntToInt64` (607) / `CastDwordToInt64`
    (608); `MOVSXDRAXEAX` opcode (296, `48 63 C0`); emitter tasks.
  - `MathOp.cpp` `WriteDBM`: BuildTask → ASMTask mappings.
- [x] GREEN: wave-15 tests + full suite (1062/0/1 skip).
- [x] Full build + ctest (100%) + docs/17 entry + plan checkboxes; delete
      `tests/test_x64_ptrmath_probe.cpp`.

## Verification

- `a(int64)=&b+n` emits `ADD RAX,RBX` (`48 01 D8`) with no DLL cast.
- The wave-8b literal gap (`a(int64)=b+5`) is native MOVSXD + ADD RAX,RBX.
- `&b` comparisons run the int64 `CMP RDX,RBX` + SETcc path.
- Narrowing `a(integer)=&b+n` stays a (correctly-valued) DLL narrowing.
