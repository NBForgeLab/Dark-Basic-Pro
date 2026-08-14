# Wave 5 — 64-bit String Pointers: varspace & the String Manager

Date: 2026-08-11 · TDD (superpowers) · x64-only migration wave 5

## 1. Problem

In Dark Basic Pro a **string variable's value is a heap pointer** (`new char[]`).
On x86 the pointer fits in a 4-byte slot; on x64 it is a full 8-byte address.
The varspace slots were widened to 8 bytes in wave 2/3
(`CStructTable::SetStructDefaults` → `AddStruct(3, "string", 'S', dwTargetAddressSize)`),
but the **data paths that read/write string values still move 32 bits**:

| Path | x86 bytes | Today on x64 | Correct x64 |
|------|-----------|--------------|-------------|
| `MOV EAX, [moffs]` (global `@name`) | `A1 <moffs32>` | `A1 <moffs64>` (32-bit load!) | `48 A1 <moffs64>` (MOV RAX, moffs64) |
| `MOV [moffs], EAX` | `A3 <moffs32>` | `A3 <moffs64>` (32-bit store!) | `48 A3 <moffs64>` |
| `MOV EAX, [EBP+disp]` (function param) | `8B 85 <disp>` | same (32-bit) | `48 8B 85 <disp>` |
| `MOV [EBP+disp], EAX` | `89 85 <disp>` | same (32-bit) | `48 89 85 <disp>` |
| `MOV EAX, [ECX+disp]` (UDT member) | `8B 81 <disp>` | same (32-bit) | `48 8B 81 <disp>` |
| `MOV [ECX+disp], EAX` | `89 81 <disp>` | same (32-bit) | `48 89 81 <disp>` |
| `MOV EAX, [ECX]` (pointer deref) | `8B 08` | same (32-bit) | `48 8B 08` |
| `MOV EAX, [EAX]` (UDT deref) | `8B 00` | same (32-bit) | `48 8B 00` |
| `MOV ECX, EAX` (value guard) | `8B C8` | same (32-bit) | `48 8B C8` |

Already correct (pin with tests):
- **String literals** — `MOVEAXIMM4` uses `ImmOrAddr` → `48 B8+rd imm64` + 8-byte reference
  slot (wave 2), patched with the full pointer (`PatchReferenceValues` writes `sizeof(void*)`).
- **Stack pushes** — `PUSHEAX` = `0x50` is `push rax` in 64-bit mode (8 bytes, full RAX).
- **Varspace slot width** — `EstablishVarOffsets` uses `GetSizeOfType("string")` = 8 on x64.

The runtime side truncates too: the string manager in `DBDLLCore.cpp` stores string
addresses in `DWORD` (`EquateSS`, `FreeSS`, `FreeStringSS`, `AddSSS`, `CreateSingleString`,
`PTR_FuncCreateStr`), and the compiler's command table resolves them by **mangled name**
(`?EquateSS@@YAKKK@Z`) — so a 64-bit heap pointer gets cut to 32 bits at the ABI boundary.

## 2. Design decisions

### 2.1 New QWORD opcode family (emitter, type 3 only)

The existing `*4` forms must stay 32-bit for dword/byte/word types (their varspace slots
are 4/2/1 bytes; widening the move would read past the slot). Therefore strings get their
own REX.W variants, appended to the `ASMOp` enum (no renumbering):

| Opcode | Bytes | Meaning |
|--------|-------|---------|
| `MOVEAXMEM8`    | `48 A1 <moffs64>`        | `mov rax, [moffs]`   (global string load) |
| `MOVMEMEAX8`    | `48 A3 <moffs64>`        | `mov [moffs], rax`   (global string store) |
| `MOVEAXEBP8`    | `48 8B 85 <disp32>`      | `mov rax, [rbp+disp]` (string param load) |
| `MOVBPEAX8`     | `48 89 85 <disp32>`      | `mov [rbp+disp], rax` (string param store) |
| `MOVEAXECXOFF8` | `48 8B 81 <disp32>`      | `mov rax, [rcx+disp]` (UDT member load) |
| `MOVECXOFFEAX8` | `48 89 81 <disp32>`      | `mov [rcx+disp], rax` (UDT member store) |
| `MOVEAXECXREL8` | `48 8B 08`               | `mov rax, [rcx]`      (deref) |
| `MOVEAXEAXREL8` | `48 8B 00`               | `mov rax, [rax]`      (deref) |
| `MOVECXEAX8`    | `48 8B C8`               | `mov rcx, rax`        (value guard) |

Each is defined with `OpcodeExpansion::RexW` (emits the `0x48` prefix) and its existing
data encoding (`Abs64` for moffs → 8-byte slot; `Imm32` for disp32).

### 2.2 `DetermineASMCall` maps type 3 → QWORD

`CTaskEmitter::DetermineASMCall(base, type)` gets a `type == 3` branch that returns the
exact 8-byte variant for each memory base opcode. Existing types keep the size-code
arithmetic (0/1/2/3 → byte/word/dword/qwordx2). `DetermineASMCallForREL` is **unchanged**
(see §2.4).

The two `WriteASMEAXtoX` Rel-store paths that move the value into ECX unconditionally
(`MOVECXEAX4`) special-case `dwPType == 3` → `MOVECXEAX8`.

