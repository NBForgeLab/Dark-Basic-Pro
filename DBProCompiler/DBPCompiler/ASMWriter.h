// ASMWriter.h: interface for the CASMWriter class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ASMWRITER_H__A3BE66B0_1587_46D3_AC0A_E4F78C0A3561__INCLUDED_)
#define AFX_ASMWRITER_H__A3BE66B0_1587_46D3_AC0A_E4F78C0A3561__INCLUDED_

// Common Includes
#include "windows.h"
#include "macros.h"
#include <cstddef>
#include <cstdint>
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
constexpr int ASMMAXCOUNT = 303;

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

	// Wave 8: emitter-side int<->float conversions (SSE2 CVT* instructions).
	CastIntToFloat      = 601,
	CastIntToDouble     = 602,
	CastFloatToInt      = 603,
	CastFloatToDouble   = 604,
	CastDoubleToInt     = 605,
	CastDoubleToFloat   = 606,

	// Wave 15: integer-family widening to int64 (REG64, no DLL).
	CastIntToInt64      = 607, // MOVSXD RAX,EAX (sign-extend)
	CastDwordToInt64    = 608, // zero-extend (load width does the work)

	// Wave 16: int64 <-> float/double conversions (SSE2 CVT*, REX.W).
	CastFloatToInt64    = 609, // CVTTSS2SI RAX,XMM0 (F3 48 0F 2C C0)
	CastDoubleToInt64   = 610, // CVTTSD2SI RAX,XMM0 (F2 48 0F 2C C0)
	CastInt64ToLower    = 611, // truncating store at target width (R->L/D/B/Y/W)
	CastInt64ToFloat    = 612, // CVTSI2SS XMM0,RAX (F3 48 0F 2A C0)
	CastInt64ToDouble   = 613, // CVTSI2SD XMM0,RAX (F2 48 0F 2A C0)

	// Wave 18: narrowing casts to byte/word/dword (store-width truncation).
	CastToNarrow        = 614, // L/D -> B/Y/W/D (load + target-width store)
	CastFloatToNarrow   = 615, // F/O -> B/Y/W (CVTT* then target-width store)

	// Wave 19: widening casts from byte/word (MOVZX then width store/CVT*).
	CastWiden           = 616, // B/Y/W -> L/W/D (MOVZX + target-width store)
	CastWidenToFloat    = 617, // B/Y/W -> F/O (MOVZX + CVTSI2SS/CVTSI2SD)

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
	// Wave 8: legacy x87 ST0 forms are now SSE2 XMM0 (MOVSD) in place.
	MOVSDMEMXMM0              = 41, // F2 0F 11 03          movsd [rbx], xmm0
	MOVSDXMM0MEM              = 42, // F2 0F 10 03          movsd xmm0, [rbx]
	MOVECXIMM4                = 43,
	MOVSDXMM0ECXOFF           = 44, // F2 0F 10 81 <disp32>  movsd xmm0, [rcx+disp]
	MOVSDECXOFFXMM0           = 45, // F2 0F 11 81 <disp32>  movsd [rcx+disp], xmm0
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
	MOVSDEBPXMM0              = 94, // F2 0F 11 85 <disp32>  movsd [rbp+disp], xmm0
	MOVSDXMM0EBP              = 95, // F2 0F 10 85 <disp32>  movsd xmm0, [rbp+disp]
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
	MOVSDEAXXMM0              = 108, // F2 0F 11 80 <disp32>  movsd [rax+disp], xmm0
	MOVSDXMM0EAX              = 109, // F2 0F 10 80 <disp32>  movsd xmm0, [rax+disp]
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

	// Wave 5: full-width (REX.W) moves for string pointers. Strings are
	// heap addresses; on x64 every load/store of a string value must move
	// 8 bytes, while the *4 forms stay 32-bit for dword/byte/word types.
	MOVEAXMEM8                = 240, // 48 A1 <moffs64>      mov rax, [moffs]
	MOVMEMEAX8                = 241, // 48 A3 <moffs64>      mov [moffs], rax
	MOVEAXEBP8                = 242, // 48 8B 85 <disp32>    mov rax, [rbp+disp]
	MOVEBPEAX8                = 243, // 48 89 85 <disp32>    mov [rbp+disp], rax
	MOVEAXECXOFF8             = 244, // 48 8B 81 <disp32>    mov rax, [rcx+disp]
	MOVECXOFFEAX8             = 245, // 48 89 81 <disp32>    mov [rcx+disp], rax
	MOVEAXECXREL8             = 246, // 48 8B 08             mov rax, [rcx]
	MOVEAXEAXREL8             = 247, // 48 8B 00             mov rax, [rax]
	MOVECXEAX8                = 248, // 48 8B C8             mov rcx, rax
	// Wave 6: runtime array ABI — 8-byte ref table (SIB ×8) and QWORD
	// string-array element access.
	MOVEAXSIB8                = 249, // 48 8B 04 D8          mov rax, [rax+rbx*8]
	MOVECXEAXOFF8             = 250, // 48 8B 88 <disp32>    mov rcx, [rax+disp]
	MOVEAXECX8                = 251, // 48 8B C1             mov rax, rcx
	MOVEAXOFFECX8             = 252, // 48 89 88 <disp32>    mov [rax+disp], rcx

	// Wave 8: SSE2 float pipeline — MOVSS memory forms (XMM0 accumulator).
	// MOVSD memory forms reuse the (renamed) legacy slots 41/42/44/45/94/95/108/109.
	MOVSSXMM0MEM              = 253, // F3 0F 10 03          movss xmm0, [rbx]
	MOVSSMEMXMM0              = 254, // F3 0F 11 03          movss [rbx], xmm0
	MOVSSXMM0EBP              = 255, // F3 0F 10 85 <disp32>  movss xmm0, [rbp+disp]
	MOVSSEBPXMM0              = 256, // F3 0F 11 85 <disp32>  movss [rbp+disp], xmm0
	MOVSSXMM0EAX              = 257, // F3 0F 10 80 <disp32>  movss xmm0, [rax+disp]
	MOVSSEAXXMM0              = 258, // F3 0F 11 80 <disp32>  movss [rax+disp], xmm0
	MOVSSXMM0ECXOFF           = 259, // F3 0F 10 81 <disp32>  movss xmm0, [rcx+disp]
	MOVSSECXOFFXMM0           = 260, // F3 0F 11 81 <disp32>  movss [rcx+disp], xmm0
	// Reg-reg moves / arithmetic / compares / conversions.
	MOVSDXMM1XMM0             = 261, // F2 0F 10 C8          movsd xmm1, xmm0
	MOVSSXMM1XMM0             = 262, // F3 0F 10 C8          movss xmm1, xmm0
	ADDSDXMM0XMM1             = 263, // F2 0F 58 C1          addsd xmm0, xmm1
	SUBSDXMM0XMM1             = 264, // F2 0F 5C C1          subsd xmm0, xmm1
	MULSDXMM0XMM1             = 265, // F2 0F 59 C1          mulsd xmm0, xmm1
	DIVSDXMM0XMM1             = 266, // F2 0F 5E C1          divsd xmm0, xmm1
	ADDSSXMM0XMM1             = 267, // F3 0F 58 C1          addss xmm0, xmm1
	SUBSSXMM0XMM1             = 268, // F3 0F 5C C1          subss xmm0, xmm1
	MULSSXMM0XMM1             = 269, // F3 0F 59 C1          mulss xmm0, xmm1
	DIVSSXMM0XMM1             = 270, // F3 0F 5E C1          divss xmm0, xmm1
	UCOMISDXMM0XMM1           = 271, // 66 0F 2E C1          ucomisd xmm0, xmm1
	UCOMISSXMM0XMM1           = 272, // 0F 2E C1             ucomiss xmm0, xmm1
	MOVDXMM0EAX               = 273, // 66 0F 6E C0          movd xmm0, eax
	CVTSI2SDXMM0EAX           = 274, // F2 0F 2A C0          cvtsi2sd xmm0, eax
	CVTSI2SSXMM0EAX           = 275, // F3 0F 2A C0          cvtsi2ss xmm0, eax
	CVTTSD2SIEAXXMM0          = 276, // F2 0F 2C C0          cvttsd2si eax, xmm0
	CVTTSS2SIEAXXMM0          = 277, // F3 0F 2C C0          cvttss2si eax, xmm0
	CVTSD2SSXMM0XMM0          = 278, // F2 0F 5A C0          cvtsd2ss xmm0, xmm0
	CVTSS2SDXMM0XMM0          = 279, // F3 0F 5A C0          cvtss2sd xmm0, xmm0
	// Unsigned/ordered setcc for UCOMIS* flags (CF=below, ZF=equal, PF=unordered).
	SETA                      = 280, // 0F 97 C0             seta al
	SETAE                     = 281, // 0F 93 C0             setae al
	SETB                      = 282, // 0F 92 C0             setb al
	SETBE                     = 283, // 0F 96 C0             setbe al
	SETP                      = 284, // 0F 9A C0             setp al
	SETNP                     = 285, // 0F 9B C0             setnp al
	ANDALAH                   = 286, // 20 E0                and al, ah
	ORALAH                    = 287, // 08 E0                or al, ah

	// Wave 8b: int64 (type 9) arithmetic — full-width REG64. The 4-byte
	// ladder ops keep their slots; these are the REX.W forms used when the
	// operand type is 9 (double integer).
	ADDEAXEBX8                = 288, // 48 01 D8             add rax, rbx
	SUBEAXEBX8                = 289, // 48 29 D8             sub rax, rbx
	MULEAXEBX8                = 290, // 48 0F AF D8          imul rax, rbx
	DIVEAXEBX8                = 291, // 48 F7 FB             idiv rbx
	CQO                       = 292, // 48 99                cqo
	MOVEAXEDX8                = 293, // 48 8B C2             mov rax, rdx
	MOVEBXRAX8                = 294, // 48 8B D8             mov rbx, rax
	CMPEDXEBX8                = 295, // 48 3B DA             cmp rdx, rbx
	MOVSXDRAXEAX              = 296, // 48 63 C0             movsxd rax, eax

	// Wave 16: 64-bit CVT forms — legacy prefix, then REX.W.
	CVTTSS2SIRAXXMM0          = 297, // F3 48 0F 2C C0       cvttss2si rax, xmm0
	CVTTSD2SIRAXXMM0          = 298, // F2 48 0F 2C C0       cvttsd2si rax, xmm0
	CVTSI2SSXMM0RAX           = 299, // F3 48 0F 2A C0       cvtsi2ss xmm0, rax
	CVTSI2SDXMM0RAX           = 300, // F2 48 0F 2A C0       cvtsi2sd xmm0, rax

	// Wave 19: register-form zero-extension after a width-sized load.
	// The emitter loads byte/word sources with MOV AL/AX (upper bits of EAX
	// preserved), so widening needs an explicit MOVZX before any wider store.
	MOVZXEAXAL                = 301, // 0F B6 C0             movzx eax, al
	MOVZXEAXAX                = 302, // 0F B7 C0             movzx eax, ax
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

