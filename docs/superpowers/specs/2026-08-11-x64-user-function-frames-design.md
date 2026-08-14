# Wave 4 — x64 Custom User-Function Frames (Design)

Date: 2026-08-11 · Superpowers TDD methodology · Precedes any implementation.

## 1. Goal

Convert the custom user-function frame model from the 32-bit convention to the
64-bit model, **without changing the `.dbpro` format or the compiler's data
structures**: every stack slot becomes 8 bytes, the frame base register (RBP)
becomes full-width via REX.W, and every EBP-relative displacement formula in
the emitter is scaled by ×2. TDD tests pin the byte-level contract first.

## 2. Current 32-bit model (verified in source)

### 2.1 Prologue / epilogue (ParseUserFunction.cpp — `WriteDBM`)

```
TOP:   PUSH EBP            (55)
       MOV  EBP,ESP        (89 E5)
       SUB  ESP,size+4     (81 EC imm32)     ; size = GetSizeOfType(name)
       ClearStack(locals)                    ; zero the local region
BOTTOM: ... return value into EAX/EDX/ST0 ...
       MOV  ESP,EBP        (89 EC)
       POP  EBP            (5D)
       RET                 (C3)
```

### 2.2 DEC-chain offsets

`CStructTable::CalculateSize` assigns 4-byte-unit offsets. A user function's
chain is: `[returnvalue @0] [param0 @4] [param1 @8] ... [locals ...]`.

### 2.3 x86 frame layout

```
EBP+4+4k  : param k          (k=0 → EBP+8)
EBP+0     : saved EBP
EBP-4     : returnvalue
EBP-8-4m  : locals
```

Displacement formulas (all in ParseUserFunction.cpp / Str.cpp):

| Region | x86 formula |
|---|---|
| returnvalue (dwOffset==0) | `-4` |
| string param (special recreate) | `4 + dwOffset` |
| local var | `(lastParamOffset - dwOffset) - sizeOfData` |
| UDT field within var/param | `iGlobalDisplacement + dwOffset` |
| prologue local size | `dwTypeSize + 4` |
| ClearStack byte count | `dwSizeOfLocalParams` |
| caller cleanup after call | `dwMustPopStack * 4` |

`@:disp` is decoded as `[EBP+disp]` (opcodes `MOVEAXEBP1/2/4`,
`MOVEBXEBP1/2/4`, `MOVEBPIMM1/2/4`, `MOVEBPST08`/`MOVST0EBP8`).

## 3. x64 model

### 3.1 Stack slot size doubles

The wave-3 caller pushes every argument as an **8-byte slot** (doubles/int64
span two slots). The callee sees, after `PUSH RBP; MOV RBP,RSP`:

```
RBP+16+8k : param k
RBP+8     : return address
RBP+0     : saved RBP
RBP-8     : returnvalue
RBP-16-8m : locals
```

Because the DEC chain is still expressed in 4-byte units, **every displacement
doubles**. The uniform mapping `x64_disp = 2 × x86_disp` holds for every case:

| Region | x86 | x64 |
|---|---|---|
| returnvalue | `-4` | `-8` |
| string param | `4 + dwOffset` | `8 + 2·dwOffset` |
| local var | `(L - dwOffset) - size` | `2·((L - dwOffset) - size)` |
| UDT field | `base + dwOffset` | `base + 2·dwOffset` |
| prologue local size | `typeSize + 4` | `2·(typeSize + 4)` |
| ClearStack bytes | `locals` | `2·locals` |
| caller cleanup | `slots·4` | `slots·8` |

Sanity check, 2-param function: param0 → `8+2·4 = 16` ✓, param1 → `8+2·8 = 24` ✓.

The compiler-level test compiles `function add(a,b)` (chain: returnvalue@0,
a@4, b@8, reported type size 16 with the filler) and observes the real stream:
prologue `55 48 89 E5 48 81 EC 28 00 00 00` (SUB RSP, 2·(16+4)=40), ClearStack
`... B9 10 00 00 00 FC F3 AA` (2·8=16 bytes), param1 store `89 85 18 00 00 00`
(RBP+24), param0 load `8B 85 10 00 00 00` (RBP+16), epilogue `48 89 EC 5D C3`.

### 3.2 Data access width stays 32-bit per slot

The `MOV*EBP1/2/4` opcodes use `[RBP+disp32]` ModRM with 32-bit operands —
already valid x64 (RBP addressing needs no REX for 4-byte operands). Doubles
are accessed as two DWORDs at `disp`/`disp+4`, matching the wave-3 two-slot
push layout. **No opcode byte changes needed for the access path.**

### 3.3 Opcode changes (REX.W where the stack pointer is an operand)

