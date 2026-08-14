# Design — Wave 21: Native float modulo (ModFFF via CRT fmod)

## Problem

`?ModFFF@@YAKMM@Z` is the last scalar-arithmetic dbprocore.dll row. DBDLLCore.cpp:

```cpp
DARKSDK DWORD ModFFF(float fValueA, float fValueB)
{
    if(fValueB==0) return 0;
    double w = (double)fValueA;
    double x = (double)fValueB;
    double z = fmod( w, x );
    float result = (float)z;
    return *((DWORD*)&result);
}
```

Semantics to preserve:
1. **Zero divisor → 0.0f** (`fValueB==0` in C: `+0.0` and `-0.0` are zero;
   NaN is not).
2. **fmod in double precision** of the widened operands.
3. **Float round-trip** of the result (`(float)z`).

## Why the wave-17 helper is not sufficient

`EmitTranscendentalCall` loads one double into XMM0, calls, stores back.
`fmod` takes **two** double arguments (XMM0 = first, XMM1 = second per the
x64 ABI), so a binary variant is needed. The frame/`AddCommandToTable`/
CALL/restore core is shared and is extracted into `EmitAlignedCrtCall`.

## Design

### Refactor

- `EmitAlignedCrtCall(const char* pCommand)` — the pad/frame, msvcrt
  resolution, `MOV EBX,[idx]; CALL EBX`, and SUB/ADD restore from wave 17.
- `EmitTranscendentalCall(pCommand, pTempSlot)` keeps its behavior:
  load XMM0 ← slot, call, store XMM0 → slot.
- `EmitBinaryTranscendentalCall(pCommand, pTempA, pTempB)` — new:
  ```
  MOVSD XMM0,[pTempB];  MOVSD XMM1,XMM0;  MOVSD XMM0,[pTempA]
  EmitAlignedCrtCall(pCommand)
  MOVSD [pTempA],XMM0
  ```
  (XMM0 = first arg A, XMM1 = second arg B.)

### Float-mod branch (in the Add/Sub/Mul/Div/Mod task block)

The branch fires for `ASMTask::Mod` with float-class source
(`dwP1Type==2/102`). The `bFloatMath`/`bInt64Math` branches above it do not
cover Mod-with-float; without this branch a float Mod would fall into the
integer IDIV path and misbehave.

```
; ---- guard: B == ±0.0f  ->  store 0.0f ----
WriteASMXtoEAX(P2)                       ; EAX = float B bits
WriteASMLine2IMM(ANDEAX4, NULL, "2147483647", 2)  ; 25 7F FF FF FF (strip sign)
WriteASMLeapMarkerJump(JNE, 5)           ; real divisor -> fmod path
WriteASMLine(MOVEAXIMM4, "0")            ; EAX = 0.0f bits
WriteASMEAXtoX(P3)                       ; P3 = 0.0f
WriteASMLeapMarkerJump(JMP, 6)           ; skip the fmod block
WriteASMLeapMarkerEnd(5)                 ; fmod path starts here
; ---- widen A and B to double ----
WriteASMXtoEAX(P2); MOVD XMM0,EAX; CVTSS2SD XMM0,XMM0; MOVSD [TEMPB],XMM0
WriteASMXtoEAX(P1); MOVD XMM0,EAX; CVTSS2SD XMM0,XMM0; MOVSD [TEMPA],XMM0
EmitBinaryTranscendentalCall("fmod", TEMPA, TEMPB)
; ---- P3 = (float)TEMPA ----
MOVSD XMM0,[TEMPA]; CVTSD2SS XMM0,XMM0
MOVSS [@$_TEMPA_],XMM0; MOV EAX,[@$_TEMPA_]; WriteASMEAXtoX(P3)
WriteASMLeapMarkerEnd(6)
```

- **Guard exactness**: `AND EAX,0x7FFFFFFF` sets ZF iff the divisor bits are
  `0` (covers `+0.0` and `-0.0`); NaN mantissa bits are nonzero so NaN
  divisors take the JNE (fmod → NaN), exactly like `fValueB==0` in C.
- **Leap markers 5/6**: forward jumps over a variable-length block (the
  aligned call frame pad depends on the current RSP state); indices 5/6 are
  unused by the control-flow/array machinery (0–4).
- **Float result store**: the wave-8 spill-through-`@$_TEMPA_` contract.

### Registration

`ModFFF` flips to `AddBuildCommand("+mathfloat", "MODFFF", "FF", 1, 2,
InternalInstruction::ModFFF, BuildTask::Mod)`; `MathOp::WriteDBM` already
maps `BuildTask::Mod` → `ASMTask::Mod`.

## Semantics notes

- Integer MODLLL/MODBBB/.../MODRRR are untouched (IDIV/CDQ paths).
- No double-mod row exists; the branch is float-only.
- The `fmod` primitive is an undecorated msvcrt.dll export, resolved
  through the command table exactly like wave-17 `exp`/`log`.

## Verification

- Task-level: float Mod asserts the guard bytes (`25 7F FF FF FF`), the
  CVTSS2SD widening, the double `MOVSD XMM1,XMM0`, the two aligned frames
  (`48 83 EC`/`48 83 C4`), `CALL EBX` (`FF D3`), CVTSD2SS narrowing, and
  the absence of any `ModFFF` reference.
- Compiled: `a#=b# mod c#` and a zero-divisor program (`b#=0; a#=c# mod
  b#`); `msvcrt.dll`/`,fmod` registered; no `?ModFFF` reference.
