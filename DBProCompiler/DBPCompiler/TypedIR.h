#pragma once
#include <string>
#include <vector>
#include <windows.h>
#include "ASTNodes.h"

enum class IROpCode {
    LoadConst,
    LoadVar,
    BinaryOp,
    StoreVar
};

struct IRInstruction {
    IROpCode opCode;
    std::string operandStr;
    DWORD typeVal = 0;
    BinaryOpType opType = BinaryOpType::Add;
};

struct IRProgram {
    std::vector<IRInstruction> instructions;
};

#include <sstream>

inline std::string PrintIR(const IRProgram& ir) {
    std::stringstream ss;
    for (size_t i = 0; i < ir.instructions.size(); ++i) {
        const auto& inst = ir.instructions[i];
        ss << "  [" << i << "] ";
        switch (inst.opCode) {
            case IROpCode::LoadConst:
                ss << "LoadConst: " << inst.operandStr << " (Type " << inst.typeVal << ")";
                break;
            case IROpCode::LoadVar:
                ss << "LoadVar: " << inst.operandStr;
                break;
            case IROpCode::BinaryOp: {
                std::string opStr;
                switch (inst.opType) {
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
                ss << "BinaryOp: " << opStr;
                break;
            }
            case IROpCode::StoreVar:
                ss << "StoreVar: " << inst.operandStr;
                break;
        }
        ss << "\n";
    }
    return ss.str();
}
