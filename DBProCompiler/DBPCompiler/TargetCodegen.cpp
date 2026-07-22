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
                m_codeGen->WriteASMTaskCoreP1(m_lineNumber, ASMTASK_PUSH, &valStr, inst.typeVal);
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
                    rd.m_pStringToken = &varName;
                    rd.m_pAdditionalOffset = NULL;
                    rd.m_dwType = dwType;
                    rd.m_dwDataOffset = dwOffset;
                    rd.m_pStruct = pStruct;
                    varName.TranslateForDBM(&rd);
                    dwOffset = 0;
                }

                DWORD dwAccessMode = m_codeGen->DetMode(&varName, dwType, dwOffset);
                m_codeGen->WriteASMXtoEAX(dwAccessMode, &varName, NULL, dwType, dwOffset);
                m_codeGen->WriteASMEAXtoX(PMODE_STACK, NULL, NULL, dwType, dwOffset);
                break;
            }
            case IROpCode::BinaryOp: {
                m_codeGen->WriteASMLine(ASM_POPEBX, "");
                m_codeGen->WriteASMLine(ASM_POPEAX, "");
                if (inst.opType == BinaryOpType::Add) {
                    m_codeGen->WriteASMLine(ASM_ADDEAXEBX4, "");
                } else if (inst.opType == BinaryOpType::Subtract) {
                    m_codeGen->WriteASMLine(ASM_SUBEAXEBX4, "");
                } else if (inst.opType == BinaryOpType::Multiply) {
                    m_codeGen->WriteASMLine(ASM_MULEAXEBX4, "");
                } else if (inst.opType == BinaryOpType::Divide) {
                    m_codeGen->WriteASMLine(ASM_DIVEAXEBX4, "");
                }
                m_codeGen->WriteASMEAXtoX(PMODE_STACK, NULL, NULL, 1, 0);
                break;
            }
            case IROpCode::StoreVar: {
                m_codeGen->WriteASMLine(ASM_POPEAX, "");
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
                    rd.m_pStringToken = &varName;
                    rd.m_pAdditionalOffset = NULL;
                    rd.m_dwType = dwType;
                    rd.m_dwDataOffset = dwOffset;
                    rd.m_pStruct = pStruct;
                    varName.TranslateForDBM(&rd);
                    dwOffset = 0;
                }

                DWORD dwAccessMode = m_codeGen->DetMode(&varName, dwType, dwOffset);
                m_codeGen->WriteASMEAXtoX(dwAccessMode, &varName, NULL, dwType, dwOffset);
                break;
            }
            case IROpCode::JumpIfFalse: {
                m_codeGen->WriteASMLine(ASM_POPEAX, "");
                m_codeGen->WriteASMLine(ASM_CMPEAX4, "0");
                CStr labelName(const_cast<LPSTR>(inst.operandStr.c_str()));
                m_codeGen->WriteASMLine(ASM_JE, labelName.GetStr());
                break;
            }
            case IROpCode::Jump: {
                CStr labelName(const_cast<LPSTR>(inst.operandStr.c_str()));
                m_codeGen->WriteASMLine(ASM_JMP, labelName.GetStr());
                break;
            }
            case IROpCode::Label: {
                CStr labelName(const_cast<LPSTR>(inst.operandStr.c_str()));
                m_codeGen->WriteASMLine(0, labelName.GetStr());
                break;
            }
            case IROpCode::Call: {
                CStr funcLabel(const_cast<LPSTR>(inst.operandStr.c_str()));
                m_codeGen->WriteASMLine(ASM_CALLABS, funcLabel.GetStr());
                break;
            }
        }
    }
    return true;
}