// x64 general-purpose registers (64-bit only). These live on CASMWriter
// itself: the x64 backend is not a separate writer class, it is the same
// emitter converted to emit 64-bit code in place.
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

// x64 SSE scalar registers
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

class CASMWriter : public ICodeGenerator  
{
	public:
		CASMWriter();
		virtual ~CASMWriter() override;

		// --- Microsoft x64 calling-convention helpers ---
		// First four arguments pass in RCX, RDX, R8, R9; 32 bytes of shadow
		// space precede calls; the stack is kept 16-byte aligned.
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

		// --- x64 instruction emission helpers ---
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
		// Wave 17: fully-balanced direct x64 call to a CRT transcendental
		// (msvcrt exp/log) — 32-byte shadow space, RSP 16-aligned at the CALL.
		void EmitAlignedCrtCall(const char* pCommand);
		void EmitTranscendentalCall(const char* pCommand, const char* pTempSlot);
		// Wave 21: two-double-argument CRT call (msvcrt fmod) — XMM0 carries
		// the first argument, XMM1 the second; result returns in XMM0.
		void EmitBinaryTranscendentalCall(const char* pCommand, const char* pTempA, const char* pTempB);
		void EmitNop();
		void EmitMovss(XMMRegister dst, XMMRegister src);
		void EmitAddss(XMMRegister dst, XMMRegister src);
		void EmitMulss(XMMRegister dst, XMMRegister src);

