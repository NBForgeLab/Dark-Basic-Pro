# Wave 15 — Address Arithmetic (`x = &b + n`) at Full int64 Width — Design

Date: 2026-08-11 (wave 15 of the x64-only transition)

## 1. Problem

Wave 14 made `&b` carry a full 8-byte address (type 107). But **arithmetic on
that value still ran at DWORD width**: in `CMathOp::DoValue` the result
type-mode is `dwTypeMode = dwLType % 100`, and `107 % 100 = 7` → DWORD mode →
4-byte `ADD EAX,EBX` with 4-byte temporaries.

Measured emission for `a(int64) = &b + 4` (pre-wave):

```
B8 04 00 00 00        MOV EAX,4
50 ... FF D3 ...      CALL ?CastLtoR@@...      ; literal 4 → int64 (DLL)
A3 <moffs>            MOV [@tmp],EAX           ; 4-byte store — truncation
...
48 8B 88 ... 48 8B C1 ; &b read (8 bytes, wave 14)
5B                    POP RBX
01 D8                 ADD EAX,EBX              ; ← 4-byte math — truncation
A3 <moffs>            MOV [@a],EAX             ; 4-byte result
50 ... FF D3 ...      CALL ?CastLtoR@@...      ; widen the truncated result
48 A3 <moffs>         MOV [@a],RAX
```

The address is truncated to 32 bits before/inside the arithmetic.

## 2. Design

### 2.1 Pointer math runs in int64 mode

`CMathOp::DoValue` type-mode resolution: when either operand is type 107
(address-of / full-width pointer), the mode becomes **9 (int64)**. Placed
after the existing 102/109/108 rules so float operands still win for mixed
float+pointer expressions.

- `&b + 4` → int64 math → wave-8b native `ADD RAX,RBX` (`48 01 D8`).
- `&b - n`, `&b * 2`, `&b / 2`, comparisons → the corresponding REG64 paths.
- Result type 9 → `a(int64) = &b + n` is a direct `MOV [@a],RAX` — no DLL.

### 2.2 The operand casts become native REG64 (closes the wave-8b literal gap)

The mode-9 math casts both operands to 9:

- **literal / int var → int64** (`CastLToR`, symbol 109): was a
  `?CastLtoR@@YA_JH@Z` DLL call — becomes an internal emitter task emitting
  `MOVSXD RAX,EAX` (`48 63 C0`, sign-extend, matching the DLL semantics).
  This also fixes the wave-8b gap where `a(int64) = b + 5` called a DLL for
  the literal.
- **dword var / 107 address → int64** (`CastDTOR`, symbol 169): was a
  `?CastDtoR@@YA_JK@Z` DLL call — becomes an internal emitter task. A dword
  load already zero-extends to RAX (writing EAX clears the upper 32 bits),
  and a 107 load is already the full 8-byte wave-14 read, so the task is
  load + 8-byte store, no conversion instruction.

Registration flips from `AddCommandCore` (DLL) to `AddBuildCommand` with new
`BuildTask::CastIntToInt64` / `BuildTask::CastDwordToInt64` IDs, mapped in
`CMathOp::WriteDBM` to new `ASMTask::CastIntToInt64` / `ASMTask::CastDwordToInt64`
emitter tasks — exactly the wave-8 SSE2-cast pattern.

### 2.3 Out of scope

- Narrowing back to int (`a(integer) = &b + n`): result is genuinely int64 →
  `?CastRtoL@@YAK_J@Z` DLL narrowing, same class as today's DWORD narrowing;
  the value is now correct *before* the narrowing.
- float↔int64 conversions (`CastFTOR`/`CastRTOF` etc.): stay DLL (documented
  follow-up); not touched by this wave.

## 3. TDD plan and discoveries

1. RED: `tests/test_x64_ptrmath.cpp` —
   - task-level: `CastIntToInt64` emits `48 63 C0`; `CastDwordToInt64` with a
     107 source reads 8 bytes (`48 8B 88`).
   - compiled: `a(int64) = &b + 4` / `&b - 4` / `&b + n` / `&b + d` emit
     `48 01 D8` / `48 29 D8` with **no** `CastLtoR`/`CastDtoR` references;
     `a(int64) = b + 5` (wave-8b literal gap) emits `48 01 D8`, no DLL;
     `r = &b > 5` emits the int64 `48 3B DA` compare.
2. GREEN: type-mode rule + build registrations + emitter tasks.
3. Full build + ctest + docs.

### TDD discoveries (post-implementation)

- **Rule ordering matters**: the pre-existing `dwLType%100 in 4..7` rule
  (forcing DWORD math) overrode the initial 107→9 rule — `107%100==7`.
  The wave-15 rule had to be placed *after* it (and after the 10/20 rules).
- **The wave-8b literal gap closed for free**: `a(int64) = b + 5` used to
  route the literal through `?CastLtoR@@...`; the new internal
  `CastIntToInt64` task (MOVSXD) removes that DLL too.
- **Zero-extension is free**: `CastDwordToInt64` needs no instruction — a
  4-byte dword load zero-extends to RAX, and a 107 (address-of) source is
  already read at full QWORD width by wave 14.
- **Narrowing stays DLL**: `a(integer) = &b + n` (int64 result → int LHS)
  still uses `?CastRtoL@@YAK_J@Z`; the value is correct *before* the
  narrowing, which is the documented out-of-scope boundary.
