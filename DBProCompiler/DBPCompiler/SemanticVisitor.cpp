#include "SemanticVisitor.h"
#include "ASTNodes.h"
#include "VarTable.h"
#include "Error.h"
#include "ParseUserFunction.h"

extern CVarTable* g_pVarTable;
extern CError* g_pErrorReport;
extern CParseUserFunction* g_pUserFunctionWithin;

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
    LPSTR pScope = nullptr;
    if (g_pUserFunctionWithin && g_pUserFunctionWithin->GetName()) {
        pScope = g_pUserFunctionWithin->GetName()->GetStr();
    }

    uint32_t varType = 1;
    if (node->m_expression) {
        node->m_expression->Accept(this);
        if (m_inferredType != 0) varType = m_inferredType;
    }
    m_declaredVars[node->m_varName] = varType;

    if (g_pVarTable) {
        CVarTable* pVar = g_pVarTable->FindVariable(pScope, const_cast<LPCSTR>(node->m_varName.c_str()), 0);
        if (!pVar) {
            pVar = g_pVarTable->FindVariable(const_cast<LPCSTR>(""), const_cast<LPCSTR>(node->m_varName.c_str()), 0);
        }
        if (!pVar) {
            uint32_t dwAction = 0;
            g_pVarTable->AddVariable(const_cast<LPCSTR>(node->m_varName.c_str()), const_cast<LPCSTR>("integer"), 0, 0, true, &dwAction, false);
        }
    }
}

void SemanticVisitor::Visit(ASTLiteralNode* node) {
    m_inferredType = node->m_type;
}

void SemanticVisitor::Visit(ASTVariableNode* node) {
    auto it = m_declaredVars.find(node->m_varName);
    if (it != m_declaredVars.end()) {
        m_inferredType = it->second;
        return;
    }

    if (g_pVarTable) {
        LPCSTR pScope = nullptr;
        if (g_pUserFunctionWithin && g_pUserFunctionWithin->GetName()) {
            pScope = g_pUserFunctionWithin->GetName()->GetStr();
        }

        CVarTable* pVar = g_pVarTable->FindVariable(pScope, const_cast<LPCSTR>(node->m_varName.c_str()), 0);
        if (!pVar) {
            pVar = g_pVarTable->FindVariable(const_cast<LPCSTR>(""), const_cast<LPCSTR>(node->m_varName.c_str()), 0);
        }

        if (pVar) {
            m_inferredType = pVar->GetVarTypeValue();
            return;
        }
    }

    m_inferredType = 1;
}

void SemanticVisitor::Visit(ASTBinaryOpNode* node) {
    uint32_t leftType = 0;
    uint32_t rightType = 0;
    if (node->m_left) {
        node->m_left->Accept(this);
        leftType = m_inferredType;
    }
    if (node->m_right) {
        node->m_right->Accept(this);
        rightType = m_inferredType;
    }
    if (leftType != rightType && leftType != 0 && rightType != 0) {
        m_hasErrors = true;
    }
    if (node->m_op == BinaryOpType::Equal || node->m_op == BinaryOpType::LessThan ||
        node->m_op == BinaryOpType::GreaterThan || node->m_op == BinaryOpType::LessEqual ||
        node->m_op == BinaryOpType::GreaterEqual || node->m_op == BinaryOpType::NotEqual) {
        m_inferredType = 1; // Relational comparison result is boolean/integer
    } else {
        m_inferredType = leftType;
    }
}

void SemanticVisitor::Visit(ASTIfNode* node) {
    if (node->m_condition) {
        node->m_condition->Accept(this);
    }
    if (node->m_thenBranch) {
        node->m_thenBranch->Accept(this);
    }
    if (node->m_elseBranch) {
        node->m_elseBranch->Accept(this);
    }
}

void SemanticVisitor::Visit(ASTWhileNode* node) {
    if (node->m_condition) node->m_condition->Accept(this);
    if (node->m_body) node->m_body->Accept(this);
}

void SemanticVisitor::Visit(ASTForNode* node) {
    if (node->m_startExpr) node->m_startExpr->Accept(this);
    if (node->m_endExpr) node->m_endExpr->Accept(this);
    if (node->m_stepExpr) node->m_stepExpr->Accept(this);
    if (node->m_body) node->m_body->Accept(this);
}

void SemanticVisitor::Visit(ASTFunctionCallNode* node) {
    for (auto& arg : node->m_arguments) {
        if (arg) arg->Accept(this);
    }
}

void SemanticVisitor::Visit(ASTFunctionDeclNode* node) {
    if (node->m_body) node->m_body->Accept(this);
    if (node->m_returnExpr) node->m_returnExpr->Accept(this);
}

void SemanticVisitor::Visit(ASTArrayDimNode* node) {
    for (auto& dim : node->m_dimensions) {
        if (dim) dim->Accept(this);
    }
}

void SemanticVisitor::Visit(ASTArrayAccessNode* node) {
    for (auto& idx : node->m_indices) {
        if (idx) idx->Accept(this);
    }
    m_inferredType = 1;
}

void SemanticVisitor::Visit([[maybe_unused]] ASTStructDeclNode* node) {
}

void SemanticVisitor::Visit([[maybe_unused]] ASTStructAccessNode* node) {
    m_inferredType = 1;
}
