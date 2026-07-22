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

enum class BinaryOpType {
    Add,
    Subtract,
    Multiply,
    Divide,
    Modulo,
    Equal,
    LessThan,
    GreaterThan,
    LessEqual,
    GreaterEqual,
    NotEqual
};

class ASTBinaryOpNode : public ASTNode {
public:
    BinaryOpType m_op;
    std::unique_ptr<ASTNode> m_left;
    std::unique_ptr<ASTNode> m_right;

    ASTBinaryOpNode(BinaryOpType op, std::unique_ptr<ASTNode> left, std::unique_ptr<ASTNode> right)
        : m_op(op), m_left(std::move(left)), m_right(std::move(right)) {}

    void Accept(ASTVisitor* visitor) override {
        visitor->Visit(this);
    }
};

class CASTAssignment {
public:
    std::string m_varName;
    std::string m_valStr;
    DWORD m_lineNumber;

    CASTAssignment(const std::string& varName, const std::string& valStr, DWORD lineNumber)
        : m_varName(varName), m_valStr(valStr), m_lineNumber(lineNumber) {}

    bool WriteDBM();
};
