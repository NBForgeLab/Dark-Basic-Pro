#include "TargetCodegen.h"
#include "VarTable.h"
#include "ASMWriter.h"

extern CVarTable* g_pVarTable;

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
                CVarTable* pVar = g_pVarTable->FindVariable(NULL, const_cast<LPSTR>(inst.operandStr.c_str()), 0);
                DWORD dwType = 1, dwOffset = 0;
                if (pVar) {
                    dwType = pVar->GetVarTypeValue();
                    dwOffset = pVar->GetOffsetValue();
                }
                CStr varName(const_cast<LPSTR>(inst.operandStr.c_str()));
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
                }
                m_codeGen->WriteASMEAXtoX(PMODE_STACK, NULL, NULL, 1, 0);
                break;
            }
            case IROpCode::StoreVar: {
                m_codeGen->WriteASMLine(ASM_POPEAX, "");
                CVarTable* pVar = g_pVarTable->FindVariable(NULL, const_cast<LPSTR>(inst.operandStr.c_str()), 0);
                if (!pVar) return false;
                CStr varName(const_cast<LPSTR>(inst.operandStr.c_str()));
                DWORD dwAccessMode = m_codeGen->DetMode(&varName, pVar->GetVarTypeValue(), pVar->GetOffsetValue());
                m_codeGen->WriteASMEAXtoX(dwAccessMode, &varName, NULL, pVar->GetVarTypeValue(), pVar->GetOffsetValue());
                break;
            }
        }
    }
    return true;
}
