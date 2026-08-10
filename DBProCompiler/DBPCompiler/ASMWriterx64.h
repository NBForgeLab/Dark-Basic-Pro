#pragma once
#include "ICodeGenerator.h"
#include <cstdint>
#include <cstddef>
#include <vector>

enum class X64Register : uint8_t {
    None = 0,
    RAX,
    RCX,
    RDX,
    RBX,
    RSP,
    RBP,
    RSI,
    RDI,
    R8,
    R9,
    R10,
    R11,
    R12,
    R13,
    R14,
    R15
};

class CASMWriterx64 : public ICodeGenerator {
public:
    CASMWriterx64();
    ~CASMWriterx64() override = default;

    static X64Register GetArgumentRegister(size_t index) noexcept {
        switch (index) {
            case 0: return X64Register::RCX;
            case 1: return X64Register::RDX;
            case 2: return X64Register::R8;
            case 3: return X64Register::R9;
            default: return X64Register::None;
        }
    }

    static constexpr uint32_t GetShadowSpaceSize() noexcept {
        return 32U;
    }

    static constexpr uint32_t AlignStackFrame(uint32_t bytes) noexcept {
        const uint32_t aligned = (bytes + 15U) & ~15U;
        return aligned < 32U ? 32U : aligned;
    }

    // Instruction Emission Helpers
    void EmitByte(uint8_t b);
    void EmitDword(uint32_t dw);
    void EmitQword(uint64_t qw);

    void EmitMovRegImm64(X64Register reg, uint64_t val);
    void EmitPushReg(X64Register reg);
    void EmitPopReg(X64Register reg);
    void EmitSubRegImm32(X64Register reg, uint32_t val);
    void EmitAddRegImm32(X64Register reg, uint32_t val);
    void EmitRet();

    [[nodiscard]] const std::vector<uint8_t>& GetCodeBuffer() const noexcept { return m_codeBuffer; }
    [[nodiscard]] size_t GetCodeSize() const noexcept { return m_codeBuffer.size(); }
    void ClearCodeBuffer() noexcept { m_codeBuffer.clear(); }

    // ICodeGenerator interface implementation
    void SetDefaultCompileFlags(bool bArraySafetyFlag) override;
    void SetArrayCheckFlag(bool bFlag) override;
    bool GetArrayCheckFlag(void) override;

    void GenerateASMCodes(void) override;
    void DefineASM(DWORD dwASMCode, LPSTR pDebugStr, int iPreOp, int iOp1, int iOp2, bool bOpData) override;

    bool CreateASMHeader(void) override;
    bool CreateASMMiddle(int iPreOpCode, int iOpCode1, int iOpCode2, LPSTR lpOpData) override;
    bool CreateASMMiddleCore(int iPreOpCode, int iOpCode1, int iOpCode2, LPSTR lpOpData, LPSTR lpOpData2, bool bSecondOpDataIsIMM, DWORD dwSecondOpDataIMMSize) override;
    bool CheckAndExpandMCBMemory(void) override;
    bool CheckAndExpandREFMemory(void) override;

    DWORD GetCurrentMCPosition(void) override;

    bool ReportAnyErrorsToCLI(void) override;
    bool PrepareEXE(LPSTR pEXEFilename, bool bParsingMainProgram, bool bProceedToUpdate) override;
    bool UpdateMCB(DWORD dwProgramSize) override;
    bool UpdateMCBRefData(void) override;
    bool UpdateDLLData(void) override;
    bool UpdateCommandData(void) override;
    bool UpdateStringData(void) override;
    bool UpdateDataData(void) override;
    bool UpdateDynamicData(void) override;

    void UpdateStructurePatternDataRec(LPSTR pPattern, CDeclaration* pDecMain) override;
    bool UpdateStructurePatternData(void) override;

    LPSTR MakeVarDataForTransfer(DWORD* dwDataSize) override;
    LPSTR MakeVarValuesForTransfer(DWORD* dwDataSize) override;

