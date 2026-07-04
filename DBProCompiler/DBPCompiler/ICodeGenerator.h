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
    virtual void DefineASM(DWORD dwASMCode, LPSTR pDebugStr, int iPreOp, int iOp1, int iOp2, bool bOpData) = 0;

    virtual bool CreateASMHeader(void) = 0;
    virtual bool CreateASMMiddle(int iPreOpCode, int iOpCode1, int iOpCode2, LPSTR lpOpData) = 0;
    virtual bool CreateASMMiddleCore(int iPreOpCode, int iOpCode1, int iOpCode2, LPSTR lpOpData, LPSTR lpOpData2, bool bSecondOpDataIsIMM, DWORD dwSecondOpDataIMMSize) = 0;
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

    virtual void UpdateStructurePatternDataRec(LPSTR pPattern, CDeclaration* pDecMain) = 0;
    virtual bool UpdateStructurePatternData(void) = 0;

    virtual LPSTR MakeVarDataForTransfer(DWORD* dwDataSize) = 0;
    virtual LPSTR MakeVarValuesForTransfer(DWORD* dwDataSize) = 0;

    virtual void TraverseDecForPattern(DWORD dwBaseOffset, short pass, DWORD* dwPatternArrayCounter, DWORD* dwSizeOfUserTypePattern, CDeclaration* pDecMain) = 0;
    virtual void FreeMachineBlock(void) = 0;
    virtual void FreeAll(void) = 0;

    virtual bool HideAnyHiddenCode(LPSTR pData, DWORD dwSize) = 0;
    virtual LRESULT SendDataToDebugger(int iType, LPSTR pData, DWORD dwDataSize) = 0;
    virtual void GetDataFromDebugger(int iType, LPSTR* pData, DWORD* dwDataSize) = 0;

    virtual DWORD GetBytePosOfLastInstruction(void) = 0;

    virtual DWORD DetermineASMCall(DWORD dwASMCodeAsAByte, DWORD dwTypeValue) = 0;
    virtual DWORD DetermineASMCallForREL(DWORD dwASMCodeAsAByte, DWORD dwTypeValue) = 0;
    virtual DWORD DetMode(CStr* pP, DWORD dwPType, DWORD dwPOffset) = 0;

    virtual void CalculateArrayOffsetInEBX(CStr* pStr) = 0;
    virtual void WriteASMEAXtoARR(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset) = 0;
    virtual void WriteASMARRtoEAX(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset) = 0;
    virtual void WriteASMXtoEAX(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset) = 0;
    virtual void WriteASMEAXtoX(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset) = 0;

    virtual bool WriteASMCall(DWORD dwLine, LPSTR pDLL, LPSTR pDecoratedName) = 0;
    virtual bool WriteASMTaskP1(DWORD dwLine, DWORD dwTask, CResultData* pP1) = 0;
    virtual bool WriteASMTaskP2(DWORD dwLine, DWORD dwTask, CResultData* pP1, CResultData* pP2) = 0;
    virtual bool WriteASMTaskP3(DWORD dwLine, DWORD dwTask, CResultData* pP1, CResultData* pP2, CResultData* pP3) = 0;
    virtual bool WriteASMTaskCoreP1(DWORD dwLine, DWORD dwTask, CStr* pP1, DWORD dwP1Type) = 0;
    virtual bool WriteASMTaskCoreP2(DWORD dwLine, DWORD dwTask, CStr* pP1, DWORD dwP1Type, CStr* pP2, DWORD dwP2Type) = 0;
    virtual bool WriteASMTaskCore(DWORD dwLine, DWORD dwTask, CStr* pP1, CStr* pP1Off, DWORD dwP1Type, DWORD dwP1Offset, CStr* pP2, CStr* pP2Off, DWORD dwP2Type, DWORD dwP2Offset) = 0;
    virtual bool WriteASMTaskCore(DWORD dwLine, DWORD dwTask, CStr* pP1, CStr* pP1Off, DWORD dwP1Type, DWORD dwP1Offset,
                                  CStr* pP2, CStr* pP2Off, DWORD dwP2Type, DWORD dwP2Offset,
                                  CStr* pP3, CStr* pP3Off, DWORD dwP3Type, DWORD dwP3Offset) = 0;

    virtual bool WriteASMLine(DWORD dwOp, LPSTR pOpData) = 0;
    virtual bool WriteASMLine2(DWORD dwOp, LPSTR pOpData, LPSTR pOpData2) = 0;
    virtual bool WriteASMLine1IMM(DWORD dwOp, LPSTR pOpData, DWORD dwSizeIMM) = 0;
    virtual bool WriteASMLine2IMM(DWORD dwOp, LPSTR pOpData, LPSTR pOpData2, DWORD dwSizeIMM) = 0;
    virtual bool WriteASMComment(LPSTR pTitle, LPSTR pC1, LPSTR pC2, LPSTR pC3) = 0;

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

    virtual DWORD AddCommandToTable(LPSTR pDLLString, LPSTR pCommandString) = 0;
    virtual bool AddProtectionToSelectedDLLs(LPSTR pDLLString) = 0;

    virtual bool GetCondToggle(void) = 0;
    virtual void SetCondToggle(bool bFlag) = 0;
};
