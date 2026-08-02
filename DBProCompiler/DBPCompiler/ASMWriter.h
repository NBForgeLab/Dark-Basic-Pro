// ASMWriter.h: interface for the CASMWriter class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ASMWRITER_H__A3BE66B0_1587_46D3_AC0A_E4F78C0A3561__INCLUDED_)
#define AFX_ASMWRITER_H__A3BE66B0_1587_46D3_AC0A_E4F78C0A3561__INCLUDED_

// Common Includes
#include "windows.h"
#include "macros.h"
#include <vector>
#include <string>

// Custom Includes
#include "Statement.h"
#include "EXEBlock.h"
#include "Str.h"
#include "ICodeGenerator.h"
#include "DebuggerInterface.h"
#include "LeapMarkerManager.h"
#include "MachineCodeBuffer.h"
#include "ReferenceTracker.h"
#include "TaskEmitter.h"
#include "PEBuilder.h"

// ASM task codes (converted from #define constants)
constexpr int ASMMAXCOUNT = 300;

enum class ASMTask : int {
	Assign              = 1,
	Test                = 4,
	Call                = 5,
	Push                = 6,
	PopEax              = 7,
	PopEbx              = 8,
	Unknown             = 9,
	Condition           = 10,
	CondJumpNE          = 11,
	CondJumpE           = 12,
	CondGreater         = 13,
	CondLess            = 14,
	Jump                = 15,
	JumpSubroutine      = 16,
	Return              = 17,
	AssignToEax         = 18,
	ConditionData       = 19,
	AddEsp              = 20,
	SubEsp              = 21,
	PushEbp             = 22,
	PopEbp              = 23,
	MovBpEsp            = 24,
	MovSpEbp            = 25,
	StoreEsp            = 26,
	RestoreEsp          = 27,
	PushRegisters       = 28,
	PopRegisters        = 29,
	ClearStack          = 30,
	DebugStatementHook  = 31,
	DebugJumpHook       = 32,
	DebugReturnHook     = 33,
	RuntimeErrorHook    = 34,
	BreakpointResume    = 37,
	JumpMem             = 38,
	PushEsp             = 39,
	PureReturn          = 40,
	PushAddress         = 41,
	PushUdt             = 42,

	Power               = 101,
	Mul                 = 102,
	Div                 = 103,
	Add                 = 104,
	Sub                 = 105,
	Mod                 = 106,

	Equal               = 111,
	Greater             = 112,
	Less                = 113,
	NotEqual            = 114,
	GreaterEqual        = 115,
	LessEqual           = 116,

	Shl                 = 131,
	Shr                 = 132,
	And                 = 133,
	Or                  = 134,
	Not                 = 135,
	Xor                 = 136,
	BitNot              = 137,

	SetNoReturnIfEspLeak = 501,
	CalcArrayOffset     = 502,
	PushInternalArrayIndex = 503,

	IncVar              = 1001,
	DecVar              = 1002,
};

