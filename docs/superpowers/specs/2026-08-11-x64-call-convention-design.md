# Wave 3 — Microsoft x64 Calling Convention in the Emitter

Status: in progress · Design date: 2026-08-11
Parent: `docs/superpowers/plans/2026-08-11-x64-opcode-emission.md` (wave 2),
this wave converts the **call mechanism** from x86 stack-passing to the
Microsoft x64 ABI (RCX/RDX/R8/R9 + XMM0-3, 32-byte shadow space, 16-byte
RSP alignment).

## 1. What exists today (x86 model)

Every DLL / math-instruction call flows through `ASMTask::Call`:

```
; per parameter (right-to-left), ASMTask::Push:
WriteASMXtoEAX(mode, pP, ...)          ; value into EAX (double/int64 via EDX:EAX or TEMPA/TEMPB)
WriteASMEAXtoX(ParamMode::Stack, ...)  ; PUSH EAX (doubles/int64: PUSH EDX; PUSH EAX)
; then ASMTask::Call:
MOV EBX, [commandIndex]                ; command index -> 64-bit absolute address in x64
CALL EBX                               ; FF D3 == CALL RBX in 64-bit mode
; then ASMTask::PopEbx / PopEax (one per pushed DWORD):
POP EBX / POP EAX                      ; cdecl cleanup
```

The emitted cleanup pops are produced by the **callers** (MathOp,
ParseInstruction, ParseInit, ParseJump, ParseUserFunction) and always count
pushed **DWORD slots**: 1 per scalar arg, 2 per double/int64 arg.

### Caller pop accounting (verified)

| Caller | Pop sequence | Count |
|---|---|---|
| MathOp `WriteDBMBit` | 1×`PopEbx` + (n−1)×`PopEax` | `dwNumberOfPopsToMake` (2 base + 1 per double + 1 per string dest) |
| ParseInstruction `EquateSS`/`CopyUdt` | n×`PopEbx` | pushed slot count |
| ParseInit string init | 2×`PopEbx` | 2 slots |
| ParseJump `EqualSS` | 2×`PopEbx` | 2 slots |
| ParseUserFunction free | 1×`PopEbx` | 1 slot |

**Invariant:** after a `Call`, the emitter emits exactly as many task-level
`PopEbx`/`PopEax` as there were pushed slots, with only register-only tasks
(`Assign`, `ConditionData`) in between.

## 2. Target model (Microsoft x64 ABI)

At the `CALL` instruction RSP must be 16-byte aligned; the callee reads:

- args 1-4: RCX, RDX, R8, R9 (integer/pointer) or XMM0-3 (float/double)
- args 5+: stack slots at `[RSP+32]`, `[RSP+40]`, ... (8 bytes each)
- 32 bytes shadow space below the stack args (callee-owned, caller-allocated)

Return: RAX (integer/pointer), XMM0 (float/double). Low 32-bit reads (EAX)
remain valid for integer returns.

## 3. Frame construction (unchanged push model, tracked)

`ASMTask::Push` keeps emitting the current instructions (PUSH EAX, or
PUSH EDX; PUSH EAX for doubles/int64). Every push is **8 bytes** on x64, so
the frame at Call time is uniform: a scalar arg occupies 1 slot, a
double/int64 arg occupies 2 adjacent slots with the low half at the lower
address and a 4-byte zero gap between the halves (PUSH zero-extension).

### Pending-arg tracking (new CASMWriter state)

- `m_pendingArgTypes` — LIFO list of pushed slot types (one entry per
  pushed slot: doubles/int64 push 2 entries).
  - `ASMTask::Push` / `PushAddress` append the type (×2 for 8/9/108/109).
  - Opcode-level `POP*` (from `CalcArrayOffset` subscript consumption)
    pops the top entry, keeping the list in lock-step with the stack.
- `m_iPendingCleanupPops` — the number of caller pops to suppress after the
  next `Call` (equals the slot count).
- `m_bPendingFramePoisoned` — set by `ASMTask::PushUdt`; forces the next
  `Call` to the legacy emission (documented below).

### Stack/alignment tracking (new CASMWriter state)

- `m_iFrameDepth` — bytes below the body-entry RSP (from opcode deltas).
- `m_iRSPMod16` — RSP mod 16 (0..15); seeded 8 at `CreateASMHeader` (entry
  via a C call), so the program body starts at 0 after the 7-register
  `PushRegisters` prologue (8 + 56 ≡ 0 mod 16).
- `m_bRSPAlignmentKnown` — poisoned by ops that move RSP by a runtime
  amount: `SUBESPEAX`, `MOVESPEBP`, `MOVESPMEM4` (RestoreEsp).
