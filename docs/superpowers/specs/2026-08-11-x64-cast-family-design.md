# Wave 16 — Native int64 <-> float cast family (SSE2/REG64)

**Goal**: Convert every remaining `Cast*ToR` / `CastRto*` runtime DLL call in
the instruction table to emitter-native instructions.

## Current state (pre-wave)

Waves 8/15 already converted the integer-family casts:

| Cast | Source → Target | Before wave 16 |
|---|---|---|
| `CastLToF` (int→float) | 4 → 3 | native (wave 8, CVTSI2SS) |
| `CastLToO` (int→double) | 4 → 8 | native (wave 8, CVTSI2SD) |
| `CastDTOF`/`CastDTOO` (dword→float/double) | 6 → 3/8 | native (wave 8) |
| `CastFToL`/`CastFToO` (float→int/double) | 3 → 4/8 | native (wave 8) |
| `CastOToL`/`CastOToF` (double→int/float) | 8 → 4/3 | native (wave 8) |
| `CastLToR` (int→int64) | 4 → 9 | native (wave 15, MOVSXD RAX,EAX) |
| `CastDToR` (dword→int64) | 6 → 9 | native (wave 15, zero-extend) |

Still DLL (`?CastFtoR@@YA_JM@Z`, `?CastOtoR@@YA_JN@Z`, `?CastRtoL@@YAK_J@Z`,
`?CastRtoF@@YAK_J@Z`, `?CastRtoB@@YAK_J@Z`, `?CastRtoW@@YAK_J@Z`,
`?CastRtoD@@YAK_J@Z`, `?CastRtoO@@YAN_J@Z`, and the unsigned
`?CastBtoR@@YA_JE@Z`/`?CastWtoR@@YA_JG@Z`): the full int64<->float/double
boundary still round-trips through dbprocore.dll.

## Design

### 1. Opcodes — REX.W variants of the CVT family

The existing SSE2 CVT opcodes (waves 8) emit `REX.W` *before* the legacy
prefix, which is only valid for instructions with no legacy prefix. The
64-bit CVT forms need the legacy prefix *first*:

- `F3 48 0F 2C C0` = `CVTTSS2SI RAX,XMM0` (float→int64)
- `F2 48 0F 2C C0` = `CVTTSD2SI RAX,XMM0` (double→int64)
- `F3 48 0F 2A C0` = `CVTSI2SS XMM0,RAX` (int64→float)
- `F2 48 0F 2A C0` = `CVTSI2SD XMM0,RAX` (int64→double)

New `OpcodeExpansion::RexWAfterPrefix` writes preOp → 0x48 → op1 → op2 → modrm.

### 2. Task model

New ASMTasks (609-613) and BuildTasks (1109-1113):

| BuildTask | InternalInstructions | ASMTask | Emission |
|---|---|---|---|
| `CastFloatToInt64` | `CastFTOR` | `CastFloatToInt64` | load float bits → `MOVD XMM0,EAX` → `CVTTSS2SI RAX,XMM0` → store 8B |
| `CastDoubleToInt64` | `CastOTOR` | `CastDoubleToInt64` | load double into XMM0 → `CVTTSD2SI RAX,XMM0` → store 8B |
| `CastDwordToInt64` (existing 1108) | `CastDTOR` **+** `CastBTOR`, `CastYTOR`, `CastWTOR` | `CastDwordToInt64` | load at source width (zero-extends EAX) → store 8B |
| `CastInt64ToLower` | `CastRTOL`, `CastRTOD`, `CastRTOB`, `CastRTOY`, `CastRTOW` | `CastInt64ToLower` | load 8B into RAX → truncating store at target width |
| `CastInt64ToFloat` | `CastRTOF` | `CastInt64ToFloat` | load 8B into RAX → `CVTSI2SS XMM0,RAX` → `MOVSS [TEMP]` → `MOV EAX,[TEMP]` → store 4B |
| `CastInt64ToDouble` | `CastRTOO` | `CastInt64ToDouble` | load 8B into RAX → `CVTSI2SD XMM0,RAX` → `MOVSD [dst],XMM0` (store path) |

Semantics notes:

- unsigned byte/word/dword → int64 is **zero-extension**: any ≤32-bit load
  into EAX clears the upper half of RAX, so "load + 8-byte store" is exact.
  This matches the mangled DLL signatures (`E`/`G`/`K` unsigned args).
- int → int64 stays **sign-extension** (`MOVSXD`, wave 15, `H` signed arg).
- int64 → int/dword/byte/word is a **truncating store** at the target width,
  matching `?CastRtoL@@YAK_J@Z` (low 32 bits of the value).
- float/double → int64 uses the **truncating** CVT (CVTT*) to match the C++
  `(long long)float` semantics of the DLL.

### 3. Registration flips

`InstructionTable.cpp`: the nine rows move from `AddCommandCore("+cast",
"dbprocore.dll", ...)` to `AddBuildCommand("+cast", "CAST*", type, 1, 1,
InternalInstruction::Cast*, BuildTask::*)`. The `.rc` name conflict is
resolved by the existing wave-11 precedence (internal x64 entries win).

### 4. Blast radius

- `MathOp.cpp` `WriteDBM`: five new BuildTask→ASMTask mappings.
- `ASMWriter.h/.cpp`: 4 ASMOps, 5 ASMTasks, 4 DefineASM rows, 5 emission
  blocks, the `RexWAfterPrefix` expansion branch.
- `InstructionTable.h`: 5 BuildTask IDs.
- Result/store width follows the existing convention: `WriteASMEAXtoX` uses
  the P3 (result) type to pick the store width; `WriteASMXtoEAX` uses the P1
  (source) type for the load. `CastInt64ToLower` therefore serves all five
  narrowing targets with the exact same two-line emission.

### 5. Out of scope (unchanged)

- `Power` remains a DLL call (no single-instruction x64 form; int math too).
- Other narrowing casts (`CastLToB` etc.) stay DLL — they are correct-width
  value semantics, not x64 truncation, and out of the R-family scope.

## TDD discoveries (implemented)

1. **Legacy prefix must precede REX.W.** The pre-wave emitter wrote `0x48`
   before the F2/F3/66 prefix, which is only valid for instructions with no
   legacy prefix. Added `OpcodeExpansion::RexWAfterPrefix` (preOp → 0x48 →
   op1 → op2 → modrm); all four new CVT opcodes use it. Unit byte tests
   assert the exact `F3 48 0F 2C C0` order.
2. **DBPro's 8-byte float type is `"double float"`, not `"double"`.**
   `dim d as double` silently declares an *integer*, so `r=d` initially
   compiled through `CastIntToInt64` (MOVSXD) instead of CVTTSD2SI. The
   compiled tests now use `dim d as double float`.
3. **`ASMMAXCOUNT` grew 300 → 301** for the four new opcodes; the
   `AsmMaxCountValue` characterization test was updated in lockstep.
4. **Zero-extension is free**: any ≤32-bit load into EAX clears the upper
   half of RAX, so `byte/word/dword → int64` needs no conversion instruction
   — the shared `CastDwordToInt64` load+8-byte-store emission is exact and
   matches the unsigned `E`/`G`/`K` mangled argument types.

## Result

All 9 remaining R-family DLL rows are now `AddBuildCommand`. `float→int64`,
`double→int64`, `int64→float`, `int64→double` emit native SSE2 CVT* with
REX.W; `int64→int/dword/byte/word` emit a truncating store; `byte/word→int64`
emit a zero-extending load+store. 15 tests; full suite 1073/0/1; ctest 100%.
