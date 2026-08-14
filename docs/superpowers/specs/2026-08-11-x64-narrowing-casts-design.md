# Wave 18 — Native narrowing casts (`Cast*toB/W/D`)

**Goal**: Replace the 13 narrowing-cast dbprocore.dll rows (sources L/F/O/D
→ targets byte/word/dword) with emitter-native truncating stores — the same
width-driven pattern established by `CastInt64ToLower` (wave 16).

## Current state (pre-wave)

The DLL rows and their exact C++ semantics (verified in DBDLLCore.cpp):

| Row | Source | Target | DLL body |
|---|---|---|---|
| `CastLToB`/`CastLToY` | int | byte | `(unsigned char)iValue` — truncate low byte |
| `CastLToW` | int | word | `(WORD)iValue` — truncate low word |
| `CastLToD` | int | dword | `(DWORD)iValue` — same bits |
| `CastFToB`/`CastFToY` | float | byte | `(unsigned char)fValue` — float→int, low byte |
| `CastFToW` | float | word | `(WORD)fValue` |
| `CastOToB`/`CastOToY` | double | byte | `(unsigned char)dValue` |
| `CastOToW` | double | word | `(WORD)dValue` |
| `CastDToB`/`CastDToY` | dword | byte | `(unsigned char)dwValue` — truncate low byte |
| `CastDToW` | dword | word | `(WORD)dwValue` — truncate low word |

All are pure value truncations (or bit-preserving 4→4 moves for L→D). No
string/heap involvement — fully emitter-native-able.

## Design

### 1. Task model

| ASMTask | BuildTask | Rows |
|---|---|---|
| `CastToNarrow` (614) | `CastToNarrow` (1114) | `CastLToB/Y/W/D`, `CastDToB/Y/W` |
| `CastFloatToNarrow` (615) | `CastFloatToNarrow` (1115) | `CastFToB/Y/W`, `CastOToB/Y/W` |

### 2. Emitter sequences

**`CastToNarrow`** (integer-family source L/D → byte/word/dword target):

```
WriteASMXtoEAX(P1)   ; 4-byte load into EAX (int or dword)
WriteASMEAXtoX(P3)   ; truncating store at the target width
```

The store width does the truncation — `MOV [dst],AL` (byte), `MOV
[dst],AX` (word), `MOV [dst],EAX` (dword) — exactly the C++ cast semantics.
L→D / D→L are same-width 4-byte moves.

**`CastFloatToNarrow`** (float/double source → byte/word target):

```
WriteASMXtoEAX(P1)        ; F: float bits in EAX ; O: double in XMM0
if float: MOVD XMM0,EAX
CVTTSS2SI EAX,XMM0        ; float -> int (truncating toward zero)
CVTTSD2SI EAX,XMM0        ; double -> int
WriteASMEAXtoX(P3)        ; truncating store at the target width
```

Identical to the wave-8 `CastFloatToInt`/`CastDoubleToInt` blocks, differing
only in the store width (the P3 type drives it).

### 3. Registration flips

13 rows `AddCommandCore("+cast", "dbprocore.dll", "?CastXtoY@@...")` →
`AddBuildCommand("+cast", "CASTXTOY", type, 1, 1, InternalInstruction::*,
BuildTask::*)`. `MathOp.cpp WriteDBM`: two new BuildTask→ASMTask mappings.

### 4. Out of scope

- `CastDToL` (dword→int, same-width move) — target L, outside the named
  B/W/D scope; trivially convertible later with the same task.
- The widening family (`CastBToL/F/W/D/O/R`, `CastYTo*`, `CastWTo*`,
  `CastLToR`, `CastDToR` — already native in waves 8/15/16) — not narrowing.
- String casts and comparisons remain DLL (heap-managed runtime).

## TDD discoveries (implemented)

1. **Two tasks cover all 13 rows.** Because `WriteASMEAXtoX` follows the P3
   type for the store width, `CastToNarrow` (load + width store) serves
   every L/D→B/Y/W/D row and `CastFloatToNarrow` (CVTT* + width store)
   every F/O→B/Y/W row — one emission block each, no per-row code.
2. **Zero new opcodes.** The float/double→int conversion reused the wave-8
   `CVTTSS2SI`/`CVTTSD2SI` opcodes; integer-family narrowing is pure
   load/store. `ASMMAXCOUNT` unchanged.
3. **B (boolean) and Y (byte) are distinct type letters but identical
   1-byte targets** — the shared task handles both through the P3 type.
4. **Compiled float/double→byte tests pass whether the compiler routes the
   cast directly or through an intermediate int cast** — the CVTT* bytes and
   the no-DLL guard hold in both cases, so the tests pin the requirement
   without over-constraining the routing.

## Result

All 13 narrowing rows are now `AddBuildCommand`. Integer-family sources
(L/D) emit a target-width truncating store; float-family sources (F/O) emit
the truncating CVTT* then the width store. 10 tests; full suite 1087/0/1;
ctest 100%. The only remaining arithmetic cast row is `CastDToL`
(dword→int, same-width move) — deliberately out of the named B/W/D scope.
