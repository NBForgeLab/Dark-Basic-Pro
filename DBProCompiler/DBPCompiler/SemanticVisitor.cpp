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
    LPSTR pScope = NULL;
    if (g_pUserFunctionWithin && g_pUserFunctionWithin->GetName()) {
        pScope = g_pUserFunctionWithin->GetName()->GetStr();
    }

    CVarTable* pVar = g_pVarTable->FindVariable(pScope, const_cast<LPSTR>(node->m_varName.c_str()), 0);
    if (!pVar) {
        pVar = g_pVarTable->FindVariable(const_cast<LPSTR>(""), const_cast<LPSTR>(node->m_varName.c_str()), 0);
    }

    if (!pVar) {
        m_hasErrors = true;
        if (g_pErrorReport) {
            DWORD lineNum = node->GetLocation().line;
            if (lineNum == 0) lineNum = 1;
            g_pErrorReport->SetError(lineNum, 100000 + 6, const_cast<LPSTR>(node->m_varName.c_str()));
        }
        return;
    }
    if (node->m_expression) {
        node->m_expression->Accept(this);
    }
}

void SemanticVisitor::Visit(ASTLiteralNode* node) {
    m_inferredType = node->m_type;
}

void SemanticVisitor::Visit(ASTVariableNode* node) {
    LPSTR pScope = NULL;
    if (g_pUserFunctionWithin && g_pUserFunctionWithin->GetName()) {
        pScope = g_pUserFunctionWithin->GetName()->GetStr();
    }

    CVarTable* pVar = g_pVarTable->FindVariable(pScope, const_cast<LPSTR>(node->m_varName.c_str()), 0);
    if (!pVar) {
        pVar = g_pVarTable->FindVariable(const_cast<LPSTR>(""), const_cast<LPSTR>(node->m_varName.c_str()), 0);
    }

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
