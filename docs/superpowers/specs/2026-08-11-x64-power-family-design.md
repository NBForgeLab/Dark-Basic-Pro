# Design — Wave 20: Remaining Power family native (exp/log)

## Problem

Six dbprocore.dll rows still implement `^` for the non-int/non-float DB
types. Wave 17 built the exp/log sequence (`ASMTask::Power`) and wired
`PowerLLL`/`PowerFFF` to it; the other rows stayed DLL calls because their
source/target widths hit cases the wave-17 block did not yet handle.

DLL semantics (verified in DBDLLCore.cpp):

| Row | C++ implementation |
| :-- | :-- |
| `PowerBBB` | `(unsigned char)pow((long double)a, (long double)b)` |
| `PowerYYY` | same entry `?PowerBBB@@YAKKK@Z` (Y shares B) |
| `PowerWWW` | `(WORD)pow((long double)a, (long double)b)` |
| `PowerDDD` | `(DWORD)pow((double)a, (double)b)` |
| `PowerOOO` | `double result = (float)pow(dValueA, dValueB)` |
| `PowerRRR` | `(LONGLONG)pow((double)a, (double)b)` |

## Why the wave-17 block is not yet sufficient

1. **Byte/word sources** — the generic load is `MOV AL/AX` (upper bits of
   EAX preserved), so widening to double would convert garbage. The DLL
   reads the values as unsigned (`(unsigned char)/(WORD)` cast + `(long
   double)`), so the emitter must MOVZX (wave-19 opcodes) before
   `CVTSI2SD XMM0,EAX`.
2. **int64 sources** — `WriteASMXtoEAX` loads int64 as a full 8-byte RAX
   (`48 A1`), so the double conversion needs the REX.W
   `CVTSI2SD XMM0,RAX` (`F2 48 0F 2A C0`), not the 32-bit EAX form.
3. **int64 target** — `(LONGLONG)` truncation needs the REX.W
   `CVTTSD2SI RAX,XMM0` (`F2 48 0F 2C C0`) + 8-byte store; the wave-17
   integer path used the 32-bit EAX form.
4. **double target** — `PowerOOO` explicitly rounds through float
   (`(float)pow`), so the result needs `CVTSD2SS` + `CVTSS2SD` before the
   MOVSD store; the wave-17 double path stored the full-precision double.

## Design

Extend the single `ASMTask::Power` emission block (no new tasks, no new
BuildTasks — the wave-17 `BuildTask::Power` mapping is reused for all 6
rows).

### Source widening (both operands)

```
bByteWordSource = (dwP1Type==4||5||104||105)          // MOVZX EAX,AL
bWordSource     = (dwP1Type==6||106)                  // MOVZX EAX,AX
bInt64Source    = (dwP1Type==9||109)                  // REX.W CVTSI2SD
bFloatSource    = (dwP1Type==2||102)                  // MOVD + CVTSS2SD
bDoubleSource   = (dwP1Type==8||108)                  // already double
```

- byte/word: `MOVZX EAX,AL/AX` then `CVTSI2SD XMM0,EAX`
- int64: `CVTSI2SD XMM0,RAX` (load already produced RAX)
- float/double: unchanged from wave 17

### Result conversion (P3)

- float: unchanged (CVTSD2SS + spill through `@$_TEMPA_`)
- double: `CVTSD2SS XMM0,XMM0` + `CVTSS2SD XMM0,XMM0` then the MOVSD store
  (matches the `(float)` round-trip in PowerOOO)
- int64: `CVTTSD2SI RAX,XMM0` (REX.W) then 8-byte store
- int/dword/byte/word: unchanged (CVTTSD2SI EAX + width store — the store
  width does the `(unsigned char)/(WORD)/(DWORD)` truncation, same
  reasoning as wave 18)

### Registration

The 6 rows flip `AddCommandCore` → `AddBuildCommand` with
`BuildTask::Power`; `MathOp::WriteDBM` already maps
`BuildTask::Power` → `ASMTask::Power`.

## Semantics notes

- B/Y sources never sign-extend: byte/boolean are unsigned (0-255),
  matching the shared `?PowerBBB@@YAKKK@Z` entry.
- `(DWORD)`/`(unsigned char)`/`(WORD)` truncation of the truncated int32
  matches the C++ casts (wave-18 reasoning); the low-byte/word store is
  exact for in-range results and mod-256/65536 for out-of-range ones.
- Negative bases produce NaN through log (documented wave-17 domain note);
  `(LONGLONG)`/`(int)` of NaN is INT64_MIN/INT_MIN — same as CRT `pow`
  through the int casts.
- PowerOOO's float round-trip is preserved, so the observable double is
  float-precision like the DLL's.

## Verification

- Task-level: `WriteASMTaskCore(Power)` per operand type asserts the
  MOVZX / REX.W CVT byte sequences and the absence of any `Power*` DLL
  reference; the double case asserts CVTSD2SS+CVTSS2SD.
- Compiled: `b=b^b`, `w=w^w`, `d=d^d`, `dd=dd^dd`, `r=r^r` with no-DLL
  guards and msvcrt `,log`/`,exp` command-table presence (wave-17 pattern).
