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

private:
    ICodeGenerator* m_codeGen;
    DWORD m_lineNumber;
};
