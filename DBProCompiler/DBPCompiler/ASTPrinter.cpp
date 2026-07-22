#include "ASTPrinter.h"
#include "ASTNodes.h"

std::string ASTPrinter::Print(ASTNode* node) {
    m_ss.str("");
    m_ss.clear();
    m_indent = 0;
    if (node) {
        node->Accept(this);
    }
    return m_ss.str();
}

void ASTPrinter::PrintIndent() {
    for (int i = 0; i < m_indent; ++i) {
        m_ss << "  ";
    }
}

void ASTPrinter::Visit(ASTProgramNode* node) {
    PrintIndent();
    m_ss << "Program:\n";
    m_indent++;
    for (auto& stmt : node->m_statements) {
        if (stmt) stmt->Accept(this);
    }
    m_indent--;
}

void ASTPrinter::Visit(ASTBlockNode* node) {
    PrintIndent();
    m_ss << "Block:\n";
    m_indent++;
    for (auto& stmt : node->m_statements) {
        if (stmt) stmt->Accept(this);
    }
    m_indent--;
}

void ASTPrinter::Visit(ASTAssignmentNode* node) {
    PrintIndent();
    m_ss << "Assignment: " << node->m_varName << " =\n";
    m_indent++;
    if (node->m_expression) {
        node->m_expression->Accept(this);
    } else {
        PrintIndent();
        m_ss << "<null_expr>\n";
    }
    m_indent--;
}

void ASTPrinter::Visit(ASTLiteralNode* node) {
    PrintIndent();
    m_ss << "Literal: " << node->m_value << " (Type " << node->m_type << ")\n";
}

void ASTPrinter::Visit(ASTVariableNode* node) {
    PrintIndent();
    m_ss << "Variable: " << node->m_varName << "\n";
}

void ASTPrinter::Visit(ASTBinaryOpNode* node) {
    PrintIndent();
    std::string opStr;
    switch (node->m_op) {
        case BinaryOpType::Add: opStr = "+"; break;
        case BinaryOpType::Subtract: opStr = "-"; break;
        case BinaryOpType::Multiply: opStr = "*"; break;
        case BinaryOpType::Divide: opStr = "/"; break;
    }
    m_ss << "BinaryOp: " << opStr << "\n";
    m_indent++;
    if (node->m_left) node->m_left->Accept(this);
    if (node->m_right) node->m_right->Accept(this);
    m_indent--;
}
