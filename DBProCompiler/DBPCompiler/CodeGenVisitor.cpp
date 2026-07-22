#include "CodeGenVisitor.h"
#include "ASTNodes.h"
#include "VarTable.h"
#include "ASMWriter.h"
#include "DBPLogger.h"

extern CVarTable* g_pVarTable;
extern CDBMWriter* g_pDBMWriter;

CodeGenVisitor::CodeGenVisitor(ICodeGenerator* codeGen, DWORD lineNumber)
    : m_codeGen(codeGen), m_lineNumber(lineNumber) {}

void CodeGenVisitor::Visit(ASTProgramNode* node) {
    DBP_TRACE("ASTCodeGen: Visiting ASTProgramNode");
    for (auto& stmt : node->m_statements) {
        stmt->Accept(this);
    }
}

void CodeGenVisitor::Visit(ASTBlockNode* node) {
    DBP_TRACE("ASTCodeGen: Visiting ASTBlockNode");
    for (auto& stmt : node->m_statements) {
        stmt->Accept(this);
    }
}

void CodeGenVisitor::Visit(ASTAssignmentNode* node) {
    DBP_TRACE("ASTCodeGen: Visiting ASTAssignmentNode for {}", node->m_varName);
    
    // 1. Evaluate the expression first (pushes result onto the stack)
    if (node->m_expression) {
        node->m_expression->Accept(this);
    }

    // 2. Pop value from stack into EAX
    m_codeGen->WriteASMLine(ASM_POPEAX, "");

    // 3. Find variable information in symbol table
    CVarTable* pVar = g_pVarTable->FindVariable(NULL, const_cast<LPSTR>(node->m_varName.c_str()), 0);
    if (!pVar) {
        extern CError* g_pErrorReport;
        if (g_pErrorReport) {
            g_pErrorReport->SetError(m_lineNumber, 100000 + 18, const_cast<LPSTR>(node->m_varName.c_str()));
        }
        return;
    }
    DWORD dwType = pVar->GetVarTypeValue();
    DWORD dwOffset = pVar->GetOffsetValue();

    // 4. Determine variable access mode and write EAX to variable
    CStr varName(const_cast<LPSTR>(node->m_varName.c_str()));
    DWORD dwAccessMode = m_codeGen->DetMode(&varName, dwType, dwOffset);
    m_codeGen->WriteASMEAXtoX(dwAccessMode, &varName, NULL, dwType, dwOffset);
    
    // 5. Write ASM comment
    m_codeGen->WriteASMComment("ASSIGN EAX TO X (AST)", "", "", "");
}

void CodeGenVisitor::Visit(ASTLiteralNode* node) {
    DBP_TRACE("ASTCodeGen: Visiting ASTLiteralNode: {}", node->m_value);
    CStr literalVal(const_cast<LPSTR>(node->m_value.c_str()));
    // Directly push the literal onto the stack
    m_codeGen->WriteASMTaskCoreP1(m_lineNumber, ASMTASK_PUSH, &literalVal, node->m_type);
}

void CodeGenVisitor::Visit(ASTVariableNode* node) {
    DBP_TRACE("ASTCodeGen: Visiting ASTVariableNode: {}", node->m_varName);
    CVarTable* pVar = g_pVarTable->FindVariable(NULL, const_cast<LPSTR>(node->m_varName.c_str()), 0);
    DWORD dwType = 1;
    DWORD dwOffset = 0;
    if (pVar) {
        dwType = pVar->GetVarTypeValue();
        dwOffset = pVar->GetOffsetValue();
    }

    // Load variable value to EAX, then push EAX onto stack
    CStr varName(const_cast<LPSTR>(node->m_varName.c_str()));
    DWORD dwAccessMode = m_codeGen->DetMode(&varName, dwType, dwOffset);
    m_codeGen->WriteASMXtoEAX(dwAccessMode, &varName, NULL, dwType, dwOffset);
    m_codeGen->WriteASMEAXtoX(PMODE_STACK, NULL, NULL, dwType, dwOffset);
}

void CodeGenVisitor::Visit(ASTBinaryOpNode* node) {
    DBP_TRACE("ASTCodeGen: Visiting ASTBinaryOpNode");
    if (node->m_left) node->m_left->Accept(this);
    if (node->m_right) node->m_right->Accept(this);
    m_codeGen->WriteASMLine(ASM_POPEBX, "");
    m_codeGen->WriteASMLine(ASM_POPEAX, "");
    if (node->m_op == BinaryOpType::Add) {
        m_codeGen->WriteASMLine(ASM_ADDEAXEBX4, "");
    } else if (node->m_op == BinaryOpType::Subtract) {
        m_codeGen->WriteASMLine(ASM_SUBEAXEBX4, "");
    }
    m_codeGen->WriteASMEAXtoX(PMODE_STACK, NULL, NULL, 1, 0);
}

void CodeGenVisitor::Visit(ASTIfNode* node) {
    DBP_TRACE("ASTCodeGen: Visiting ASTIfNode");
    if (node->m_condition) node->m_condition->Accept(this);
    if (node->m_thenBranch) node->m_thenBranch->Accept(this);
    if (node->m_elseBranch) node->m_elseBranch->Accept(this);
}

void CodeGenVisitor::Visit(ASTWhileNode* node) {
    DBP_TRACE("ASTCodeGen: Visiting ASTWhileNode");
    if (node->m_condition) node->m_condition->Accept(this);
    if (node->m_body) node->m_body->Accept(this);
}

void CodeGenVisitor::Visit(ASTForNode* node) {
    DBP_TRACE("ASTCodeGen: Visiting ASTForNode");
    if (node->m_startExpr) node->m_startExpr->Accept(this);
    if (node->m_endExpr) node->m_endExpr->Accept(this);
    if (node->m_stepExpr) node->m_stepExpr->Accept(this);
    if (node->m_body) node->m_body->Accept(this);
}

void CodeGenVisitor::Visit(ASTFunctionCallNode* node) {
    DBP_TRACE("ASTCodeGen: Visiting ASTFunctionCallNode: {}", node->m_funcName);
    for (auto& arg : node->m_arguments) {
        if (arg) arg->Accept(this);
    }
}

void CodeGenVisitor::Visit(ASTFunctionDeclNode* node) {
    DBP_TRACE("ASTCodeGen: Visiting ASTFunctionDeclNode: {}", node->m_funcName);
    if (node->m_body) node->m_body->Accept(this);
    if (node->m_returnExpr) node->m_returnExpr->Accept(this);
}