| Opcode | x86 bytes | x64 bytes | Why |
|---|---|---|---|
| `SUBESP` | `81 EC imm32` | `48 81 EC imm32` | SUB RSP,imm32 (REX.W) |
| `ADDESP` | `81 C4 imm32` | `48 81 C4 imm32` | ADD RSP,imm32 (REX.W) |
| `MOVEAXESP` | `89 E0` | keep `89 E0` | 32-bit truncated ESP compares (Return/SetNoReturn safe checks); shared opcode |
| `MOVSIB4IMM4/1` | `C7/C6 04 88/08` | unchanged (still in the table) | SIB base/index are implicitly 64-bit in long mode |
| `MOVECXIMM4` | `B9 imm32` | unchanged | MOV ECX,imm32 zero-extends into RCX |
| `LOOP` | `E2 rel8` | unchanged | decrements RCX in 64-bit mode |
| `PUSHEBP`/`POPEBP` | `55`/`5D` | unchanged | PUSH/POP r64 need no REX |
| `CALLMEM`/`RET` | `E8 rel32`/`C3` | unchanged | |

### 3.3.1 ClearStack is rewritten to REP STOSB

The legacy ClearStack loop (`MOV ECX,count; MOV SIB[EAX:ECX*4],0; LOOP`)
couples the loop counter to the addressing index — with a truncated EAX base
and a rel8 that lands mid-instruction — so it cannot clear a region in bounds
on any architecture. The x64 prologue instead emits a 5-instruction sequence
that zeroes `[RSP, RSP+count)` exactly with independent registers:

```
48 89 E0        MOV RAX,RSP
33 C0           XOR EAX,EAX
48 8B FC        MOV RDI,RSP
B9 imm32        MOV ECX,count        (real value, no patch slot)
FC              CLD
F3 AA           REP STOSB
```

EDI is part of the saved register file (wave-3 prologue) and ClearStack only
runs inside the function prologue before any body code, so clobbering it is
safe. With ×2 scaling the byte count is always a multiple of 8, but REP STOSB
handles any count. No new table opcodes were needed; the task handler emits
raw bytes (bypassing the patch-slot machinery because the count is a
compile-time constant).

### 3.4 RSP tracker (wave-3 machinery, no changes needed)

`TrackStackForOpcode` already tracks `PUSHEBP` (+8), `POPEBP` (−8), `SUBESP`
(+size from the op data — now the doubled size flows automatically), `ADDESP`
(−size). `MOVESPEBP`/`MOVESPMEM4`/`SUBESPEAX` already poison alignment. The
caller cleanup `ADD RSP, slots·8` is likewise tracked. `JumpSubroutine`
already resets the pending call frame (wave 3), so the user-function param
pushes do not leak into the next DLL call's frame.

## 4. Caller side (unchanged structure, ×8 cleanup)

`CParseInstruction::WriteDBMBit` (type-3 user-function call): params pushed via
`ASMTask::Push` (8-byte slots, wave 3), `JumpSubroutine` → `CALLMEM` (E8 rel32),
then caller cleanup `dwMustPopStack*4` → **`dwMustPopStack*8`** via `ADDESP`
(now REX.W = ADD RSP,imm32).

## 5. Deferred (documented, not in this wave)

- String pointers crossing the varspace boundary (wave 5 — varspace x64).
- x87 FPU (`MOVEBPST08`/`MOVST0EBP8`) → SSE2.
- `PUSHEBP4` (`FF B5 imm32`, PUSH qword [RBP+disp32]) is already valid x64 but
  unused; kept as-is.
- UDT-by-value params (`PushUdt`) keep the poisoned/legacy path.
- `StoreEsp`/`RestoreEsp` (`_ESP_` DWORD) keep 32-bit truncation semantics.

## 6. Test plan (TDD — red first)

`tests/test_x64_user_function_frames.cpp`:

1. Prologue bytes: `55 48 89 E5 48 81 EC 20 00 00 00` + ClearStack sequence.
2. Epilogue bytes: `48 89 EC 5D C3`.
3. `SUBESP`/`ADDESP` carry REX.W (`48 81 EC/ C4 imm32`).
4. ClearStack uses `48 89 E0` (MOV RAX,RSP) and keeps `C7 04 88` + `E2` loop.
5. `@:` param access: `MOVEAXEBP4 "16"` → `8B 85 10 00 00 00`; `"24"` → `8B 85 18 00 00 00`.
6. `@:` local access: `"-8"` → `8B 85 F8 FF FF FF`; `"-16"` → `8B 85 F0 FF FF FF`.
7. Caller cleanup after user-function call: `ADD RSP,16` → `48 81 C4 10 00 00 00`.
8. Nested DLL call inside the frame aligns: frame `PushEbp+MovBpEsp+SubEsp(32)`
   then a 2-arg DLL call emits `48 83 EC 20` (no padding needed at RSP%16=0
   after 8+32) followed by RCX/RDX loads, `FF D3`, and the `48 83 C4 20`
   frame teardown.
9. Compiler-level: `MakeStatements` + statement `WriteDBM()` on a real
   `function add(a,b)` source; scan the machine-code stream for the prologue
   bytes and for the `[RBP+16]`/`[RBP+24]` param-access bytes.
