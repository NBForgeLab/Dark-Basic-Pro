#include "ASTOptimizer.h"
#include <cstdlib>
#include <string>

std::unique_ptr<ASTNode> ASTOptimizer::Optimize(std::unique_ptr<ASTNode> node) {
    if (!node) return nullptr;
    node->Accept(this);
    return std::move(m_resultNode);
}

void ASTOptimizer::Visit(ASTProgramNode* node) {
    for (auto& stmt : node->m_statements) {
        stmt = Optimize(std::move(stmt));
    }
    m_resultNode = nullptr;
}

void ASTOptimizer::Visit(ASTBlockNode* node) {
    for (auto& stmt : node->m_statements) {
        stmt = Optimize(std::move(stmt));
    }
    m_resultNode = nullptr;
}

void ASTOptimizer::Visit(ASTAssignmentNode* node) {
    if (node->m_expression) {
        node->m_expression = Optimize(std::move(node->m_expression));
    }
    m_resultNode = nullptr;
}

void ASTOptimizer::Visit(ASTLiteralNode* node) {
    m_resultNode = std::make_unique<ASTLiteralNode>(node->m_value, node->m_type);
}

void ASTOptimizer::Visit(ASTVariableNode* node) {
    m_resultNode = std::make_unique<ASTVariableNode>(node->m_varName);
}

void ASTOptimizer::Visit(ASTBinaryOpNode* node) {
    auto leftOpt = Optimize(std::move(node->m_left));
    auto rightOpt = Optimize(std::move(node->m_right));

    auto leftLit = dynamic_cast<ASTLiteralNode*>(leftOpt.get());
    auto rightLit = dynamic_cast<ASTLiteralNode*>(rightOpt.get());

    if (leftLit && rightLit && leftLit->m_type == 1 && rightLit->m_type == 1) {
        int leftVal = std::atoi(leftLit->m_value.c_str());
        int rightVal = std::atoi(rightLit->m_value.c_str());
        int resultVal = 0;
        bool folded = true;

        switch (node->m_op) {
            case BinaryOpType::Add: resultVal = leftVal + rightVal; break;
            case BinaryOpType::Subtract: resultVal = leftVal - rightVal; break;
            case BinaryOpType::Multiply: resultVal = leftVal * rightVal; break;
            case BinaryOpType::Divide: 
                if (rightVal != 0) resultVal = leftVal / rightVal;
                else folded = false;
                break;
            default:
                folded = false;
                break;
        }

        if (folded) {
            m_resultNode = std::make_unique<ASTLiteralNode>(std::to_string(resultVal), 1);
            return;
        }
    }

    m_resultNode = std::make_unique<ASTBinaryOpNode>(node->m_op, std::move(leftOpt), std::move(rightOpt));
}

void ASTOptimizer::Visit(ASTIfNode* node) {
    if (node->m_condition) node->m_condition = Optimize(std::move(node->m_condition));
    if (node->m_thenBranch) Optimize(std::move(node->m_thenBranch));
    if (node->m_elseBranch) Optimize(std::move(node->m_elseBranch));
    m_resultNode = nullptr;
}

void ASTOptimizer::Visit(ASTWhileNode* node) {
    if (node->m_condition) node->m_condition = Optimize(std::move(node->m_condition));
    if (node->m_body) Optimize(std::move(node->m_body));
    m_resultNode = nullptr;
}

void ASTOptimizer::Visit(ASTForNode* node) {
    if (node->m_startExpr) node->m_startExpr = Optimize(std::move(node->m_startExpr));
    if (node->m_endExpr) node->m_endExpr = Optimize(std::move(node->m_endExpr));
    if (node->m_stepExpr) node->m_stepExpr = Optimize(std::move(node->m_stepExpr));
    if (node->m_body) Optimize(std::move(node->m_body));
    m_resultNode = nullptr;
}

void ASTOptimizer::Visit(ASTFunctionCallNode* node) {
    for (auto& arg : node->m_arguments) {
        arg = Optimize(std::move(arg));
    }
    m_resultNode = nullptr;
}

void ASTOptimizer::Visit(ASTFunctionDeclNode* node) {
    if (node->m_body) Optimize(std::move(node->m_body));
    if (node->m_returnExpr) node->m_returnExpr = Optimize(std::move(node->m_returnExpr));
    m_resultNode = nullptr;
}

void ASTOptimizer::Visit(ASTArrayDimNode* node) {
    for (auto& dim : node->m_dimensions) {
        dim = Optimize(std::move(dim));
    }
    m_resultNode = nullptr;
}

void ASTOptimizer::Visit(ASTArrayAccessNode* node) {
    for (auto& idx : node->m_indices) {
        idx = Optimize(std::move(idx));
    }
    m_resultNode = nullptr;
}

void ASTOptimizer::Visit(ASTStructDeclNode* node) {
    m_resultNode = nullptr;
}

void ASTOptimizer::Visit(ASTStructAccessNode* node) {
    m_resultNode = std::make_unique<ASTStructAccessNode>(node->m_varName, node->m_fieldName);
}
