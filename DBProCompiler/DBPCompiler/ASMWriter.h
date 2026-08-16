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

// 64-Bit x86-64 Register Enumerations
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

enum class XMMRegister : uint8_t {
	XMM0 = 0,
	XMM1,
	XMM2,
	XMM3,
	XMM4,
	XMM5,
	XMM6,
	XMM7,
	XMM8,
	XMM9,
	XMM10,
	XMM11,
	XMM12,
	XMM13,
	XMM14,
	XMM15
};

// ASM task codes (converted from #define constants)
constexpr int ASMMAXCOUNT = 300;

enum class ASMTask : int {
	Assign              = 1,
	Test                = 4,
	Call                = 5,
	Push                = 6,
	PopRax              = 7,
	PopRbx              = 8,
	Unknown             = 9,
	Condition           = 10,
	CondJumpNE          = 11,
	CondJumpE           = 12,
	CondGreater         = 13,
	CondLess            = 14,
	Jump                = 15,
	JumpSubroutine      = 16,
	Return              = 17,
	AssignToRax         = 18,
	ConditionData       = 19,
	AddRsp              = 20,
	SubRsp              = 21,
	PushRbp             = 22,
	PopRbp              = 23,
	MovRbpRsp            = 24,
	MovRspRbp            = 25,
	StoreRsp            = 26,
	RestoreRsp          = 27,
	PushRegisters       = 28,
	PopRegisters        = 29,
	ClearStack          = 30,
	DebugStatementHook  = 31,
	DebugJumpHook       = 32,
	DebugReturnHook     = 33,
	RuntimeErrorHook    = 34,
	BreakpointResume    = 37,
	JumpMem             = 38,
	PushRsp             = 39,
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

	SetNoReturnIfRspLeak = 501,
	CalcArrayOffset     = 502,
	PushInternalArrayIndex = 503,

	IncVar              = 1001,
	DecVar              = 1002,
};

