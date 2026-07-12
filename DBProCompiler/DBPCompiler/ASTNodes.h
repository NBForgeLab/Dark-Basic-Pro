#pragma once
#include <windows.h>
#include "ASTNode.h"
#include "ASTVisitor.h"
#include <string>
#include <memory>
#include <vector>

class ASTProgramNode : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> m_statements;

    void Accept(ASTVisitor* visitor) override {
        visitor->Visit(this);
    }
};

class ASTBlockNode : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> m_statements;

    void Accept(ASTVisitor* visitor) override {
        visitor->Visit(this);
    }
};

class ASTAssignmentNode : public ASTNode {
public:
    std::string m_varName;
    std::unique_ptr<ASTNode> m_expression;

    ASTAssignmentNode(const std::string& varName, std::unique_ptr<ASTNode> expression)
        : m_varName(varName), m_expression(std::move(expression)) {}

    void Accept(ASTVisitor* visitor) override {
        visitor->Visit(this);
    }
};

class ASTLiteralNode : public ASTNode {
public:
    std::string m_value;
    DWORD m_type; // e.g., 1 for int, 2 for float, 3 for string

    ASTLiteralNode(const std::string& value, DWORD type)
        : m_value(value), m_type(type) {}

    void Accept(ASTVisitor* visitor) override {
        visitor->Visit(this);
    }
};

class ASTVariableNode : public ASTNode {
public:
    std::string m_varName;

    ASTVariableNode(const std::string& varName)
        : m_varName(varName) {}

    void Accept(ASTVisitor* visitor) override {
        visitor->Visit(this);
    }
};
