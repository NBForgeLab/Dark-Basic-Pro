# x64-Native Opcode Emission Design

> Status: approved — implements wave 2 of the x64-only transition (see `docs/17_x64_only_transition_research.md`, roadmap item 1: "opcode table + emission").

## Problem

The `CASMWriter` opcode table (`GenerateASMCodes`, 210 entries) stores each
instruction as three raw bytes (`preOp`, `op1`, `op2`) plus a boolean
"has data". `CreateASMMiddleCore` writes those bytes followed by a *4-byte*
placeholder that the runtime patches — for **values** (immediates) and for
**addresses** (variables, strings, commands, data) alike. On x64:

1. **Addresses are 64-bit.** A 4-byte placeholder truncates every runtime
   address (function pointers, varspace pointers, string pointers).
2. **`[disp32]` absolute operands became RIP-relative.** Instructions such as
   `8B 1D disp32` (`MOV EBX, [var]`) silently change meaning in 64-bit mode.
3. **`PUSHAD`/`POPAD` (0x60/0x61) do not exist in 64-bit mode.**
4. **`MOV r, imm32` cannot carry an address** (`B8+rd` truncates).
5. The `.dbpro` on-disk format has exactly three reference arrays
   (position/type/index) and no version field — the patch semantics must be
   expressible with those arrays unchanged.

## Design

### 1. Structured opcode descriptor

Replace the four parallel arrays (`m_ASMDebugStrings`, `m_iASMPreOp`,
`m_iASMOp1`, `m_iASMOp2`, `m_bASMOpData`) with one descriptor vector. Every
entry states its operand data encoding explicitly.

```cpp
enum class DataEncoding : uint8_t {
    None = 0,    // no operand data
    Imm8,        // 1-byte value slot  (immediate / displacement constant)
    Imm16,       // 2-byte value slot
    Imm32,       // 4-byte value slot  (also code-label rel32 slots)
    Abs64,       // 8-byte absolute-address slot (moffs A0-A3 only)
    ImmOrAddr,   // MOV r, imm: 4-byte value slot, or 48 B8+rd imm64 for addresses
    PtrIndirect, // expand to "MOV RBX, imm64" + [RBX] operand (x64 rewrite)
};

enum class OpcodeExpansion : uint8_t {
    None,
    PushAll,     // PUSHAD  -> PUSH RAX,RBX,RCX,RDX,RSI,RDI,RBP
    PopAll,      // POPAD   -> POP RBP,RDI,RSI,RDX,RCX,RBX,RAX
};

struct ASMOpcodeDef {
    const char*     name;
    int             preOp;        // legacy prefix (0x66) or -1
    int             op1;          // first opcode byte, or -1
    int             op2;          // ModRM / second opcode byte, or -1
    DataEncoding    data1;
    DataEncoding    data2 = DataEncoding::None;
    OpcodeExpansion expansion = OpcodeExpansion::None;
};
```

### 2. Data slot widths (the core rule)

The emitter decides the placeholder width **per reference label kind** at
emission time, using `ParseReferenceLabel` (the same parser the runtime uses):

| Reference kind           | Slot width | Encoding form |
|--------------------------|-----------|---------------|
| Immediate (`"42"`)       | 4 bytes   | value written verbatim |
| CodeLabel (`$label...`)  | 4 bytes   | rel32 patched at runtime (kind 5, unchanged) |
| Variable / String / Command / Data (`@v`, `$$1`, `[1`) | `sizeof(void*)` = 8 bytes | 64-bit absolute address |

The runtime patch rule is therefore uniform and needs **no new arrays and no
format change**:

```
kind 1,2,3,6 (address kinds) -> write sizeof(void*) bytes at position
kind 4 (Immediate)           -> write 4 bytes (value) at position
kind 5 (CodeLabel)           -> write int32 rel32 at position  (existing)
```

The slot width always matches because the emitter and the runtime derive it
from the same parsed label kind.

### 3. Per-form x64 emission rules

| x86 form (examples) | x64 emission |
|---|---|
| moffs `A0-A3` (`MOVEAXMEM4`, `MOVMEMEAX4`) | unchanged bytes, **8-byte** address slot (64-bit mode moffs is 8-byte) |
| `MOV r, imm32` (`B8+rd`: `MOVEAXIMM4`, `MOVEBXIMM4`, `MOVECXIMM4`, `MOVEDXIMM4`) | value → `B8+rd imm32` (5B); address → `48 B8+rd imm64` (10B) |
| `[disp32]` absolute (modrm `mod=00, rm=101`): `MOVEBXMEM4` `8B 1D`, `MOVMEMEBX4` `89 1D`, `MOVMEMIMM*` `C7 05`, `MOVMEMESP4`/`MOVESPMEM4`, `MOVMEMST08`/`MOVST0MEM8`, `INCMEM*`/`DECMEM*` `FF 05/0D`, `JMPREL` `FF 25` | expand to `48 BB <imm64>` + same opcode with modrm rm `101→011` (`modrm & 0xF8 \| 3`) — the `[RBX]` form. The address is embedded as a true 64-bit immediate. |
| register-relative `[reg+disp32]`, reg-reg, reg-imm (179 forms) | byte-identical (32-bit operand semantics; in 64-bit mode the base registers are the full 64-bit registers) |
| `PUSHAD`/`POPAD` | explicit `PUSH RAX,RBX,RCX,RDX,RSI,RDI,RBP` / reverse (all one-byte, no REX needed) |
| jumps `E9`/`0F 8x` rel32 (`JMP`, `JNE`, `JE`, `JGE`, `JLE`, `CALLABS` `E8`) | unchanged — CodeLabel rel32, code lives inside one MCB so ±2GB holds |

The `[RBX]` expansion makes the trailing-immediate RIP-relative problem
(`C7 05 disp32 imm32`) disappear: the store form is `C7 03 imm32` after the
address load, so **every rel32 slot has base `pos + 4`** and the runtime math
stays `rel32 = target - (pos + 4)` exactly as today.

### 4. Runtime changes (`CEXEBlock::RunProgram`)

- `pProgramRefPtr` becomes `uintptr_t[]`; all address stores become
  `uintptr_t` (fixes 8 truncation sites: `(DWORD)dwAdd`, `(DWORD)pStr`,
  `(DWORD)(m_pVariableSpace+index)`, `(DWORD)(m_pDataSpace+(index*10))`,
  the four special-var pointers).
- The patch loop writes `sizeof(void*)` bytes for address kinds.
- `g_pGlob->g_pMachineCodeBlock` stops truncating to `DWORD`.

### 5. Explicitly deferred (later waves)

- Frame migration (`MOV RBP, RSP` with REX.W, 64-bit stack addressing) — wave 3.
- Calling convention (register args + shadow space) — wave 3.
- `PUSH [RBP+disp]` pushes 8 bytes on x64 (stack-slot semantics) — wave 3.
- Array ABI in the runtime DLLs — wave 4.

## Compatibility

- `ICodeGenerator`/`CASMWriter` public emission API (`WriteASMLine*`,
  `WriteASMTaskCore*`, `DetermineASMCall*`, `DetMode`) is unchanged.
- `CreateASMMiddle(iPreOp, iOp1, iOp2, data)` stays as the raw byte emitter
  (used by `CLeapMarkerManager` for precomputed offsets and by tests) with a
  fixed Imm32 data slot.
- `CreateASMMiddleCore` changes signature from raw bytes to `dwASMCode`
  (code-driven x64 emission); only `CASMWriter` implements it.
- The `.dbpro` on-disk format is unchanged (same three reference arrays;
  kinds 1-6 semantics preserved; width decided by `sizeof(void*)`).
