# Phase 11: AST & Multi-Pass Compiler Architecture

## 🎯 Goal
Refactor the compiler frontend from a single-pass design to a structured multi-pass compiler utilizing an **Abstract Syntax Tree (AST)**. This separates code parsing from code generation, enabling type analysis and compiler optimization passes before generating target machine code.

---

## ⚠️ Current Architecture Issues
The current parser translates the input script line-by-line and generates binary machine opcodes on-the-fly:
* **Problem**:
  * Syntax errors at the bottom of a file are only discovered after generating machine code for the top, leaving corrupted files.
  * Compiler optimizations like dead code elimination and constant folding are impossible.
  * Adding modern language structures (like OOP or advanced data types) is extremely difficult.

---

## 🛠️ Design: AST Nodes & Visitors

We propose structuring the compilation pipeline into distinct phases:

```
[Source .dba] ──> (1. Lexer & Parser) ──> [Abstract Syntax Tree (AST)]
                                                    │
                                                    ▼
(3. CodeGen) <── [Optimized AST] <── (2. Type Checker & Optimizer)
```

### 1. Abstract Syntax Tree (AST) Nodes
Every language element is represented as a C++ object node:
```cpp
#include <memory>
#include <vector>
#include <string>

class ASTVisitor;

class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void Accept(ASTVisitor* visitor) = 0;
};

// Represents assignment: x = Expression
class ASTAssignmentNode : public ASTNode {
public:
    ASTAssignmentNode(const std::string& varName, std::unique_ptr<ASTNode> expr)
        : m_varName(varName), m_expression(std::move(expr)) {}

    void Accept(ASTVisitor* visitor) override;

    std::string m_varName;
    std::unique_ptr<ASTNode> m_expression;
};
```

### 2. Visitor Pattern for Multi-Pass Execution
By implementing the Visitor pattern, different compiler compiler passes can traverse the syntax tree independently:
* **`TypeCheckerVisitor`**: Checks type compatibility and scope declarations.
* **`OptimizationVisitor`**: Optimizes operations (e.g., constant folding, converting `5 + 10` to `15` at compile time).
* **`CodeGenVisitor`**: Traverses the optimized tree and calls `ICodeGenerator` to emit target binary blocks (32-bit x86 now, 64-bit x64 later).

---

## 🚀 Benefits
* **High Extensibility**: New language features require only declaring a new AST node and updating the parser, keeping the code generation backend completely clean.
* **Optimized Output**: Compiles faster, tighter game executables by eliminating redundant or unused instructions.
