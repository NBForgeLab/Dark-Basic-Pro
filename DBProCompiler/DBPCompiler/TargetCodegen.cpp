#include "TargetCodegen.h"
#include "VarTable.h"
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
                CStr valStr(const_cast<LPSTR>(inst.operandStr.c_str()));
                m_codeGen->WriteASMTaskCoreP1(m_lineNumber, static_cast<DWORD>(ASMTask::Push), &valStr, inst.typeVal);
                break;
            }
            case IROpCode::LoadVar: {
                LPSTR pScope = NULL;
                if (g_pUserFunctionWithin && g_pUserFunctionWithin->GetName()) {
                    pScope = g_pUserFunctionWithin->GetName()->GetStr();
                }
                CVarTable* pVar = g_pVarTable->FindVariable(pScope, const_cast<LPSTR>(inst.operandStr.c_str()), 0);
                if (!pVar) {
                    pVar = g_pVarTable->FindVariable(const_cast<LPSTR>(""), const_cast<LPSTR>(inst.operandStr.c_str()), 0);
                    pScope = NULL; // It's global
                }
                DWORD dwType = 1, dwOffset = 0;
                CStructTable* pStruct = NULL;
                if (pVar) {
                    dwType = pVar->GetVarTypeValue();
                    dwOffset = pVar->GetOffsetValue();
                    pStruct = pVar->GetVarStruct();
                }
                
                std::string decorated;
                if (pVar && pScope && stricmp(pScope, "") != 0) {
                    decorated = "FS@" + std::string(pScope) + "@" + inst.operandStr;
                } else {
                    decorated = "@" + inst.operandStr;
                }

                CStr varName(const_cast<LPSTR>(decorated.c_str()));
                if (pVar && pScope && stricmp(pScope, "") != 0) {
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

                DWORD dwAccessMode = m_codeGen->DetMode(&varName, dwType, dwOffset);
                m_codeGen->WriteASMXtoEAX(dwAccessMode, &varName, NULL, dwType, dwOffset);
                m_codeGen->WriteASMEAXtoX(static_cast<DWORD>(ParamMode::Stack), NULL, NULL, dwType, dwOffset);
                break;
            }
            case IROpCode::BinaryOp: {
                m_codeGen->WriteASMLine(static_cast<DWORD>(ASMOp::POPEBX), "");
                m_codeGen->WriteASMLine(static_cast<DWORD>(ASMOp::POPEAX), "");
                if (inst.opType == BinaryOpType::Add) {
                    m_codeGen->WriteASMLine(static_cast<DWORD>(ASMOp::ADDEAXEBX4), "");
                } else if (inst.opType == BinaryOpType::Subtract) {
                    m_codeGen->WriteASMLine(static_cast<DWORD>(ASMOp::SUBEAXEBX4), "");
                } else if (inst.opType == BinaryOpType::Multiply) {
                    m_codeGen->WriteASMLine(static_cast<DWORD>(ASMOp::MULEAXEBX4), "");
                } else if (inst.opType == BinaryOpType::Divide) {
                    m_codeGen->WriteASMLine(static_cast<DWORD>(ASMOp::DIVEAXEBX4), "");
                }
                m_codeGen->WriteASMEAXtoX(static_cast<DWORD>(ParamMode::Stack), NULL, NULL, 1, 0);
                break;
            }
            case IROpCode::StoreVar: {
                m_codeGen->WriteASMLine(static_cast<DWORD>(ASMOp::POPEAX), "");
                LPSTR pScope = NULL;
                if (g_pUserFunctionWithin && g_pUserFunctionWithin->GetName()) {
                    pScope = g_pUserFunctionWithin->GetName()->GetStr();
                }
                CVarTable* pVar = g_pVarTable->FindVariable(pScope, const_cast<LPSTR>(inst.operandStr.c_str()), 0);
                if (!pVar) {
                    pVar = g_pVarTable->FindVariable(const_cast<LPSTR>(""), const_cast<LPSTR>(inst.operandStr.c_str()), 0);
                    pScope = NULL; // It's global
                }
                if (!pVar) return false;
                DWORD dwType = pVar->GetVarTypeValue();
                DWORD dwOffset = pVar->GetOffsetValue();
                CStructTable* pStruct = pVar->GetVarStruct();

                std::string decorated;
                if (pScope && stricmp(pScope, "") != 0) {
                    decorated = "FS@" + std::string(pScope) + "@" + inst.operandStr;
                } else {
                    decorated = "@" + inst.operandStr;
                }

                CStr varName(const_cast<LPSTR>(decorated.c_str()));
                if (pScope && stricmp(pScope, "") != 0) {
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

                DWORD dwAccessMode = m_codeGen->DetMode(&varName, dwType, dwOffset);
                m_codeGen->WriteASMEAXtoX(dwAccessMode, &varName, NULL, dwType, dwOffset);
                break;
            }
            case IROpCode::JumpIfFalse: {
                m_codeGen->WriteASMLine(static_cast<DWORD>(ASMOp::POPEAX), "");
                m_codeGen->WriteASMLine(static_cast<DWORD>(ASMOp::CMPEAX4), "0");
                CStr labelName(const_cast<LPSTR>(inst.operandStr.c_str()));
                m_codeGen->WriteASMLine(static_cast<DWORD>(ASMOp::JE), labelName.GetStr());
                break;
            }
            case IROpCode::Jump: {
                CStr labelName(const_cast<LPSTR>(inst.operandStr.c_str()));
                m_codeGen->WriteASMLine(static_cast<DWORD>(ASMOp::JMP), labelName.GetStr());
                break;
            }
            case IROpCode::Label: {
                CStr labelName(const_cast<LPSTR>(inst.operandStr.c_str()));
                m_codeGen->WriteASMLine(0, labelName.GetStr());
                break;
            }
            case IROpCode::Call: {
                CStr funcLabel(const_cast<LPSTR>(inst.operandStr.c_str()));
                m_codeGen->WriteASMLine(static_cast<DWORD>(ASMOp::CALLABS), funcLabel.GetStr());
                break;
            }
        }
    }
    return true;
}
