# Wave 4 — x64 Custom User-Function Frames (Plan)

Method: Superpowers TDD — red (failing tests first), green (minimal
implementation), verify (full suite), document.

## Tasks

- [x] Design doc: `docs/superpowers/specs/2026-08-11-x64-user-function-frames-design.md`
- [x] RED: `tests/test_x64_user_function_frames.cpp` — task-level byte tests for
      prologue/epilogue, `SUBESP`/`ADDESP` REX.W, ClearStack 64-bit base,
      `@:` displacements, caller cleanup ×8, nested-call alignment; plus one
      compiler-level test driving `MakeStatements`+`WriteDBM` on a real
      function and scanning the emitted stream for `[RBP+16]`/`[RBP+24]`.
- [x] Register test in `tests/CMakeLists.txt`; build and confirm the new tests
      fail (red).
- [x] GREEN — ParseUserFunction.cpp: ×2 displacement formulas (string param,
      local var, UDT field), ×2 prologue size, ×2 ClearStack size.
- [x] GREEN — Str.cpp: ×2 in the three `FS@` displacement cases.
- [x] GREEN — ParseInstruction.cpp: caller cleanup `dwMustPopStack*8`.
- [x] GREEN — ASMWriter.cpp: REX.W on `SUBESP`/`ADDESP`; ClearStack rewritten
      as a raw-byte REP STOSB sequence (48 89 E0 / 33 C0 / 48 8B FC / B9 imm32
      / FC / F3 AA) instead of the broken SIB+LOOP loop — no new opcodes needed.
- [x] Run the new tests to green; fix only the new-test expectations where the
      emitted bytes prove the design wrong (document any change).
- [x] Full suite (`dbp_tests`), full build, `ctest` — 0 failures.
- [x] Update `docs/17_x64_only_transition_research.md` and this plan's
      checkboxes.
