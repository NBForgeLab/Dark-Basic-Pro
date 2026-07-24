# Type-Safe CStatement Object — Replace void* with std::variant

## Summary

Replace the unsafe `void* m_pObjectClass` tagged-union in `CStatement` with a type-safe
`std::variant` that provides compile-time type checking, automatic destruction, and
eliminates the manual switch/case dispatch pattern.

## Problem

`CStatement::m_pObjectClass` is a `void*` that stores one of 8 different parser object
types, discriminated by the integer field `m_dwObjectType`. This pattern:

- Has **zero compile-time safety** — wrong cast = UB/crash
- Requires **manual switch/case for deletion** (`FreeObjects()`) — miss a case = memory leak
- Requires **manual switch/case for dispatch** (`WriteDBM()`) — miss a case = silent bug
- Uses **magic integer tags** (1, 2, 3, 6, 8, 11, 12, 20) with no semantic meaning
- Prevents the compiler from catching errors at build time

## Solution: std::variant

Replace with:

```cpp
using StatementObject = std::variant<
    std::monostate,                         // empty (type 0, 999)
    std::unique_ptr<CParseLoop>,            // type 1
    std::unique_ptr<CParseType>,            // type 2
    std::unique_ptr<CParseInit>,            // type 3
    std::unique_ptr<CParseUserFunction>,    // type 6
    std::unique_ptr<CParseJump>,            // type 8
    std::unique_ptr<CParseInstruction>,     // type 11
    std::unique_ptr<CParseFunction>,        // type 12
    std::unique_ptr<CASTAssignment>         // type 20
>;
```

## Benefits

1. **Compile-time type safety** — impossible to cast to wrong type
2. **Automatic destruction** — no manual `FreeObjects()` switch needed
3. **Exhaustive dispatch** — `std::visit` forces handling all alternatives
4. **Self-documenting** — the variant declaration IS the type documentation
5. **Eliminates SAFE_DELETE** — unique_ptr handles it automatically

## Changes Required

### Statement.h

- Remove `void* m_pObjectClass` and `DWORD m_dwObjectType`
- Add `StatementObject m_object;` variant member
- Remove `SetObjectClass(void*)`, `GetObjectClass()`, `SetObjectType()`, `GetObjectType()`
- Add typed setters: `SetObject(std::unique_ptr<CParseLoop>)`, etc.
- Add `GetObjectType()` that maps variant index to legacy type number (for `WriteDBM` compatibility)
- Keep `SetData` with new overloads per type

### Statement.cpp — FreeObjects()

- Delete entirely — `std::variant<unique_ptr<T>...>` destructs automatically

### Statement.cpp — WriteDBM()

- Replace switch/case with `std::visit` using overloaded lambdas or a visitor struct

### Statement.cpp — All assignment sites (~12 locations)

- Replace `obj->m_pObjectClass = (void*)ptr` with typed setter calls:
  ```cpp
  // Before:
  TheObject->m_dwObjectType = 11;
  TheObject->m_pObjectClass = (void*)pInstruction;
  
  // After:
  TheObject->SetObject(std::unique_ptr<CParseInstruction>(pInstruction));
  ```

### MathOp.cpp — SetData call

- Replace `SetData(line, 11, (void*)ptr)` with typed overload

### CoreRuntimeApi.h — No change

The `void*` in `CoreSymbolLookup` is correct (FFI boundary with `GetProcAddress`).
This is semantically a different pattern and should not be changed.

## Backward Compatibility

- `GetObjectType()` still returns DWORD for any code that checks the type numerically
- The variant index maps deterministically to the legacy type tags
- External interface unchanged — only internal representation modernized

## TDD Approach

1. **Write tests** verifying variant construction, type-safe access, automatic destruction
2. **RED:** Tests fail (new API doesn't exist yet)
3. **GREEN:** Implement the variant-based API
4. **Refactor:** Remove legacy void* code paths
5. **VERIFY:** Full CI (598 tests) passes

## Scope

- ~38 void*-related code sites total
- 1 header file change (Statement.h)
- 2 source files changed (Statement.cpp, MathOp.cpp)
- 0 behavioral changes
- CoreRuntimeApi.h: explicitly out of scope
