#pragma once

class ASTProgramNode;
class ASTBlockNode;
class ASTAssignmentNode;
class ASTLiteralNode;
class ASTVariableNode;
class ASTBinaryOpNode;
class ASTIfNode;
class ASTWhileNode;
class ASTForNode;
class ASTFunctionCallNode;
class ASTFunctionDeclNode;
class ASTArrayDimNode;
class ASTArrayAccessNode;
class ASTStructDeclNode;
class ASTStructAccessNode;

class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;
    virtual void Visit(ASTProgramNode* node) = 0;
    virtual void Visit(ASTBlockNode* node) = 0;
    virtual void Visit(ASTAssignmentNode* node) = 0;
    virtual void Visit(ASTLiteralNode* node) = 0;
    virtual void Visit(ASTVariableNode* node) = 0;
    virtual void Visit(ASTBinaryOpNode* node) = 0;
    virtual void Visit(ASTIfNode* node) = 0;
    virtual void Visit(ASTWhileNode* node) = 0;
    virtual void Visit(ASTForNode* node) = 0;
    virtual void Visit(ASTFunctionCallNode* node) = 0;
    virtual void Visit(ASTFunctionDeclNode* node) = 0;
    virtual void Visit(ASTArrayDimNode* node) = 0;
    virtual void Visit(ASTArrayAccessNode* node) = 0;
    virtual void Visit(ASTStructDeclNode* node) = 0;
    virtual void Visit(ASTStructAccessNode* node) = 0;
};
