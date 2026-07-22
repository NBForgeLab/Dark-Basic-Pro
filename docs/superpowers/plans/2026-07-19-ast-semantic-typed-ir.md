# AST, Semantic Model, and Typed IR Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Establish a modern, decoupled compiler pipeline (AST ➔ Semantic Checker ➔ Typed IR ➔ Target Codegen) for variable assignments and mathematical expressions, resolving legacy single-pass parsing code.

**Architecture:** Extend AST nodes to support binary operations, implement a `SemanticVisitor` to validate type consistency, lower AST to a linear `TypedIR` representation, and generate target x86 assembly from Typed IR using `TargetCodegen`.

**Tech Stack:** C++17, CMake, GoogleTest, spdlog.

---

### Task 1: Extend AST Nodes and Visitor Interface

**Files:**
- Modify: `DBProCompiler/DBPCompiler/ASTNodes.h`
- Modify: `DBProCompiler/DBPCompiler/ASTVisitor.h`
- Modify: `DBProCompiler/DBPCompiler/CodeGenVisitor.h`
- Modify: `DBProCompiler/DBPCompiler/CodeGenVisitor.cpp`
- Modify: `tests/test_ast.cpp`

- [ ] **Step 1: Declare BinaryOpType and ASTBinaryOpNode in ASTNodes.h**
  
  Add `BinaryOpType` enum and `ASTBinaryOpNode` class to [ASTNodes.h](file:///D:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/ASTNodes.h):
  ```cpp
  enum class BinaryOpType {
      Add,
      Subtract,
      Multiply,
      Divide
  };

  class ASTBinaryOpNode : public ASTNode {
  public:
      BinaryOpType m_op;
      std::unique_ptr<ASTNode> m_left;
      std::unique_ptr<ASTNode> m_right;

      ASTBinaryOpNode(BinaryOpType op, std::unique_ptr<ASTNode> left, std::unique_ptr<ASTNode> right)
          : m_op(op), m_left(std::move(left)), m_right(std::move(right)) {}

      void Accept(ASTVisitor* visitor) override {
          visitor->Visit(this);
      }
  };
  ```

- [ ] **Step 2: Add Visit(ASTBinaryOpNode*) declaration to ASTVisitor.h**
  
  Modify [ASTVisitor.h](file:///D:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/ASTVisitor.h):
  ```cpp
  class ASTBinaryOpNode;
  // inside class ASTVisitor:
  virtual void Visit(ASTBinaryOpNode* node) = 0;
  ```

- [ ] **Step 3: Update existing CodeGenVisitor and test counters**
  
  Modify [CodeGenVisitor.h](file:///D:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/CodeGenVisitor.h):
  ```cpp
  void Visit(ASTBinaryOpNode* node) override;
  ```

  Modify [CodeGenVisitor.cpp](file:///D:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/CodeGenVisitor.cpp):
  ```cpp
  void CodeGenVisitor::Visit(ASTBinaryOpNode* node) {
      // Temporary basic codegen for tests
      if (node->m_left) node->m_left->Accept(this);
      if (node->m_right) node->m_right->Accept(this);
      m_codeGen->WriteASMLine(ASM_POPEBX, "");
      m_codeGen->WriteASMLine(ASM_POPEAX, "");
      if (node->m_op == BinaryOpType::Add) {
          m_codeGen->WriteASMLine(ASM_ADDEAXEBX, "");
      }
      m_codeGen->WriteASMEAXtoX(PMODE_STACK, NULL, NULL, 1, 0);
  }
  ```

  Modify [tests/test_ast.cpp](file:///D:/GitHub-repo/Dark-Basic-Pro/tests/test_ast.cpp):
  - In `NodeCounterVisitor`, implement the mock function:
    ```cpp
    void Visit(ASTBinaryOpNode* node) override {
        for (auto& stmt : { &node->m_left, &node->m_right }) {
            if (*stmt) (*stmt)->Accept(this);
        }
    }
    ```

- [ ] **Step 4: Write failing unit test in test_ast.cpp**
  
  Add `ASTTest.BinaryOpConstruction` in [test_ast.cpp](file:///D:/GitHub-repo/Dark-Basic-Pro/tests/test_ast.cpp):
  ```cpp
  TEST(ASTTest, BinaryOpConstruction) {
      auto left = std::make_unique<ASTLiteralNode>("10", 1);
      auto right = std::make_unique<ASTLiteralNode>("20", 1);
      auto binaryOp = std::make_unique<ASTBinaryOpNode>(BinaryOpType::Add, std::move(left), std::move(right));
      
      NodeCounterVisitor visitor;
      binaryOp->Accept(&visitor);
      EXPECT_EQ(visitor.literalCount, 2);
  }
  ```

- [ ] **Step 5: Run tests and verify success**
  
  Run: `cmake --build build --config Release` and execute `build\bin\Release\dbp_tests.exe --gtest_filter=ASTTest.BinaryOpConstruction`
  Expected: PASS

- [ ] **Step 6: Commit**
  
  ```bash
  git add DBProCompiler/DBPCompiler/ASTNodes.h DBProCompiler/DBPCompiler/ASTVisitor.h DBProCompiler/DBPCompiler/CodeGenVisitor.h DBProCompiler/DBPCompiler/CodeGenVisitor.cpp tests/test_ast.cpp
  git commit -m "feat: add binary op AST node and update visitor interface"
  ```

---

### Task 2: Implement Semantic Type Checker

**Files:**
- Create: `DBProCompiler/DBPCompiler/SemanticVisitor.h`
- Create: `DBProCompiler/DBPCompiler/SemanticVisitor.cpp`
- Modify: `DBProCompiler/DBPCompiler/CMakeLists.txt`
- Modify: `tests/test_ast.cpp`

- [ ] **Step 1: Create SemanticVisitor.h**
  
  Create [SemanticVisitor.h](file:///D:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/SemanticVisitor.h):
  ```cpp
  #pragma once
  #include "ASTVisitor.h"
  #include <windows.h>

  class SemanticVisitor : public ASTVisitor {
  public:
      SemanticVisitor() = default;
      ~SemanticVisitor() override = default;

      void Visit(ASTProgramNode* node) override;
      void Visit(ASTBlockNode* node) override;
      void Visit(ASTAssignmentNode* node) override;
      void Visit(ASTLiteralNode* node) override;
      void Visit(ASTVariableNode* node) override;
      void Visit(ASTBinaryOpNode* node) override;

      DWORD GetInferredType() const { return m_inferredType; }
      bool HasErrors() const { return m_hasErrors; }

  private:
      DWORD m_inferredType = 0; // 1 = int, 2 = float, 3 = string
      bool m_hasErrors = false;
  };
  ```

- [ ] **Step 2: Create SemanticVisitor.cpp**
  
  Create [SemanticVisitor.cpp](file:///D:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/SemanticVisitor.cpp):
  ```cpp
  #include "SemanticVisitor.h"
  #include "ASTNodes.h"
  #include "VarTable.h"
  #include "Error.h"

  extern CVarTable* g_pVarTable;
  extern CError* g_pErrorReport;

  void SemanticVisitor::Visit(ASTProgramNode* node) {
      for (auto& stmt : node->m_statements) {
          stmt->Accept(this);
      }
  }

  void SemanticVisitor::Visit(ASTBlockNode* node) {
      for (auto& stmt : node->m_statements) {
          stmt->Accept(this);
      }
  }

  void SemanticVisitor::Visit(ASTAssignmentNode* node) {
      CVarTable* pVar = g_pVarTable->FindVariable(NULL, const_cast<LPSTR>(node->m_varName.c_str()), 0);
      if (!pVar) {
          m_hasErrors = true;
          if (g_pErrorReport) {
              g_pErrorReport->SetError(1, 100000 + 18, const_cast<LPSTR>(node->m_varName.c_str()));
          }
          return;
      }
      DWORD varType = pVar->GetVarTypeValue();
      if (node->m_expression) {
          node->m_expression->Accept(this);
          if (m_inferredType != varType && m_inferredType != 0) {
              m_hasErrors = true;
              if (g_pErrorReport) {
                  g_pErrorReport->SetError(1, 100000 + 19, const_cast<LPSTR>("Type mismatch in assignment"));
              }
          }
      }
  }

  void SemanticVisitor::Visit(ASTLiteralNode* node) {
      m_inferredType = node->m_type;
  }

  void SemanticVisitor::Visit(ASTVariableNode* node) {
      CVarTable* pVar = g_pVarTable->FindVariable(NULL, const_cast<LPSTR>(node->m_varName.c_str()), 0);
      if (pVar) {
          m_inferredType = pVar->GetVarTypeValue();
      } else {
          m_hasErrors = true;
          m_inferredType = 0;
      }
  }

  void SemanticVisitor::Visit(ASTBinaryOpNode* node) {
      DWORD leftType = 0;
      DWORD rightType = 0;
      if (node->m_left) {
          node->m_left->Accept(this);
          leftType = m_inferredType;
      }
      if (node->m_right) {
          node->m_right->Accept(this);
          rightType = m_inferredType;
      }
      if (leftType != rightType) {
          m_hasErrors = true;
      }
      m_inferredType = leftType;
  }
  ```

- [ ] **Step 3: Add SemanticVisitor.cpp to CMakeLists.txt**
  
  Modify [CMakeLists.txt](file:///D:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/CMakeLists.txt):
  Add `SemanticVisitor.cpp` inside `LIB_SOURCES` (line 37).

- [ ] **Step 4: Write unit test in test_ast.cpp**
  
  Add `SemanticVisitorTest` in [test_ast.cpp](file:///D:/GitHub-repo/Dark-Basic-Pro/tests/test_ast.cpp):
  ```cpp
  #include "SemanticVisitor.h"

  TEST_F(ASTCodeGenTest, SemanticTypeCheck) {
      g_pStatementList->SetVariableAddParse(true);
      DWORD dwAction = 0;
      g_pVarTable->AddVariable("myIntVar", "integer", 0, 1, true, &dwAction, false);

      // Assign literal integer: myIntVar = 42
      auto literal = std::make_unique<ASTLiteralNode>("42", 1);
      auto assignment = std::make_unique<ASTAssignmentNode>("myIntVar", std::move(literal));

      SemanticVisitor visitor;
      assignment->Accept(&visitor);
      EXPECT_FALSE(visitor.HasErrors());
  }
  ```

- [ ] **Step 5: Run tests and verify success**
  
  Run: `cmake --build build --config Release` and execute `build\bin\Release\dbp_tests.exe --gtest_filter=ASTCodeGenTest.SemanticTypeCheck`
  Expected: PASS

- [ ] **Step 6: Commit**
  
  ```bash
  git add DBProCompiler/DBPCompiler/SemanticVisitor.h DBProCompiler/DBPCompiler/SemanticVisitor.cpp DBProCompiler/DBPCompiler/CMakeLists.txt tests/test_ast.cpp
  git commit -m "feat: implement SemanticVisitor type checking pass"
  ```

---

### Task 3: Implement Typed Intermediate Representation (Typed IR)

**Files:**
- Create: `DBProCompiler/DBPCompiler/TypedIR.h`
- Create: `DBProCompiler/DBPCompiler/IRLoweringVisitor.h`
- Create: `DBProCompiler/DBPCompiler/IRLoweringVisitor.cpp`
- Modify: `DBProCompiler/DBPCompiler/CMakeLists.txt`
- Modify: `tests/test_ast.cpp`

- [ ] **Step 1: Create TypedIR.h**
  
  Create [TypedIR.h](file:///D:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/TypedIR.h):
  ```cpp
  #pragma once
  #include <string>
  #include <vector>
  #include <windows.h>
  #include "ASTNodes.h"

  enum class IROpCode {
      LoadConst,
      LoadVar,
      BinaryOp,
      StoreVar
  };

  struct IRInstruction {
      IROpCode opCode;
      std::string operandStr;
      DWORD typeVal = 0;
      BinaryOpType opType = BinaryOpType::Add;
  };

  struct IRProgram {
      std::vector<IRInstruction> instructions;
  };
  ```

- [ ] **Step 2: Create IRLoweringVisitor.h**
  
  Create [IRLoweringVisitor.h](file:///D:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/IRLoweringVisitor.h):
  ```cpp
  #pragma once
  #include "ASTVisitor.h"
  #include "TypedIR.h"

  class IRLoweringVisitor : public ASTVisitor {
  public:
      IRLoweringVisitor() = default;
      ~IRLoweringVisitor() override = default;

      void Visit(ASTProgramNode* node) override;
      void Visit(ASTBlockNode* node) override;
      void Visit(ASTAssignmentNode* node) override;
      void Visit(ASTLiteralNode* node) override;
      void Visit(ASTVariableNode* node) override;
      void Visit(ASTBinaryOpNode* node) override;

      IRProgram GetProgram() const { return m_program; }

  private:
      IRProgram m_program;
  };
  ```

- [ ] **Step 3: Create IRLoweringVisitor.cpp**
  
  Create [IRLoweringVisitor.cpp](file:///D:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/IRLoweringVisitor.cpp):
  ```cpp
  #include "IRLoweringVisitor.h"
  #include "ASTNodes.h"

  void IRLoweringVisitor::Visit(ASTProgramNode* node) {
      for (auto& stmt : node->m_statements) {
          stmt->Accept(this);
      }
  }

  void IRLoweringVisitor::Visit(ASTBlockNode* node) {
      for (auto& stmt : node->m_statements) {
          stmt->Accept(this);
      }
  }

  void IRLoweringVisitor::Visit(ASTAssignmentNode* node) {
      if (node->m_expression) {
          node->m_expression->Accept(this);
      }
      IRInstruction inst;
      inst.opCode = IROpCode::StoreVar;
      inst.operandStr = node->m_varName;
      m_program.instructions.push_back(inst);
  }

  void IRLoweringVisitor::Visit(ASTLiteralNode* node) {
      IRInstruction inst;
      inst.opCode = IROpCode::LoadConst;
      inst.operandStr = node->m_value;
      inst.typeVal = node->m_type;
      m_program.instructions.push_back(inst);
  }

  void IRLoweringVisitor::Visit(ASTVariableNode* node) {
      IRInstruction inst;
      inst.opCode = IROpCode::LoadVar;
      inst.operandStr = node->m_varName;
      m_program.instructions.push_back(inst);
  }

  void IRLoweringVisitor::Visit(ASTBinaryOpNode* node) {
      if (node->m_left) node->m_left->Accept(this);
      if (node->m_right) node->m_right->Accept(this);
      IRInstruction inst;
      inst.opCode = IROpCode::BinaryOp;
      inst.opType = node->m_op;
      m_program.instructions.push_back(inst);
  }
  ```

- [ ] **Step 4: Add IRLoweringVisitor.cpp to CMakeLists.txt**
  
  Modify [CMakeLists.txt](file:///D:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/CMakeLists.txt):
  Add `IRLoweringVisitor.cpp` inside `LIB_SOURCES` (line 37).

- [ ] **Step 5: Write unit test in test_ast.cpp**
  
  Add `IRLoweringTest` in [test_ast.cpp](file:///D:/GitHub-repo/Dark-Basic-Pro/tests/test_ast.cpp):
  ```cpp
  #include "IRLoweringVisitor.h"

  TEST(ASTTest, IRLowering) {
      auto left = std::make_unique<ASTLiteralNode>("10", 1);
      auto right = std::make_unique<ASTLiteralNode>("20", 1);
      auto binaryOp = std::make_unique<ASTBinaryOpNode>(BinaryOpType::Add, std::move(left), std::move(right));
      auto assignment = std::make_unique<ASTAssignmentNode>("x", std::move(binaryOp));

      IRLoweringVisitor lowering;
      assignment->Accept(&lowering);
      IRProgram ir = lowering.GetProgram();
      
      ASSERT_EQ(ir.instructions.size(), 4u);
      EXPECT_EQ(ir.instructions[0].opCode, IROpCode::LoadConst);
      EXPECT_EQ(ir.instructions[1].opCode, IROpCode::LoadConst);
      EXPECT_EQ(ir.instructions[2].opCode, IROpCode::BinaryOp);
      EXPECT_EQ(ir.instructions[3].opCode, IROpCode::StoreVar);
  }
  ```

- [ ] **Step 6: Run tests and verify success**
  
  Run: `cmake --build build --config Release` and execute `build\bin\Release\dbp_tests.exe --gtest_filter=ASTTest.IRLowering`
  Expected: PASS

- [ ] **Step 7: Commit**
  
  ```bash
  git add DBProCompiler/DBPCompiler/TypedIR.h DBProCompiler/DBPCompiler/IRLoweringVisitor.h DBProCompiler/DBPCompiler/IRLoweringVisitor.cpp DBProCompiler/DBPCompiler/CMakeLists.txt tests/test_ast.cpp
  git commit -m "feat: implement linear Typed IR lowering pass"
  ```

---

### Task 4: Implement Target Code Generator

**Files:**
- Create: `DBProCompiler/DBPCompiler/TargetCodegen.h`
- Create: `DBProCompiler/DBPCompiler/TargetCodegen.cpp`
- Modify: `DBProCompiler/DBPCompiler/CMakeLists.txt`
- Modify: `tests/test_ast.cpp`

- [ ] **Step 1: Create TargetCodegen.h**
  
  Create [TargetCodegen.h](file:///D:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/TargetCodegen.h):
  ```cpp
  #pragma once
  #include "TypedIR.h"
  #include "ICodeGenerator.h"

  class TargetCodegen {
  public:
      TargetCodegen(ICodeGenerator* codeGen, DWORD lineNumber = 1);
      ~TargetCodegen() = default;

      bool Generate(const IRProgram& ir);

  private:
      ICodeGenerator* m_codeGen;
      DWORD m_lineNumber;
  };
  ```

- [ ] **Step 2: Create TargetCodegen.cpp**
  
  Create [TargetCodegen.cpp](file:///D:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/TargetCodegen.cpp):
  ```cpp
  #include "TargetCodegen.h"
  #include "VarTable.h"
  #include "ASMWriter.h"

  extern CVarTable* g_pVarTable;

  TargetCodegen::TargetCodegen(ICodeGenerator* codeGen, DWORD lineNumber)
      : m_codeGen(codeGen), m_lineNumber(lineNumber) {}

  bool TargetCodegen::Generate(const IRProgram& ir) {
      for (const auto& inst : ir.instructions) {
          switch (inst.opCode) {
              case IROpCode::LoadConst: {
                  CStr valStr(const_cast<LPSTR>(inst.operandStr.c_str()));
                  m_codeGen->WriteASMTaskCoreP1(m_lineNumber, ASMTASK_PUSH, &valStr, inst.typeVal);
                  break;
              }
              case IROpCode::LoadVar: {
                  CVarTable* pVar = g_pVarTable->FindVariable(NULL, const_cast<LPSTR>(inst.operandStr.c_str()), 0);
                  DWORD dwType = 1, dwOffset = 0;
                  if (pVar) {
                      dwType = pVar->GetVarTypeValue();
                      dwOffset = pVar->GetOffsetValue();
                  }
                  CStr varName(const_cast<LPSTR>(inst.operandStr.c_str()));
                  DWORD dwAccessMode = m_codeGen->DetMode(&varName, dwType, dwOffset);
                  m_codeGen->WriteASMXtoEAX(dwAccessMode, &varName, NULL, dwType, dwOffset);
                  m_codeGen->WriteASMEAXtoX(PMODE_STACK, NULL, NULL, dwType, dwOffset);
                  break;
              }
              case IROpCode::BinaryOp: {
                  m_codeGen->WriteASMLine(ASM_POPEBX, "");
                  m_codeGen->WriteASMLine(ASM_POPEAX, "");
                  if (inst.opType == BinaryOpType::Add) {
                      m_codeGen->WriteASMLine(ASM_ADDEAXEBX, "");
                  } else if (inst.opType == BinaryOpType::Subtract) {
                      m_codeGen->WriteASMLine(ASM_SUBEAXEBX, "");
                  }
                  m_codeGen->WriteASMEAXtoX(PMODE_STACK, NULL, NULL, 1, 0);
                  break;
              }
              case IROpCode::StoreVar: {
                  m_codeGen->WriteASMLine(ASM_POPEAX, "");
                  CVarTable* pVar = g_pVarTable->FindVariable(NULL, const_cast<LPSTR>(inst.operandStr.c_str()), 0);
                  if (!pVar) return false;
                  CStr varName(const_cast<LPSTR>(inst.operandStr.c_str()));
                  DWORD dwAccessMode = m_codeGen->DetMode(&varName, pVar->GetVarTypeValue(), pVar->GetOffsetValue());
                  m_codeGen->WriteASMEAXtoX(dwAccessMode, &varName, NULL, pVar->GetVarTypeValue(), pVar->GetOffsetValue());
                  break;
              }
          }
      }
      return true;
  }
  ```

- [ ] **Step 3: Add TargetCodegen.cpp to CMakeLists.txt**
  
  Modify [CMakeLists.txt](file:///D:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/CMakeLists.txt):
  Add `TargetCodegen.cpp` inside `LIB_SOURCES` (line 37).

- [ ] **Step 4: Write unit test in test_ast.cpp**
  
  Add `ASTCodeGenTest.PipelineCodegen` in [test_ast.cpp](file:///D:/GitHub-repo/Dark-Basic-Pro/tests/test_ast.cpp):
  ```cpp
  #include "TargetCodegen.h"

  TEST_F(ASTCodeGenTest, PipelineCodegen) {
      g_pStatementList->SetVariableAddParse(true);
      DWORD dwAction = 0;
      g_pVarTable->AddVariable("testTargetVar", "integer", 0, 1, true, &dwAction, false);

      // myIntVar = 42
      auto literal = std::make_unique<ASTLiteralNode>("42", 1);
      auto assignment = std::make_unique<ASTAssignmentNode>("testTargetVar", std::move(literal));

      // Lower
      IRLoweringVisitor lowering;
      assignment->Accept(&lowering);
      IRProgram ir = lowering.GetProgram();

      // Compile
      TargetCodegen codegen(g_pASMWriter, 1);
      EXPECT_TRUE(codegen.Generate(ir));
  }
  ```

- [ ] **Step 5: Run tests and verify success**
  
  Run: `cmake --build build --config Release` and execute `build\bin\Release\dbp_tests.exe --gtest_filter=ASTCodeGenTest.PipelineCodegen`
  Expected: PASS

- [ ] **Step 6: Commit**
  
  ```bash
  git add DBProCompiler/DBPCompiler/TargetCodegen.h DBProCompiler/DBPCompiler/TargetCodegen.cpp DBProCompiler/DBPCompiler/CMakeLists.txt tests/test_ast.cpp
  git commit -m "feat: implement TargetCodegen translation backend"
  ```

---

### Task 5: Integrate into the Compiler's Assignment Parser

**Files:**
- Modify: `DBProCompiler/DBPCompiler/Statement.cpp`

- [ ] **Step 1: Refactor DoAssignment to utilize the new pipeline**
  
  Modify `DoAssignment` in [Statement.cpp](file:///D:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/Statement.cpp):
  We intercept the variable assignment, parse the value as AST literal node, run type checking, lower it, and execute compilation:
  ```cpp
  #include "ASTNodes.h"
  #include "SemanticVisitor.h"
  #include "IRLoweringVisitor.h"
  #include "TargetCodegen.h"

  // inside CStatement::DoAssignment:
  bool CStatement::DoAssignment(DWORD StatementLineNumber, DWORD TokenID)
  {
      LPSTR pPointer = g_pStatementList->GetFileDataPointer();
      LPSTR pAlternateFullString = ProduceFullSegment(&pPointer);
      CStr* pAltString = new CStr(pAlternateFullString);
      DWORD dwPos = pAltString->FindFirstChar('=');
      
      // We extract variable name and assigned value string
      std::string varName = std::string(pAlternateFullString, dwPos);
      // Trim spaces
      varName.erase(varName.find_last_not_of(" \t\r\n") + 1);
      std::string valStr = std::string(pAlternateFullString + dwPos + 1);
      valStr.erase(0, valStr.find_first_not_of(" \t\r\n"));
      valStr.erase(valStr.find_last_not_of(" \t\r\n") + 1);

      SAFE_DELETE(pAlternateFullString);
      SAFE_DELETE(pAltString);

      // If it's a simple numeric assignment, route through modern AST pipeline
      if (valStr.find_first_not_of("0123456789") == std::string::npos && !varName.empty()) {
          auto literal = std::make_unique<ASTLiteralNode>(valStr, 1); // 1 = integer type
          auto assignment = std::make_unique<ASTAssignmentNode>(varName, std::move(literal));

          SemanticVisitor semantic;
          assignment->Accept(&semantic);
          if (semantic.HasErrors()) return false;

          IRLoweringVisitor lowering;
          assignment->Accept(&lowering);
          
          TargetCodegen codegen(g_pASMWriter, StatementLineNumber);
          return codegen.Generate(lowering.GetProgram());
      }

      // Fallback to legacy assignment mechanism for complex expressions
      pPointer = g_pStatementList->GetFileDataPointer();
      pPointer[dwPos]=',';
      g_pStatementList->SetInstructionType(2);
      g_pStatementList->SetInstructionRef(g_pInstructionTable->GetRef(IT_INTERNAL_ASSIGNLL));
      g_pStatementList->SetInstructionValue(g_pInstructionTable->GetIIValue(IT_INTERNAL_ASSIGNLL));
      g_pStatementList->SetInstructionParamMax(2);
      return DoInstruction(StatementLineNumber, TokenID);
  }
  ```

- [ ] **Step 2: Run conformance tests via the Local CI script**
  
  Run: `& D:\GitHub-repo\Dark-Basic-Pro\scripts\run-local-ci.ps1 -Configuration Release`
  Expected: PASS (All C++ unit tests and conformance suites compile and pass successfully, verifying no regressions).

- [ ] **Step 3: Commit**
  
  ```bash
  git add DBProCompiler/DBPCompiler/Statement.cpp
  git commit -m "feat: integrate AST Semantic and Typed IR pipeline into DoAssignment"
  ```
