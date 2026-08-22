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

static int g_labelCounter = 0;

void IRLoweringVisitor::Visit(ASTIfNode* node) {
    int id = ++g_labelCounter;
    std::string elseLabel = "L_else_" + std::to_string(id);
    std::string endLabel = "L_endif_" + std::to_string(id);

    if (node->m_condition) {
        node->m_condition->Accept(this);
    }

    IRInstruction jmpFalse;
    jmpFalse.opCode = IROpCode::JumpIfFalse;
    jmpFalse.operandStr = node->m_elseBranch ? elseLabel : endLabel;
    m_program.instructions.push_back(jmpFalse);

    if (node->m_thenBranch) {
        node->m_thenBranch->Accept(this);
    }

    if (node->m_elseBranch) {
        IRInstruction jmpEnd;
        jmpEnd.opCode = IROpCode::Jump;
        jmpEnd.operandStr = endLabel;
        m_program.instructions.push_back(jmpEnd);

        IRInstruction elseLblInst;
        elseLblInst.opCode = IROpCode::Label;
        elseLblInst.operandStr = elseLabel;
        m_program.instructions.push_back(elseLblInst);

        node->m_elseBranch->Accept(this);
    }

    IRInstruction endLblInst;
    endLblInst.opCode = IROpCode::Label;
    endLblInst.operandStr = endLabel;
    m_program.instructions.push_back(endLblInst);
}

void IRLoweringVisitor::Visit(ASTWhileNode* node) {
    int id = ++g_labelCounter;
    std::string startLabel = "L_while_start_" + std::to_string(id);
    std::string endLabel = "L_while_end_" + std::to_string(id);

    IRInstruction startLblInst;
    startLblInst.opCode = IROpCode::Label;
    startLblInst.operandStr = startLabel;
    m_program.instructions.push_back(startLblInst);

    if (node->m_condition) {
        node->m_condition->Accept(this);
    }

    IRInstruction jmpFalse;
    jmpFalse.opCode = IROpCode::JumpIfFalse;
    jmpFalse.operandStr = endLabel;
    m_program.instructions.push_back(jmpFalse);

    if (node->m_body) {
        node->m_body->Accept(this);
    }

    IRInstruction jmpStart;
    jmpStart.opCode = IROpCode::Jump;
    jmpStart.operandStr = startLabel;
    m_program.instructions.push_back(jmpStart);

    IRInstruction endLblInst;
    endLblInst.opCode = IROpCode::Label;
    endLblInst.operandStr = endLabel;
    m_program.instructions.push_back(endLblInst);
}

void IRLoweringVisitor::Visit(ASTForNode* node) {
    int id = ++g_labelCounter;
    std::string startLabel = "L_for_start_" + std::to_string(id);
    std::string endLabel = "L_for_end_" + std::to_string(id);

    if (node->m_startExpr) {
        node->m_startExpr->Accept(this);
        IRInstruction storeInst;
        storeInst.opCode = IROpCode::StoreVar;
        storeInst.operandStr = node->m_varName;
        m_program.instructions.push_back(storeInst);
    }

    IRInstruction startLblInst;
    startLblInst.opCode = IROpCode::Label;
    startLblInst.operandStr = startLabel;
    m_program.instructions.push_back(startLblInst);

    IRInstruction loadVarInst;
    loadVarInst.opCode = IROpCode::LoadVar;
    loadVarInst.operandStr = node->m_varName;
    m_program.instructions.push_back(loadVarInst);

    if (node->m_endExpr) {
        node->m_endExpr->Accept(this);
    }

    IRInstruction cmpInst;
    cmpInst.opCode = IROpCode::BinaryOp;
    cmpInst.opType = BinaryOpType::LessEqual;
    m_program.instructions.push_back(cmpInst);

    IRInstruction jmpFalse;
    jmpFalse.opCode = IROpCode::JumpIfFalse;
    jmpFalse.operandStr = endLabel;
    m_program.instructions.push_back(jmpFalse);

    if (node->m_body) {
        node->m_body->Accept(this);
    }

    m_program.instructions.push_back(loadVarInst);
    if (node->m_stepExpr) {
        node->m_stepExpr->Accept(this);
    } else {
        IRInstruction stepOne;
        stepOne.opCode = IROpCode::LoadConst;
        stepOne.operandStr = "1";
        stepOne.typeVal = 1;
        m_program.instructions.push_back(stepOne);
    }
    IRInstruction addInst;
    addInst.opCode = IROpCode::BinaryOp;
    addInst.opType = BinaryOpType::Add;
    m_program.instructions.push_back(addInst);

    IRInstruction storeStep;
    storeStep.opCode = IROpCode::StoreVar;
    storeStep.operandStr = node->m_varName;
    m_program.instructions.push_back(storeStep);

    IRInstruction jmpStart;
    jmpStart.opCode = IROpCode::Jump;
    jmpStart.operandStr = startLabel;
    m_program.instructions.push_back(jmpStart);

    IRInstruction endLblInst;
    endLblInst.opCode = IROpCode::Label;
    endLblInst.operandStr = endLabel;
    m_program.instructions.push_back(endLblInst);
}

void IRLoweringVisitor::Visit(ASTFunctionCallNode* node) {
    for (auto& arg : node->m_arguments) {
        if (arg) arg->Accept(this);
    }
    IRInstruction inst;
    inst.opCode = IROpCode::Call;
    inst.operandStr = node->m_funcName;
    m_program.instructions.push_back(inst);
}

void IRLoweringVisitor::Visit(ASTFunctionDeclNode* node) {
    IRInstruction funcLbl;
    funcLbl.opCode = IROpCode::Label;
    funcLbl.operandStr = "func_" + node->m_funcName;
    m_program.instructions.push_back(funcLbl);

    if (node->m_body) {
        node->m_body->Accept(this);
    }

    if (node->m_returnExpr) {
        node->m_returnExpr->Accept(this);
    }
}

void IRLoweringVisitor::Visit(ASTArrayDimNode* node) {
    for (auto& dim : node->m_dimensions) {
        if (dim) dim->Accept(this);
    }
}

void IRLoweringVisitor::Visit(ASTArrayAccessNode* node) {
    for (auto& idx : node->m_indices) {
        if (idx) idx->Accept(this);
    }
    IRInstruction inst;
    inst.opCode = IROpCode::LoadVar;
    inst.operandStr = node->m_arrayName;
    m_program.instructions.push_back(inst);
}

void IRLoweringVisitor::Visit([[maybe_unused]] ASTStructDeclNode* node) {
}

void IRLoweringVisitor::Visit(ASTStructAccessNode* node) {
    IRInstruction inst;
    inst.opCode = IROpCode::LoadVar;
    inst.operandStr = node->m_varName + "." + node->m_fieldName;
    m_program.instructions.push_back(inst);
}