// ASM operation codes (converted from #define constants)
enum class ASMOp : int {
	MOVEAXMEM1                = 2,
	MOVEAXMEM2                = 3,
	MOVEAXMEM4                = 4,
	MOVMEMEAX1                = 5,
	MOVMEMEAX2                = 6,
	MOVMEMEAX4                = 7,
	MOVECXOFFEAX1             = 8,
	MOVECXOFFEAX2             = 9,
	MOVECXOFFEAX4             = 10,
	RELMOVEAXIMM              = 11,
	RELMOVEAXMEM1             = 12,
	RELMOVEAXMEM2             = 13,
	RELMOVEAXMEM4             = 14,
	RELMOVMEMEAX1             = 15,
	RELMOVMEMEAX2             = 16,
	RELMOVMEMEAX4             = 17,
	RELMOVEAXEDX1             = 18,
	RELMOVEAXEDX2             = 19,
	RELMOVEAXEDX4             = 20,
	MOVEAXIMM1                = 21,
	MOVEAXIMM2                = 22,
	MOVEAXIMM4                = 23,
	MOVEDXIMM4                = 24,
	MOVEDXEAX4                = 26,
	RELMOVEAXREDX1            = 27,
	RELMOVEAXREDX2            = 28,
	RELMOVEAXREDX4            = 29,
	ADDEAX1                   = 31,
	ADDEAX2                   = 32,
	ADDEAX4                   = 33,
	ADDEAXEBX1                = 34,
	ADDEAXEBX2                = 35,
	ADDEAXEBX4                = 36,
	ADDEAXECX4                = 37,
	MOVEAXECXOFF1             = 38,
	MOVEAXECXOFF2             = 39,
	MOVEAXECXOFF4             = 40,
	MOVMEMST08                = 41,
	MOVST0MEM8                = 42,
	MOVECXIMM4                = 43,
	MOVST0ECXOFF8             = 44,
	MOVECXOFFST08             = 45,
	MOVMEMIMM1                = 46,
	MOVMEMIMM2                = 47,
	MOVMEMIMM4                = 48,
	PUSHEAX                   = 51,
	PUSHEDX                   = 52,
	PUSHRELEAX1               = 53,
	PUSHRELEAX2               = 54,
	PUSHRELEAX4               = 55,
	POPEAX                    = 57,
	POPEBX                    = 58,
	CALLEAX                   = 59,
	CALLMEM                   = 60,
	RET                       = 61,
	ADDESP                    = 62,
	SUBESP                    = 63,
	PUSHEBP                   = 64,
	POPEBP                    = 65,
	MOVEBPESP                 = 66,
	MOVESPEBP                 = 67,
	PUSHEBX                   = 68,
	UNKNOWN                   = 71,
	MOVEBPIMM1                = 72,
	MOVEBPIMM2                = 73,
	MOVEBPIMM4                = 74,
	CMPEAX1                   = 77,
	CMPEAX2                   = 78,
	CMPEAX4                   = 79,
	JMP                       = 81,
	JNE                       = 82,
	JE                        = 83,
	MOVEAXSIB4                = 85,
	MOVECXEAX4                = 86,
	MOVEAXEBP1                = 87,
	MOVEAXEBP2                = 88,
	MOVEAXEBP4                = 89,
	MOVEAXESP                 = 90,
	MOVEBPEAX1                = 91,
	MOVEBPEAX2                = 92,
	MOVEBPEAX4                = 93,
	MOVEBPST08                = 94,
	MOVST0EBP8                = 95,
	PUSHEBP4                  = 96,
	MOVMEMESP4                = 97,
	MOVESPMEM4                = 98,
	MOVEAXOFFECX1             = 99,
	MOVEAXOFFECX2             = 100,
	MOVEAXOFFECX4             = 101,
	MOVECXEAXOFF1             = 102,
	MOVECXEAXOFF2             = 103,
	MOVECXEAXOFF4             = 104,
	MOVECXEDX4                = 106,
	MOVEDXECX4                = 107,
	MOVEAXST08                = 108,
	MOVST0EAX8                = 109,
	PUSHAD                    = 110,
	POPAD                     = 111,
	LOOP                      = 112,
	MOVSIB4IMM4               = 113,
	MOVSIB4IMM1               = 114,
	MOVEAXECX1                = 115,
	MOVEAXECX2                = 116,
	MOVEAXECX4                = 117,
	PUSHIMM4                  = 118,
	MOVEBXMEM4                = 119,
	INCMEM1                   = 120,
	INCMEM2                   = 121,
	INCMEM4                   = 122,
	DECMEM1                   = 123,
	DECMEM2                   = 124,
	DECMEM4                   = 125,
	CALLABS                   = 126,
	PUSHESP                   = 127,
	CALLEBX                   = 128,
	SUBESPEAX                 = 129,
	JMPREL                    = 130,
	JMPEBX                    = 131,
	MOVEBXEAXOFF1             = 135,
	MOVEBXEAXOFF2             = 136,
	MOVEBXEAXOFF4             = 137,
	MOVEDXEAXOFF1             = 138,
	MOVEDXEAXOFF2             = 139,
	MOVEDXEAXOFF4             = 140,
	CMPGREEDXEBX              = 141,
	JGE                       = 142,
	JLE                       = 143,
	MOVEAXECXREL1             = 144,
	MOVEAXECXREL2             = 145,
	MOVEAXECXREL4             = 146,
	MOVEAXEAXREL1             = 147,
	MOVEAXEAXREL2             = 148,
	MOVEAXEAXREL4             = 149,
	MOVEBXEAX1                = 150,
	MOVEBXEAX2                = 151,
	MOVEBXEAX4                = 152,
	SUBEAX1                   = 153,
	SUBEAX2                   = 154,
	SUBEAX4                   = 155,
	SUBEAXEBX1                = 156,
	SUBEAXEBX2                = 157,
	SUBEAXEBX4                = 158,
	DIVEAXEBX1                = 159,
	DIVEAXEBX2                = 160,
	DIVEAXEBX4                = 161,
	MULEAXEBX1                = 162,
	MULEAXEBX2                = 163,
	MULEAXEBX4                = 164,
	MOVEBXIMM1                = 165,
	MOVEBXIMM2                = 166,
	MOVEBXIMM4                = 167,
	CDQ                       = 168,
	CMPEDXEBX1                = 169,
	CMPEDXEBX2                = 170,
	CMPEDXEBX4                = 171,
	SETE                      = 172,
	SETNE                     = 173,
	SETG                      = 174,
	SETGE                     = 175,
	SETL                      = 176,
	SETLE                     = 177,
	CMPEBX1                   = 178,
	CMPEBX2                   = 179,
	CMPEBX4                   = 180,
	ANDEAX1                   = 181,
	ANDEAX2                   = 182,
	ANDEAX4                   = 183,
	ANDEAXEBX1                = 184,
	ANDEAXEBX2                = 185,
	ANDEAXEBX4                = 186,
	OREAX1                    = 187,
	OREAX2                    = 188,
	OREAX4                    = 189,
	OREAXEBX1                 = 190,
	OREAXEBX2                 = 191,
	OREAXEBX4                 = 192,
	NOTEAX1                   = 193,
	NOTEAX2                   = 194,
	NOTEAX4                   = 195,
	MOVEAXEDX1                = 196,
	MOVEAXEDX2                = 197,
	MOVEAXEDX4                = 198,
	XOREAX1                   = 199,
	XOREAX2                   = 200,
	XOREAX4                   = 201,
	XOREAXEBX1                = 202,
	XOREAXEBX2                = 203,
	XOREAXEBX4                = 204,
	SHREAX1                   = 205,
	SHREAX2                   = 206,
	SHREAX4                   = 207,
	SHLEAXCLC1                = 208,
	SHLEAXCLC2                = 209,
	SHLEAXCLC4                = 210,
	SHLEAX1                   = 211,
	SHLEAX2                   = 212,
	SHLEAX4                   = 213,
	SHREAXCLC1                = 214,
	SHREAXCLC2                = 215,
	SHREAXCLC4                = 216,
	MOVECXEBX1                = 217,
	MOVECXEBX2                = 218,
	MOVECXEBX4                = 219,
	CMPEAXEBX4                = 220,
	MOVEBXEBP1                = 221,
	MOVEBXEBP2                = 222,
	MOVEBXEBP4                = 223,
	POPEDX                    = 224,
	POPECX                    = 225,
	PUSHECX                   = 226,
	MULECXEDX4                = 227,
	ADDEBXEDX4                = 228,
	MULEDXEAXOFF4             = 229,
	MOVMEMEBX4                = 230,
	MOVEAXEBX4                = 231,
	PUSHFROMEAX               = 232,
	MOVEAXEBP                 = 233,
};

