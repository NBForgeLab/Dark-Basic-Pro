#pragma once
#include "windows.h"
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
    virtual bool CreateASMMiddle(int iPreOpCode, int iOpCode1, int iOpCode2, LPCSTR lpOpData, int iOp3 = -1) = 0;
    virtual bool CreateASMMiddleCore(int iPreOpCode, int iOpCode1, int iOpCode2, LPCSTR lpOpData, LPCSTR lpOpData2, bool bSecondOpDataIsIMM, DWORD dwSecondOpDataIMMSize, int iOp3 = -1) = 0;
    virtual bool CheckAndExpandMCBMemory(void) = 0;
    virtual bool CheckAndExpandREFMemory(void) = 0;

    virtual DWORD GetCurrentMCPosition(void) = 0;

    virtual bool ReportAnyErrorsToCLI(void) = 0;
    virtual bool PrepareEXE(LPSTR pEXEFilename, bool bParsingMainProgram, bool bProceedToUpdate) = 0;
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
    virtual DWORD DetMode(CStr* pP, DWORD dwPType, DWORD dwPOffset) = 0;

    virtual void CalculateArrayOffsetInRBX(CStr* pStr) = 0;
    virtual void WriteASMRAXtoARR(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset) = 0;
    virtual void WriteASMARRtoRAX(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset) = 0;
    virtual void WriteASMXtoRAX(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset) = 0;
    virtual void WriteASMRAXtoX(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset) = 0;

    virtual bool WriteASMCall(DWORD dwLine, LPCSTR pDLL, LPCSTR pDecoratedName) = 0;
    virtual bool WriteASMTaskP1(DWORD dwLine, DWORD dwTask, CResultData* pP1) = 0;
    virtual bool WriteASMTaskP2(DWORD dwLine, DWORD dwTask, CResultData* pP1, CResultData* pP2) = 0;
    virtual bool WriteASMTaskP3(DWORD dwLine, DWORD dwTask, CResultData* pP1, CResultData* pP2, CResultData* pP3) = 0;
    virtual bool WriteASMTaskCoreP1(DWORD dwLine, DWORD dwTask, CStr* pP1, DWORD dwP1Type) = 0;
    virtual bool WriteASMTaskCoreP2(DWORD dwLine, DWORD dwTask, CStr* pP1, DWORD dwP1Type, CStr* pP2, DWORD dwP2Type) = 0;
    virtual bool WriteASMTaskCore(DWORD dwLine, DWORD dwTask, CStr* pP1, CStr* pP1Off, DWORD dwP1Type, DWORD dwP1Offset, CStr* pP2, CStr* pP2Off, DWORD dwP2Type, DWORD dwP2Offset) = 0;
    virtual bool WriteASMTaskCore(DWORD dwLine, DWORD dwTask, CStr* pP1, CStr* pP1Off, DWORD dwP1Type, DWORD dwP1Offset,
                                  CStr* pP2, CStr* pP2Off, DWORD dwP2Type, DWORD dwP2Offset,
                                  CStr* pP3, CStr* pP3Off, DWORD dwP3Type, DWORD dwP3Offset) = 0;

    virtual bool WriteASMLine(DWORD dwOp, LPCSTR pOpData) = 0;
    virtual bool WriteASMLine2(DWORD dwOp, LPCSTR pOpData, LPCSTR pOpData2) = 0;
    virtual bool WriteASMLine1IMM(DWORD dwOp, LPCSTR pOpData, DWORD dwSizeIMM) = 0;
    virtual bool WriteASMLine2IMM(DWORD dwOp, LPCSTR pOpData, LPCSTR pOpData2, DWORD dwSizeIMM) = 0;
    virtual bool WriteASMComment(LPCSTR pTitle, LPCSTR pC1, LPCSTR pC2, LPCSTR pC3) = 0;

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

    virtual DWORD AddCommandToTable(LPCSTR pDLLString, LPCSTR pCommandString) = 0;
};