// ASM operation codes (converted from #define constants)
enum class ASMOp : int {
	MOVRAXMEM1                = 2,
	MOVRAXMEM2                = 3,
	MOVRAXMEM4                = 4,
	MOVMEMRAX1                = 5,
	MOVMEMRAX2                = 6,
	MOVMEMRAX4                = 7,
	MOVRCXOFFRAX1             = 8,
	MOVRCXOFFRAX2             = 9,
	MOVRCXOFFRAX4             = 10,
	RELMOVRAXIMM              = 11,
	RELMOVRAXMEM1             = 12,
	RELMOVRAXMEM2             = 13,
	RELMOVRAXMEM4             = 14,
	RELMOVMEMRAX1             = 15,
	RELMOVMEMRAX2             = 16,
	RELMOVMEMRAX4             = 17,
	RELMOVRAXRDX1             = 18,
	RELMOVRAXRDX2             = 19,
	RELMOVRAXRDX4             = 20,
	MOVRAXIMM1                = 21,
	MOVRAXIMM2                = 22,
	MOVRAXIMM4                = 23,
	MOVRDXIMM4                = 24,
	MOVRDXRAX4                = 26,
	RELMOVRAXRRDX1            = 27,
	RELMOVRAXRRDX2            = 28,
	RELMOVRAXRRDX4            = 29,
	ADDRAX1                   = 31,
	ADDRAX2                   = 32,
	ADDRAX4                   = 33,
	ADDRAXRBX1                = 34,
	ADDRAXRBX2                = 35,
	ADDRAXRBX4                = 36,
	ADDRAXRCX4                = 37,
	MOVRAXRCXOFF1             = 38,
	MOVRAXRCXOFF2             = 39,
	MOVRAXRCXOFF4             = 40,
	MOVMEMXMM0                = 41,
	MOVXMM0MEM8                = 42,
	MOVRCXIMM4                = 43,
	MOVXMM0RCXOFF8             = 44,
	MOVRCXOFFXMM0             = 45,
	MOVMEMIMM1                = 46,
	MOVMEMIMM2                = 47,
	MOVMEMIMM4                = 48,
	PUSHRAX                   = 51,
	PUSHRDX                   = 52,
	PUSHRELRAX1               = 53,
	PUSHRELRAX2               = 54,
	PUSHRELRAX4               = 55,
	POPRAX                    = 57,
	POPRBX                    = 58,
	CALLRAX                   = 59,
	CALLMEM                   = 60,
	RET                       = 61,
	ADDRSP                    = 62,
	SUBRSP                    = 63,
	PUSHRBP                   = 64,
	POPRBP                    = 65,
	MOVRBPRSP                 = 66,
	MOVRSPRBP                 = 67,
	PUSHRBX                   = 68,
	UNKNOWN                   = 71,
	MOVRBPIMM1                = 72,
	MOVRBPIMM2                = 73,
	MOVRBPIMM4                = 74,
	CMPRAX1                   = 77,
	CMPRAX2                   = 78,
	CMPRAX4                   = 79,
	JMP                       = 81,
	JNE                       = 82,
	JE                        = 83,
	MOVRAXSIB                 = 85,
	MOVRCXRAX4                = 86,
	MOVRAXRBP1                = 87,
	MOVRAXRBP2                = 88,
	MOVRAXRBP4                = 89,
	MOVRAXRSP                 = 90,
	MOVRBPRAX1                = 91,
	MOVRBPRAX2                = 92,
	MOVRBPRAX4                = 93,
	MOVRBPXMM0                = 94,
	MOVXMM0RBP8                = 95,
	PUSHRBP4                  = 96,
	MOVMEMRSP4                = 97,
	MOVRSPMEM4                = 98,
	MOVRAXOFFRCX1             = 99,
	MOVRAXOFFRCX2             = 100,
	MOVRAXOFFRCX4             = 101,
	MOVRCXRAXOFF1             = 102,
	MOVRCXRAXOFF2             = 103,
	MOVRCXRAXOFF4             = 104,
	MOVRCXRDX4                = 106,
	MOVRDXRCX4                = 107,
	MOVRAXXMM0                = 108,
	MOVXMM0RAX8                = 109,
	MOVRAXRCX1                = 115,
	MOVRAXRCX2                = 116,
	MOVRAXRCX4                = 117,
	PUSHIMM4                  = 118,
	MOVRBXMEM4                = 119,
	INCMEM1                   = 120,
	INCMEM2                   = 121,
	INCMEM4                   = 122,
	DECMEM1                   = 123,
	DECMEM2                   = 124,
	DECMEM4                   = 125,
	CALLABS                   = 126,
	PUSHRSP                   = 127,
	CALLRBX                   = 128,
	SUBRSPRAX                 = 129,
	JMPREL                    = 130,
	JMPRBX                    = 131,
	MOVRBXRAXOFF1             = 135,
	MOVRBXRAXOFF2             = 136,
	MOVRBXRAXOFF4             = 137,
	MOVRDXRAXOFF1             = 138,
	MOVRDXRAXOFF2             = 139,
	MOVRDXRAXOFF4             = 140,
	CMPRDXRBX              = 141,
	JGE                       = 142,
	JLE                       = 143,
	MOVRAXRCXREL1             = 144,
	MOVRAXRCXREL2             = 145,
	MOVRAXRCXREL4             = 146,
	MOVRAXRAXREL1             = 147,
	MOVRAXRAXREL2             = 148,
	MOVRAXRAXREL4             = 149,
	MOVRBXRAX1                = 150,
	MOVRBXRAX2                = 151,
	MOVRBXRAX4                = 152,
	SUBRAX1                   = 153,
	SUBRAX2                   = 154,
	SUBRAX4                   = 155,
	SUBRAXRBX1                = 156,
	SUBRAXRBX2                = 157,
	SUBRAXRBX4                = 158,
	DIVRAXRBX1                = 159,
	DIVRAXRBX2                = 160,
	DIVRAXRBX4                = 161,
	MULRAXRBX1                = 162,
	MULRAXRBX2                = 163,
	MULRAXRBX4                = 164,
	MOVRBXIMM1                = 165,
	MOVRBXIMM2                = 166,
	MOVRBXIMM4                = 167,
	CQO                       = 168,
	CMPRDXRBX1                = 169,
	CMPRDXRBX2                = 170,
	CMPRDXRBX4                = 171,
	SETE                      = 172,
	SETNE                     = 173,
	SETG                      = 174,
	SETGE                     = 175,
	SETL                      = 176,
	SETLE                     = 177,
	CMPRBX1                   = 178,
	CMPRBX2                   = 179,
	CMPRBX4                   = 180,
	ANDRAX1                   = 181,
	ANDRAX2                   = 182,
	ANDRAX4                   = 183,
	ANDRAXRBX1                = 184,
	ANDRAXRBX2                = 185,
	ANDRAXRBX4                = 186,
	ORRAX1                    = 187,
	ORRAX2                    = 188,
	ORRAX4                    = 189,
	ORRAXRBX1                 = 190,
	ORRAXRBX2                 = 191,
	ORRAXRBX4                 = 192,
	NOTRAX1                   = 193,
	NOTRAX2                   = 194,
	NOTRAX4                   = 195,
	MOVRAXRDX1                = 196,
	MOVRAXRDX2                = 197,
	MOVRAXRDX4                = 198,
	XORRAX1                   = 199,
	XORRAX2                   = 200,
	XORRAX4                   = 201,
	XORRAXRBX1                = 202,
	XORRAXRBX2                = 203,
	XORRAXRBX4                = 204,
	SHRRAX1                   = 205,
	SHRRAX2                   = 206,
	SHRRAX4                   = 207,
	SHLRAXCLC1                = 208,
	SHLRAXCLC2                = 209,
	SHLRAXCLC4                = 210,
	SHLRAX1                   = 211,
	SHLRAX2                   = 212,
	SHLRAX4                   = 213,
	SHRRAXCLC1                = 214,
	SHRRAXCLC2                = 215,
	SHRRAXCLC4                = 216,
	MOVRCXRBX1                = 217,
	MOVRCXRBX2                = 218,
	MOVRCXRBX4                = 219,
	CMPRAXRBX4                = 220,
	MOVRBXRBP1                = 221,
	MOVRBXRBP2                = 222,
	MOVRBXRBP4                = 223,
	POPRDX                    = 224,
	POPRCX                    = 225,
	PUSHRCX                   = 226,
	MULRCXRDX4                = 227,
	ADDRBXRDX4                = 228,
	MULRDXRAXOFF4             = 229,
	MOVMEMRBX4                = 230,
	MOVRAXRBX4                = 231,
	PUSHFROMRAX               = 232,
	MOVRAXRBP                 = 233,

