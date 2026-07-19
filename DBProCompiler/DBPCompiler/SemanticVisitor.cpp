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
