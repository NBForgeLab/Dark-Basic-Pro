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
    void Visit(ASTIfNode* node) override;
    void Visit(ASTWhileNode* node) override;
    void Visit(ASTForNode* node) override;
    void Visit(ASTFunctionCallNode* node) override;
    void Visit(ASTFunctionDeclNode* node) override;

    IRProgram GetProgram() const { return m_program; }

private:
    IRProgram m_program;
};
