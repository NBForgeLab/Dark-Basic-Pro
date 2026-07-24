# Remove `/Zc:forScope-` — Standard C++17 Scoping Compliance

## Summary

Remove the deprecated MSVC `/Zc:forScope-` compiler flag and fix all code that relies on
non-standard for-loop variable scoping. This eliminates the `D9035` deprecation warning
and ensures the codebase compiles with any future MSVC version.

## Problem

The flag `/Zc:forScope-` disables standard C++ scoping for variables declared in
`for(int i=0; ...)` — allowing them to leak into the enclosing scope. This was default
behavior in MSVC 6.0/2003 but has been non-standard since C++98. MSVC now warns:

```
warning D9035: option 'Zc:forScope-' has been deprecated and will be removed in a future release
```

## Scope

- **Target:** All C++ files compiled with `dbp_apply_legacy_cpp_options`
- **Files affected:** 5 confirmed (Statement.cpp, MathOp.cpp, ASMWriter.cpp, EXEBlock.cpp, DBPCompiler.cpp)
- **Instances:** ~10-12 total
- **Risk:** Zero behavioral change — only variable scope boundaries change

## Fix Strategy

Two dependency patterns exist. Each gets the cleanest fix:

### Pattern A — Variable reused in subsequent for-loop

The variable is declared in the first loop's initializer, then reused without
re-declaration in a later loop.

**Fix:** Add type specifier in the second for-loop (each loop owns its own variable).

```cpp
// Before (non-standard):
for(DWORD di=0; di<9; di++) dwLeapRelDiff[di] = ...;
for(di=0; di<9; di++) m_pRecordBytePosition[di] = ...;

// After (standard):
for(DWORD di=0; di<9; di++) dwLeapRelDiff[di] = ...;
for(DWORD di=0; di<9; di++) m_pRecordBytePosition[di] = ...;
```

### Pattern B — Post-loop value usage

The variable's final value after loop termination is used in subsequent code.

**Fix:** Declare the variable before the loop so it remains in scope.

```cpp
// Before (non-standard):
for(DWORD n=0; n<length-dwPos; n++)
    *(*pArrValue+n) = pString.GetChar(dwPos+n);
*(*pArrValue+n) = 0;

// After (standard):
DWORD n = 0;
for(n=0; n<length-dwPos; n++)
    *(*pArrValue+n) = pString.GetChar(dwPos+n);
*(*pArrValue+n) = 0;
```

## Instances to Fix

| # | File | Line | Pattern | Variable | Fix |
|---|------|------|---------|----------|-----|
| 1 | Statement.cpp | 3267→3276,3421,3450 | A+B | `d` | Declare `DWORD d=0;` before first loop |
| 2 | Statement.cpp | 4964→4966 | B | `n` | Declare `DWORD n=0;` before loop |
| 3 | MathOp.cpp | 1290→1296,1300 | A+B | `n` | Declare `DWORD n=0;` before loop |
| 4 | MathOp.cpp | 2155→2175 | B | `n` | Declare `DWORD n=0;` before loop |
| 5 | ASMWriter.cpp | 531→547 | A | `di` | Add `DWORD` in second loop |
| 6 | EXEBlock.cpp | 1690→1702 | A | `dll` | Add `int` in second loop |
| 7 | DBPCompiler.cpp | 1494→1498 | B | `n` | Declare `DWORD n=0;` before loop |

## CMake Change

```cmake
# Remove this line from cmake/ProjectOptions.cmake (line 44):
$<$<COMPILE_LANGUAGE:CXX>:/Zc:forScope->
```

## TDD Approach

1. **RED:** Remove the flag → compilation fails at all non-standard sites
2. **GREEN:** Fix each site with the appropriate pattern → compilation succeeds
3. **VERIFY:** Full CI passes (598 tests, all phases green)

## Verification Criteria

- [ ] No `D9035` warning in build output
- [ ] Debug build compiles cleanly
- [ ] Release build compiles cleanly
- [ ] All 598 tests pass
- [ ] Full CI dashboard: all phases PASSED

## Out of Scope

- No logic or behavioral changes
- No changes to C files (icons/) — flag was CXX-only
- `/EHa` and `/W3` flags remain unchanged
- `dbp_apply_modern_cpp_options` function unchanged (already standard-compliant)
