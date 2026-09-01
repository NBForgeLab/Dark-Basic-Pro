#pragma once
#include <cstdint>
#include <string_view>
#include "Str.h"

class CDeclaration; // Forward declaration
class CResultData;  // Forward declaration

class ICodeGenerator {
public:
    virtual ~ICodeGenerator() = default;

    DWORD m_dwLineNumber = 0;

    virtual void SetDefaultCompileFlags(bool bArraySafetyFlag) = 0;
    virtual void SetArrayCheckFlag(bool bFlag) = 0;
    virtual bool GetArrayCheckFlag(void) = 0;

    virtual void GenerateASMCodes(void) = 0;
    virtual void DefineASM(DWORD dwASMCode, LPCSTR pDebugStr, int iPreOp, int iOp1, int iOp2, bool bOpData, int iOp3 = -1) = 0;

    virtual bool CreateASMHeader(void) = 0;
    virtual bool CreateASMMiddle(int iPreOpCode, int iOpCode1, int iOpCode2, std::string_view opData, int iOp3 = -1) = 0;
    virtual bool CreateASMMiddle(int iPreOpCode, int iOpCode1, int iOpCode2, const char* lpOpData, int iOp3 = -1) {
        return CreateASMMiddle(iPreOpCode, iOpCode1, iOpCode2, lpOpData ? std::string_view(lpOpData) : std::string_view{}, iOp3);
    }
    virtual bool CreateASMMiddleCore(int iPreOpCode, int iOpCode1, int iOpCode2, std::string_view opData, std::string_view opData2, bool bSecondOpDataIsIMM, DWORD dwSecondOpDataIMMSize, int iOp3 = -1) = 0;
    virtual bool CreateASMMiddleCore(int iPreOpCode, int iOpCode1, int iOpCode2, const char* lpOpData, const char* lpOpData2, bool bSecondOpDataIsIMM, DWORD dwSecondOpDataIMMSize, int iOp3 = -1) {
        return CreateASMMiddleCore(iPreOpCode, iOpCode1, iOpCode2, lpOpData ? std::string_view(lpOpData) : std::string_view{}, lpOpData2 ? std::string_view(lpOpData2) : std::string_view{}, bSecondOpDataIsIMM, dwSecondOpDataIMMSize, iOp3);
    }
    virtual bool CheckAndExpandMCBMemory(void) = 0;
    virtual bool CheckAndExpandREFMemory(void) = 0;

    virtual DWORD GetCurrentMCPosition(void) = 0;

    virtual bool ReportAnyErrorsToCLI(void) = 0;
    virtual bool PrepareEXE(const char* pEXEFilename, bool bParsingMainProgram, bool bProceedToUpdate) = 0;
    virtual bool PrepareEXE(LPSTR pEXEFilename, bool bParsingMainProgram, bool bProceedToUpdate) {
        return PrepareEXE(static_cast<const char*>(pEXEFilename), bParsingMainProgram, bProceedToUpdate);
    }
    virtual bool UpdateMCB(DWORD dwProgramSize) = 0;
    virtual bool UpdateMCBRefData(void) = 0;
    virtual bool UpdateDLLData(void) = 0;
    virtual bool UpdateCommandData(void) = 0;
    virtual bool UpdateStringData(void) = 0;
    virtual bool UpdateDataData(void) = 0;
    virtual bool UpdateDynamicData(void) = 0;
    virtual bool UpdateStructurePatternData(void) = 0;

    virtual LPSTR MakeVarDataForTransfer(DWORD* dwDataSize) = 0;
    virtual LPSTR MakeVarValuesForTransfer(DWORD* dwDataSize) = 0;

    virtual void TraverseDecForPattern(DWORD dwBaseOffset, short pass, DWORD* dwPatternArrayCounter, DWORD* dwSizeOfUserTypePattern, CDeclaration* pDecMain) = 0;
    virtual void FreeMachineBlock(void) = 0;
    virtual void FreeAll(void) = 0;

    virtual DWORD GetBytePosOfLastInstruction(void) = 0;

    virtual DWORD DetermineASMCall(DWORD dwASMCodeAsAByte, DWORD dwTypeValue) = 0;
    virtual DWORD DetermineASMCallForREL(DWORD dwASMCodeAsAByte, DWORD dwTypeValue) = 0;
    virtual DWORD DetMode(CStr* pP, DWORD dwPType, DWORD dwPOffset, CStr* pPIndex) = 0;

    virtual void CalculateArrayOffsetInRBX(CStr* pStr) = 0;
    virtual void WriteASMRAXtoARR(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset) = 0;
    virtual void WriteASMARRtoRAX(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset) = 0;
    virtual void WriteASMXtoRAX(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset) = 0;
    virtual void WriteASMRAXtoX(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset) = 0;