		[[nodiscard]] const std::vector<uint8_t>& GetCodeBuffer() const noexcept { return m_codeBuffer; }
		[[nodiscard]] size_t GetCodeSize() const noexcept { return m_codeBuffer.size(); }
		void ClearCodeBuffer() noexcept { m_codeBuffer.clear(); }

		void SetDefaultCompileFlags ( bool bArraySafetyFlag );
		void SetArrayCheckFlag(bool bFlag) { m_bArrayCheckFlag = bFlag; }
		bool GetArrayCheckFlag(void) { return m_bArrayCheckFlag; }

		void GenerateASMCodes(void);
		void DefineASM(DWORD dwASMCode, const char* pDebugStr, int iPreOp, int iOp1, int iOp2, DataEncoding data1, DataEncoding data2 = DataEncoding::None, OpcodeExpansion expansion = OpcodeExpansion::None, int iModRm = -1);

		bool CreateASMHeader(void);
		bool CreateASMMiddle(int iPreOpCode, int iOpCode1, int iOpCode2, LPSTR lpOpData);
		bool CreateASMMiddleCore(DWORD dwASMCode, LPSTR lpOpData, LPSTR lpOpData2, bool bSecondOpDataIsIMM, DWORD dwSecondOpDataIMMSize);

		// --- x64 data-slot emission helpers ---
		void EmitDataSlot(DataEncoding encoding, LPSTR pData);
		void EmitMovRegImmSlot(int op1, LPSTR pData);
		void EmitAddressSlotIntoRbx(LPSTR pData);
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