### 2.3 Runtime string manager → `uintptr_t`

- `DBDLLCore.cpp`: `EquateSS`, `FreeSS`, `FreeStringSS`, `AddSSS` take/return `uintptr_t`
  for string addresses; `CreateSingleString(DWORD* → uintptr_t*)` stores `(uintptr_t)new char[]`.
- `globstruct.h`: `PTR_FuncCreateStr` → `(uintptr_t*, DWORD)`; the two inline
  `reinterpret_cast<DWORD*>` helpers → `uintptr_t*`.
- `globstruct.cpp`: cast updates at the checklist/string call sites.

This changes the MSVC x64 mangled names (`K` = DWORD, `_K` = uintptr_t):
- `?EquateSS@@YAKKK@Z` → `?EquateSS@@YA_K_K_K@Z`
- `?FreeSS@@YAKK@Z`   → `?FreeSS@@YA_K_K@Z`

The compiler's command table (`InstructionTable.cpp` entries for `AssignSS`/`StrFree`) and
the hard-coded `WriteASMCall(...)` sites (`ParseInit.cpp`, `ParseInstruction.cpp`,
`ParseUserFunction.cpp`) are updated in lock-step so the emitted `GetProcAddress` name
matches the runtime export.

### 2.4 Active target ABI flip (TDD discovery)

The varspace-width test failed immediately after the emitter work: `ActiveTargetAbi`
was still `TargetAbi32`, so `SetStructDefaults()` sized string slots at **4 bytes**
despite the x64 emitter. Flipped to `TargetAbi64` (project is x64-only; the alias is
the single switch). This also fixes the two `dbp::abi::ReadPointer` sites
(`MakeVarValuesForTransfer`, EXEBlock dynamic-var cleanup) which read 4-byte slots
from an 8-byte varspace. Existing tests pinning the 32-bit default
(`test_target_abi.cpp`, `test_plugin_build_matrix.cpp`) were updated to the x64
contract, and a default-ABI full-width `ReadPointer` test was added.

While building the plugin chain, two pre-existing legacy-SDK incompatibilities that
blocked x64 compilation were fixed: `GWL_WNDPROC`/`SetWindowLong` →
`GWLP_WNDPROC`/`SetWindowLongPtr` in `DBDLLCore.cpp`, and
`GCL_HICON`/`GetClassLong` → `GCLP_HICON`/`GetClassLongPtr` in `CGfxC.cpp`.

### 2.5 Deferred to wave 6 (runtime array ABI)

String **arrays** stay 32-bit for now, deliberately:
- `CreateArray` writes `pRef[r] = (DWORD)pDataPointer` — the ref table is always
  `dwSizeOfArray * 4` bytes, truncating element addresses on x64. This is a runtime layout
  change (8-byte ref entries) that belongs to the array-ABI wave.
- Consequently `DetermineASMCallForREL(103)` keeps size code 2 and `MOVEAXSIB4` stays
  `8B 04 98` — internally consistent with the 4-byte ref table, no regression.
- The `CalcArrayOffset` slot load (`MOVEAXMEM4` for the array pointer) is likewise deferred.

Also deferred: `type 10/20` label slots (code-label addresses, separate label system),
x87→SSE (separate wave), and `_ESP_`/varspace-runtime pointer plumbing beyond the
string manager.

## 3. Test surface (TDD)

Byte-level (task-emitter driven, like waves 3/4):
1. Global string load: `WriteASMXtoEAX(Mem, "@name", 3)` → `48 A1` + 8-byte moffs slot.
2. Global string store: `WriteASMEAXtoX(Mem, "@name", 3)` → `48 A3` + 8-byte slot.
3. Param string load: `WriteASMXtoEAX(Ebp, "@:4", 3)` → `48 8B 85` + disp32.
4. Param string store: `WriteASMEAXtoX(Ebp, "@:4", 3)` → `48 89 85` + disp32.
5. String literal: `WriteASMXtoEAX(Imm, "hello", 3)` → `48 B8` + 8-byte ref slot.
6. UDT member (MemOff/EbpOff): `48 8B 81` / `48 89 81` with disp32.
7. UDT deref (MemRel/EbpRel): `48 8B 00`/`48 8B 08` load, `48 8B C8`+`48 89 08` store.
8. Stack push: `WriteASMEAXtoX(Stack, ..., 3)` → `0x50`.
9. **Non-regression**: dword (7) stays `A1`/`8B 85`; byte (5) stays byte-width; type 9
   stays 64-bit-two-slot; string array (103) unchanged (32-bit).
10. Varspace widths: string var = 8, integer = 4, string array = 8 (`EstablishVarOffsets`).
11. Command table: `GetRef(AssignSS)->GetDecoratedName()` = `?EquateSS@@YA_K_K_K@Z`;
    `StrFree` = `?FreeSS@@YA_K_K@Z`.
12. Reference patch: a string-literal `48 B8` slot patches the full 8-byte pointer
    (address > 4 GiB survives).

## 4. Risks & mitigations

- **Mangled-name contract**: both sides live in this repo (compiler table + DBDLLCore
  source); they are changed together and the name test pins the compiler side.
- **32-bit types untouched**: only `type == 3` routes to the REX.W variants; all other
  types keep the exact current byte streams (test 9).
- **Enum safety**: new opcodes are appended with explicit values; no renumbering.
