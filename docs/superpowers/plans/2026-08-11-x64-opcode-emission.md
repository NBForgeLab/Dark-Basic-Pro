# x64-Native Opcode Emission Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. TDD: write the failing test first, verify it fails, then implement, then verify it passes.

**Goal:** Convert the `CASMWriter` opcode emission layer from x86 to x64-native: structured opcode descriptor table, x64 slot widths (moffs 8-byte, `MOV r, imm64` for addresses, `[RBX]` expansion for absolute memory forms), PUSHAD/POPAD expansion, and an address-width-aware runtime patch loop — with tests per instruction encoding.

**Design:** `docs/superpowers/specs/2026-08-11-x64-opcode-emission-design.md`

**Tech Stack:** C++17, GoogleTest, MSVC v145, x64 only.

---

### Task 1: Write failing tests for the x64 emission layer

**Files:** `tests/test_x64_opcode_emission.cpp` (new)

- [x] **Step 1: Descriptor table integrity tests** — every `ASMOp` in the table has a non-empty descriptor; `(preOp, op1, op2)` bytes match the legacy x86 encodings for unchanged forms; no opcode is left unclassified; data-bearing opcodes have explicit encodings.
- [x] **Step 2: Data slot width tests** — moffs forms emit an 8-byte placeholder; `MOV r, imm` emits `imm32` for values and `48 B8+rd imm64` for addresses; PtrIndirect forms expand to `48 BB <imm64>` + `[RBX]` opcode; Imm forms keep `imm8/16/32` widths.
- [x] **Step 3: PUSHAD/POPAD expansion tests** — exact byte sequences.
- [x] **Step 4: Reference record tests** — records carry the correct MCB position for each slot width.
- [x] **Step 5: Runtime patch loop tests** — address kinds patch `sizeof(void*)` bytes; values patch 4; CodeLabels patch rel32 (base `pos+4`).
- [x] **Step 6: Build and run the new tests; verify they FAIL (red).** (Red confirmed: new API did not exist; emission tests failed on old 4-byte slots.)

### Task 2: Implement the structured opcode descriptor

**Files:** `DBProCompiler/DBPCompiler/ASMWriter.h`, `DBProCompiler/DBPCompiler/ASMWriter.cpp`, `DBProCompiler/DBPCompiler/ICodeGenerator.h`

- [x] **Step 1:** Add `DataEncoding`, `OpcodeExpansion`, `ASMOpcodeDef` to `ICodeGenerator.h`; replace `DefineASM`/`CreateASMMiddleCore` virtual signatures with code-driven forms.
- [x] **Step 2:** Replace the four parallel arrays in `CASMWriter` with `std::vector<ASMOpcodeDef>`; add public `GetASMOpcodeDef`/`GetASMOpcodeCount` accessors.
- [x] **Step 3:** Rewrite `GenerateASMCodes` with explicit per-entry encodings (verified programmatically against the legacy table).
- [x] **Step 4:** Update `WriteASMLine*` and `CLeapMarkerManager` call sites.

### Task 3: Implement the x64 emitter core

**Files:** `DBProCompiler/DBPCompiler/ASMWriter.cpp`

- [x] **Step 1:** Rewrite `CreateASMMiddleCore(dwASMCode, ...)` to emit per-encoding slots: Imm8/16/32 value bytes, Abs64 8-byte placeholder, ImmOrAddr 4/8-byte decision via `ParseReferenceLabel`, PtrIndirect `48 BB imm64` + modrm `& 0xF8 | 3`, PUSHAD/POPAD expansion.
- [x] **Step 2:** Keep `CreateASMMiddle(bytes, data)` as the raw byte emitter (leap markers, tests) with a fixed Imm32 data slot.
- [x] **Step 3:** Build; iterate to compile clean. (Also handled `WriteASMLine2IMM(op, NULL, imm, size)` filling the data1 slot.)

### Task 4: Runtime patch loop

**Files:** `DBProCompiler/DBPCompiler/EXEBlock.cpp`

- [x] **Step 1:** `pProgramRefPtr` → `uintptr_t[]`; fix all address truncations to `uintptr_t` (8 sites).
- [x] **Step 2:** Patch loop writes `sizeof(void*)` bytes for address kinds (1,2,3,6), 4 bytes for values (4), rel32 for CodeLabels (5) — extracted as tested `CEXEBlock::PatchReferenceValues`.
- [x] **Step 3:** `g_pGlob->g_pMachineCodeBlock` truncation deferred to wave 3 (shared globstruct ABI with plugins); documented.

### Task 5: Update legacy tests and verify green

**Files:** `tests/test_asmwriter_emission.cpp` (+ any test asserting old x86 bytes)

- [x] **Step 1:** Update `CompilerGeneratedLineZeroEmitsPrologueTasks` (PUSHAD `0x60` → PUSH RAX `0x50`).
- [x] **Step 2:** Run the full x64 test suite; all pass (903 passed / 0 failed).

### Task 6: Documentation

- [x] **Step 1:** Update `docs/17_x64_only_transition_research.md` with wave-2 completion status (section 9).
- [x] **Step 2:** Mark this plan's checkboxes complete.

### Commit

```bash
git add DBProCompiler/DBPCompiler/ASMWriter.h DBProCompiler/DBPCompiler/ASMWriter.cpp DBProCompiler/DBPCompiler/ICodeGenerator.h DBProCompiler/DBPCompiler/EXEBlock.cpp tests/test_x64_opcode_emission.cpp tests/test_asmwriter_emission.cpp docs/superpowers/specs/2026-08-11-x64-opcode-emission-design.md docs/superpowers/plans/2026-08-11-x64-opcode-emission.md docs/17_x64_only_transition_research.md
git commit -m "feat(x64): convert CASMWriter opcode emission to x64-native encodings"
```
