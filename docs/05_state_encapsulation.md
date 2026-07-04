# Phase 5: State Encapsulation

## 🎯 Goal
Eliminate dangerous global variables and `extern` declarations across compiler source files. State encapsulation groups all symbol tables, error reports, and configuration states into a single, unified context object (`CompilerContext`). This provides thread safety, a clean lifecycle, and enables unit testing.

---

## ⚠️ The Problem
The current compiler uses a large set of global pointers declared as `extern` across different files (see [DBPCompiler.cpp](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/DBPCompiler.cpp) lines 26-44):
```cpp
CEXEBlock*          g_pEXE              = NULL;
CDBPCompiler*       g_pDBPCompiler      = NULL;
CError*             g_pErrorReport      = NULL;
CVarTable*          g_pVarTable         = NULL;
CLabelTable*        g_pLabelTable       = NULL;
```
This causes:
1. **State Leakage**: If a compilation fails midway, tables might retain old data, leading to unpredictable compiler bugs when compiling a second script.
2. **Untestable Code**: We cannot run unit tests for individual functions because they all depend on these global pointers being initialized in a particular order.
3. **Thread Instability**: The compiler is not thread-safe and cannot compile multiple scripts in parallel.

---

## 🛠️ Solution: Introducing `CompilerContext`

We define a context class that manages the compiler's state lifecycle:

```cpp
#pragma once
#include <memory>

class CVarTable;
class CLabelTable;
class CStructTable;
class ICodeGenerator;
class CError;

class CompilerContext {
public:
    CompilerContext() {
        m_varTable = std::make_unique<CVarTable>();
        m_labelTable = std::make_unique<CLabelTable>();
        m_structTable = std::make_unique<CStructTable>();
        m_errorReport = std::make_unique<CError>();
    }

    ~CompilerContext() = default;

    // Delete copy operations
    CompilerContext(const CompilerContext&) = delete;
    CompilerContext& operator=(const CompilerContext&) = delete;

    CVarTable* GetVarTable() { return m_varTable.get(); }
    CLabelTable* GetLabelTable() { return m_labelTable.get(); }
    CStructTable* GetStructTable() { return m_structTable.get(); }
    CError* GetErrorReport() { return m_errorReport.get(); }

    ICodeGenerator* GetCodeGenerator() { return m_codeGenerator.get(); }
    void SetCodeGenerator(std::unique_ptr<ICodeGenerator> codeGen) {
        m_codeGenerator = std::move(codeGen);
    }

private:
    std::unique_ptr<CVarTable>     m_varTable;
    std::unique_ptr<CLabelTable>   m_labelTable;
    std::unique_ptr<CStructTable>  m_structTable;
    std::unique_ptr<CError>        m_errorReport;
    std::unique_ptr<ICodeGenerator> m_codeGenerator;
};
```

---

## 🔄 Implementation Steps

1. **Instantiate the Context**:
   * Create a `CompilerContext` instance in the main compilation loop in [Main.cpp](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/Main.cpp).
2. **Pass Context References**:
   * Modify function signatures in sub-components to take `CompilerContext& ctx`:
     ```cpp
     bool ParseStatement(CompilerContext& ctx);
     ```
3. **Eliminate `extern` Globals**:
   * Gradually remove the `extern g_pVarTable` and similar declarations, replacing references with calls to the context object `ctx.GetVarTable()`.

---

## 🚀 Benefits
* **Clean State Lifecycle**: All tables are cleanly destroyed when the `CompilerContext` goes out of scope, preventing memory leaks and state contamination.
* **Isolated Testing**: Enable GTest to easily mock or create minimal compiler contexts for verifying individual parser functions.