    void TraverseDecForPattern(DWORD dwBaseOffset, short pass, DWORD* dwPatternArrayCounter, DWORD* dwSizeOfUserTypePattern, CDeclaration* pDecMain) override;
    void FreeMachineBlock(void) override;
    void FreeAll(void) override;

    DWORD GetBytePosOfLastInstruction(void) override;

    DWORD DetermineASMCall(DWORD dwASMCodeAsAByte, DWORD dwTypeValue) override;
    DWORD DetermineASMCallForREL(DWORD dwASMCodeAsAByte, DWORD dwTypeValue) override;
    DWORD DetMode(CStr* pP, DWORD dwPType, DWORD dwPOffset) override;

    void CalculateArrayOffsetInEBX(CStr* pStr) override;
    void WriteASMEAXtoARR(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset) override;
    void WriteASMARRtoEAX(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset) override;
    void WriteASMXtoEAX(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset) override;
    void WriteASMEAXtoX(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset) override;

    bool WriteASMCall(DWORD dwLine, LPSTR pDLL, LPSTR pDecoratedName) override;
    bool WriteASMTaskP1(DWORD dwLine, DWORD dwTask, CResultData* pP1) override;
    bool WriteASMTaskP2(DWORD dwLine, DWORD dwTask, CResultData* pP1, CResultData* pP2) override;
    bool WriteASMTaskP3(DWORD dwLine, DWORD dwTask, CResultData* pP1, CResultData* pP2, CResultData* pP3) override;
    bool WriteASMTaskCoreP1(DWORD dwLine, DWORD dwTask, CStr* pP1, DWORD dwP1Type) override;
    bool WriteASMTaskCoreP2(DWORD dwLine, DWORD dwTask, CStr* pP1, DWORD dwP1Type, CStr* pP2, DWORD dwP2Type) override;
    bool WriteASMTaskCore(DWORD dwLine, DWORD dwTask, CStr* pP1, CStr* pP1Off, DWORD dwP1Type, DWORD dwP1Offset, CStr* pP2, CStr* pP2Off, DWORD dwP2Type, DWORD dwP2Offset) override;
    bool WriteASMTaskCore(DWORD dwLine, DWORD dwTask, CStr* pP1, CStr* pP1Off, DWORD dwP1Type, DWORD dwP1Offset,
                          CStr* pP2, CStr* pP2Off, DWORD dwP2Type, DWORD dwP2Offset,
                          CStr* pP3, CStr* pP3Off, DWORD dwP3Type, DWORD dwP3Offset) override;

    bool WriteASMLine(DWORD dwOp, LPSTR pOpData) override;
    bool WriteASMLine2(DWORD dwOp, LPSTR pOpData, LPSTR pOpData2) override;
    bool WriteASMLine1IMM(DWORD dwOp, LPSTR pOpData, DWORD dwSizeIMM) override;
    bool WriteASMLine2IMM(DWORD dwOp, LPSTR pOpData, LPSTR pOpData2, DWORD dwSizeIMM) override;
    bool WriteASMComment(LPSTR pTitle, LPSTR pC1, LPSTR pC2, LPSTR pC3) override;

    bool WriteASMLeapMarkerTop(void) override;
    bool WriteASMLineLeapToTop(DWORD dwOp) override;
    bool WriteASMLeapMarkerJumpToTop(void) override;

    bool WriteASMLeapForwardMarker(void) override;
    bool WriteASMLineLeap(DWORD dwOp, DWORD di) override;
    bool WriteASMLeapMarkerJump(DWORD dwOp, DWORD di) override;
    bool WriteASMLeapMarkerJumpNotEqual(DWORD di) override;
    bool WriteASMLeapMarkerEnd(DWORD di) override;
    bool WriteASMCheckBreakPointVar(void) override;
    bool WriteASMForceEscapeAtCodeBREAK(void) override;
    void SetBreakPointValue(void) override;

    DWORD AddCommandToTable(LPSTR pDLLString, LPSTR pCommandString) override;
    bool AddProtectionToSelectedDLLs(LPSTR pDLLString) override;

private:
    bool m_bArraySafetyFlag = false;
    std::vector<uint8_t> m_codeBuffer;
};
