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

private:
    std::stringstream m_ss;
    int m_indent = 0;

    void PrintIndent();
};