	public:
		/** Returns the descriptor for an opcode code; undefined entries have an empty name. */
		[[nodiscard]] const ASMOpcodeDef& GetASMOpcodeDef(DWORD dwASMCode) const noexcept;
		/** Upper bound of the descriptor table (enum gaps are undefined). */
		[[nodiscard]] static constexpr DWORD GetASMOpcodeCount() noexcept { return ASMMAXCOUNT; }

		// --- Microsoft x64 calling-convention support ---

		/** Seeds the stack/alignment tracker for a fresh machine block (RSP%16==8
		 *  at entry via a C call; the prologue's 7 pushes land the body at 0). */
		void ResetStackTracking() noexcept;
		/** Returns the tracked RSP mod 16 at the current emission point. */
		[[nodiscard]] int GetTrackedRSPMod16() const noexcept { return m_iRSPMod16; }
		/** Returns whether RSP alignment is statically known here. */
		[[nodiscard]] bool IsRSPAlignmentKnown() const noexcept { return m_bRSPAlignmentKnown; }

	private:

		std::vector<ASMOpcodeDef>	m_asmoOpcodeDefs;

		// Standalone x64 machine-code stream (REX-prefixed emission helpers)
		std::vector<uint8_t>	m_codeBuffer;

		// --- Microsoft x64 calling-convention state ---

		// Stack/alignment tracking (RSP mod 16, frame depth below body entry).
		int						m_iRSPMod16 = 8;
		int						m_iFrameDepth = 0;
		bool					m_bRSPAlignmentKnown = true;

		// Pending call frame: one type entry per pushed 8-byte slot.
		// ASMTask::Push/PushAddress append; opcode-level POPs remove; the
		// ASMTask::Call handler consumes the frame and records the caller's
		// cleanup-pop count so the trailing pops are suppressed.
		std::vector<DWORD>		m_pendingArgTypes;
		DWORD					m_iPendingCleanupPops = 0;
		bool					m_bPendingFramePoisoned = false;

		void ApplyStackDelta(int iDeltaBytes, bool bPoisonAlignment) noexcept;
		void TrackStackForOpcode(DWORD dwASMCode, LPSTR lpOpData) noexcept;
		void RecordPendingArg(DWORD dwTypeValue) noexcept;
		void PopPendingArgSlot() noexcept;
		void ResetPendingFrame() noexcept;

		[[nodiscard]] static bool IsDoubleSlotType(DWORD dwType) noexcept;
		[[nodiscard]] static bool IsFloatClassType(DWORD dwType) noexcept;

		// Raw x64 call-frame emission helpers (write into the MCB).
		void EmitRspModRmByte(uint8_t reg, int iDisp);
		void EmitRawByte(uint8_t b);
		void EmitSubRspImm(int iBytes);
		void EmitAddRspImm(int iBytes);
		void EmitMovRegFromRspOffset(int iRegIndex, bool bRexR, int iDisp);
		void EmitMovRcxFromRspOffset(int iDisp);
		void EmitMovRaxFromRspOffset(int iDisp);
		void EmitMovToRspOffsetFromRax(int iDisp);
		void EmitMovEaxFromRspOffset(int iDisp);
		void EmitMovToRspOffsetFromEax(int iDisp);
		void EmitMovssXmmFromRspOffset(int iXmmIndex, int iDisp);
		void EmitMovqXmmFromRspOffset(int iXmmIndex, int iDisp);
		void EmitMovqXmmFromRax(int iXmmIndex);
		void EmitShlRcxImm32();
		void EmitOrRaxRcx();
		void EmitMovRcxFromRax();
		void EmitX64CallFrame(DWORD dwCommandIndex);
};

#endif // !defined(AFX_ASMWRITER_H__A3BE66B0_1587_46D3_AC0A_E4F78C0A3561__INCLUDED_)
