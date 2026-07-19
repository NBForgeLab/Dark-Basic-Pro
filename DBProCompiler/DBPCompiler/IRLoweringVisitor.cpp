#include "IRLoweringVisitor.h"
#include "ASTNodes.h"

void IRLoweringVisitor::Visit(ASTProgramNode* node) {
    for (auto& stmt : node->m_statements) {
        stmt->Accept(this);
    }
}

void IRLoweringVisitor::Visit(ASTBlockNode* node) {
    for (auto& stmt : node->m_statements) {
        stmt->Accept(this);
    }
}

void IRLoweringVisitor::Visit(ASTAssignmentNode* node) {
    if (node->m_expression) {
        node->m_expression->Accept(this);
    }
    IRInstruction inst;
    inst.opCode = IROpCode::StoreVar;
    inst.operandStr = node->m_varName;
    m_program.instructions.push_back(inst);
}

void IRLoweringVisitor::Visit(ASTLiteralNode* node) {
    IRInstruction inst;
    inst.opCode = IROpCode::LoadConst;
    inst.operandStr = node->m_value;
    inst.typeVal = node->m_type;
    m_program.instructions.push_back(inst);
}

void IRLoweringVisitor::Visit(ASTVariableNode* node) {
    IRInstruction inst;
    inst.opCode = IROpCode::LoadVar;
    inst.operandStr = node->m_varName;
    m_program.instructions.push_back(inst);
}

void IRLoweringVisitor::Visit(ASTBinaryOpNode* node) {
    if (node->m_left) node->m_left->Accept(this);
    if (node->m_right) node->m_right->Accept(this);
    IRInstruction inst;
    inst.opCode = IROpCode::BinaryOp;
    inst.opType = node->m_op;
    m_program.instructions.push_back(inst);
}
