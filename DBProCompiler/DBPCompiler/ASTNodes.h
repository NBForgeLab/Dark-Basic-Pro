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

class ASTIfNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> m_condition;
    std::unique_ptr<ASTBlockNode> m_thenBranch;
    std::unique_ptr<ASTBlockNode> m_elseBranch;

    ASTIfNode(std::unique_ptr<ASTNode> condition,
              std::unique_ptr<ASTBlockNode> thenBranch,
              std::unique_ptr<ASTBlockNode> elseBranch = nullptr)
        : m_condition(std::move(condition)),
          m_thenBranch(std::move(thenBranch)),
          m_elseBranch(std::move(elseBranch)) {}

    void Accept(ASTVisitor* visitor) override {
        visitor->Visit(this);
    }
};

class ASTWhileNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> m_condition;
    std::unique_ptr<ASTBlockNode> m_body;

    ASTWhileNode(std::unique_ptr<ASTNode> condition, std::unique_ptr<ASTBlockNode> body)
        : m_condition(std::move(condition)), m_body(std::move(body)) {}

    void Accept(ASTVisitor* visitor) override {
        visitor->Visit(this);
    }
};

class ASTForNode : public ASTNode {
public:
    std::string m_varName;
    std::unique_ptr<ASTNode> m_startExpr;
    std::unique_ptr<ASTNode> m_endExpr;
    std::unique_ptr<ASTNode> m_stepExpr;
    std::unique_ptr<ASTBlockNode> m_body;

    ASTForNode(const std::string& varName,
               std::unique_ptr<ASTNode> startExpr,
               std::unique_ptr<ASTNode> endExpr,
               std::unique_ptr<ASTNode> stepExpr,
               std::unique_ptr<ASTBlockNode> body)
        : m_varName(varName),
          m_startExpr(std::move(startExpr)),
          m_endExpr(std::move(endExpr)),
          m_stepExpr(std::move(stepExpr)),
          m_body(std::move(body)) {}

    void Accept(ASTVisitor* visitor) override {
        visitor->Visit(this);
    }
};

class ASTFunctionCallNode : public ASTNode {
public:
    std::string m_funcName;
    std::vector<std::unique_ptr<ASTNode>> m_arguments;

    ASTFunctionCallNode(const std::string& funcName, std::vector<std::unique_ptr<ASTNode>> arguments)
        : m_funcName(funcName), m_arguments(std::move(arguments)) {}

    void Accept(ASTVisitor* visitor) override {
        visitor->Visit(this);
    }
};

class ASTFunctionDeclNode : public ASTNode {
public:
    std::string m_funcName;
    std::vector<std::string> m_parameters;
    std::unique_ptr<ASTBlockNode> m_body;
    std::unique_ptr<ASTNode> m_returnExpr;

    ASTFunctionDeclNode(const std::string& funcName,
                        std::vector<std::string> parameters,
                        std::unique_ptr<ASTBlockNode> body,
                        std::unique_ptr<ASTNode> returnExpr = nullptr)
        : m_funcName(funcName),
          m_parameters(std::move(parameters)),
          m_body(std::move(body)),
          m_returnExpr(std::move(returnExpr)) {}

    void Accept(ASTVisitor* visitor) override {
        visitor->Visit(this);
    }
};

class ASTArrayDimNode : public ASTNode {
public:
    std::string m_arrayName;
    std::vector<std::unique_ptr<ASTNode>> m_dimensions;
    DWORD m_elemType = 1;

    ASTArrayDimNode(const std::string& arrayName, std::vector<std::unique_ptr<ASTNode>> dimensions, DWORD elemType = 1)
        : m_arrayName(arrayName), m_dimensions(std::move(dimensions)), m_elemType(elemType) {}

    void Accept(ASTVisitor* visitor) override {
        visitor->Visit(this);
    }
};

class ASTArrayAccessNode : public ASTNode {
public:
    std::string m_arrayName;
    std::vector<std::unique_ptr<ASTNode>> m_indices;

    ASTArrayAccessNode(const std::string& arrayName, std::vector<std::unique_ptr<ASTNode>> indices)
        : m_arrayName(arrayName), m_indices(std::move(indices)) {}

    void Accept(ASTVisitor* visitor) override {
        visitor->Visit(this);
    }
};

struct ASTStructField {
    std::string name;
    DWORD type = 1;
};

class ASTStructDeclNode : public ASTNode {
public:
    std::string m_structName;
    std::vector<ASTStructField> m_fields;

    ASTStructDeclNode(const std::string& structName, std::vector<ASTStructField> fields)
        : m_structName(structName), m_fields(std::move(fields)) {}

    void Accept(ASTVisitor* visitor) override {
        visitor->Visit(this);
    }
};

class ASTStructAccessNode : public ASTNode {
public:
    std::string m_varName;
    std::string m_fieldName;

    ASTStructAccessNode(const std::string& varName, const std::string& fieldName)
        : m_varName(varName), m_fieldName(fieldName) {}

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
