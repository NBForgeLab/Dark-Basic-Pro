# Wave 3 — Microsoft x64 Calling Convention (TDD)

Design: `docs/superpowers/specs/2026-08-11-x64-call-convention-design.md`
Parent: wave 2 (`docs/superpowers/plans/2026-08-11-x64-opcode-emission.md`)

## Goal

Convert the emitter's call mechanism from x86 stack-passing to the
Microsoft x64 ABI: RCX/RDX/R8/R9 (+ XMM0-3), 32-byte shadow space, 16-byte
RSP alignment, caller cleanup consumed by the Call task, cleanup pops
suppressed, with TDD tests for every call path.

## Checklist

- [x] RED: `tests/test_x64_call_convention.cpp` written and failing
  (pending-arg tracking, call sequences for 0/1/2/4/5/8 args, alignment
  pads, float/double args, nested calls, cleanup suppression, UDT
  fallback, frame ops REX.W).
- [x] Pending-arg frame state in `CASMWriter` (type list, cleanup counter,
  poison flag) + opcode-level delta table (push/pop/sub/add) and
  alignment tracker (mod 16, seed 8, poison ops).
- [x] `WriteASMTaskCore`: `Push`/`PushAddress` record slot types;
  `Call` emits the x64 sequence (SUB RSP,F; register loads; stack-arg
  moves; CALL; ADD RSP,F) with cleanup-pops suppression; `PopEbx`/`PopEax`
  suppression; `JumpSubroutine`/`Return`/`PushRegisters`/`PopRegisters`
  reset points; `PushUdt` poisons.
- [x] Raw-byte helpers: MOV r64,[RSP+disp8/32], MOVSS/MOVQ XMM,[..],
  MOVQ XMM,RAX, SUB/ADD RSP imm, stack-arg move (REX.W moffs load was
  not needed — the double-slot frame is reassembled instead).
- [x] `MOVEBPESP`/`MOVESPEBP` REX.W expansion (frame ops).
- [x] GREEN: build + new tests pass; full suite stays green
  (ctest: 927 passed / 0 failed / 1 expected skip).
- [x] Docs: wave state appended to `docs/17_x64_only_transition_research.md`
  (§10); spec/plan checkboxes completed.

## Notes

- Deferred: string-pointer width, x87→SSE, user-function frames,
  Master.dll ESP detection (see spec §6).
