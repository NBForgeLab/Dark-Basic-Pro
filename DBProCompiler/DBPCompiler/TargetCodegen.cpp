#include "TargetCodegen.h"
#include "VarTable.h"
#include "StringUtils.h"
#include "ASMWriter.h"
#include "ParseUserFunction.h"
#include "ParserResultData.h"

extern CVarTable* g_pVarTable;
extern CParseUserFunction* g_pUserFunctionWithin;

TargetCodegen::TargetCodegen(ICodeGenerator* codeGen, DWORD lineNumber)
    : m_codeGen(codeGen), m_lineNumber(lineNumber) {}

bool TargetCodegen::Generate(const IRProgram& ir) {
    for (const auto& inst : ir.instructions) {
        switch (inst.opCode) {
            case IROpCode::LoadConst: {
                m_codeGen->WriteASMTaskCoreP1(m_lineNumber, static_cast<DWORD>(ASMTask::Push), inst.operandStr, inst.typeVal);
                break;
            }
            case IROpCode::LoadVar: {
                LPSTR pScope = nullptr;
                if (g_pUserFunctionWithin && g_pUserFunctionWithin->GetName()) {
                    pScope = g_pUserFunctionWithin->GetName()->GetStr();
                }
                CVarTable* pVar = g_pVarTable->FindVariable(pScope, const_cast<LPCSTR>(inst.operandStr.c_str()), 0);
                if (!pVar) {
                    pVar = g_pVarTable->FindVariable(const_cast<LPCSTR>(""), const_cast<LPCSTR>(inst.operandStr.c_str()), 0);
                    pScope = nullptr; // It's global
                }
                DWORD dwType = 1, dwOffset = 0;
                CStructTable* pStruct = nullptr;
                if (pVar) {
                    dwType = pVar->GetVarTypeValue();
                    dwOffset = pVar->GetOffsetValue();
                    pStruct = pVar->GetVarStruct();
                }
                
                std::string decorated;
                if (pVar && pScope && !dbp::iequals(pScope, "")) {
                    decorated = "FS@" + std::string(pScope) + "@" + inst.operandStr;
                } else {
                    decorated = "@" + inst.operandStr;
                }

                CStr varName(const_cast<LPCSTR>(decorated.c_str()));
                if (pVar && pScope && !dbp::iequals(pScope, "")) {
                    CResultData rd;
                    rd.m_pStringToken.reset(&varName);
                    rd.m_pAdditionalOffset.reset();
                    rd.m_dwType = dwType;
                    rd.m_dwDataOffset = dwOffset;
                    rd.m_pStruct = pStruct;
                    varName.TranslateForDBM(&rd);
                    rd.m_pStringToken.release(); // non-owning: don't delete stack variable
                    dwOffset = 0;
                }

                DWORD dwAccessMode = m_codeGen->DetMode(&varName, dwType, dwOffset, nullptr);
                m_codeGen->WriteASMXtoRAX(dwAccessMode, &varName, nullptr, dwType, dwOffset);
                m_codeGen->WriteASMRAXtoX(static_cast<DWORD>(ParamMode::Stack), nullptr, nullptr, dwType, dwOffset);
                break;
            }
            case IROpCode::BinaryOp: {
                m_codeGen->WriteASMLine(static_cast<DWORD>(ASMOp::POPRBX), "");
                m_codeGen->WriteASMLine(static_cast<DWORD>(ASMOp::POPRAX), "");
                if (inst.opType == BinaryOpType::Add) {
                    m_codeGen->WriteASMLine(static_cast<DWORD>(ASMOp::ADDRAXRBX4), "");
                } else if (inst.opType == BinaryOpType::Subtract) {
                    m_codeGen->WriteASMLine(static_cast<DWORD>(ASMOp::SUBRAXRBX4), "");
                } else if (inst.opType == BinaryOpType::Multiply) {
                    m_codeGen->WriteASMLine(static_cast<DWORD>(ASMOp::MULRAXRBX4), "");
                } else if (inst.opType == BinaryOpType::Divide) {
                    m_codeGen->WriteASMLine(static_cast<DWORD>(ASMOp::DIVRAXRBX4), "");
                }
                m_codeGen->WriteASMRAXtoX(static_cast<DWORD>(ParamMode::Stack), nullptr, nullptr, 1, 0);
                break;
            }
            case IROpCode::StoreVar: {
                m_codeGen->WriteASMLine(static_cast<DWORD>(ASMOp::POPRAX), "");
                LPCSTR pScope = nullptr;
                if (g_pUserFunctionWithin && g_pUserFunctionWithin->GetName()) {
                    pScope = g_pUserFunctionWithin->GetName()->GetStr();
                }
                CVarTable* pVar = g_pVarTable->FindVariable(pScope, const_cast<LPCSTR>(inst.operandStr.c_str()), 0);
                if (!pVar) {
                    pVar = g_pVarTable->FindVariable(const_cast<LPCSTR>(""), const_cast<LPCSTR>(inst.operandStr.c_str()), 0);
                    pScope = nullptr; // It's global
                }
                if (!pVar) return false;
                DWORD dwType = pVar->GetVarTypeValue();
                DWORD dwOffset = pVar->GetOffsetValue();
                CStructTable* pStruct = pVar->GetVarStruct();

                std::string decorated;
                if (pScope && !dbp::iequals(pScope, "")) {
                    decorated = "FS@" + std::string(pScope) + "@" + inst.operandStr;
                } else {
                    decorated = "@" + inst.operandStr;
                }

                CStr varName(const_cast<LPCSTR>(decorated.c_str()));
                if (pScope && !dbp::iequals(pScope, "")) {
                    CResultData rd;
                    rd.m_pStringToken.reset(&varName);
                    rd.m_pAdditionalOffset.reset();
                    rd.m_dwType = dwType;
                    rd.m_dwDataOffset = dwOffset;
                    rd.m_pStruct = pStruct;
                    varName.TranslateForDBM(&rd);
                    rd.m_pStringToken.release(); // non-owning: don't delete stack variable
                    dwOffset = 0;
                }

                DWORD dwAccessMode = m_codeGen->DetMode(&varName, dwType, dwOffset, nullptr);
                m_codeGen->WriteASMRAXtoX(dwAccessMode, &varName, nullptr, dwType, dwOffset);
                break;
            }
            case IROpCode::JumpIfFalse: {
                m_codeGen->WriteASMLine(static_cast<DWORD>(ASMOp::POPRAX), "");
                m_codeGen->WriteASMLine(static_cast<DWORD>(ASMOp::CMPRAX4), "0");
                m_codeGen->WriteASMLine(static_cast<DWORD>(ASMOp::JE), inst.operandStr);
                break;
            }
            case IROpCode::Jump: {
                m_codeGen->WriteASMLine(static_cast<DWORD>(ASMOp::JMP), inst.operandStr);
                break;
            }
            case IROpCode::Label: {
                m_codeGen->WriteASMLine(0, inst.operandStr);
                break;
            }
            case IROpCode::Call: {
                m_codeGen->WriteASMLine(static_cast<DWORD>(ASMOp::CALLABS), inst.operandStr);
                break;
            }
        }
    }
    return true;
}
