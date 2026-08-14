# Wave 17 — Internal Power instruction (`x^y = exp(y·log(x))`)

**Goal**: Replace the `PowerLLL` (int) and `PowerFFF` (float) dbprocore.dll
calls with an emitter-built sequence of exp/log primitives — the same math
the DLL itself performs (`(int)pow((long double)a,(long double)b)` /
`(float)pow(a,b)` call CRT `pow`, which is `exp(y·log(x))` for positive
bases).

## Current state (pre-wave)

- `^` on integer types maps to `InternalInstruction::PowerLLL` and on float
  types to `PowerFFF`, both registered `AddCommandCore("+math",
  "dbprocore.dll", "?PowerLLL@@YAKHH@Z" / "?PowerFFF@@YAKMM@Z", ...)`.
- The emitter pushes both operands and makes one opaque `CALL` into
  dbprocore. Byte probe (wave 17): `PUSH RAX; PUSH RAX; SUB RSP,32;
  MOVSS XMM0/XMM1,[RSP+..]; MOV RBX,[cmd]; CALL RBX; ADD RSP,32;
  MOV [result],EAX`.
- The rest of the Power family (byte/word/dword `PowerBBB/WWW/DDD`,
  double `PowerOOO`, int64 `PowerRRR`) stays DLL — out of scope.

## Design

### 1. Task model

- `ASMTask::Power` (614), `BuildTask::Power` (1114).
- `InstructionTable.cpp`: `PowerLLL` and `PowerFFF` flip to
  `AddBuildCommand("+math", "POWERLLL"/"POWERFFF", "LL"/"FF", 1, 2,
  InternalInstruction::PowerLLL/PowerFFF, BuildTask::Power)`.
- `MathOp.cpp WriteDBM`: `BuildTask::Power → ASMTask::Power` mapping.
- Emitter block receives P1 (base x), P2 (exponent y), P3 (result) with
  their real types.

### 2. Emitter sequence (all operands widen to double)

```
; TEMPA = (double)x        TEMPB = (double)y
;   float:  MOVD XMM0,EAX ; CVTSS2SD XMM0,XMM0
;   int:    CVTSI2SD XMM0,EAX
;   double: already in XMM0 (WriteASMXtoEAX type 8)
; TEMPA = log(TEMPA)       (msvcrt log, direct x64 call)
; TEMPA = TEMPA * TEMPB    (MULSD XMM0,XMM1)
; TEMPA = exp(TEMPA)       (msvcrt exp, direct x64 call)
; P3 = (target-type)TEMPA
;   float:  CVTSD2SS XMM0,XMM0 ; spill 4-byte temp ; store
;   int:    CVTTSD2SI EAX,XMM0 ; store 4 bytes
;   double: store XMM0 (MOVSD form)
```

### 3. Direct x64 calls to msvcrt exp/log

The emitter already owns a complete x64 call-frame builder
(`EmitX64CallFrame`) but it is coupled to the value-stack push/call/pop
accounting. The Power block instead performs fully-balanced direct calls:

- `AddCommandToTable("[msvcrt.dll", ",log")` / `(",exp")` → command index
  (deduped; embedded in the EXE DLL/command tables and resolved at runtime
  via `LoadLibrary("msvcrt.dll")` + `GetProcAddress("exp"/"log")`).
- Frame: `pad = (m_iRSPMod16 - 32) % 16` (positive), `frame = 32 + pad`
  (identical formula to `EmitX64CallFrame` — 32-byte shadow space, RSP
  16-aligned at the CALL).
- `MOVSD XMM0,[slot]` → `SUB RSP,frame` → `MOV EBX,[index]` → `CALL EBX` →
  `ADD RSP,frame` → `MOVSD [slot],XMM0`.
- The SUB/ADD pair is self-balancing: no value-stack involvement, no
  pending-cleanup state, RSP mod 16 restored exactly.

msvcrt.dll is a guaranteed system DLL (Windows ships it in System32); `exp`
and `log` are classic undecorated exports.

### 4. Semantics

- Matches the DLL for positive bases exactly: double intermediates, float
  results cast to float, int results truncated (CVTTSD2SI, matching
  `(int)pow(...)`).
- **Known domain difference (documented)**: `log(x)` is undefined for
  x ≤ 0, so negative bases produce NaN (→ INT_MIN as int) where CRT `pow`
  would return a signed result for integer exponents. Matches C `exp/log`
  domain semantics; noted in docs/17.

### 5. Out of scope

- `PowerBBB/WWW/DDD/OOO/RRR` remain DLL calls (byte/word/dword/double/int64
  family members not requested; same pattern applies if wanted later).
- The value-stack call accounting of other DLL calls is untouched.

## TDD discoveries (implemented)

1. **Both `ASMTask::Power` (101) and `BuildTask::Power` (101) already existed
   as legacy, unused enum entries** — no new IDs were needed. The new
   registration (InstructionTable) and mapping (MathOp) simply point
   `PowerLLL`/`PowerFFF` at the existing `BuildTask::Power`, and the new
   emitter block implements `ASMTask::Power`. The `TaskPowerValue`
   characterization test stayed green.
2. **Zero new opcodes**: the exponent operand rides `MOVSD XMM1,XMM0`
   (copy) + reload of `[TEMPB]` into XMM0; everything else came from waves
   8/15/16 (MOVSD mem forms, MULSD, CVTSI2SD/CVTSS2SD/CVTSD2SS/CVTTSD2SI).
3. **Command-table row convention**: `AddCommandToTable` expects a `"["`
   DLL marker and a `","` command marker (both stripped internally); the
   helper builds `"[msvcrt.dll"` and `",log"`/`",exp"` exactly like
   `WriteASMCall`. Tests assert `g_pDLLTable->FindString("msvcrt.dll")` and
   a walk of `g_pCommandTable` for `",log"`/`",exp"`.
4. **No value-stack side effects**: the SUB/ADD frame pair is balanced and
   leaves `m_pendingArgTypes`/`m_iPendingCleanupPops` untouched, so Power
   composes safely inside larger expressions.

## Result

`a#=b#^c#` and `a=b^c` now compile to an emitter-owned `exp(y·log(x))`
sequence with zero dbprocore Power references. 4 tests; full suite 1077/0/1;
ctest 100%. Known documented domain note: negative bases → NaN (log domain)
where CRT `pow` returned signed results for integer exponents.
