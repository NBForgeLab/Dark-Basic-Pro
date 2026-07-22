#pragma once
#include "ASTVisitor.h"
#include <windows.h>
#include <map>
#include <string>

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
    void Visit(ASTIfNode* node) override;
    void Visit(ASTWhileNode* node) override;
    void Visit(ASTForNode* node) override;
    void Visit(ASTFunctionCallNode* node) override;
    void Visit(ASTFunctionDeclNode* node) override;
    void Visit(ASTArrayDimNode* node) override;
    void Visit(ASTArrayAccessNode* node) override;
    void Visit(ASTStructDeclNode* node) override;
    void Visit(ASTStructAccessNode* node) override;

    DWORD GetInferredType() const { return m_inferredType; }
    bool HasErrors() const { return m_hasErrors; }

private:
    DWORD m_inferredType = 0; // 1 = int, 2 = float, 3 = string
    bool m_hasErrors = false;
    std::map<std::string, DWORD> m_declaredVars;
};
