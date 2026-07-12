#pragma once

class ASTProgramNode;
class ASTBlockNode;
class ASTAssignmentNode;
class ASTLiteralNode;
class ASTVariableNode;

class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;
    virtual void Visit(ASTProgramNode* node) = 0;
    virtual void Visit(ASTBlockNode* node) = 0;
    virtual void Visit(ASTAssignmentNode* node) = 0;
    virtual void Visit(ASTLiteralNode* node) = 0;
    virtual void Visit(ASTVariableNode* node) = 0;
};
