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
        case BinaryOpType::Modulo: opStr = "MOD"; break;
        case BinaryOpType::Equal: opStr = "="; break;
        case BinaryOpType::LessThan: opStr = "<"; break;
        case BinaryOpType::GreaterThan: opStr = ">"; break;
        case BinaryOpType::LessEqual: opStr = "<="; break;
        case BinaryOpType::GreaterEqual: opStr = ">="; break;
        case BinaryOpType::NotEqual: opStr = "<>"; break;
    }
    m_ss << "BinaryOp: " << opStr << "\n";
    m_indent++;
    if (node->m_left) node->m_left->Accept(this);
    if (node->m_right) node->m_right->Accept(this);
    m_indent--;
}

void ASTPrinter::Visit(ASTIfNode* node) {
    PrintIndent();
    m_ss << "If:\n";
    m_indent++;
    PrintIndent();
    m_ss << "Condition:\n";
    m_indent++;
    if (node->m_condition) node->m_condition->Accept(this);
    m_indent--;
    PrintIndent();
    m_ss << "Then:\n";
    m_indent++;
    if (node->m_thenBranch) node->m_thenBranch->Accept(this);
    m_indent--;
    if (node->m_elseBranch) {
        PrintIndent();
        m_ss << "Else:\n";
        m_indent++;
        node->m_elseBranch->Accept(this);
        m_indent--;
    }
    m_indent--;
}

void ASTPrinter::Visit(ASTWhileNode* node) {
    PrintIndent();
    m_ss << "While:\n";
    m_indent++;
    PrintIndent();
    m_ss << "Condition:\n";
    m_indent++;
    if (node->m_condition) node->m_condition->Accept(this);
    m_indent--;
    PrintIndent();
    m_ss << "Body:\n";
    m_indent++;
    if (node->m_body) node->m_body->Accept(this);
    m_indent--;
    m_indent--;
}

void ASTPrinter::Visit(ASTForNode* node) {
    PrintIndent();
    m_ss << "For: " << node->m_varName << "\n";
    m_indent++;
    PrintIndent();
    m_ss << "Start:\n";
    m_indent++;
    if (node->m_startExpr) node->m_startExpr->Accept(this);
    m_indent--;
    PrintIndent();
    m_ss << "End:\n";
    m_indent++;
    if (node->m_endExpr) node->m_endExpr->Accept(this);
    m_indent--;
    if (node->m_stepExpr) {
        PrintIndent();
        m_ss << "Step:\n";
        m_indent++;
        node->m_stepExpr->Accept(this);
        m_indent--;
    }
    PrintIndent();
    m_ss << "Body:\n";
    m_indent++;
    if (node->m_body) node->m_body->Accept(this);
    m_indent--;
    m_indent--;
}

void ASTPrinter::Visit(ASTFunctionCallNode* node) {
    PrintIndent();
    m_ss << "FunctionCall: " << node->m_funcName << "\n";
    m_indent++;
    for (auto& arg : node->m_arguments) {
        if (arg) arg->Accept(this);
    }
    m_indent--;
}

void ASTPrinter::Visit(ASTFunctionDeclNode* node) {
    PrintIndent();
    m_ss << "FunctionDecl: " << node->m_funcName << "\n";
    m_indent++;
    PrintIndent();
    m_ss << "Body:\n";
    m_indent++;
    if (node->m_body) node->m_body->Accept(this);
    m_indent--;
    if (node->m_returnExpr) {
        PrintIndent();
        m_ss << "Return:\n";
        m_indent++;
        node->m_returnExpr->Accept(this);
        m_indent--;
    }
    m_indent--;
}
