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
