# Wave 10 — Scalar typed declarations via DIM (`dim d as float`)

**Status**: Design
**Date**: 2026-08-12

## 1. Problem

`dim d as float` fails in the pre-scan with `ERR_SYNTAX+43`, while:

- `dim d(10) as float` (array form) works;
- `global d as float` and `local d as float` work.

The user-facing symptom is `MakeStatements` returning `false` with
`Failed to 'DoPreScanBlock(0)'`.

## 2. Root cause

`CStatement::DoDeclaration` (Statement.cpp) is dispatched with
`bDoneDim==true` for DIM statements (`DoStatement` case 4). When
`bDoneDim==true` the token is forced to `Token::Dim`, so the parser always
enters the `if(dwToken==Token::Dim)` branch:

```
ProduceNextArrayToken(&pPointer)   // expects "NAME(dims)"
SeperateValueFromArrayString(...)  // requires '(' brackets
if(bSeperateOK==false) -> ERR_SYNTAX+43, return false
```

For a scalar name (`d as float`) `ProduceNextArrayToken` scans for a `(`
bracket, finds none, and returns NULL **without advancing the caller's
pointer** (the `*pOrigPointer` update is inside the `if(pPointer)` success
block). `SeperateValueFromArrayString` then fails on the NULL name and the
declaration is rejected.

The GLOBAL/LOCAL path (`bDoneDim==false`) never enters that branch: it reads
the name through `ProduceNextTokenEx` in the `else` branch and succeeds.

## 3. Fix

In the `Token::Dim` branch, when `SeperateValueFromArrayString` fails (no
array brackets — a scalar declaration), fall back to reading the name as a
plain variable token — exactly what the `else` (GLOBAL/LOCAL) branch does:

```
if(bSeperateOK==true)
    -> dwDecArr=1, name=pArrayName, init=pInitValue
else
    -> pString = ProduceNextToken(&pPointer, ...)   // pointer was untouched
       if non-empty:
           init = SeperateInitFromType(pString)
           name = pString, dwDecArr=0
       else:
           break
```

The pointer is guaranteed to sit at the start of the name because
`ProduceNextArrayToken` leaves `*pOrigPointer` untouched on NULL return
(verified in Tokenizer.cpp: the update only happens inside the
`if(pPointer)` block).

The rest of `DoDeclaration` already handles `dwDecArr==0` correctly (no `&`
prefix, scalar varspace slot, no `DoAllocation` call), and the type
specifier block below already parses `as float` (the `AS` keyword maps to
`Token::Asterisk`).

## 4. TDD discoveries (red phase)

- **All scalar DIM forms were broken**, not just typed ones: `dim d`
  (plain), `dim d as float`, `dim d as integer`, `dim d as string`, and
  `dim a as integer, b as float` all failed in the pre-scan.
- `global d as float` and `local d as float` already worked — the
  GLOBAL/LOCAL path uses the `else` branch with a plain token read.
- A probe-only crash (`dg=1.5` after `global dg as float`) turned out to be
  a **test artifact**: `MakeStatements` mutates its input buffer
  (`DoAssignment` rewrites `=` to `,`), so passing a string literal crashes
  with 0xC0000005. Tests must copy the program into writable memory first.
- The array regression (`dim d(10) as float`) emits a runtime `DimDDD` call
  (`FF D3` = CALL RBX), asserted by byte pattern, not reference labels.

## 5. Test plan (TDD)

Red-phase tests in `tests/test_x64_scalar_declarations.cpp`:

1. `dim d as float` — MakeStatements OK; pre-scan + program WriteDBM OK;
   no `AddFFF` reference; varspace slot created at float width.
2. `dim d as float=1.5` — same, with init value emitted.
3. `dim d as integer` / `dim d as string` — typed scalars compile.
4. `dim d` — plain scalar dim compiles (previously also broken).
5. `dim d(10) as float` — regression: array form still compiles.
6. `dim a(10), b(20)` — multi-declaration chain still works.
7. Emitted code: assigning `d=1.5` after `dim d as float` stores the float
   inlined (no `AddFFF`/`CastLtoF` DLL call), matching wave 8/9.