	// Native x64-only register ops (no 32-bit equivalent exists)
	PUSHRDI                   = 240,
	PUSHRSI                   = 241,
	PUSHR12                   = 242,
	PUSHR13                   = 243,
	PUSHR14                   = 244,
	PUSHR15                   = 245,
	POPRDI                    = 246,
	POPRSI                    = 247,
	POPR12                    = 248,
	POPR13                    = 249,
	POPR14                    = 250,
	POPR15                    = 251,
	MOVRDIRSP                 = 252,
	ADDRDIIMM                 = 253,
	REPSTOSQ                  = 254,
	REPSTOSB                  = 255,
};

// Parameter mode codes (converted from #define constants)
enum class ParamMode : int {
	None      = 0,
	Imm       = 1,
	Mem       = 2,
	Rbp       = 3,
	MemOff    = 4,
	RbpOff    = 5,
	MemArr    = 6,
	RbpArr    = 7,
	Stack     = 8,
	MemRel    = 9,
	RbpRel    = 10,
};

class CASMWriter : public ICodeGenerator  
{
	public:
		CASMWriter();
		virtual ~CASMWriter() override;

		void SetDefaultCompileFlags ( bool bArraySafetyFlag );
		void SetArrayCheckFlag(bool bFlag) { m_bArrayCheckFlag = bFlag; }
		bool GetArrayCheckFlag(void) { return m_bArrayCheckFlag; }

		// Native x64 Calling Convention & Register Helpers
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

		// Native x64 Instruction Emission
		void EmitByte(uint8_t b);
		void EmitDword(uint32_t dw);
		void EmitQword(uint64_t qw);

		void EmitMovRegImm64(X64Register reg, uint64_t val);
		void EmitPushReg(X64Register reg);
		void EmitPopReg(X64Register reg);
		void EmitSubRegImm32(X64Register reg, uint32_t val);
		void EmitAddRegImm32(X64Register reg, uint32_t val);
		void EmitRet();