- Updated in `CreateASMMiddleCore` from a per-opcode delta table:
  +8 `PUSH*`/`PUSHFROMEAX`, −8 `POP*`, ±imm `SUBESP`/`ADDESP`, 0 everything
  else. Task sequences (`PushRegisters` = 7 pushes, the Call frame) are
  updated at their emission sites.

## 4. Call emission (x64 path, unpoisoned frame)

Given N args (walked from the slot list) and the current alignment:

1. `M = max(0, N−4)`; `pad = (m_iRSPMod16 − (32 + 8·M)) mod 16` (0 or 8);
   `F = 32 + 8·M + pad`.
2. `SUB RSP, F` (`48 81 EC <imm32>`, or `48 83 EC <imm8>` when F < 128).
3. For arg k (1..N), source base `b = 8·(slotsRemaining − slotsForArg)`:
   - k ≤ 4, integer/pointer type → `MOV RCX/RDX/R8/R9, [RSP+F+b]`
     (`48 8B /r` + SIB; 64-bit load of the slot).
   - k ≤ 4, float (2/102) → `MOVSS XMM(k−1), [RSP+F+b]` (`F3 0F 10 /r`).
   - k ≤ 4, double (8/108) → reassemble low@[RSP+F+b], high@[RSP+F+b+8]:
     `MOV RAX,[..]; MOV RCX,[..]; SHL RCX,32; OR RAX,RCX; MOVQ XMM(k−1),RAX`.
   - k > 4 → move the slot to `[RSP + 32 + 8·(k−5)]` (doubles: reassemble
     then one 8-byte store; floats: 4-byte store of the low half).
4. `MOV EBX, [commandIndex]` (existing `MOVEBXIMM4`; command-address
   reference → `48 BB <imm64>` via the wave-2 `Abs64` path) + `CALL EBX`.
5. `ADD RSP, F`; set `m_iPendingCleanupPops` = slot count; clear the
   pending list.

### Cleanup suppression

The following `m_iPendingCleanupPops` task-level `PopEbx`/`PopEax` emit
nothing (the frame was consumed by the Call). A task-level pop with no
pending cleanup emits normally (this cannot occur for callers today —
every task-level pop follows a `WriteASMCall`). Value-stack pops in the IR
codegen (`TargetCodegen`/`CodeGenVisitor`) use raw `WriteASMLine(POPEAX)`
and are unaffected.

### Reset points

The pending list is cleared (and cleanup count zeroed) by `Call`,
`JumpSubroutine` (user-function calls keep the x86 stack convention until
wave 4), `Return`, `PushRegisters`, `PopRegisters`, `ClearStack`.

### Legacy fallback (poisoned frame — UDT args)

If `PushUdt` was emitted into the open frame, `Call` emits exactly the old
bytes (`MOV EBX,[index]; CALL EBX`, no register passing, no frame
consumption, no suppression) so the caller's pops remain balanced. UDT
by-value calls are broken on x64 regardless (32-bit data/pointer layout)
until the wave-4 varspace/UDT work; the fallback keeps the compiler from
emitting self-inconsistent code.

## 5. Frame ops (part of this wave)

- `MOVEBPESP` (89 E5) and `MOVESPEBP` (89 EC) gain `OpcodeExpansion::RexW`
  (`48 89 E5` = MOV RBP,RSP; `48 89 EC` = MOV RSP,RBP). 32-bit EBP would
  truncate the frame base on x64.
- `MOV ESP, RBP` / `MOV ESP, [mem]` poison the alignment tracker.

## 6. Scope boundaries (explicitly NOT this wave)

- String/pointer argument width (32-bit truncation of addresses) — depends
  on the x64 varspace layout (wave 4). The slot mechanics here already
  handle them as integer-class args; the truncation itself is a separate
  varspace fix.
- x87→SSE conversion of the float pipeline (`FLD`/`FSTP`, type-8 storage).
  Float args are passed via `MOVSS`/`MOVQ` from the (bit-compatible) frame
  slot; the internal value representation is untouched.
- User-function frames (RBP-based locals, CALLMEM params) — wave 4.
- Master.dll ESP tamper-detection (stores 32-bit ESP) — documented,
  unchanged.

## 7. Compatibility

- Public emission API unchanged. `.dbpro` format unchanged.
- All existing task-level emission paths (`Push`, `Call`, `PopEbx`,
  `PopEax`, `JumpSubroutine`, `PushRegisters`, ...) keep their signatures;
  only the `Call` handler and the tracking state change behavior.
- Raw opcode byte-emission for PUSH/POP/etc. unchanged (byte-identical in
  64-bit mode).
