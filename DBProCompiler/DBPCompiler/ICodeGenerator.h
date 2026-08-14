#pragma once
#include "windows.h"
#include "Str.h"

#include <cstdint>

class CDeclaration; // Forward declaration
class CResultData;  // Forward declaration

// ---------------------------------------------------------------------------
// x64 opcode descriptor types (see docs/superpowers/specs/2026-08-11-x64-opcode-emission-design.md)
// ---------------------------------------------------------------------------

/**
 * Data slot encoding for an opcode's operand fields.
 *
 * The slot width is a property of the instruction form AND of the resolved
 * reference kind: address-bearing slots are sizeof(void*) wide on x64 while
 * value/immediate slots stay 4 bytes. The emitter and the runtime derive the
 * width from the same parsed label kind, so no on-disk format change is
 * needed for reference patching.
 */
enum class DataEncoding : std::uint8_t
{
    None = 0,    // no operand data
    Imm8,        // 1-byte value slot (immediate / displacement constant)
    Imm16,       // 2-byte value slot
    Imm32,       // 4-byte value slot (also CodeLabel rel32 slots)
    Abs64,       // 8-byte absolute-address slot (moffs A0-A3 only)
    ImmOrAddr,   // MOV r, imm: 4-byte value slot, or 48 B8+rd imm64 for addresses
    PtrIndirect, // absolute [disp32]: expand to MOV RBX, imm64 + [RBX] operand
};

/** Whole-instruction expansions that have no direct x64 encoding. */
enum class OpcodeExpansion : std::uint8_t
{
    None,
    PushAll,     // PUSHAD -> PUSH RAX,RBX,RCX,RDX,RSI,RDI,RBP
    PopAll,      // POPAD  -> POP RBP,RDI,RSI,RDX,RCX,RBX,RAX
    RexW,        // 0x48 operand-size prefix (MOV RBP,RSP / MOV RSP,RBP)
    RexWAfterPrefix, // legacy F2/F3/66 prefix first, then 0x48 REX.W
                    // (64-bit CVT forms: F3 48 0F 2C C0 etc.)
};

/** Structured descriptor replacing the legacy parallel byte arrays. */
struct ASMOpcodeDef
{
    const char*    name = nullptr;   // DBM debug string
    int            preOp = -1;       // legacy prefix (0x66) or -1
    int            op1 = -1;         // first opcode byte or -1
    int            op2 = -1;         // ModRM / second opcode byte or -1
    // Wave 8: explicit ModRM byte for instructions whose opcode spans
    // preOp+op1+op2 (SSE2 memory forms: F2 0F 10 <modrm>). When -1 the
    // legacy behaviour stands (op2 carries the ModRM where applicable).
    int            modrm = -1;       // explicit ModRM byte or -1
    DataEncoding   data1 = DataEncoding::None;
    DataEncoding   data2 = DataEncoding::None;
    OpcodeExpansion expansion = OpcodeExpansion::None;
};

class ICodeGenerator {
public:
    virtual ~ICodeGenerator() = default;

    DWORD m_dwLineNumber = 0;

    virtual void SetDefaultCompileFlags(bool bArraySafetyFlag) = 0;
    virtual void SetArrayCheckFlag(bool bFlag) = 0;
    virtual bool GetArrayCheckFlag(void) = 0;

    virtual void GenerateASMCodes(void) = 0;
    virtual void DefineASM(DWORD dwASMCode, const char* pDebugStr, int iPreOp, int iOp1, int iOp2, DataEncoding data1, DataEncoding data2 = DataEncoding::None, OpcodeExpansion expansion = OpcodeExpansion::None, int iModRm = -1) = 0;

    virtual bool CreateASMHeader(void) = 0;
    // Raw byte emitter (leap markers, tests): fixed Imm32 data slot.
    virtual bool CreateASMMiddle(int iPreOpCode, int iOpCode1, int iOpCode2, LPSTR lpOpData) = 0;
    // x64-aware, descriptor-driven emission.
    virtual bool CreateASMMiddleCore(DWORD dwASMCode, LPSTR lpOpData, LPSTR lpOpData2, bool bSecondOpDataIsIMM, DWORD dwSecondOpDataIMMSize) = 0;
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
};
