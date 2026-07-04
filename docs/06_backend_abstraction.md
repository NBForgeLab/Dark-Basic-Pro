# Phase 6: Backend Abstraction

## 🎯 Goal
Decouple the frontend compiler parser (syntax and statements) from the target backend (binary machine code generator). Currently, the parser directly references `CASMWriter` to write raw 32-bit x86 opcodes. Decoupling this logic via an interface is essential for future x64 target support.

---

## 🛠️ Design: Introducing `ICodeGenerator`

We introduce an abstract interface class `ICodeGenerator` containing the pure virtual functions required by the compiler:

### `ICodeGenerator` Interface
```cpp
class ICodeGenerator {
public:
    virtual ~ICodeGenerator() = default;

    // Initialize and write executable header
    virtual bool InitializeHeader() = 0;

    // Write primary instruction opcodes
    virtual bool WriteInstruction(DWORD dwOpCode, int param1 = -1, int param2 = -1) = 0;

    // Handle stack operations
    virtual bool PushImmediate(DWORD dwValue) = 0;
    virtual bool PushMemoryAddress(DWORD dwAddress) = 0;
    virtual bool PopRegister(DWORD dwRegister) = 0;

    // Dispatch function calls
    virtual bool CallFunction(DWORD dwAddress) = 0;
    virtual bool CallDynamicLibrary(LPSTR lpDllName, LPSTR lpFunctionName) = 0;

    // Handle jumps and labels
    virtual bool DefineLabel(LPSTR lpLabelName) = 0;
    virtual bool ConditionalJump(DWORD dwCondition, LPSTR lpLabelName) = 0;
    virtual bool Jump(LPSTR lpLabelName) = 0;

    // Resolve placeholders and relocations
    virtual bool ResolveReferences() = 0;

    // Retrieve final binary bytecode/machine code block
    virtual LPSTR GetCompiledBlock(DWORD* pdwSize) = 0;
};
```

---

## 🔄 Implementation Steps

1. **Add `ICodeGenerator` Header**:
   * Create `ICodeGenerator.h` in the compiler source directory.
2. **Refactor `CASMWriter`**:
   * Rename `CASMWriter` to `CASMWriterx86` and make it implement `ICodeGenerator`.
   * Move 32-bit x86 opcode assembly templates to `CASMWriterx86`.
3. **Refactor the Parser**:
   * Change `g_pASMWriter` references in `CDBPCompiler.cpp` to use the abstract interface pointer:
     ```cpp
     ICodeGenerator* g_pCodeGenerator = nullptr;
     ```
4. **Compile & Verify**:
   * Ensure 32-bit compilation works exactly as before.

---

## 🚀 Benefits
* **Backend Extensibility**: Adding 64-bit target generation requires writing a new class `CASMWriterx64 : public ICodeGenerator` and plugging it into the compiler. The frontend parsing logic remains untouched.
* **Alternate Backends**: Enables future possibilities like generating WebAssembly, LLVM IR, or byte-code interpreters.