// Parameter mode codes (converted from #define constants)
enum class ParamMode : int {
	None      = 0,
	Imm       = 1,
	Mem       = 2,
	Ebp       = 3,
	MemOff    = 4,
	EbpOff    = 5,
	MemArr    = 6,
	EbpArr    = 7,
	Stack     = 8,
	MemRel    = 9,
	EbpRel    = 10,
};

class CASMWriter : public ICodeGenerator  
{
	public:
		CASMWriter();
		virtual ~CASMWriter() override;

		void SetDefaultCompileFlags ( bool bArraySafetyFlag );
		void SetArrayCheckFlag(bool bFlag) { m_bArrayCheckFlag = bFlag; }
		bool GetArrayCheckFlag(void) { return m_bArrayCheckFlag; }

		void GenerateASMCodes(void);
		void DefineASM(DWORD dwASMCode, LPSTR pDebugStr, int iPreOp, int iOp1, int iOp2, bool bOpData);

		bool CreateASMHeader(void);
		bool CreateASMMiddle(int iPreOpCode, int iOpCode1, int iOpCode2, LPSTR lpOpData);
		bool CreateASMMiddleCore(int iPreOpCode, int iOpCode1, int iOpCode2, LPSTR lpOpData, LPSTR lpOpData2, bool bSecondOpDataIsIMM, DWORD dwSecondOpDataIMMSize);
		bool CheckAndExpandMCBMemory(void);
		bool CheckAndExpandREFMemory(void);