    virtual bool WriteASMCall(DWORD dwLine, std::string_view dll, std::string_view decoratedName) = 0;
    virtual bool WriteASMCall(DWORD dwLine, const char* pDLL, const char* pDecoratedName) {
        return WriteASMCall(dwLine, pDLL ? std::string_view(pDLL) : std::string_view{}, pDecoratedName ? std::string_view(pDecoratedName) : std::string_view{});
    }
    virtual bool WriteASMTaskP1(DWORD dwLine, DWORD dwTask, CResultData* pP1) = 0;
    virtual bool WriteASMTaskP2(DWORD dwLine, DWORD dwTask, CResultData* pP1, CResultData* pP2) = 0;
    virtual bool WriteASMTaskP3(DWORD dwLine, DWORD dwTask, CResultData* pP1, CResultData* pP2, CResultData* pP3) = 0;
    virtual bool WriteASMTaskCoreP1(DWORD dwLine, DWORD dwTask, CStr* pP1, DWORD dwP1Type) = 0;
    virtual bool WriteASMTaskCoreP1(DWORD dwLine, DWORD dwTask, std::string_view p1, DWORD dwP1Type) {
        CStr s(p1);
        return WriteASMTaskCoreP1(dwLine, dwTask, &s, dwP1Type);
    }
    virtual bool WriteASMTaskCoreP2(DWORD dwLine, DWORD dwTask, CStr* pP1, DWORD dwP1Type, CStr* pP2, DWORD dwP2Type) = 0;
    virtual bool WriteASMTaskCoreP2(DWORD dwLine, DWORD dwTask, std::string_view p1, DWORD dwP1Type, std::string_view p2, DWORD dwP2Type) {
        CStr s1(p1), s2(p2);
        return WriteASMTaskCoreP2(dwLine, dwTask, &s1, dwP1Type, &s2, dwP2Type);
    }
    virtual bool WriteASMTaskCore(DWORD dwLine, DWORD dwTask, CStr* pP1, CStr* pP1Off, DWORD dwP1Type, DWORD dwP1Offset, CStr* pP2, CStr* pP2Off, DWORD dwP2Type, DWORD dwP2Offset) = 0;
    virtual bool WriteASMTaskCore(DWORD dwLine, DWORD dwTask, CStr* pP1, CStr* pP1Off, DWORD dwP1Type, DWORD dwP1Offset,
                                  CStr* pP2, CStr* pP2Off, DWORD dwP2Type, DWORD dwP2Offset,
                                  CStr* pP3, CStr* pP3Off, DWORD dwP3Type, DWORD dwP3Offset) = 0;

    virtual bool WriteASMLine(DWORD dwOp, std::string_view opData) = 0;
    virtual bool WriteASMLine(DWORD dwOp, const char* pOpData) {
        return WriteASMLine(dwOp, pOpData ? std::string_view(pOpData) : std::string_view{});
    }
    virtual bool WriteASMLine2(DWORD dwOp, std::string_view opData, std::string_view opData2) = 0;
    virtual bool WriteASMLine2(DWORD dwOp, const char* pOpData, const char* pOpData2) {
        return WriteASMLine2(dwOp, pOpData ? std::string_view(pOpData) : std::string_view{}, pOpData2 ? std::string_view(pOpData2) : std::string_view{});
    }
    virtual bool WriteASMLine1IMM(DWORD dwOp, std::string_view opData, DWORD dwSizeIMM) = 0;
    virtual bool WriteASMLine1IMM(DWORD dwOp, const char* pOpData, DWORD dwSizeIMM) {
        return WriteASMLine1IMM(dwOp, pOpData ? std::string_view(pOpData) : std::string_view{}, dwSizeIMM);
    }
    virtual bool WriteASMLine2IMM(DWORD dwOp, std::string_view opData, std::string_view opData2, DWORD dwSizeIMM) = 0;
    virtual bool WriteASMLine2IMM(DWORD dwOp, const char* pOpData, const char* pOpData2, DWORD dwSizeIMM) {
        return WriteASMLine2IMM(dwOp, pOpData ? std::string_view(pOpData) : std::string_view{}, pOpData2 ? std::string_view(pOpData2) : std::string_view{}, dwSizeIMM);
    }
    virtual bool WriteASMComment(std::string_view title, std::string_view c1, std::string_view c2, std::string_view c3) = 0;
    virtual bool WriteASMComment(const char* pTitle, const char* pC1, const char* pC2, const char* pC3) {
        return WriteASMComment(pTitle ? std::string_view(pTitle) : std::string_view{},
                               pC1 ? std::string_view(pC1) : std::string_view{},
                               pC2 ? std::string_view(pC2) : std::string_view{},
                               pC3 ? std::string_view(pC3) : std::string_view{});
    }

    virtual bool WriteASMLeapMarkerTop(void) = 0;
    virtual bool WriteASMLineLeapToTop(DWORD dwOp) = 0;
    virtual bool WriteASMLeapMarkerJumpToTop(void) = 0;

    virtual bool WriteASMLeapForwardMarker(void) = 0;
    virtual bool WriteASMLineLeap(DWORD dwOp, DWORD di) = 0;
    virtual bool WriteASMLeapMarkerJump(DWORD dwOp, DWORD di) = 0;
    virtual bool WriteASMLeapMarkerJumpNotEqual(DWORD di) = 0;
    virtual bool WriteASMLeapMarkerEnd(DWORD di) = 0;
    virtual bool WriteASMCheckBreakPointVar(void) = 0;
    virtual bool WriteASMForceEscapeAtCodeBREAK(void) = 0;
    virtual void SetBreakPointValue(void) = 0;
    virtual void RecordPendingCallArg(DWORD dwType, DWORD slotCount) {}

    virtual DWORD AddCommandToTable(std::string_view dllString, std::string_view commandString) = 0;
    virtual DWORD AddCommandToTable(const char* pDLLString, const char* pCommandString) {
        return AddCommandToTable(pDLLString ? std::string_view(pDLLString) : std::string_view{},
                                 pCommandString ? std::string_view(pCommandString) : std::string_view{});
    }
};