		void EmitCmpRegReg(X64Register reg1, X64Register reg2);
		void EmitTestRegReg(X64Register reg1, X64Register reg2);
		void EmitJmpRel32(int32_t relOffset);
		void EmitJneRel32(int32_t relOffset);
		void EmitJeRel32(int32_t relOffset);
		void EmitCallReg(X64Register reg);
		void EmitNop();
		void EmitMovss(XMMRegister dst, XMMRegister src);
		void EmitAddss(XMMRegister dst, XMMRegister src);
		void EmitMulss(XMMRegister dst, XMMRegister src);

		[[nodiscard]] const std::vector<uint8_t>& GetCodeBuffer() const noexcept { return m_codeBuffer; }
		[[nodiscard]] size_t GetCodeSize() const noexcept { return m_codeBuffer.size(); }
		void ClearCodeBuffer() noexcept { m_codeBuffer.clear(); }

		void GenerateASMCodes(void);
		void DefineASM(DWORD dwASMCode, LPCSTR pDebugStr, int iPreOp, int iOp1, int iOp2, bool bOpData, int iOp3 = -1);

		bool CreateASMHeader(void);
		bool CreateASMMiddle(int iPreOpCode, int iOpCode1, int iOpCode2, LPCSTR lpOpData, int iOp3 = -1);
		bool CreateASMMiddleCore(int iPreOpCode, int iOpCode1, int iOpCode2, LPCSTR lpOpData, LPCSTR lpOpData2, bool bSecondOpDataIsIMM, DWORD dwSecondOpDataIMMSize, int iOp3 = -1);
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

		void CalculateArrayOffsetInRBX ( CStr* pStr );
		void WriteASMRAXtoARR(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset);
		void WriteASMARRtoRAX(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset);
		void WriteASMXtoRAX(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset);
		void WriteASMRAXtoX(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset);

		bool WriteASMCall(DWORD dwLine, LPCSTR pDLL, LPCSTR pDecoratedName);
		bool WriteASMTaskP1(DWORD dwLine, DWORD dwTask, CResultData* pP1);
		bool WriteASMTaskP2(DWORD dwLine, DWORD dwTask, CResultData* pP1, CResultData* pP2);
		bool WriteASMTaskP3(DWORD dwLine, DWORD dwTask, CResultData* pP1, CResultData* pP2, CResultData* pP3);
		bool WriteASMTaskCoreP1(DWORD dwLine, DWORD dwTask, CStr* pP1, DWORD dwP1Type);
		bool WriteASMTaskCoreP2(DWORD dwLine, DWORD dwTask, CStr* pP1, DWORD dwP1Type, CStr* pP2, DWORD dwP2Type);
		bool WriteASMTaskCore(DWORD dwLine, DWORD dwTask, CStr* pP1, CStr* pP1Off, DWORD dwP1Type, DWORD dwP1Offset, CStr* pP2, CStr* pP2Off, DWORD dwP2Type, DWORD dwP2Offset);
		bool WriteASMTaskCore(DWORD dwLine, DWORD dwTask,	CStr* pP1, CStr* pP1Off, DWORD dwP1Type, DWORD dwP1Offset,
															CStr* pP2, CStr* pP2Off, DWORD dwP2Type, DWORD dwP2Offset,
															CStr* pP3, CStr* pP3Off, DWORD dwP3Type, DWORD dwP3Offset );

		bool WriteASMLine(DWORD dwOp, LPCSTR pOpData);
		bool WriteASMLine2(DWORD dwOp, LPCSTR pOpData, LPCSTR pOpData2);
		bool WriteASMLine1IMM(DWORD dwOp, LPCSTR pOpData, DWORD dwSizeIMM);
		bool WriteASMLine2IMM(DWORD dwOp, LPCSTR pOpData, LPCSTR pOpData2, DWORD dwSizeIMM);
		bool WriteASMComment(LPCSTR pTitle, LPCSTR pC1, LPCSTR pC2, LPCSTR pC3);

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

		DWORD AddCommandToTable(LPCSTR pDLLString, LPCSTR pCommandString);

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
		int						m_iASMOp3[ASMMAXCOUNT];
		bool					m_bASMOpData[ASMMAXCOUNT];
		std::vector<uint8_t>	m_codeBuffer;
};

#endif // !defined(AFX_ASMWRITER_H__A3BE66B0_1587_46D3_AC0A_E4F78C0A3561__INCLUDED_)