		DWORD GetCurrentMCPosition(void);

		bool ReportAnyErrorsToCLI(void);
		bool PrepareEXE(LPSTR pEXEFilename, bool bParsingMainProgram, bool bProceedToUpdate);
		bool UpdateMCB(DWORD dwProgramSize);
		bool UpdateMCBRefData(void);
		bool UpdateDLLData(void);
		bool UpdateCommandData(void);
		bool UpdateStringData(void);
		bool UpdateDataData(void);
		bool UpdateDynamicData(void);

		void UpdateStructurePatternDataRec ( LPSTR pPattern, CDeclaration* pDecMain );
		bool UpdateStructurePatternData ( void );

		LPSTR MakeVarDataForTransfer(DWORD* dwDataSize);
		LPSTR MakeVarValuesForTransfer(DWORD* dwDataSize);

		void TraverseDecForPattern(DWORD dwBaseOffset, short pass, DWORD* dwPatternArrayCounter, DWORD* dwSizeOfUserTypePattern, CDeclaration* pDecMain);
		void FreeMachineBlock(void) noexcept;
		void FreeAll(void);

		DWORD GetBytePosOfLastInstruction(void);

		DWORD DetermineASMCall(DWORD dwASMCodeAsAByte, DWORD dwTypeValue);
		DWORD DetermineASMCallForREL(DWORD dwASMCodeAsAByte, DWORD dwTypeValue);
		DWORD DetMode(CStr* pP, DWORD dwPType, DWORD dwPOffset);

		void CalculateArrayOffsetInEBX ( CStr* pStr );
		void WriteASMEAXtoARR(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset);
		void WriteASMARRtoEAX(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset);
		void WriteASMXtoEAX(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset);
		void WriteASMEAXtoX(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset);

		bool WriteASMCall(DWORD dwLine, LPSTR pDLL, LPSTR pDecoratedName);
		bool WriteASMTaskP1(DWORD dwLine, DWORD dwTask, CResultData* pP1);
		bool WriteASMTaskP2(DWORD dwLine, DWORD dwTask, CResultData* pP1, CResultData* pP2);
		bool WriteASMTaskP3(DWORD dwLine, DWORD dwTask, CResultData* pP1, CResultData* pP2, CResultData* pP3);
		bool WriteASMTaskCoreP1(DWORD dwLine, DWORD dwTask, CStr* pP1, DWORD dwP1Type);
		bool WriteASMTaskCoreP2(DWORD dwLine, DWORD dwTask, CStr* pP1, DWORD dwP1Type, CStr* pP2, DWORD dwP2Type);
		bool WriteASMTaskCore(DWORD dwLine, DWORD dwTask, CStr* pP1, CStr* pP1Off, DWORD dwP1Type, DWORD dwP1Offset, CStr* pP2, CStr* pP2Off, DWORD dwP2Type, DWORD dwP2Offset);
		bool WriteASMTaskCore(DWORD dwLine, DWORD dwTask,	CStr* pP1, CStr* pP1Off, DWORD dwP1Type, DWORD dwP1Offset,
															CStr* pP2, CStr* pP2Off, DWORD dwP2Type, DWORD dwP2Offset,
															CStr* pP3, CStr* pP3Off, DWORD dwP3Type, DWORD dwP3Offset );

