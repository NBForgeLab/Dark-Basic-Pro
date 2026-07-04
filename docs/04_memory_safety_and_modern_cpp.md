# Phase 4: Memory Safety & Modern C++

## 🎯 Goal
Upgrade the codebase from legacy standards to modern C++17/C++20. The objective is to eliminate traditional memory management issues (such as memory leaks, buffer overflows, and use-after-free bugs) and improve the performance and safety of string and data processing in the compiler.

---

## 📝 Key Refactoring Areas

### 1. Replace Legacy String class `CStr` with `std::string` & `std::string_view`
* **Current State**: The codebase uses a custom string utility class [CStr](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/Str.h) for string formatting and manipulation.
* **Problem**: The old class performs excessive memory allocations, copies, and lacks integration with standard C++ APIs.
* **Solution**:
  * Transition all string operations to use `std::string`.
  * Use `std::string_view` for read-only string parameters to achieve zero-allocation string passing, significantly accelerating compilation times for large DarkBasic source files.

### 2. Modernize Memory Ownership with Smart Pointers
* **Current State**: The code relies on manual allocations and macros like `SAFE_DELETE` and `SAFE_FREE`.
* **Problem**: Any exception or early `return` statement before these deletes immediately causes memory leaks.
* **Solution**:
  * Use `std::unique_ptr` for single-ownership resources (such as compiler tables and intermediate AST nodes).
  * Use `std::shared_ptr` for shared objects.
  * Discard deprecated Windows heap allocation functions (`GlobalAlloc` and `GlobalFree`) in favor of standard smart allocations.

### 3. Transition to Standard Library (STL) Containers
* **Current State**: Custom dynamic array layouts managed by raw pointers in the variable and label tables.
* **Solution**:
  * Use `std::vector` for dynamic array storage.
  * Use `std::unordered_map` for O(1) keyword and symbol table lookups by name, replacing slow linear array searches.

---

## 💻 Code Example Comparison

### Legacy Style (Manual Allocations)
```cpp
// VarTable.cpp
CVarTable::CVarTable() {
    m_dwCount = 0;
    m_pVars = (LPSTR*)GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, sizeof(LPSTR) * 100);
}

CVarTable::~CVarTable() {
    for(DWORD i = 0; i < m_dwCount; i++) {
        SAFE_DELETE(m_pVars[i]);
    }
    SAFE_FREE(m_pVars);
}
```

### Modern Style (Automatic Cleanups)
```cpp
// VarTable.h (Modernized)
#pragma once
#include <vector>
#include <string>

class CVarTable {
public:
    CVarTable() = default;
    ~CVarTable() = default; // Memory is automatically freed!

    void AddVariable(const std::string& name) {
        m_vars.push_back(name);
    }

private:
    std::vector<std::string> m_vars;
};
```

---

## 🚀 Benefits
* **Clean Code**: Reduces the boilerplate code for cleanup and deletes by up to 20%.
* **Memory Safety**: Completely prevents memory leaks caused by early returns or exceptions.
* **Higher Speed**: Boosts symbol table lookup speeds using standard optimized hash maps.
