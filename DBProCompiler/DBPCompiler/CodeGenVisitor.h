#pragma once
#include "ASTVisitor.h"
#include "ICodeGenerator.h"

class CodeGenVisitor : public ASTVisitor {
public:
    CodeGenVisitor(ICodeGenerator* codeGen, DWORD lineNumber = 1);
    ~CodeGenVisitor() override = default;

    void Visit(ASTProgramNode* node) override;
    void Visit(ASTBlockNode* node) override;
    void Visit(ASTAssignmentNode* node) override;
    void Visit(ASTLiteralNode* node) override;
    void Visit(ASTVariableNode* node) override;

private:
    ICodeGenerator* m_codeGen;
    DWORD m_lineNumber;
};