		bool WriteASMLine(DWORD dwOp, LPSTR pOpData);
		bool WriteASMLine2(DWORD dwOp, LPSTR pOpData, LPSTR pOpData2);
		bool WriteASMLine1IMM(DWORD dwOp, LPSTR pOpData, DWORD dwSizeIMM);
		bool WriteASMLine2IMM(DWORD dwOp, LPSTR pOpData, LPSTR pOpData2, DWORD dwSizeIMM);
		bool WriteASMComment(LPSTR pTitle, LPSTR pC1, LPSTR pC2, LPSTR pC3);

		bool WriteASMLeapMarkerTop(void);
		bool WriteASMLineLeapToTop(DWORD dwOp);
		bool WriteASMLeapMarkerJumpToTop(void);

		bool WriteASMLeapForwardMarker(void);
		bool WriteASMLineLeap(DWORD dwOp, DWORD di);
		bool WriteASMLeapMarkerJump(DWORD dwOp, DWORD di);
		bool WriteASMLeapMarkerJumpNotEqual(DWORD di);
		bool WriteASMLeapMarkerEnd(DWORD di);
		bool WriteASMCheckBreakPointVar(void);
		bool WriteASMForceEscapeAtCodeBREAK(void);
		void SetBreakPointValue(void);

		DWORD AddCommandToTable(LPSTR pDLLString, LPSTR pCommandString);
		bool AddProtectionToSelectedDLLs(LPSTR pDLLString);

	friend class CLeapMarkerManager;

	private:

		// Program Execution Settings
		int						m_iInitialDisplayMode;
//		bool					m_bIsInternalDebugger;
//		PROCESS_INFORMATION		g_InternalDebuggerProcessInfo;
		
		// Compile Flag States
		bool					m_bArrayCheckFlag;

		// Machine Code Buffer (extracted subsystem)
		CMachineCodeBuffer		m_machineCodeBuffer;

		// Reference Tracking (extracted subsystem)
		CReferenceTracker		m_referenceTracker;

		// Task Emitter (extracted subsystem)
		CTaskEmitter			m_taskEmitter;

		// PE Builder (extracted subsystem)
		CPEBuilder				m_peBuilder;

	public:
		[[nodiscard]] CReferenceTracker& GetReferenceTracker() noexcept { return m_referenceTracker; }
		[[nodiscard]] const CReferenceTracker& GetReferenceTracker() const noexcept { return m_referenceTracker; }
		[[nodiscard]] const CMachineCodeBuffer& GetMachineCodeBuffer() const noexcept { return m_machineCodeBuffer; }
		[[nodiscard]] CTaskEmitter& GetTaskEmitter() noexcept { return m_taskEmitter; }
		[[nodiscard]] const CTaskEmitter& GetTaskEmitter() const noexcept { return m_taskEmitter; }
		[[nodiscard]] CPEBuilder& GetPEBuilder() noexcept { return m_peBuilder; }
		[[nodiscard]] const CPEBuilder& GetPEBuilder() const noexcept { return m_peBuilder; }

		// Work Variables
	public:

	private:
		CMathOp*				m_pP1MathOp;
		CMathOp*				m_pP2MathOp;
		CMathOp*				m_pP3MathOp;

		// Leap Marker Manager (extracted subsystem)
		CLeapMarkerManager		m_leapManager;

	private:

		std::string				m_ASMDebugStrings[ASMMAXCOUNT];
		int						m_iASMPreOp[ASMMAXCOUNT];
		int						m_iASMOp1[ASMMAXCOUNT];
		int						m_iASMOp2[ASMMAXCOUNT];
		bool					m_bASMOpData[ASMMAXCOUNT];
};

#endif // !defined(AFX_ASMWRITER_H__A3BE66B0_1587_46D3_AC0A_E4F78C0A3561__INCLUDED_)
