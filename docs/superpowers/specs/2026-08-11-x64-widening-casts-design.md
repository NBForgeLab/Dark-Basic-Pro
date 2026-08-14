# Design — Wave 19: Native widening casts (B/Y/W → L/W/D/F/O)

## Problem

16 dbprocore.dll rows widen byte/boolean/word sources:

| Row | Source (C++ type) | Target | Semantics |
| :-- | :-- | :-- | :-- |
| `CastBTOL` | `unsigned char` | long | zero-extend 8→32 |
| `CastBTOW` | `unsigned char` | word | zero-extend 8→16 |
| `CastBTOD` | `unsigned char` | dword | zero-extend 8→32 |
| `CastBTOF` | `unsigned char` | float | zero-extend → `(float)` |
| `CastBTOO` | `unsigned char` | double | zero-extend → `(double)` |
| `CastYTOL/YTOW/YTOD/YTOF/YTOO` | `unsigned char` | same 5 | same (Y rows share the B DLL entries) |
| `CastWTOL` | `unsigned short` | long | zero-extend 16→32 |
| `CastWTOD` | `unsigned short` | dword | zero-extend 16→32 |
| `CastWTOF` | `unsigned short` | float | zero-extend → `(float)` |
| `CastWTOO` | `unsigned short` | double | zero-extend → `(double)` |
| `CastWTOB` / `CastWTOY` | `unsigned short` | byte | truncate (wave-18 `CastToNarrow`) |

Verified against `DBDLLCore.cpp` (`CastBtoL(unsigned char)`,
`CastWtoL(WORD)`, ...). The Y rows literally register the same mangled
`?CastBtoL@@YAKE@Z` entries, so byte/boolean semantics are **unsigned** —
zero-extension, never sign-extension.

## Why a new opcode is needed

`WriteASMXtoEAX` loads at the source width through the size ladder
(`MOVEAXMEM1` = `A0` MOV AL,[abs]; `MOVEAXMEM2` = `66 A1` MOV AX,[abs]).
On x86-64 writing AL/AX leaves the rest of EAX untouched, so a plain
"load + wider store" (the wave-18 `CastToNarrow` shape) would store garbage
in the high bits. Wave 18 only worked for narrowing because a truncating
store reads the low byte/word.

## Design

Two new ASMTasks and two new opcodes:

### Opcodes (register form — mode-agnostic)

- `MOVZXEAXAL` = `0F B6 C0` (MOVZX EAX, AL) — zero-extend a loaded byte
- `MOVZXEAXAX` = `0F B7 C0` (MOVZX EAX, AX) — zero-extend a loaded word

The register forms were chosen over memory forms (`0F B6 05` modrm/RIP
variants per mode) so a single opcode pair serves every addressing mode the
loader can produce (Mem, MemOff, Ebp, EbpOff, MemArr, EbpArr, MemRel,
EbpRel, Imm) — the load stays `WriteASMXtoEAX`, the extension is one
instruction.

### Task `CastWiden` (616) — B/Y/W → L/W/D

```
WriteASMXtoEAX(P1);            // MOV AL/AX [src]  (source width)
WriteASMLine(MOVZXEAXAL/AX);   // zero-extend to 32 bits
WriteASMEAXtoX(P3);            // store at target width
```

Covers `CastBTOL/BTOW/BTOD`, `CastYTOL/YTOW/YTOD`, `CastWTOL/WTOD`.

### Task `CastWidenToFloat` (617) — B/Y/W → F/O

```
WriteASMXtoEAX(P1);            // MOV AL/AX [src]
WriteASMLine(MOVZXEAXAL/AX);   // zero-extend to 32 bits
if (target float) {
    CVTSI2SS XMM0,EAX;         // wave-8 pattern
    MOVSS [@$_TEMPA_],XMM0;
    MOV EAX,[@$_TEMPA_];
} else {
    CVTSI2SD XMM0,EAX;         // double path stores via MOVSD
}
WriteASMEAXtoX(P3);            // 4- or 8-byte store
```

Covers `CastBTOF/BTOO`, `CastYTOF/YTOO`, `CastWTOF/WTOO`.

### Registration

- 16 rows flip `AddCommandCore("+cast","dbprocore.dll",...)` →
  `AddBuildCommand("+cast", "CASTBTOL", "B", 1, 1, <InternalInstruction>,
  <BuildTask>)`.
- `CastWTOB`/`CastWTOY` reuse the existing `BuildTask::CastToNarrow`
  (source word ≥ target byte — pure truncation, low-byte store is correct).
- `MathOp::WriteDBM` maps `BuildTask::CastWiden`→`ASMTask::CastWiden` and
  `BuildTask::CastWidenToFloat`→`ASMTask::CastWidenToFloat`.

### Task count

Two new tasks + one reused task cover all 16 rows.

## Semantics notes

- B/Y sources never sign-extend: matches `(int)(unsigned char)` and the
  shared DLL entries.
- B→W stores AX after MOVZX (low 16 bits of the zero-extended value) —
  correct `(WORD)(unsigned char)`.
- W→B/Y keep the wave-18 store-width truncation — `(unsigned char)(WORD)`.
- No change to L/F/O/D/R-source casts (waves 8/15/16/18 untouched).

## Verification

- Task-level: `WriteASMTaskCore` with a `@b`/`@w` source asserts the
  `0F B6 C0` / `0F B7 C0` byte sequences and the absence of any `CastBto*`
  /`CastWto*`/`CastYto*` reference.
- Compiled: `a=b`, `a=w`, `d=b`, `f#=b`, `dbl=w`, `w=b`, `b=w` programs with
  no-DLL guards; `b=w` asserts no `CastWtoB` reference (truncation reuse).
