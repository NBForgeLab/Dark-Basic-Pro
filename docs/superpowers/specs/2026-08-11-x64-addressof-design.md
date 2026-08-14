# Wave 14 — `&var` (Address-Of) as a Full 8-Byte Address — Design

Date: 2026-08-11 (wave 14 of the x64-only transition)

## 1. Problem

`&variable` in user code returns the **address** of a variable. In the legacy
32-bit compiler this value was a 4-byte DWORD — correct when pointers are 4
bytes, **truncating on x64** where an address is 8 bytes.

Current (broken-on-x64) emission for `a = &b`:

```
48 A1 <moffs64>      MOV RAX,[@&b]       ; &b companion array pointer (8-byte slot)
3D ... 0F 84 ...     CMP EAX,0 / JE       ; array-exists check
BB 00 00 00 00       MOV EBX,0            ; element 0
48 8B 04 D8          MOV RAX,[RAX+RBX*8]  ; ref[0] = 8-byte element address
8B 88 00 00 00 00    MOV ECX,[RAX+0]      ; 4-byte VALUE read  ← truncation
8B C1                MOV EAX,ECX
```

The final `MOV ECX,[RAX+0]` reads only 4 bytes of the stored address. Even
`a = &b` with an **int64** target produces `PUSH EAX; CALL ?CastLtoR@@...`
(a dbprocore.dll call) that widens the already-truncated 4-byte value.

## 2. Why the type system conflates `&b` with `b(0)`

- `b(0)` (regular int array element): `DoValueComplexVariable` → result
  `{ name "@&b", type 100+1 = 101 }`.
- `&b` (address-of a scalar): `DoValueSingleVariable` → result
  `{ name "@&b", type 100+1 = 101 }`.

Identical label **and** identical type. The emitter (`ParamMode::MemArr` →
`WriteASMARRtoEAX`) decides the read width from the type via
`DetermineASMCallForREL` (101 → 4 bytes). In 32-bit this was correct because
the runtime stored a 4-byte address in the `&b` companion element; on x64 the
address needs 8 bytes, and the compiler cannot tell the two cases apart.

The type system already has the right vehicle: **type 107 = "DWORD POINTER /
RELATIVE ADDRESS TO A DWORD"** — a first-class type used in the math tables
(DWORD mode), the assignment table (`AssignDD`), and the cast table
(`CastLToD`). It is produced today only by the array-passed-whole paths
(math symbols 10002–10004), which **never** use the relative width helper
(they push array pointers / internal indexes, not element values).

## 3. Design

### 3.1 Parser — `&`-prefixed results become type 107

`CMathOp::DoValueSingleVariable` (both the global and the user-function-local
branches): when the variable name starts with `&` (address-of), set the result
type to **107** (full-width pointer) instead of `100 + baseType`.

- `&b` (int) → 107 (was 101).
- `b` (no prefix) → unchanged (1).
- `b(0)` (subscript, `DoValueComplexVariable`) → unchanged (101).
- `*p` (indirect, 200-range) → unchanged.

This is the *only* discriminator the emitter can see; it restores the
intended meaning of 107 ("this value is an address, not an element").

### 3.2 Emitter — 107 reads/writes move a full QWORD

`CTaskEmitter::DetermineASMCallForREL` (array value loads/stores) and
`CTaskEmitter::DetermineASMCall` (plain Mem/Ebp loads/stores): on x64, map
type 107 to the 8-byte (REX.W) opcodes, exactly like the existing
string/1002 full-width-pointer cases:

- `MOVECXEAXOFF1 → MOVECXEAXOFF8` (read)
- `MOVEAXECX1 → MOVEAXECX8` (read)
- `MOVEAXOFFECX1 → MOVEAXOFFECX8` (store)
- `MOVEAXMEM1 → MOVEAXMEM8` / `MOVMEMEAX1 → MOVMEMEAX8` (plain Mem)
- `MOVEAXEBP1 → MOVEAXEBP8` / `MOVEBPEAX1 → MOVEBPEAX8` (Ebp)

Result for `a = &b`:

```
48 A1 <moffs64>      MOV RAX,[@&b]
...                  bounds check
48 8B 04 D8          MOV RAX,[RAX+RBX*8]
48 8B 88 00 00 00 00 MOV RCX,[RAX+0]      ← 8-byte read (REX.W)
48 8B C1             MOV RAX,RCX
```

For `a(int64) = &b` the value is now already a full QWORD in RAX: the store
is a single `MOV [@a],RAX` — **no dbprocore.dll cast call at all**.

### 3.3 Argument casting — 107 needs no cast to integer-family types

`CParameter::CastAllParametersToInstruction`: a 107 source needs no cast when
the required type is an integer-family scalar (1, 4, 5, 6, 7, 9), alongside
the existing "array of required type" tolerance. Without this, `a = &b`
(integer target) would gain a spurious `?CastDtoL@@...` DLL call — a
regression against today's pure code. Floats (2/8) still cast via the
internal SSE2 `CASTDTOF` task.

Rationale: a pointer value carries its full 8 bytes on x64; the destination
(read/store) width follows the *destination* type, so a pointer→int-family
"cast" is a pure width choice, not a conversion.

## 4. Out of scope (documented follow-ups)

- **Address arithmetic** (`x = &b + n`): type 107 keeps DWORD-mode math
  (4-byte temporaries), matching today's int-mode behavior. Full 8-byte
  pointer arithmetic (type-mode 107 → int64) is a separate wave.
- **`&array(0)` / `&b(0)`** (subscripted address-of): flows through
  `DoValueComplexVariable` and the runtime element-address helper; already
  returns a full pointer for int64 targets. Unchanged.
- **PEEK/POKE**: live in the closed-source runtime command database; the
  compiler-side contract (8-byte address values) is what this wave delivers.

## 5. TDD plan and discoveries

1. RED: `tests/test_x64_addressof.cpp` — byte-level assertions:
   - `a = &b` emits `MOV RCX,[RAX+0]` (48 8B 88) + `MOV RAX,RCX` (48 8B C1),
     no `MOV ECX,[RAX]` (8B 88).
   - `a(int64) = &b` emits the 8-byte read followed by a single
     `MOV [@a],RAX` store — and **no** cast DLL reference at all.
   - `a(integer) = &b` stores 4 bytes, no cast DLL.
   - `b(0)` (regular int array element) still reads 4 bytes.
2. GREEN: parser + emitter + cast-rule changes above.
3. Full build + ctest + docs.

### TDD discoveries (post-implementation)

- **Task-level placeholders**: at `WriteASMTaskCore` level the leap-marker
  displacement is the unresolved `0xFFFFFFFF` placeholder, not the final
  offset — the task-level assertion matches the REX.W opcode prefixes
  (`48 8B 88`, `48 8B C1`) and scans for a non-REX `8B 88` (truncating read)
  rather than exact displacement bytes.
- **`dim a as pointer` was never supported**: the diagnostic probe's
  `a(pointer)=&b` case fails to parse at 3:1 with `a=5` too (the "pointer"
  type is not registered in this compiler). Pre-existing; the probe was
  deleted with wave 14.
- **No cast DLL anywhere in the int64 path**: with the 107 type and the
  integer-family no-cast rule, `a(int64) = &b` is pure emitter code
  (8-byte read + `MOV [@a],RAX`) — the old `?CastLtoR@@...` call is gone.
- **Address arithmetic stays DWORD-mode**: `x = &b + n` keeps 4-byte DWORD
  temporaries (type-mode 7); full 8-byte pointer arithmetic is a documented
  follow-up, not part of this wave.
