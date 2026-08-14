# Wave 5 — 64-bit String Pointers: varspace & the String Manager

**Goal:** string pointers flow as full 64-bit addresses everywhere (varspace slots,
emitter moves, ABI boundary), with TDD byte-level tests.

- [x] Design doc: `docs/superpowers/specs/2026-08-11-x64-string-pointers-design.md`
- [x] RED: `tests/test_x64_string_pointers.cpp` — task-level byte tests for
      `MOVEAXMEM8`/`MOVMEMEAX8`/`MOVEAXEBP8`/`MOVEBPEAX8`/ECX-relative/deref/imm64/stack,
      non-regression for dword/byte/int64/array, varspace widths, command-table names,
      full-pointer patch.
- [x] GREEN: append 9 QWORD opcodes (REX.W) to `ASMOp` + `DefineASM` entries.
- [x] GREEN: `DetermineASMCall` type-3 branch + `MOVECXEAX8` in Rel-store paths.
- [x] GREEN: x64 mangled names (`?EquateSS@@YA_K_K_K@Z`, `?FreeSS@@YA_K_K@Z`) in
      `InstructionTable.cpp` + hard-coded `WriteASMCall` sites.
- [x] GREEN: runtime string manager `DBDLLCore.cpp` (`EquateSS`/`FreeSS`/`FreeStringSS`/
      `AddSSS`/`CreateSingleString`) + `globstruct.h`/`globstruct.cpp` → `uintptr_t`.
- [x] GREEN: flip `ActiveTargetAbi` to `TargetAbi64` (TDD discovery: string slots
      were still 4 bytes); update the two ABI-pin tests.
- [x] GREEN: plugin matrix builds — typedef ripple (~40 `CreateDeleteString` sites)
      + two legacy SDK fixes (`GWLP_WNDPROC`, `GCLP_HICON`).
- [x] Full suite + full build + ctest green (955 pass / 0 fail / 1 skip).
- [x] Document in `docs/17_x64_only_transition_research.md`; update plan checkboxes.
