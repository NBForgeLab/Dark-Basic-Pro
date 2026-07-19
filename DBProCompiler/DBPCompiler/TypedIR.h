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
