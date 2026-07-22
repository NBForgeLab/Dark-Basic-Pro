#pragma once
#include "ASTNode.h"
#include "ASTVisitor.h"
#include <string>
#include <sstream>

class ASTPrinter : public ASTVisitor {
public:
    ASTPrinter() = default;
    ~ASTPrinter() override = default;

    std::string Print(ASTNode* node);

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

private:
    std::stringstream m_ss;
    int m_indent = 0;

    void PrintIndent();
};
