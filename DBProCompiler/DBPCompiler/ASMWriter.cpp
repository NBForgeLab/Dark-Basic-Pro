// ASMWriter.cpp: implementation of the CASMWriter class.
//
//////////////////////////////////////////////////////////////////////

// Includes
#include "ParserHeader.h"
#include "StringUtils.h"
#include "FileBuilder.h"
#include "ASMWriter.h"
#include "DataTable.h"
#include "LabelTable.h"
#include "VarTable.h"
#include "StructTable.h"
#include "DebugInfo.h"
#include "Errors.h"
#include "DBPCompiler.h"
#include "DBPLogger.h"
#include "TextConvert.h"
#include "DebuggerInterface.h"
#include "TargetABI.h"
#include "ASMWriterPreparationServices.h"
#include "ExecutablePreparationPipeline.h"
#include "ScopeExit.h"

#include <DB3Time.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>

// External Class Pointer
extern CEXEBlock* g_pEXE;
extern CDBPCompiler* g_pDBPCompiler;
extern CDataTable* g_pDataTable;
extern CDataTable* g_pStringTable;
extern CDataTable* g_pDLLTable;
extern CDataTable* g_pCommandTable;
extern CLabelTable* g_pLabelTable;
extern CVarTable* g_pVarTable;
extern CStructTable* g_pStructTable;
extern CDebugInfo g_DebugInfo;
extern CError* g_pErrorReport;
extern ICodeGenerator* g_pASMWriter;

// External Global Vars
extern DWORD_PTR g_dwEscapeValueMem;
extern DWORD_PTR g_dwBreakOutPosition;
extern LPSTR g_pVarSpaceAddressInUse;
extern DWORD g_dwVarSpaceSizeInUse;
extern GDI_RetVoidParamVoidPFN g_CORE_SyncRefresh;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

namespace {
constexpr uint8_t GetRegIndex(X64Register reg) noexcept {
    switch (reg) {
        case X64Register::RAX: return 0;
        case X64Register::RCX: return 1;
        case X64Register::RDX: return 2;
        case X64Register::RBX: return 3;
        case X64Register::RSP: return 4;
        case X64Register::RBP: return 5;
        case X64Register::RSI: return 6;
        case X64Register::RDI: return 7;
        case X64Register::R8:  return 8;
        case X64Register::R9:  return 9;
        case X64Register::R10: return 10;
        case X64Register::R11: return 11;
        case X64Register::R12: return 12;
        case X64Register::R13: return 13;
        case X64Register::R14: return 14;
        case X64Register::R15: return 15;
        default: return 0;
    }
}

constexpr uint8_t GetXmmIndex(XMMRegister reg) noexcept {
    return static_cast<uint8_t>(reg);
}
}

void CASMWriter::EmitByte(uint8_t b) {
    m_codeBuffer.push_back(b);
}

void CASMWriter::EmitDword(uint32_t dw) {
    m_codeBuffer.push_back(static_cast<uint8_t>(dw & 0xFF));
    m_codeBuffer.push_back(static_cast<uint8_t>((dw >> 8) & 0xFF));
    m_codeBuffer.push_back(static_cast<uint8_t>((dw >> 16) & 0xFF));
    m_codeBuffer.push_back(static_cast<uint8_t>((dw >> 24) & 0xFF));
}

void CASMWriter::EmitQword(uint64_t qw) {
    EmitDword(static_cast<uint32_t>(qw & 0xFFFFFFFFULL));
    EmitDword(static_cast<uint32_t>((qw >> 32) & 0xFFFFFFFFULL));
}

void CASMWriter::EmitMovRegImm64(X64Register reg, uint64_t val) {
    const uint8_t idx = GetRegIndex(reg);
    uint8_t rex = 0x48; // REX.W
    if (idx >= 8) rex |= 0x01; // REX.B
    EmitByte(rex);
    EmitByte(static_cast<uint8_t>(0xB8 + (idx & 7)));
    EmitQword(val);
}

void CASMWriter::EmitPushReg(X64Register reg) {
    const uint8_t idx = GetRegIndex(reg);
    if (idx >= 8) EmitByte(0x41); // REX.B
    EmitByte(static_cast<uint8_t>(0x50 + (idx & 7)));
}

void CASMWriter::EmitPopReg(X64Register reg) {
    const uint8_t idx = GetRegIndex(reg);
    if (idx >= 8) EmitByte(0x41); // REX.B
    EmitByte(static_cast<uint8_t>(0x58 + (idx & 7)));
}

void CASMWriter::EmitSubRegImm32(X64Register reg, uint32_t val) {
    const uint8_t idx = GetRegIndex(reg);
    uint8_t rex = 0x48; // REX.W
    if (idx >= 8) rex |= 0x01; // REX.B
    EmitByte(rex);
    EmitByte(0x81); // SUB r/m64, imm32
    EmitByte(static_cast<uint8_t>(0xE8 | (idx & 7)));
    EmitDword(val);
}

void CASMWriter::EmitAddRegImm32(X64Register reg, uint32_t val) {
    const uint8_t idx = GetRegIndex(reg);
    uint8_t rex = 0x48; // REX.W
    if (idx >= 8) rex |= 0x01; // REX.B
    EmitByte(rex);
    EmitByte(0x81); // ADD r/m64, imm32
    EmitByte(static_cast<uint8_t>(0xC0 | (idx & 7)));
    EmitDword(val);
}

void CASMWriter::EmitRet() {
    EmitByte(0xC3);
}

void CASMWriter::EmitCmpRegReg(X64Register reg1, X64Register reg2) {
    const uint8_t idx1 = GetRegIndex(reg1);
    const uint8_t idx2 = GetRegIndex(reg2);
    uint8_t rex = 0x48; // REX.W
    if (idx2 >= 8) rex |= 0x04; // REX.R
    if (idx1 >= 8) rex |= 0x01; // REX.B
    EmitByte(rex);
    EmitByte(0x39); // CMP r/m64, r64
    EmitByte(static_cast<uint8_t>(0xC0 | ((idx2 & 7) << 3) | (idx1 & 7)));
}

void CASMWriter::EmitTestRegReg(X64Register reg1, X64Register reg2) {
    const uint8_t idx1 = GetRegIndex(reg1);
    const uint8_t idx2 = GetRegIndex(reg2);
    uint8_t rex = 0x48; // REX.W
    if (idx2 >= 8) rex |= 0x04; // REX.R
    if (idx1 >= 8) rex |= 0x01; // REX.B
    EmitByte(rex);
    EmitByte(0x85); // TEST r/m64, r64
    EmitByte(static_cast<uint8_t>(0xC0 | ((idx2 & 7) << 3) | (idx1 & 7)));
}

void CASMWriter::EmitJmpRel32(int32_t relOffset) {
    EmitByte(0xE9);
    EmitDword(static_cast<uint32_t>(relOffset));
}

void CASMWriter::EmitJneRel32(int32_t relOffset) {
    EmitByte(0x0F);
    EmitByte(0x85);
    EmitDword(static_cast<uint32_t>(relOffset));
}

void CASMWriter::EmitJeRel32(int32_t relOffset) {
    EmitByte(0x0F);
    EmitByte(0x84);
    EmitDword(static_cast<uint32_t>(relOffset));
}

void CASMWriter::EmitCallReg(X64Register reg) {
    const uint8_t idx = GetRegIndex(reg);
    if (idx >= 8) EmitByte(0x41); // REX.B
    EmitByte(0xFF);
    EmitByte(static_cast<uint8_t>(0xD0 | (idx & 7)));
}

void CASMWriter::EmitNop() {
    EmitByte(0x90);
}

void CASMWriter::EmitMovss(XMMRegister dst, XMMRegister src) {
    const uint8_t dstIdx = GetXmmIndex(dst);
    const uint8_t srcIdx = GetXmmIndex(src);
    EmitByte(0xF3);
    uint8_t rex = 0x40;
    if (dstIdx >= 8) rex |= 0x04; // REX.R
    if (srcIdx >= 8) rex |= 0x01; // REX.B
    if (rex != 0x40) EmitByte(rex);
    EmitByte(0x0F);
    EmitByte(0x10);
    EmitByte(static_cast<uint8_t>(0xC0 | ((dstIdx & 7) << 3) | (srcIdx & 7)));
}

void CASMWriter::EmitAddss(XMMRegister dst, XMMRegister src) {
    const uint8_t dstIdx = GetXmmIndex(dst);
    const uint8_t srcIdx = GetXmmIndex(src);
    EmitByte(0xF3);
    uint8_t rex = 0x40;
    if (dstIdx >= 8) rex |= 0x04; // REX.R
    if (srcIdx >= 8) rex |= 0x01; // REX.B
    if (rex != 0x40) EmitByte(rex);
    EmitByte(0x0F);
    EmitByte(0x58);
    EmitByte(static_cast<uint8_t>(0xC0 | ((dstIdx & 7) << 3) | (srcIdx & 7)));
}

void CASMWriter::EmitMulss(XMMRegister dst, XMMRegister src) {
    const uint8_t dstIdx = GetXmmIndex(dst);
    const uint8_t srcIdx = GetXmmIndex(src);
    EmitByte(0xF3);
    uint8_t rex = 0x40;
    if (dstIdx >= 8) rex |= 0x04; // REX.R
    if (srcIdx >= 8) rex |= 0x01; // REX.B
    if (rex != 0x40) EmitByte(rex);
    EmitByte(0x0F);
    EmitByte(0x59);
    EmitByte(static_cast<uint8_t>(0xC0 | ((dstIdx & 7) << 3) | (srcIdx & 7)));
}

CASMWriter::CASMWriter()
{
	// Reference Tracking
	m_referenceTracker.Reset();

	// Work Variables
	m_dwLineNumber=0;

	// Reset ASM Code Database
	for(DWORD i=0; i<ASMMAXCOUNT; i++)
	{
		m_iASMPreOp[i]=0;
		m_iASMOp1[i]=0;
		m_iASMOp2[i]=0;
		m_iASMOp3[i]=-1;
		m_bASMOpData[i]=false;
	}

	// Leap marker state is managed by m_leapManager (constructed automatically)

	// First task to create default ASM Codes
	GenerateASMCodes();

	// Clear Debug Mode Runner Vars
	CDebuggerInterface::InitDebuggerState();
}

CASMWriter::~CASMWriter()
{
	// RAII handles cleanup of vector members
}

void CASMWriter::SetDefaultCompileFlags ( bool bArraySafetyFlag )
{
	// Compile Flags by default
	SetArrayCheckFlag(bArraySafetyFlag);
}

void CASMWriter::GenerateASMCodes(void)
{
	// Native 64-Bit x86-64 ASM Codes for ASMWriting
	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXIMM1),	"MOV AL IMM1",		-1,		0xB0+0,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXIMM2),	"MOV AX IMM2",		0x66,	0xB8+0,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXIMM4),	"MOV RAX IMM",		0x48,	0xB8+0,	-1,		true);

	DefineASM(static_cast<DWORD>(ASMOp::MOVRBXIMM1),	"MOV BL IMM1",		-1,		0xB0+3,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRBXIMM2),	"MOV BX IMM2",		0x66,	0xB8+3,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRBXIMM4),	"MOV RBX IMM",		0x48,	0xB8+3,	-1,		true);
	
	DefineASM(static_cast<DWORD>(ASMOp::MOVRDXIMM4),	"MOV RDX IMM",		0x48,	0xB8+2,	-1,		true);

	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXMEM1),	"MOV AL MEM1",		-1,		0x8A,	0x05,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXMEM2),	"MOV AX MEM2",		0x66,	0x8B,	0x05,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXMEM4),	"MOV RAX MEM",		0x48,	0x8B,	0x05,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVMEMRAX1),	"MOV MEM1 AL",		-1,		0x88,	0x05,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVMEMRAX2),	"MOV MEM2 AX",		0x66,	0x89,	0x05,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVMEMRAX4),	"MOV MEM RAX",		0x48,	0x89,	0x05,	true);

	DefineASM(static_cast<DWORD>(ASMOp::MOVRCXOFFRAX1),"MOV [RCX+A] AL",	-1,		0x88,	0x81,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRCXOFFRAX2),"MOV [RCX+A] AX",	0x66,	0x89,	0x81,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRCXOFFRAX4),"MOV [RCX+A] RAX",	0x48,	0x89,	0x81,	true);

	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXOFFRCX1),"MOV [RAX+A] CL",	-1,		0x88,	0x88,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXOFFRCX2),"MOV [RAX+A] CX",	0x66,	0x89,	0x88,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXOFFRCX4),"MOV [RAX+A] RCX",	0x48,	0x89,	0x88,	true);
	
	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXRCXREL1),"MOV [RAX] CL",	-1,		0x88,	0x08,	false);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXRCXREL2),"MOV [RAX] CX",	0x66,	0x89,	0x08,	false);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXRCXREL4),"MOV [RAX] RCX",	0x48,	0x89,	0x08,	false);
	
	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXRAXREL1),"MOV AL [RAX]",	-1,		0x8A,	0x00,	false);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXRAXREL2),"MOV AX [RAX]",	0x66,	0x8B,	0x00,	false);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXRAXREL4),"MOV RAX [RAX]",	0x48,	0x8B,	0x00,	false);
	
	DefineASM(static_cast<DWORD>(ASMOp::MOVRCXRAXOFF1),"MOV CL [RAX+A]",	-1,		0x8A,	0x88,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRCXRAXOFF2),"MOV CX [RAX+A]",	0x66,	0x8B,	0x88,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRCXRAXOFF4),"MOV RCX [RAX+A]",	0x48,	0x8B,	0x88,	true);
	
	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXRCXOFF1),"MOV AL [RCX+A]",	-1,		0x8A,	0x81,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXRCXOFF2),"MOV AX [RCX+A]",	0x66,	0x8B,	0x81,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXRCXOFF4),"MOV RAX [RCX+A]",	0x48,	0x8B,	0x81,	true);
	
	DefineASM(static_cast<DWORD>(ASMOp::MOVRDXRAXOFF1),"MOV DL [RAX+A]",	-1,		0x8A,	0x90,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRDXRAXOFF2),"MOV DX [RAX+A]",	0x66,	0x8B,	0x90,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRDXRAXOFF4),"MOV RDX [RAX+A]",	0x48,	0x8B,	0x90,	true);

	DefineASM(static_cast<DWORD>(ASMOp::MULRDXRAXOFF4),"IMUL RDX [RAX+A]",0x48,	0x0F,	0xAF,	true);

	DefineASM(static_cast<DWORD>(ASMOp::MOVRBXRAXOFF1),"MOV BL [RAX+A]",	-1,		0x8A,	0x98,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRBXRAXOFF2),"MOV BX [RAX+A]",	0x66,	0x8B,	0x98,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRBXRAXOFF4),"MOV RBX [RAX+A]",	0x48,	0x8B,	0x98,	true);

	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXRBP1),	"MOV AL [RBP+A]",	-1,		0x8A,	0x85,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXRBP2),	"MOV AX [RBP+A]",	0x66,	0x8B,	0x85,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXRBP4),	"MOV RAX [RBP+A]",	0x48,	0x8B,	0x85,	true);
	
	DefineASM(static_cast<DWORD>(ASMOp::MOVRBXRBP1),	"MOV BL [RBP+A]",	-1,		0x8A,	0x9D,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRBXRBP2),	"MOV BX [RBP+A]",	0x66,	0x8B,	0x9D,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRBXRBP4),	"MOV RBX [RBP+A]",	0x48,	0x8B,	0x9D,	true);
	
	DefineASM(static_cast<DWORD>(ASMOp::MOVRBPRAX1),	"MOV [RBP+A] AL",	-1,		0x88,	0x85,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRBPRAX2),	"MOV [RBP+A] AX",	0x66,	0x89,	0x85,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRBPRAX4),	"MOV [RBP+A] RAX",	0x48,	0x89,	0x85,	true);
	
	DefineASM(static_cast<DWORD>(ASMOp::MOVRDXRAX4),	"MOV RDX RAX",		0x48,	0x8B,	0xD0,	false);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRCXIMM4),	"MOV RCX IMM",		0x48,	0xB8+1, -1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRCXRAX4),	"MOV RCX RAX",		0x48,	0x8B,	0xC8,	false);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRCXRDX4),	"MOV RCX RDX",		0x48,	0x8B,	0xCA,	false);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRDXRCX4),	"MOV RDX RCX",		0x48,	0x8B,	0xD1,	false);

	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXRBX4),	"MOV RAX RBX",		0x48,	0x8B,	0xC3,	false);

	DefineASM(static_cast<DWORD>(ASMOp::MOVRCXRBX1),	"MOV CL BL",		-1,		0x8A,	0xCB,	false);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRCXRBX2),	"MOV CX BX",		0x66,	0x8B,	0xCB,	false);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRCXRBX4),	"MOV RCX RBX",		0x48,	0x8B,	0xCB,	false);
	
	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXRCX1),	"MOV AL CL",		-1,		0x8A,	0xC1,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXRCX2),	"MOV AX CX",		0x66,	0x8B,	0xC1,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXRCX4),	"MOV RAX RCX",		0x48,	0x8B,	0xC1,	true);

	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXRDX1),	"MOV AL DL",		-1,		0x8A,	0xC2,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXRDX2),	"MOV AX DX",		0x66,	0x8B,	0xC2,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXRDX4),	"MOV RAX RDX",		0x48,	0x8B,	0xC2,	true);

	DefineASM(static_cast<DWORD>(ASMOp::MOVRBXRAX1),	"MOV BL AL",		-1,		0x8A,	0xD8,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRBXRAX2),	"MOV BX AX",		0x66,	0x8B,	0xD8,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRBXRAX4),	"MOV RBX RAX",		0x48,	0x8B,	0xD8,	true);

	DefineASM(static_cast<DWORD>(ASMOp::ADDRAXRBX1),	"ADD AL BL",		-1,		0x00,	0xD8,	false);
	DefineASM(static_cast<DWORD>(ASMOp::ADDRAXRBX2),	"ADD AX BX",		0x66,	0x01,	0xD8,	false);
	DefineASM(static_cast<DWORD>(ASMOp::ADDRAXRBX4),	"ADD RAX RBX",		0x48,	0x01,	0xD8,	false);

	DefineASM(static_cast<DWORD>(ASMOp::ADDRAXRCX4),	"ADD RAX RCX",		0x48,	0x01,	0xC8,	false);

	DefineASM(static_cast<DWORD>(ASMOp::ADDRSP),		"ADD RSP",			0x48,	0x81,	0xC4,	true);
	DefineASM(static_cast<DWORD>(ASMOp::SUBRSP),		"SUB RSP",			0x48,	0x81,	0xEC,	true);
	DefineASM(static_cast<DWORD>(ASMOp::SUBRSPRAX),	"SUB RSP RAX",		0x48,	0x29,	0xC4,	false);

	DefineASM(static_cast<DWORD>(ASMOp::PUSHRBP),		"PUSH RBP",			-1,		0x55,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::POPRBP),		"POP RBP",			-1,		0x5D,	-1,		true);
	
	DefineASM(static_cast<DWORD>(ASMOp::MOVRBPRSP),	"MOV RBP RSP",		0x48,	0x89,	0xE5,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRSPRBP),	"MOV RSP RBP",		0x48,	0x89,	0xEC,	true);

	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXRSP),	"MOV RAX RSP",		0x48,	0x89,	0xE0,	false);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXRBP),	"MOV RAX RBP",		0x48,	0x89,	0xE8,	false);

	DefineASM(static_cast<DWORD>(ASMOp::MOVMEMIMM1),	"MOV MEM IMM1",		-1,		0xC6,	0x05,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVMEMIMM2),	"MOV MEM IMM2",		0x66,	0xC7,	0x05,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVMEMIMM4),	"MOV MEM IMM4",		0x48,	0xC7,	0x05,	true);

	DefineASM(static_cast<DWORD>(ASMOp::MOVRBPIMM1),	"MOV [RBP+A] IMM1",	-1,		0xC6,	0x85,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRBPIMM2),	"MOV [RBP+A] IMM2",	0x66,	0xC7,	0x85,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRBPIMM4),	"MOV [RBP+A] IMM4",	0x48,	0xC7,	0x85,	true);
	
	DefineASM(static_cast<DWORD>(ASMOp::RELMOVRAXRDX1),"REL MOV [RAX] DL",-1,		0x88,	0x10,	true);
	DefineASM(static_cast<DWORD>(ASMOp::RELMOVRAXRDX2),"REL MOV [RAX] DX",0x66,	0x89,	0x10,	true);
	DefineASM(static_cast<DWORD>(ASMOp::RELMOVRAXRDX4),"REL MOV [RAX] RDX",0x48,	0x89,	0x10,	true);

	DefineASM(static_cast<DWORD>(ASMOp::RELMOVRAXRRDX1),"REL MOV AL [RDX]",-1,	0x8A,	0x02,	true);
	DefineASM(static_cast<DWORD>(ASMOp::RELMOVRAXRRDX2),"REL MOV AX [RDX]",0x66,	0x8B,	0x02,	true);
	DefineASM(static_cast<DWORD>(ASMOp::RELMOVRAXRRDX4),"REL MOV RAX [RDX]",0x48,	0x8B,	0x02,	true);

	// SSE2 Scalar Floating Point Operations (Native 64-bit)
	DefineASM(static_cast<DWORD>(ASMOp::MOVMEMXMM0),	"MOVSD [MEM] XMM0",	0xF2,	0x0F,	0x11,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVXMM0MEM8),	"MOVSD XMM0 [MEM]",	0xF2,	0x0F,	0x10,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRBPXMM0),	"MOVSD [RBP+A] XMM0",0xF2,	0x0F,	0x11,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVXMM0RBP8),	"MOVSD XMM0 [RBP+A]",0xF2,	0x0F,	0x10,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXXMM0),	"MOVSD [RAX+A] XMM0",0xF2,	0x0F,	0x11,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVXMM0RAX8),	"MOVSD XMM0 [RAX+A]",0xF2,	0x0F,	0x10,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRCXOFFXMM0),"MOVSD [RCX+A] XMM0",0xF2,	0x0F,	0x11,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVXMM0RCXOFF8),"MOVSD XMM0 [RCX+A]",0xF2,	0x0F,	0x10,	true);
	
	DefineASM(static_cast<DWORD>(ASMOp::PUSHRAX),		"PUSH RAX",			-1,		0x50+0,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHRDX),		"PUSH RDX",			-1,		0x50+2,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHRBX),		"PUSH RBX",			-1,		0x50+3,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHRSP),		"PUSH RSP",			-1,		0x50+4,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHRCX),		"PUSH RCX",			-1,		0x50+1,	-1,		true);

	DefineASM(static_cast<DWORD>(ASMOp::PUSHRELRAX1),	"PUSH REL AL",		-1,		0xFF,	0x30,	true);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHRELRAX2),	"PUSH REL AX",		-1,		0xFF,	0x30,	true);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHRELRAX4),	"PUSH REL RAX",		-1,		0xFF,	0x30,	true);

	DefineASM(static_cast<DWORD>(ASMOp::PUSHRBP4),		"PUSH [RBP+A]",		-1,		0xFF,	0x75,	true);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHFROMRAX),	"PUSH [RAX]",		-1,		0xFF,	0x30,	false);

	DefineASM(static_cast<DWORD>(ASMOp::CALLRAX),		"CALL RAX",			-1,		0xFF,	0xD0,	false);
	DefineASM(static_cast<DWORD>(ASMOp::CALLRBX),		"CALL RBX",			-1,		0xFF,	0xD3,	false);
	DefineASM(static_cast<DWORD>(ASMOp::CALLMEM),		"CALL MEM",			-1,		0xE8,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::CALLABS),		"CALL REL",			-1,		0xE8,	-1,		true);
	
	DefineASM(static_cast<DWORD>(ASMOp::POPRAX),		"POP RAX",			-1,		0x58,	-1,		false);
	DefineASM(static_cast<DWORD>(ASMOp::POPRBX),		"POP RBX",			-1,		0x5B,	-1,		false);
	DefineASM(static_cast<DWORD>(ASMOp::RET),			"RET",				-1,		0xC3,	-1,		false);
	DefineASM(static_cast<DWORD>(ASMOp::POPRDX),		"POP RDX",			-1,		0x5A,	-1,		false);
	DefineASM(static_cast<DWORD>(ASMOp::POPRCX),		"POP RCX",			-1,		0x59,	-1,		false);

	DefineASM(static_cast<DWORD>(ASMOp::UNKNOWN),		"???",				-1,		-1,		-1,		false);

	DefineASM(static_cast<DWORD>(ASMOp::CMPRAX1),		"CMP AL",			-1,		0x3C,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::CMPRAX2),		"CMP AX",			0x66,	0x3D,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::CMPRAX4),		"CMP RAX",			0x48,	0x3D,	-1,		true);

	DefineASM(static_cast<DWORD>(ASMOp::CMPRBX1),		"CMP BL",			-1,		0x80,	0xFB,	true);
	DefineASM(static_cast<DWORD>(ASMOp::CMPRBX2),		"CMP BX",			0x66,	0x81,	0xFB,	true);
	DefineASM(static_cast<DWORD>(ASMOp::CMPRBX4),		"CMP RBX",			0x48,	0x81,	0xFB,	true);
	
	DefineASM(static_cast<DWORD>(ASMOp::CMPRDXRBX),	"CMP RDX RBX",		0x48,	0x3B,	0xDA,	false);

	DefineASM(static_cast<DWORD>(ASMOp::CMPRDXRBX1),	"CMP DL BL",		-1,		0x3A,	0xDA,	false);
	DefineASM(static_cast<DWORD>(ASMOp::CMPRDXRBX2),	"CMP DX BX",		0x66,	0x3B,	0xDA,	false);
	DefineASM(static_cast<DWORD>(ASMOp::CMPRDXRBX4),	"CMP RDX RBX",		0x48,	0x3B,	0xDA,	false);

	DefineASM(static_cast<DWORD>(ASMOp::CMPRAXRBX4),	"CMP RAX RBX",		0x48,	0x3B,	0xD8,	false);

	DefineASM(static_cast<DWORD>(ASMOp::SETE),			"SETE AL",			0x0F,	0x94,	0xC0,	false);
	DefineASM(static_cast<DWORD>(ASMOp::SETNE),		"SETNE AL",			0x0F,	0x95,	0xC0,	false);
	DefineASM(static_cast<DWORD>(ASMOp::SETG),			"SETG AL",			0x0F,	0x9F,	0xC0,	false);
	DefineASM(static_cast<DWORD>(ASMOp::SETGE),		"SETGE AL",			0x0F,	0x9D,	0xC0,	false);
	DefineASM(static_cast<DWORD>(ASMOp::SETL),			"SETL AL",			0x0F,	0x9C,	0xC0,	false);
	DefineASM(static_cast<DWORD>(ASMOp::SETLE),		"SETLE AL",			0x0F,	0x9E,	0xC0,	false);

	DefineASM(static_cast<DWORD>(ASMOp::JMP),			"JMP",				-1,		0xE9,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::JNE),			"JNE",				-1,		0x0F,	0x85,	true);
	DefineASM(static_cast<DWORD>(ASMOp::JE),			"JE",				-1,		0x0F,	0x84,	true);
	DefineASM(static_cast<DWORD>(ASMOp::JMPREL),		"JMP REL",			-1,		0xFF,	0x25,	true);
	DefineASM(static_cast<DWORD>(ASMOp::JMPRBX),		"JMP RBX",			-1,		0xFF,	0xE3,	true);

	DefineASM(static_cast<DWORD>(ASMOp::JGE),			"JGE",				-1,		0x0F,	0x8D,	true);
	DefineASM(static_cast<DWORD>(ASMOp::JLE),			"JLE",				-1,		0x0F,	0x8E,	true);

	DefineASM(static_cast<DWORD>(ASMOp::MOVMEMRSP4),	"MOV MEM RSP",		0x48,	0x89,	0x25,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRSPMEM4),	"MOV RSP MEM",		0x48,	0x8B,	0x25,	true);

	DefineASM(static_cast<DWORD>(ASMOp::MOVRBXMEM4),	"MOV RBX MEM",		0x48,	0x8B,	0x1D,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVMEMRBX4),	"MOV MEM RBX",		0x48,	0x89,	0x1D,	true);

	// SIB byte 0x98 = [RAX + RBX*8] (scale=8, index=RBX, base=RAX)
	DefineASM(static_cast<DWORD>(ASMOp::MOVRAXSIB),	"MOV RAX [RAX+RBX*8]",	0x48,	0x8B,	0x04,	false,	0x98);

	DefineASM(static_cast<DWORD>(ASMOp::PUSHIMM4),		"PUSH IMM4",		-1,		0x68,	-1,		true);

	DefineASM(static_cast<DWORD>(ASMOp::INCMEM1),		"INC MEM1",			-1,		0xFE,	0x05,	true);
	DefineASM(static_cast<DWORD>(ASMOp::INCMEM2),		"INC MEM2",			0x66,	0xFF,	0x05,	true);
	DefineASM(static_cast<DWORD>(ASMOp::INCMEM4),		"INC MEM",			0x48,	0xFF,	0x05,	true);
	DefineASM(static_cast<DWORD>(ASMOp::DECMEM1),		"DEC MEM1",			-1,		0xFE,	0x0D,	true);
	DefineASM(static_cast<DWORD>(ASMOp::DECMEM2),		"DEC MEM2",			0x66,	0xFF,	0x0D,	true);
	DefineASM(static_cast<DWORD>(ASMOp::DECMEM4),		"DEC MEM",			0x48,	0xFF,	0x0D,	true);

	DefineASM(static_cast<DWORD>(ASMOp::ADDRAX1),		"ADD AL IMM1",		-1,		0x04,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::ADDRAX2),		"ADD AX IMM2",		0x66,	0x05,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::ADDRAX4),		"ADD RAX IMM",		0x48,	0x05,	-1,		true);
	
	DefineASM(static_cast<DWORD>(ASMOp::SUBRAX1),		"SUB AL IMM1",		-1,		0x2C,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::SUBRAX2),		"SUB AX IMM2",		0x66,	0x2D,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::SUBRAX4),		"SUB RAX IMM",		0x48,	0x2D,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::SUBRAXRBX1),	"SUB AL BL",		-1,		0x28,	0xD8,	false);
	DefineASM(static_cast<DWORD>(ASMOp::SUBRAXRBX2),	"SUB AX BX",		0x66,	0x29,	0xD8,	false);
	DefineASM(static_cast<DWORD>(ASMOp::SUBRAXRBX4),	"SUB RAX RBX",		0x48,	0x29,	0xD8,	false);

	DefineASM(static_cast<DWORD>(ASMOp::MULRAXRBX1),	"IMUL AL BL",		-1,		0xF6,	0xEB,	false);
	DefineASM(static_cast<DWORD>(ASMOp::MULRAXRBX2),	"IMUL AX BX",		0x66,	0xF7,	0xEB,	false);
	DefineASM(static_cast<DWORD>(ASMOp::MULRAXRBX4),	"IMUL RAX RBX",		0x48,	0x0F,	0xAF,	false);

	DefineASM(static_cast<DWORD>(ASMOp::CQO),			"CQO",				0x48,	0x99,	-1,		false);

	DefineASM(static_cast<DWORD>(ASMOp::DIVRAXRBX1),	"IDIV AL BL",		-1,		0xF6,	0xFB,	false);
	DefineASM(static_cast<DWORD>(ASMOp::DIVRAXRBX2),	"IDIV AX BX",		0x66,	0xF7,	0xFB,	false);
	DefineASM(static_cast<DWORD>(ASMOp::DIVRAXRBX4),	"IDIV RBX",			0x48,	0xF7,	0xFB,	false);

	DefineASM(static_cast<DWORD>(ASMOp::ANDRAX1),		"AND AL IMM1",		-1,		0x24,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::ANDRAX2),		"AND AX IMM2",		0x66,	0x25,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::ANDRAX4),		"AND RAX IMM",		0x48,	0x25,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::ANDRAXRBX1),	"AND AL BL",		-1,		0x20,	0xD8,	false);
	DefineASM(static_cast<DWORD>(ASMOp::ANDRAXRBX2),	"AND AX BX",		0x66,	0x21,	0xD8,	false);
	DefineASM(static_cast<DWORD>(ASMOp::ANDRAXRBX4),	"AND RAX RBX",		0x48,	0x21,	0xD8,	false);

	DefineASM(static_cast<DWORD>(ASMOp::ORRAX1),		"OR AL IMM1",		-1,		0x0C,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::ORRAX2),		"OR AX IMM2",		0x66,	0x0D,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::ORRAX4),		"OR RAX IMM",		0x48,	0x0D,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::ORRAXRBX1),	"OR AL BL",			-1,		0x08,	0xD8,	false);
	DefineASM(static_cast<DWORD>(ASMOp::ORRAXRBX2),	"OR AX BX",			0x66,	0x09,	0xD8,	false);
	DefineASM(static_cast<DWORD>(ASMOp::ORRAXRBX4),	"OR RAX RBX",		0x48,	0x09,	0xD8,	false);

	DefineASM(static_cast<DWORD>(ASMOp::NOTRAX1),		"NOT AL",			-1,		0xF6,	0xD0,	false);
	DefineASM(static_cast<DWORD>(ASMOp::NOTRAX2),		"NOT AX",			0x66,	0xF7,	0xD0,	false);
	DefineASM(static_cast<DWORD>(ASMOp::NOTRAX4),		"NOT RAX",			0x48,	0xF7,	0xD0,	false);

	DefineASM(static_cast<DWORD>(ASMOp::XORRAX1),		"XOR AL IMM1",		-1,		0x34,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::XORRAX2),		"XOR AX IMM2",		0x66,	0x35,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::XORRAX4),		"XOR RAX IMM",		0x48,	0x35,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::XORRAXRBX1),	"XOR AL BL",		-1,		0x30,	0xD8,	false);
	DefineASM(static_cast<DWORD>(ASMOp::XORRAXRBX2),	"XOR AX BX",		0x66,	0x31,	0xD8,	false);
	DefineASM(static_cast<DWORD>(ASMOp::XORRAXRBX4),	"XOR RAX RBX",		0x48,	0x31,	0xD8,	false);
	
	DefineASM(static_cast<DWORD>(ASMOp::SHLRAX1),		"SHL AL IMM1",		-1,		0xC0,	0xE0,	true);
	DefineASM(static_cast<DWORD>(ASMOp::SHLRAX2),		"SHL AX IMM2",		0x66,	0xC1,	0xE0,	true);
	DefineASM(static_cast<DWORD>(ASMOp::SHLRAX4),		"SHL RAX IMM",		0x48,	0xC1,	0xE0,	true);
	DefineASM(static_cast<DWORD>(ASMOp::SHLRAXCLC1),	"SHL AL CL",		-1,		0xD2,	0xE0,	false);
	DefineASM(static_cast<DWORD>(ASMOp::SHLRAXCLC2),	"SHL AX CL",		0x66,	0xD3,	0xE0,	false);
	DefineASM(static_cast<DWORD>(ASMOp::SHLRAXCLC4),	"SHL RAX CL",		0x48,	0xD3,	0xE0,	false);

	DefineASM(static_cast<DWORD>(ASMOp::SHRRAX1),		"SHR AL IMM1",		-1,		0xC0,	0xE8,	true);
	DefineASM(static_cast<DWORD>(ASMOp::SHRRAX2),		"SHR AX IMM2",		0x66,	0xC1,	0xE8,	true);
	DefineASM(static_cast<DWORD>(ASMOp::SHRRAX4),		"SHR RAX IMM",		0x48,	0xC1,	0xE8,	true);
	DefineASM(static_cast<DWORD>(ASMOp::SHRRAXCLC1),	"SHR AL CL",		-1,		0xD2,	0xE8,	false);
	DefineASM(static_cast<DWORD>(ASMOp::SHRRAXCLC2),	"SHR AX CL",		0x66,	0xD3,	0xE8,	false);
	DefineASM(static_cast<DWORD>(ASMOp::SHRRAXCLC4),	"SHR RAX CL",		0x48,	0xD3,	0xE8,	false);

	DefineASM(static_cast<DWORD>(ASMOp::MULRCXRDX4),	"IMUL RCX RDX",		0x48,	0x0F,	0xAF,	false);
	DefineASM(static_cast<DWORD>(ASMOp::ADDRBXRDX4),	"ADD RBX RDX",		0x48,	0x01,	0xD3,	false);

	// Native x64-only register operations (Microsoft x64 ABI callee-saved set
	// and string primitives with no 32-bit instruction equivalent)
	DefineASM(static_cast<DWORD>(ASMOp::PUSHRDI),		"PUSH RDI",			-1,		0x57,	-1,		false);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHRSI),		"PUSH RSI",			-1,		0x56,	-1,		false);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHR12),		"PUSH R12",			0x41,	0x54,	-1,		false);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHR13),		"PUSH R13",			0x41,	0x55,	-1,		false);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHR14),		"PUSH R14",			0x41,	0x56,	-1,		false);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHR15),		"PUSH R15",			0x41,	0x57,	-1,		false);
	DefineASM(static_cast<DWORD>(ASMOp::POPRDI),		"POP RDI",			-1,		0x5F,	-1,		false);
	DefineASM(static_cast<DWORD>(ASMOp::POPRSI),		"POP RSI",			-1,		0x5E,	-1,		false);
	DefineASM(static_cast<DWORD>(ASMOp::POPR12),		"POP R12",			0x41,	0x5C,	-1,		false);
	DefineASM(static_cast<DWORD>(ASMOp::POPR13),		"POP R13",			0x41,	0x5D,	-1,		false);
	DefineASM(static_cast<DWORD>(ASMOp::POPR14),		"POP R14",			0x41,	0x5E,	-1,		false);
	DefineASM(static_cast<DWORD>(ASMOp::POPR15),		"POP R15",			0x41,	0x5F,	-1,		false);
	DefineASM(static_cast<DWORD>(ASMOp::MOVRDIRSP),	"MOV RDI RSP",		0x48,	0x89,	0xE7,	false);
	DefineASM(static_cast<DWORD>(ASMOp::ADDRDIIMM),	"ADD RDI IMM",		0x48,	0x81,	0xC7,	true);
	DefineASM(static_cast<DWORD>(ASMOp::REPSTOSQ),		"REP STOSQ",		0xF3,	0x48,	0xAB,	false);
	DefineASM(static_cast<DWORD>(ASMOp::REPSTOSB),		"REP STOSB",		0xF3,	0xAA,	-1,		false);
}

int CASMWriter::DetermineOpDataWidth(int iPreOp, int iOp1, int iOp2)
{
	// rel32 direct branches: CALL/JMP (E8/E9) and Jcc (0F 8x)
	if (iOp1 == 0xE8 || iOp1 == 0xE9) return 4;
	if (iOp1 == 0x0F && iOp2 != -1 && (iOp2 & 0xF0) == 0x80) return 4;
	if (iOp2 == 0xE8 || iOp2 == 0xE9) return 4;

	if (iOp2 == -1)
	{
		// No ModRM byte: accumulator/relative immediate forms, PUSH imm, or
		// MOV reg, imm64 (the only 8-byte ref slot in this family).
		if (iOp1 == 0x68) return 4;              // PUSH imm32
		if (iOp1 == 0x6A) return 1;              // PUSH imm8
		if (iOp1 == 0x04 || iOp1 == 0x2C || iOp1 == 0x24 ||
			iOp1 == 0x0C || iOp1 == 0x34 || iOp1 == 0x3C) return 1; // AL imm8
		if (iOp1 == 0x05 || iOp1 == 0x2D || iOp1 == 0x25 ||
			iOp1 == 0x0D || iOp1 == 0x35 || iOp1 == 0x3D)
			return (iPreOp == 0x66) ? 2 : 4;                       // AX imm16 / EAX imm32 (RAX imm32 with 48)
		if (iOp1 >= 0xB0 && iOp1 <= 0xB7) return 1;                // MOV r8, imm8
		if (iOp1 >= 0xB8 && iOp1 <= 0xBF)                          // MOV r, imm
		{
			if (iPreOp == 0x48) return 8;                          // MOV r64, imm64
			if (iPreOp == 0x66) return 2;                          // MOV r16, imm16
			return 4;                                              // MOV r32, imm32
		}
		return 8;
	}

	// Shift/rotate by imm8 (C0/C1 group).
	if (iOp1 == 0xC0 || iOp1 == 0xC1) return 1;
	// MOV r/m8/16/32, imm: C6 = imm8, C7 = imm16 with 66 prefix / imm32 otherwise.
	if (iOp1 == 0xC6) return 1;
	if (iOp1 == 0xC7) return (iPreOp == 0x66) ? 2 : 4;

	const int mod = (iOp2 >> 6) & 3;
	const int rm  = iOp2 & 7;
	if (mod == 2) return 4;                      // [reg+disp32]
	if (mod == 0 && rm == 5) return 4;           // [RIP+disp32]
	if (mod == 1) return 1;                      // [reg+disp8]
	if (mod == 3)
	{
		// Register forms with an immediate: 81 = imm32, 83 = imm8.
		if (iOp1 == 0x81) return 4;
		if (iOp1 == 0x83) return 1;
	}
	return 8;
}

int CASMWriter::DetermineSecondOpDataWidth(int iPreOp, int iOp1, int iOp2)
{
	// MOV r/m, imm: the value slot is imm8/imm16/imm32 for C6/C7; register
	// forms 80/81/83 carry imm8/imm32/imm8. All other instructions take a
	// single operand, so the second slot is empty.
	if (iOp1 == 0xC6) return 1;
	if (iOp1 == 0xC7) return (iPreOp == 0x66) ? 2 : 4;
	if (iOp1 == 0x80 || iOp1 == 0x83) return 1;
	if (iOp1 == 0x81) return 4;
	return 0;
}

void CASMWriter::DefineASM(DWORD dwASMCode, LPCSTR pDebugStr, int iPreOp, int iOp1, int iOp2, bool bOpData, int iOp3)
{
	// Store Debug String for ASM Code
	m_ASMDebugStrings[dwASMCode] = pDebugStr;

	// Store OpCodes for ASM Code
	m_iASMPreOp[dwASMCode]=iPreOp;
	m_iASMOp1[dwASMCode]=iOp1;
	m_iASMOp2[dwASMCode]=iOp2;
	m_iASMOp3[dwASMCode]=iOp3;

	// Store OpData Flag for ASM Code
	m_bASMOpData[dwASMCode]=bOpData;

	// Store operand slot width derived from the encoding (4 for disp32/rel32,
	// 8 for imm64; other immediate widths for the value operands).
	m_iASMOpDataWidth[dwASMCode] = DetermineOpDataWidth(iPreOp, iOp1, iOp2);
	m_iASMOpData2Width[dwASMCode] = DetermineSecondOpDataWidth(iPreOp, iOp1, iOp2);
}

bool CASMWriter::CreateASMHeader(void)
{
	// Create Empty MC Block via machine code buffer
	m_machineCodeBuffer.Initialize(1024);

	// Prepare RefData
	m_referenceTracker.Reset();

	// In Debug Mode, hooks are always present
	if(g_DebugInfo.DebugModeOn())
	{
		// Add To DLL&Command Table
		AddCommandToTable("[EXE", "!DHookS");
		AddCommandToTable("[EXE", "!DHookJ");
		AddCommandToTable("[EXE", "!DHookR");
	}

	return true;
}

bool CASMWriter::CreateASMMiddle(int iPreOpCode, int iOpCode1, int iOpCode2, LPCSTR lpOpData, int iOp3)
{
	return CreateASMMiddleCore(iPreOpCode, iOpCode1, iOpCode2, lpOpData, nullptr, false, 0, iOp3);
}

bool CASMWriter::CreateASMMiddleCore(int iPreOpCode, int iOpCode1, int iOpCode2, LPCSTR lpOpData, LPCSTR lpOpData2, bool bSecondOpDataIsIMM, DWORD dwSecondOpDataIMMSize, int iOp3)
{
	if(m_machineCodeBuffer.GetProgramStart()==nullptr || m_machineCodeBuffer.GetMachineBlock()==nullptr)
	{
		if(g_pErrorReport)
			g_pErrorReport->AddErrorString(
				"DBP2001: code emission attempted before backend initialization.");
		return false;
	}
	DBP_TRACE("Generated instruction: preOp={}, op1={}, op2={}, op3={}", iPreOpCode, iOpCode1, iOpCode2, iOp3);

	// Check and expand if MCB too small
	CheckAndExpandMCBMemory();

	// x64-native addressing: RIP-relative disp32 ([rip+disp32]) physically
	// cannot reach targets outside the +-2GB window - module globals
	// (_ERR_/_ESC_/_REK_ live in the EXE image), the variable space, and the
	// heap can all be far beyond it. Detect the RIP-relative memory forms
	// (ModRM mod=00, rm=101) and rewrite them to register-indirect through
	// R11, a scratch register this codegen never otherwise uses:
	//     mov r11, <imm64 addr>    49 BB <addr>
	//     <op> [r11]               ModRM rm=011, no disp32
	// The address ref slot moves into the imm64 of the mov r11; the memory
	// instruction itself carries no displacement and works at any distance.
	const bool bRipRelMemOp =
		(iOpCode2 != -1) && ((iOpCode2 & 0x07) == 0x05) && ((iOpCode2 & 0xC0) == 0x00);

	if (bRipRelMemOp && lpOpData != nullptr && lpOpData[0] != '\0')
	{
		// mov r11, imm64 (REX.W|REX.B 0x49 + 0xBB), with an 8-byte ref slot
		// for the absolute target address.
		CheckAndExpandREFMemory();
		const DWORD addrPos = m_machineCodeBuffer.GetCurrentMCPosition();
		m_machineCodeBuffer.WriteByte(0x49);
		m_machineCodeBuffer.WriteByte(0xBB);
		CStr cleanAddr(lpOpData);
		cleanAddr.EatEdgeSpacesandTabs(nullptr);
		m_referenceTracker.AddReference(addrPos + 2, cleanAddr.GetStr(), 8u, addrPos + 10u);
		m_machineCodeBuffer.WriteQWORD(0xFFFFFFFFFFFFFFFFULL, 8u);
		// The address is consumed by the mov r11; the memory op now uses [r11].
		lpOpData = nullptr;
		iOpCode2 = (iOpCode2 & 0xF8) | 0x03;   // rm=101 (RIP) -> rm=011 (R11)
	}

	// Write OpCode(s). The rewritten memory form needs REX.B for R11:
	// 48 (REX.W) becomes 49 (REX.W+B), 66 keeps operand size with a 41
	// prefix, and no-prefix forms get 41.
	if(bRipRelMemOp)
	{
		if (iPreOpCode == 0x48)
		{
			m_machineCodeBuffer.WriteByte(0x49);
		}
		else if (iPreOpCode == 0x66)
		{
			m_machineCodeBuffer.WriteByte(0x41);
			m_machineCodeBuffer.WriteByte(0x66);
		}
		else
		{
			m_machineCodeBuffer.WriteByte(0x41);
		}
	}
	else if(iPreOpCode!=-1)
	{
		m_machineCodeBuffer.WriteByte(iPreOpCode);
	}
	if(iOpCode1!=-1)
	{
		m_machineCodeBuffer.WriteByte(iOpCode1);
	}
	if(iOpCode2!=-1)
	{
		m_machineCodeBuffer.WriteByte(iOpCode2);
	}
	if(iOp3!=-1)
	{
		m_machineCodeBuffer.WriteByte(iOp3);
	}

	// x64 operand slot widths derived from the instruction encoding. The
	// legacy uniform-8-byte ref emission corrupted every disp32/rel32 slot
	// (writing absolute 64-bit addresses into 4-byte PC-relative fields) and
	// clobbered the following instruction - the root cause of boot-time AVs
	// in compiled applications. For the rewritten register-indirect memory
	// form the address lives in the mov r11 imm64, so the memory op itself
	// has no disp32 slot.
	const int iOpDataWidth = bRipRelMemOp
		? 0 : DetermineOpDataWidth(iPreOpCode, iOpCode1, iOpCode2);
	const int iOpData2Width = DetermineSecondOpDataWidth(iPreOpCode, iOpCode1, iOpCode2);
	int iOpcodeLen = 0;
	if(iPreOpCode!=-1) iOpcodeLen++;
	if(iOpCode1!=-1) iOpcodeLen++;
	if(iOpCode2!=-1) iOpcodeLen++;
	if(iOp3!=-1) iOpcodeLen++;
	if (bRipRelMemOp)
	{
		// 49 BB mov r11, imm64 prefix; the 66 form also gains a 41 REX.B.
		iOpcodeLen += 2;
		if (iPreOpCode == 0x66) iOpcodeLen += 1;
	}
	const int iFullInstLen = iOpcodeLen + iOpDataWidth + iOpData2Width;

	// Write Optional OpData 1 and 2
	for(DWORD n=0; n<2; n++)
	{
		LPCSTR pData=lpOpData;
		if(n==1) pData=lpOpData2;
		if(pData)
		{
			if(strcmp(pData, "")!=0)
			{
				// Ensure reference array always large enough for new reference
				CheckAndExpandREFMemory();

				// REF or IMM
				if(bSecondOpDataIsIMM==true && n==1)
				{
				// WRITE IMM INTO MC
				uint64_t dwDataAsQWORD = static_cast<uint64_t>(_atoi64(pData));
				// The immediate slot width is a property of the instruction
				// encoding, not of the caller's legacy size code: MOV r64,imm64
				// (48 B8-BF) requires 8 bytes while C6/C7 imm slots are
				// 1/2/4 bytes. The 64-bit port widened the opcode (48 REX.W)
				// without widening the emitted immediate, which truncated every
				// 64-bit constant load and desynchronized the instruction
				// stream. Derive the width from the encoding instead.
				DWORD dwByteSize = (iOpCode2 != -1)
					? static_cast<DWORD>(DetermineSecondOpDataWidth(iPreOpCode, iOpCode1, iOpCode2))
					: static_cast<DWORD>(DetermineOpDataWidth(iPreOpCode, iOpCode1, iOpCode2));
				if (dwByteSize == 0) dwByteSize = 4;
				m_machineCodeBuffer.WriteQWORD(dwDataAsQWORD, dwByteSize);
				}
				else
				{
					// Record a value-owned reference label. Host pointers must never
					// leak into the serialized target reference representation.
					CStr cleanStr(pData);
					cleanStr.EatEdgeSpacesandTabs(nullptr);

					const DWORD MCBBytePos = m_machineCodeBuffer.GetCurrentMCPosition();
					const int iSlotBytes = (n==0) ? iOpDataWidth : iOpData2Width;
					// PC-relative reference point: the end of the enclosing
					// instruction (RIP for disp32, next instruction for rel32).
					const DWORD dwRelEnd = (n==0)
						? (MCBBytePos + static_cast<DWORD>(iFullInstLen - iOpcodeLen))
						: (MCBBytePos + static_cast<DWORD>(iOpData2Width));
					m_referenceTracker.AddReference(MCBBytePos, cleanStr.GetStr(),
						static_cast<std::uint32_t>(iSlotBytes), dwRelEnd);

					// WRITE BLANK(XX) INTO MB sized to the operand slot
					m_machineCodeBuffer.WriteQWORD(0xFFFFFFFFFFFFFFFFULL, static_cast<DWORD>(iSlotBytes));
				}
			}
		}
		if(lpOpData2==nullptr) break;
	}

	// Complete
	return true;
}

bool CASMWriter::CheckAndExpandMCBMemory(void)
{
	// Save leap marker relative offsets before expansion
	LPSTR pOldStart = m_machineCodeBuffer.GetProgramStart();
	DWORD dwByteOffset = 0;
	DWORD dwLeapRelDiff[9];
	if (pOldStart)
	{
		dwByteOffset = m_leapManager.GetRecordTopBytePosition() - pOldStart;
		for(DWORD di=0; di<9; di++)
			dwLeapRelDiff[di] = m_leapManager.GetRecordBytePosition(di) - pOldStart;
	}

	// Delegate expansion to machine code buffer
	bool bExpanded = m_machineCodeBuffer.CheckAndExpandMCBMemory();

	// If expansion occurred, rebase leap markers
	if (bExpanded)
	{
		LPSTR pNewStart = m_machineCodeBuffer.GetProgramStart();
		m_leapManager.SetRecordTopBytePosition(pNewStart+dwByteOffset);
		for(DWORD di=0; di<9; di++)
			m_leapManager.SetRecordBytePosition(di, pNewStart+dwLeapRelDiff[di]);
	}

	return bExpanded;
}

bool CASMWriter::CheckAndExpandREFMemory(void)
{
	return m_referenceTracker.CheckAndExpandREFMemory();
}

void VarValueSenderHook(void)
{
	// Send Message To Debugger
	DWORD dwSize=0;
	std::unique_ptr<char[]> pData(g_pASMWriter->MakeVarValuesForTransfer(&dwSize));
	CDebuggerInterface::SendDataToDebugger(21, pData.get(), dwSize);
}

LRESULT DebugHookStatementFunctionCall(DWORD dwProg, DWORD dwLine, DWORD dwStart, DWORD dwEnd)
{
	// Send Message To Debugger
	DWORD* pData = new DWORD[4];
	pData[0] = dwProg;
	pData[1] = dwLine;
	pData[2] = dwStart;
	pData[3] = dwEnd;

	// Report Progress In Program
	LRESULT lResult = CDebuggerInterface::SendDataToDebugger(11, (LPSTR)pData, sizeof(DWORD)*4);

	// The Return Value Controls the Debugger Flow
	switch(lResult)
	{
		case 0 : // Continues To Next Statement
			break;
		case 1 : // Stays At Current Statement
			break;
		case 2 : // Switches back to FullSpeed
			g_dwEscapeValueMem=0;
			lResult=0;//RAX needs to be zero
			break;
		case 11 : // Parse CLI Program
			g_dwBreakOutPosition=1;
			break;
	}

	// If compiler-loaded-debugger is closed, quit program
	if(CDebuggerInterface::IsInternalDebuggerActive()==true)
	{
		DWORD uExitCode=0;
		GetExitCodeProcess(CDebuggerInterface::GetDebuggerProcessInfo().hProcess, &uExitCode);
		if(uExitCode!=STILL_ACTIVE)
		{
			// Close program
			g_dwEscapeValueMem=2;
			PostQuitMessage(0);
		}
	}

	// Update Var Data
	VarValueSenderHook();

	return lResult;
}

void DebugHookJumpFunctionCall(DWORD dwProg, DWORD dwLine, DWORD dwStart, DWORD dwEnd)
{
	// Send Message To Debugger
	DWORD* pData = new DWORD[4];
	pData[0] = dwProg;
	pData[1] = dwLine;
	pData[2] = dwStart;
	pData[3] = dwEnd;
	CDebuggerInterface::SendDataToDebugger(12, (LPSTR)pData, sizeof(DWORD)*4);
}

void DebugHookReturnFunctionCall(void)
{
	// Send Message To Debugger
	DWORD dwData;
	CDebuggerInterface::SendDataToDebugger(13, (LPSTR)&dwData, 4);
}

bool CASMWriter::ReportAnyErrorsToCLI(void)
{
	const DWORD dwRTError=static_cast<DWORD>(g_pEXE->m_dwRuntimeErrorDWORD);
	const DWORD dwRTErrorLine=static_cast<DWORD>(g_pEXE->m_dwRuntimeErrorLineDWORD);
	if(dwRTError>0)
	{
		// Report error
		char lpReturnError[1024];
		LPSTR pRuntimeErrorString = nullptr;
		if(g_pEXE->m_pRuntimeErrorStringsArray) pRuntimeErrorString = (LPSTR)g_pEXE->m_pRuntimeErrorStringsArray[dwRTError];
		if(dwRTErrorLine>0)
			snprintf(lpReturnError, sizeof(lpReturnError), "Runtime Error %d [%s] at line %d", dwRTError, pRuntimeErrorString, dwRTErrorLine);
		else
			snprintf(lpReturnError, sizeof(lpReturnError), "Runtime Error %d [%s]", dwRTError, pRuntimeErrorString);
			
		CDebuggerInterface::SendDataToDebugger(31, lpReturnError, strlen(lpReturnError));

		// Clear error
		g_pEXE->m_dwRuntimeErrorDWORD=0;
		g_pEXE->m_dwRuntimeErrorLineDWORD=0;
	}

	// Complete
	return true;
}

bool CASMWriter::PrepareEXE(LPSTR pEXEFilename, bool bParsingMainProgram, bool bGotNewCode)
{
	db3::CProfile<> prof("CASMWriter::PrepareEXE()");

	const auto cleanup = MakeScopeExit(
		[this]() noexcept { FreeMachineBlock(); });
	ASMWriterPreparationServices services(*this);
	const ExecutablePreparationRequest request{
		pEXEFilename,
		bParsingMainProgram,
		bGotNewCode,
		g_DebugInfo.DebugModeOn()
			? ExecutableOutputMode::debug
			: ExecutableOutputMode::standalone};
	const auto result = ExecutablePreparationPipeline{}.Run(
		request, services);
	if (!result)
	{
		services.ReportFailure(result);
		return false;
	}
	return true;
}

bool CASMWriter::UpdateMCB(DWORD dwProgramSizeBytes)
{
	db3::CProfile<> prof("CASMWriter::UpdateMCB");

	if(g_pEXE->m_pMachineCodeBlock==nullptr)
	{
		// Create Array(s)
		DWORD dwNewSize = dwProgramSizeBytes;
		LPSTR pNewArray = new char[dwProgramSizeBytes];
		memcpy(pNewArray, m_machineCodeBuffer.GetProgramStart(), dwNewSize);

		// Update pointers
		g_pEXE->m_dwSizeOfMCB = dwNewSize;
		g_pEXE->m_pMachineCodeBlock = (DWORD*)pNewArray;
	}
	else
	{
		// Mark Beginning of Mini-Code M/C (so refdata points to correct bytes)
		g_pEXE->m_dwStartOfMiniMC = g_pEXE->m_dwSizeOfMCB;

		// Add To Array(s)
		DWORD dwOldSize = g_pEXE->m_dwSizeOfMCB;
		std::unique_ptr<char[]> pOldArray((LPSTR)g_pEXE->m_pMachineCodeBlock);
		DWORD dwNewSize = dwOldSize + dwProgramSizeBytes;
		LPSTR pNewArray = new char[dwNewSize];

		// Fill New Array with Old+New, then delete Old
		memcpy(pNewArray, pOldArray.get(), dwOldSize);
		memcpy(pNewArray+dwOldSize, m_machineCodeBuffer.GetProgramStart(), dwProgramSizeBytes);

		// Update pointers
		g_pEXE->m_dwSizeOfMCB = dwNewSize;
		g_pEXE->m_pMachineCodeBlock = (DWORD*)pNewArray;
	}

	// Complete
	return true;
}

bool CASMWriter::UpdateMCBRefData(void)
{
	db3::CProfile<> prof("CASMWriter::UpdateMCBRefData");
	if (g_pEXE == nullptr || g_pVarTable == nullptr || g_pLabelTable == nullptr ||
		g_pErrorReport == nullptr)
	{
		return false;
	}

	const auto resolveSymbol = [](const ParsedReference& reference)
		-> std::optional<std::uint32_t>
	{
		if (reference.kind == ReferenceKind::Variable)
		{
			auto* variable = g_pVarTable->FindVariable(
				nullptr,
				const_cast<char*>(reference.symbol.c_str()),
				reference.isArray ? 1u : 0u);
			if (variable == nullptr)
			{
				g_pErrorReport->AddErrorString(
					("Failed to resolve executable variable reference: " + reference.symbol).c_str());
				return std::nullopt;
			}

			const std::uint64_t offset =
				static_cast<std::uint64_t>(variable->GetOffsetValue()) + reference.memoryOffset;
			if (offset > (std::numeric_limits<std::uint32_t>::max)())
			{
				g_pErrorReport->AddErrorString("Executable variable reference exceeds PE32 range");
				return std::nullopt;
			}
			return static_cast<std::uint32_t>(offset);
		}

		auto* label = g_pLabelTable->FindLabel(const_cast<char*>(reference.symbol.c_str()));
		if (label == nullptr)
		{
			g_pErrorReport->AddErrorString(
				("Failed to resolve executable label reference: " + reference.symbol).c_str());
			return std::nullopt;
		}

		if (reference.kind == ReferenceKind::DataLabel)
		{
			return label->GetDataIndex();
		}

		std::uint64_t bytePosition = label->GetBytePosition();
		if (!dbp::iequals(label->GetName()->GetStr(), "$labelend"))
		{
			bytePosition += g_pEXE->m_dwStartOfMiniMC;
		}
		if (bytePosition > (std::numeric_limits<std::uint32_t>::max)())
		{
			g_pErrorReport->AddErrorString("Executable label reference exceeds PE32 range");
			return std::nullopt;
		}
		return static_cast<std::uint32_t>(bytePosition);
	};

	const bool updated = m_referenceTracker.UpdateMCBRefData(
		g_pEXE,
		g_pEXE->m_dwStartOfMiniMC,
		resolveSymbol);
	if (!updated)
	{
		g_pErrorReport->AddErrorString("Failed to parse or resolve executable reference data");
	}
	return updated;
}

bool CASMWriter::UpdateDLLData(void)
{
	return m_peBuilder.UpdateDLLData();
}

bool CASMWriter::UpdateCommandData(void)
{
	return m_peBuilder.UpdateCommandData();
}

bool CASMWriter::UpdateStringData(void)
{
	return m_peBuilder.UpdateStringData();
}

bool CASMWriter::UpdateDataData(void)
{
	return m_peBuilder.UpdateDataData();
}

bool CASMWriter::UpdateDynamicData(void)
{
	return m_peBuilder.UpdateDynamicData();
}

bool CASMWriter::UpdateStructurePatternData(void)
{
	return m_peBuilder.UpdateStructurePatternData();
}

LPSTR CASMWriter::MakeVarDataForTransfer(DWORD *pdwDataSize)
{
	LPSTR pData = nullptr;
	DWORD dwSizeOfData = 0;
	if(g_pVarTable)
	{
		// Gather Var Data
		dwSizeOfData = 4;
		DWORD dwNumberOfVariables = 0;
		CVarTable* pCurrent = g_pVarTable;
		while(pCurrent)
		{
			if(pCurrent->GetVarName()->GetChar(0)!='$' && pCurrent->GetVarScope()->Length()==0)
			{
				dwSizeOfData+=12;
				dwSizeOfData+=pCurrent->GetVarName()->Length();
				dwNumberOfVariables++;
			}
			pCurrent=pCurrent->GetNext();
		}

		// Create Data Space
		pData = new char[dwSizeOfData];

		// Fill With VarData
		LPSTR pPtr = pData;
		pCurrent = g_pVarTable;
		*((DWORD*)pPtr) = dwNumberOfVariables; pPtr+=4;
		while(pCurrent)
		{
			if(pCurrent->GetVarName()->GetChar(0)!='$' && pCurrent->GetVarScope()->Length()==0)
			{
				*((DWORD*)pPtr) = pCurrent->GetVarTypeValue(); pPtr+=4;
				*((DWORD*)pPtr) = pCurrent->GetOffsetValue(); pPtr+=4;
				DWORD dwLengthOfString=pCurrent->GetVarName()->Length();
				*((DWORD*)pPtr) = dwLengthOfString; pPtr+=4;
				for(DWORD t=0; t<dwLengthOfString; t++)
					*(pPtr++) = pCurrent->GetVarName()->GetChar(t);
			}

			// Next variable
			pCurrent=pCurrent->GetNext();
		}
	}

	// Return Data Address
	*pdwDataSize=dwSizeOfData;
	return pData;
}

LPSTR CASMWriter::MakeVarValuesForTransfer(DWORD *pdwDataSize)
{
	if(!pdwDataSize)
		return nullptr;

	*pdwDataSize = 0;
	LPSTR pData = nullptr;
	DWORD dwSizeOfData = 0;
	const auto readStringPointer = [](const DWORD offset) noexcept {
		return dbp::abi::ReadPointer<LPSTR>(
			g_pVarSpaceAddressInUse,
			g_dwVarSpaceSizeInUse,
			offset);
	};
	if(g_pVarTable)
	{
		// Gather Var Data
		dwSizeOfData = 4+g_dwVarSpaceSizeInUse;
		CVarTable* pCurrent = g_pVarTable;
		while(pCurrent)
		{
			if(pCurrent->GetVarName()->GetChar(0)!='$' && pCurrent->GetVarScope()->Length()==0)
			{
				if(pCurrent->GetVarTypeValue()==3)
				{
					// Per String header size
					dwSizeOfData+=9;

					// String Size
					DWORD dwLengthOfString=0;
					DWORD dwOffset=pCurrent->GetOffsetValue();
					const auto stringPointer = readStringPointer(dwOffset);
					if(!stringPointer)
						return nullptr;
					LPSTR pStringInMemory=*stringPointer;
					if(pStringInMemory) dwLengthOfString=strlen(pStringInMemory);
					dwSizeOfData+=dwLengthOfString;
				}
			}
			pCurrent=pCurrent->GetNext();
		}
		dwSizeOfData+=1;

		// Create Data Space
		pData = new char[dwSizeOfData];
		LPSTR pPtr = pData;
		const auto writeValue = [&](const auto& value) noexcept {
			const auto offset = static_cast<std::size_t>(pPtr-pData);
			if(!dbp::binary::WriteTrivial(
					pData,
					static_cast<std::size_t>(dwSizeOfData),
					offset,
					value))
				return false;
			pPtr += sizeof(value);
			return true;
		};

		// First DWORD is Size Of VarSpace Block
		if(!writeValue(g_dwVarSpaceSizeInUse))
		{
			delete[] pData;
			return nullptr;
		}

		// Snapshot of varspace memory
		memcpy(pPtr, g_pVarSpaceAddressInUse, g_dwVarSpaceSizeInUse);
		pPtr+=g_dwVarSpaceSizeInUse;

		// For any strings, add chardata to data
		pCurrent = g_pVarTable;
		while(pCurrent)
		{
			if(pCurrent->GetVarName()->GetChar(0)!='$' && pCurrent->GetVarScope()->Length()==0)
			{
				if(pCurrent->GetVarTypeValue()==3)
				{
					// Flag to say another string
					const unsigned char stringMarker = 1;
					if(!writeValue(stringMarker))
					{
						delete[] pData;
						return nullptr;
					}

					// Store offset to string
					DWORD dwOffset=pCurrent->GetOffsetValue();
					if(!writeValue(dwOffset))
					{
						delete[] pData;
						return nullptr;
					}

					// Locate string if any in memory at offset position
					const auto stringPointer = readStringPointer(dwOffset);
					if(!stringPointer)
					{
						delete[] pData;
						return nullptr;
					}
					LPSTR pStringInMemory=*stringPointer;

					// Store length and contents of string in memory
					DWORD dwLengthOfString=0;
					if(pStringInMemory) dwLengthOfString=strlen(pStringInMemory);
					if(!writeValue(dwLengthOfString))
					{
						delete[] pData;
						return nullptr;
					}
					for(DWORD t=0; t<dwLengthOfString; t++)
						*(pPtr++) = pStringInMemory[t];
				}
			}
			pCurrent=pCurrent->GetNext();
		}
		const unsigned char endMarker = 0;
		if(!writeValue(endMarker))
		{
			delete[] pData;
			return nullptr;
		}
	}

	// Return Data Address
	*pdwDataSize=dwSizeOfData;
	return pData;
}

void CASMWriter::TraverseDecForPattern(DWORD dwBaseOffset, short pass, DWORD* dwPatternArrayCounter, DWORD* dwSizeOfUserTypePattern, CDeclaration* pDecMain)
{
}

void CASMWriter::FreeMachineBlock(void) noexcept
{
	// Clear machine code buffer
	m_machineCodeBuffer.FreeMachineBlock();

	// Reset reference tracker
	m_referenceTracker.Reset();
}

void CASMWriter::FreeAll(void)
{
	// Clear ASM Code Database (std::string auto-clears)
	for(DWORD i=0; i<ASMMAXCOUNT; i++)
	{
		m_ASMDebugStrings[i].clear();
	}
}

DWORD CASMWriter::GetBytePosOfLastInstruction(void)
{
	return m_machineCodeBuffer.GetBytePosOfLastInstruction();
}

DWORD CASMWriter::GetCurrentMCPosition(void)
{
	return m_machineCodeBuffer.GetCurrentMCPosition();
}

DWORD CASMWriter::DetermineASMCall(DWORD dwASMCodeAsAByte, DWORD dwTypeValue)
{
	return m_taskEmitter.DetermineASMCall(dwASMCodeAsAByte, dwTypeValue);
}

DWORD CASMWriter::DetermineASMCallForREL(DWORD dwASMCodeAsAByte, DWORD dwTypeValue)
{
	return m_taskEmitter.DetermineASMCallForREL(dwASMCodeAsAByte, dwTypeValue);
}

void CASMWriter::ClearPendingCallArgs() noexcept
{
	m_pendingCallArgs.clear();
	m_pendingCallSlotCount = 0;
}

void CASMWriter::RecordPendingCallArg(DWORD dwType, DWORD dwSlotCount)
{
	PendingCallArg arg;
	arg.slotIndex = m_pendingCallSlotCount;
	arg.slotCount = dwSlotCount > 0 ? dwSlotCount : 1u;
	// DBPro type codes (CStructTable::SetStructDefaults): 2 = float
	// (4 bytes), 8 = double float (8 bytes). Everything else - integers,
	// string handles, pointers - travels in the integer argument registers.
	if (dwType == 2)
		arg.kind = PendingCallArg::Kind::Float;
	else if (dwType == 8)
		arg.kind = PendingCallArg::Kind::Double;
	m_pendingCallArgs.push_back(arg);
	m_pendingCallSlotCount += arg.slotCount;
}

bool CASMWriter::EmitCommandCallAbiSetup()
{
	if (m_machineCodeBuffer.GetProgramStart() == nullptr)
		return false;
	CheckAndExpandMCBMemory();

	// Arguments were recorded in emission order (argument #N first), so the
	// last recorded argument is #1 and sits at the lowest machine-stack slot.
	const DWORD nArgs = static_cast<DWORD>(m_pendingCallArgs.size());

	// mov r12, rsp — R12 is callee-saved in the x64 ABI (every plugin
	// preserves it across the call) and this codegen never otherwise uses it
	// as a value register, so it is a safe scratch for the pre-call RSP.
	m_machineCodeBuffer.WriteByte(0x49);
	m_machineCodeBuffer.WriteByte(0x89);
	m_machineCodeBuffer.WriteByte(0xE4);

	// Reserve the 32-byte shadow space plus room for arguments 5+.
	const DWORD dwExtraStack = (nArgs > 4) ? 8u * (nArgs - 4u) : 0u;
	const DWORD dwReserve = 32u + dwExtraStack;
	if (dwReserve <= 127)
	{
		// sub rsp, imm8
		m_machineCodeBuffer.WriteByte(0x48);
		m_machineCodeBuffer.WriteByte(0x83);
		m_machineCodeBuffer.WriteByte(0xEC);
		m_machineCodeBuffer.WriteByte(static_cast<int>(dwReserve));
	}
	else
	{
		// sub rsp, imm32
		m_machineCodeBuffer.WriteByte(0x48);
		m_machineCodeBuffer.WriteByte(0x81);
		m_machineCodeBuffer.WriteByte(0xEC);
		m_machineCodeBuffer.WriteDWORD(dwReserve, 4);
	}
	// and rsp, -16 — guarantee 16-byte alignment at the call site.
	m_machineCodeBuffer.WriteByte(0x48);
	m_machineCodeBuffer.WriteByte(0x83);
	m_machineCodeBuffer.WriteByte(0xE4);
	m_machineCodeBuffer.WriteByte(0xF0);

	// Arguments 5+ must sit at [rsp+32+8i] for the callee. Their sources and
	// destinations never overlap, so copy from the deepest argument upward.
	// R12 points at the top of the pushed argument block, so an argument's
	// displacement from R12 is the bytes pushed after it, not before.
	const DWORD dwTotalSlots = m_pendingCallSlotCount;
	for (DWORD i = 5; i <= nArgs; i++)
	{
		const DWORD argIndex = nArgs - i;
		const PendingCallArg& arg = m_pendingCallArgs[argIndex];
		const int dispSource = static_cast<int>(static_cast<std::int8_t>(8 * static_cast<int>(dwTotalSlots - arg.slotIndex - arg.slotCount)));
		const int dispDest = static_cast<int>(static_cast<std::int8_t>(32 + 8 * static_cast<int>(i - 5)));
		// mov r11, [r12+disp8]
		m_machineCodeBuffer.WriteByte(0x4D);
		m_machineCodeBuffer.WriteByte(0x8B);
		m_machineCodeBuffer.WriteByte(0x5C);
		m_machineCodeBuffer.WriteByte(0x24);
		m_machineCodeBuffer.WriteByte(dispSource);
		// mov [rsp+disp8], r11
		m_machineCodeBuffer.WriteByte(0x4C);
		m_machineCodeBuffer.WriteByte(0x89);
		m_machineCodeBuffer.WriteByte(0x5C);
		m_machineCodeBuffer.WriteByte(0x24);
		m_machineCodeBuffer.WriteByte(dispDest);
	}

	// Marshal arguments 1-4 into their ABI registers.
	static constexpr std::uint8_t kRegLoad[4][3] = {
		{ 0x49, 0x8B, 0x4C },  // mov rcx, [r12+disp8]
		{ 0x49, 0x8B, 0x54 },  // mov rdx, [r12+disp8]
		{ 0x4D, 0x8B, 0x44 },  // mov r8,  [r12+disp8]
		{ 0x4D, 0x8B, 0x4C },  // mov r9,  [r12+disp8]
	};
	static constexpr std::uint8_t kXmmLow[4] = { 0xC0, 0xC8, 0xD0, 0xD8 };
	for (DWORD i = 1; i <= nArgs && i <= 4; i++)
	{
		const PendingCallArg& arg = m_pendingCallArgs[nArgs - i];
		// R12 points at the top of the pushed argument block; the argument's
		// distance from it is the slots pushed after it, not before.
		const int disp = static_cast<int>(static_cast<std::int8_t>(8 * static_cast<int>(dwTotalSlots - arg.slotIndex - arg.slotCount)));
		if (arg.kind == PendingCallArg::Kind::Integer)
		{
			const auto& enc = kRegLoad[i - 1];
			m_machineCodeBuffer.WriteByte(enc[0]);
			m_machineCodeBuffer.WriteByte(enc[1]);
			m_machineCodeBuffer.WriteByte(enc[2]);
			m_machineCodeBuffer.WriteByte(0x24);  // SIB: [r12+disp8]
			m_machineCodeBuffer.WriteByte(disp);
		}
		else
		{
			// mov rax, [r12+disp8] — full 64-bit load; a float's bits live in
			// the low 4 bytes, a double occupies the whole slot.
			m_machineCodeBuffer.WriteByte(0x49);
			m_machineCodeBuffer.WriteByte(0x8B);
			m_machineCodeBuffer.WriteByte(0x44);
			m_machineCodeBuffer.WriteByte(0x24);
			m_machineCodeBuffer.WriteByte(disp);
			// movd (float) / movq (double) xmmN, rax
			m_machineCodeBuffer.WriteByte(0x66);
			if (arg.kind == PendingCallArg::Kind::Double)
				m_machineCodeBuffer.WriteByte(0x48);  // REX.W for movq
			m_machineCodeBuffer.WriteByte(0x0F);
			m_machineCodeBuffer.WriteByte(0x6E);
			m_machineCodeBuffer.WriteByte(kXmmLow[i - 1]);
		}
	}

	return true;
}

void CASMWriter::EmitCommandCallAbiTeardown(bool bKeepArgsOnStack)
{
	if (m_machineCodeBuffer.GetProgramStart() == nullptr)
	{
		ClearPendingCallArgs();
		return;
	}
	CheckAndExpandMCBMemory();

	if (bKeepArgsOnStack)
	{
		// Debug hooks pop their own arguments after the call: restore RSP to
		// just above the pushed arguments (r12 - 8*slotCount).
		const int disp = static_cast<int>(static_cast<std::int8_t>(-8 * static_cast<int>(m_pendingCallSlotCount)));
		// lea rsp, [r12+disp8]
		m_machineCodeBuffer.WriteByte(0x49);
		m_machineCodeBuffer.WriteByte(0x8D);
		m_machineCodeBuffer.WriteByte(0x64);
		m_machineCodeBuffer.WriteByte(0x24);
		m_machineCodeBuffer.WriteByte(disp);
	}
	else
	{
		// mov rsp, r12 — discard shadow space; the caller pops the arguments
		// exactly as before the marshalling was introduced.
		m_machineCodeBuffer.WriteByte(0x4C);
		m_machineCodeBuffer.WriteByte(0x89);
		m_machineCodeBuffer.WriteByte(0xE4);
	}
	ClearPendingCallArgs();
}

bool CASMWriter::WriteASMCall(DWORD dwLine, LPCSTR pDLL, LPCSTR pDecoratedName)
{
	CStr CommandString("");
	CommandString.SetText("[");
	CommandString.AddText(pDLL);
	CommandString.AddText(",");
	CommandString.AddText(pDecoratedName);
	return g_pASMWriter->WriteASMTaskCoreP1(dwLine, static_cast<DWORD>(ASMTask::Call), &CommandString, 1);
}

DWORD CASMWriter::DetMode(CStr* pP, DWORD dwPType, DWORD dwPOffset)
{
	return m_taskEmitter.DetermineParamMode(pP, dwPType, dwPOffset);
}

void CASMWriter::CalculateArrayOffsetInRBX ( CStr* pPIndex )
{
	// Locate Array Element (RBX)
	if(pPIndex)
	{
		// If empty, it must use internal index (list system)
		if(pPIndex->Length()==0)
		{
			// Internal unified list index
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVRBXRAXOFF4), "-4");
		}
		else
		{
			/* moved to mathop to calc-array-offset
			// Always have first dimension D1
			WriteASMLine(static_cast<DWORD>(ASMOp::POPRBX), "");

			// Loop through subsequent dimensions D2-D9
			int iCount=0;
			int iCountMax = (DWORD)(pNumSubscriptsOnStack->GetValue())-1;
			while(iCount<iCountMax)
			{
				CStr* pValue = new CStr("");
				int iHeaderOffset = (-56)+(iCount*4);
				pValue->SetNumericText(iHeaderOffset);
				WriteASMLine(static_cast<DWORD>(ASMOp::POPRDX), "");
				WriteASMLine(static_cast<DWORD>(ASMOp::MULRDXRAXOFF4), pValue->GetStr());
				WriteASMLine(static_cast<DWORD>(ASMOp::ADDRBXRDX4), "");
				SAFE_DELETE(pValue);
				iCount++;
			}
			*/
			if(pPIndex->GetChar(0)=='@')
			{
				if(pPIndex->GetChar(1)==':')
				{
					DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVRBXRBP1),1);
					WriteASMLine(dwCorrectASMCode, (pPIndex->GetStr()+2));	
				}
				else
				{
					WriteASMLine(static_cast<DWORD>(ASMOp::MOVRBXMEM4), pPIndex->GetStr());
				}
			}
			else
			{
				// Normal specified index
				WriteASMLine(static_cast<DWORD>(ASMOp::MOVRBXIMM4), pPIndex->GetStr());
			}
		}

		// If Array Check Active
		if(GetArrayCheckFlag())
		{
			// Perform array bounds (for user subscripts) check and leap over
			WriteASMLine(static_cast<DWORD>(ASMOp::PUSHRDX), "");//added 300305 - to stop RDX overwritten as it can store DOUBLE compoent!
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVRDXRAXOFF4), "-16");
			WriteASMLine(static_cast<DWORD>(ASMOp::CMPRDXRBX), "");
			WriteASMLine(static_cast<DWORD>(ASMOp::POPRDX), "");//added 300305 - to stop RDX overwritten as it can store DOUBLE compoent!
			WriteASMLeapMarkerJump(static_cast<DWORD>(ASMOp::JGE), 2);
			WriteASMLine(static_cast<DWORD>(ASMOp::CMPRBX4), "-1");
			WriteASMLeapMarkerJump(static_cast<DWORD>(ASMOp::JLE), 3);
		}
	}
	else
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVRBXIMM4), "0");
}

void CASMWriter::WriteASMARRtoRAX(DWORD dwMode, CStr* pP, CStr* pOffset, DWORD dwPType, DWORD dwPOffset)
{
	m_taskEmitter.WriteASMARRtoRAX(this, dwMode, pP, pOffset, dwPType, dwPOffset);
}

void CASMWriter::WriteASMXtoRAX(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset)
{
	m_taskEmitter.WriteASMXtoRAX(this, dwMode, pP, pPIndex, dwPType, dwPOffset);
}

void CASMWriter::WriteASMRAXtoARR(DWORD dwMode, CStr* pP, CStr* pOffset, DWORD dwPType, DWORD dwPOffset)
{
	m_taskEmitter.WriteASMRAXtoARR(this, dwMode, pP, pOffset, dwPType, dwPOffset);
}

void CASMWriter::WriteASMRAXtoX(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset)
{
	m_taskEmitter.WriteASMRAXtoX(this, dwMode, pP, pPIndex, dwPType, dwPOffset);
}

bool CASMWriter::WriteASMTaskP1(DWORD dwLine, DWORD dwTask, CResultData* pP1Result)
{
	m_taskEmitter.IncrementTaskCount();
	DWORD dwP1Type = 0;
	CStr* pP1Str = nullptr;
	DWORD dwP1Offset = 0;
	CStr* pP1OffsetStr = nullptr;
	if(pP1Result)
	{
		dwP1Type = pP1Result->m_dwType;
		pP1Str = pP1Result->m_pStringToken.get();
		dwP1Offset = pP1Result->m_dwDataOffset;
		pP1OffsetStr = pP1Result->m_pAdditionalOffset.get();
	}
	return WriteASMTaskCore(dwLine, dwTask, pP1Str, pP1OffsetStr, dwP1Type, dwP1Offset, nullptr, nullptr, 0, 0);
}

bool CASMWriter::WriteASMTaskP2(DWORD dwLine, DWORD dwTask, CResultData* pP1Result, CResultData* pP2Result)
{
	DWORD dwP1Type = 0;
	CStr* pP1Str = nullptr;
	DWORD dwP1Offset = 0;
	CStr* pP1OffsetStr = nullptr;
	DWORD dwP2Type = 0;
	CStr* pP2Str = nullptr;
	DWORD dwP2Offset = 0;
	CStr* pP2OffsetStr = nullptr;
	if(pP1Result)
	{
		dwP1Type = pP1Result->m_dwType;
		pP1Str = pP1Result->m_pStringToken.get();
		dwP1Offset = pP1Result->m_dwDataOffset;
		pP1OffsetStr = pP1Result->m_pAdditionalOffset.get();
	}
	if(pP2Result)
	{
		dwP2Type = pP2Result->m_dwType;
		pP2Str = pP2Result->m_pStringToken.get();
		dwP2Offset = pP2Result->m_dwDataOffset;
		pP2OffsetStr = pP2Result->m_pAdditionalOffset.get();
	}
	return WriteASMTaskCore(dwLine, dwTask, pP1Str, pP1OffsetStr, dwP1Type, dwP1Offset, pP2Str, pP2OffsetStr, dwP2Type, dwP2Offset);
}
bool CASMWriter::WriteASMTaskP3(DWORD dwLine, DWORD dwTask, CResultData* pP1Result, CResultData* pP2Result, CResultData* pP3Result)
{
	DWORD dwP1Type = 0;
	CStr* pP1Str = nullptr;
	DWORD dwP1Offset = 0;
	CStr* pP1OffsetStr = nullptr;
	DWORD dwP2Type = 0;
	CStr* pP2Str = nullptr;
	DWORD dwP2Offset = 0;
	CStr* pP2OffsetStr = nullptr;
	DWORD dwP3Type = 0;
	CStr* pP3Str = nullptr;
	DWORD dwP3Offset = 0;
	CStr* pP3OffsetStr = nullptr;
	if(pP1Result)
	{
		dwP1Type = pP1Result->m_dwType;
		pP1Str = pP1Result->m_pStringToken.get();
		dwP1Offset = pP1Result->m_dwDataOffset;
		pP1OffsetStr = pP1Result->m_pAdditionalOffset.get();
	}
	if(pP2Result)
	{
		dwP2Type = pP2Result->m_dwType;
		pP2Str = pP2Result->m_pStringToken.get();
		dwP2Offset = pP2Result->m_dwDataOffset;
		pP2OffsetStr = pP2Result->m_pAdditionalOffset.get();
	}
	if(pP3Result)
	{
		dwP3Type = pP3Result->m_dwType;
		pP3Str = pP3Result->m_pStringToken.get();
		dwP3Offset = pP3Result->m_dwDataOffset;
		pP3OffsetStr = pP3Result->m_pAdditionalOffset.get();
	}
	return WriteASMTaskCore(dwLine, dwTask, pP1Str, pP1OffsetStr, dwP1Type, dwP1Offset, pP2Str, pP2OffsetStr, dwP2Type, dwP2Offset, pP3Str, pP3OffsetStr, dwP3Type, dwP3Offset);
}

bool CASMWriter::WriteASMTaskCoreP1(DWORD dwLine, DWORD dwTask, CStr* pP1, DWORD dwP1Type)
{
	return WriteASMTaskCore(dwLine, dwTask, pP1, nullptr, dwP1Type, 0, nullptr, nullptr, 0, 0);
}

bool CASMWriter::WriteASMTaskCoreP2(DWORD dwLine, DWORD dwTask, CStr* pP1, DWORD dwP1Type, CStr* pP2, DWORD dwP2Type)
{
	return WriteASMTaskCore(dwLine, dwTask, pP1, nullptr, dwP1Type, 0, pP2, nullptr, dwP2Type, 0);
}

bool CASMWriter::WriteASMTaskCore(DWORD dwLine, DWORD dwTask, CStr* pP1, CStr* pP1Off, DWORD dwP1Type, DWORD dwP1Offset, CStr* pP2, CStr* pP2Off, DWORD dwP2Type, DWORD dwP2Offset)
{
	return WriteASMTaskCore(dwLine, dwTask, pP1, pP1Off, dwP1Type, dwP1Offset, pP2, pP2Off, dwP2Type, dwP2Offset,nullptr,nullptr,0,0);
}

bool CASMWriter::WriteASMTaskCore(DWORD dwLine, DWORD dwTask,	CStr* pP1, CStr* pP1Off, DWORD dwP1Type, DWORD dwP1Offset,
																CStr* pP2, CStr* pP2Off, DWORD dwP2Type, DWORD dwP2Offset,
																CStr* pP3, CStr* pP3Off, DWORD dwP3Type, DWORD dwP3Offset )
{
	// Line zero is reserved for compiler-generated prologue code. Preserve it
	// as valid source metadata and never use it as an emission guard.
	m_dwLineNumber = dwLine;

	// Determine Modes
	DWORD dwP1Mode=DetMode(pP1, dwP1Type, dwP1Offset);
	DWORD dwP2Mode=DetMode(pP2, dwP2Type, dwP2Offset);
	DWORD dwP3Mode=DetMode(pP3, dwP3Type, dwP3Offset);

	// Command-call argument tracking: the machine stack only accumulates
	// command arguments across consecutive argument-producer tasks
	// (Push/PushAddress/PushUdt). Any other task consumes whatever sits on
	// the stack (array-index bookkeeping, nested calls, user-function jumps,
	// assignments), so the pending-argument list is cleared here to keep the
	// call-site ABI marshalling count exact.
	if (dwTask != static_cast<DWORD>(ASMTask::Push) &&
		dwTask != static_cast<DWORD>(ASMTask::PushAddress) &&
		dwTask != static_cast<DWORD>(ASMTask::PushUdt) &&
		dwTask != static_cast<DWORD>(ASMTask::Call))
	{
		ClearPendingCallArgs();
	}

	// Batches of ASM Ops to perform a single task
	if(dwTask==static_cast<DWORD>(ASMTask::AssignToRax))
	{
		WriteASMXtoRAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);
		WriteASMComment("ASSIGN X TO RAX", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::Assign))
	{
		if(pP2)
		{
//			bool bWrongTypes=false;//LEEFIX 041002-allow it for things like *add=a#
//			if(dwP2Type%100!=dwP1Type%100) bWrongTypes=true;
//			if(dwP1Type!=501 && dwP2Type!=501)//type501 is anytype
//			{
//				WriteASMComment("DIFFERENT ASSIGN TYPES IN X TO X", "", "", "");
//			}
//			else
//			{
				WriteASMXtoRAX(dwP2Mode, pP2, pP2Off, dwP2Type, dwP2Offset);
				WriteASMRAXtoX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);
				WriteASMComment("ASSIGN X TO X", "", "", "");
//			}
		}
		else
		{
			WriteASMRAXtoX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);
			WriteASMComment("ASSIGN RAX TO X", "", "", "");
		}
	}
	if(dwTask==static_cast<DWORD>(ASMTask::Push))
	{
		if(pP1)
		{
			if(_strnicmp(pP1->GetStr(),"fs@",3)==0)
			{
				WriteASMComment("PUSH TO STACK", "", "", "");
			}
		}
		WriteASMXtoRAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);
		WriteASMRAXtoX(static_cast<DWORD>(ParamMode::Stack), nullptr, nullptr, dwP1Type, dwP1Offset);
		// 8-byte stack values (double float / double integer) occupy two slots.
		const bool bDoubleWidth =
			(dwP1Type == 8 || dwP1Type == 9 || dwP1Type == 108 || dwP1Type == 109);
		RecordPendingCallArg(dwP1Type, bDoubleWidth ? 2u : 1u);
		WriteASMComment("PUSH TO STACK", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::PushAddress))
	{
		// leefix - include dataofsfet if any
		if ( dwP1Mode==static_cast<DWORD>(ParamMode::Mem) )
		{
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXIMM4), pP1->GetStr());
		}
		if ( dwP1Mode==static_cast<DWORD>(ParamMode::Rbp) )
		{
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXRBP), nullptr );
			WriteASMLine(static_cast<DWORD>(ASMOp::ADDRAX4), pP1->GetStr()+2 );
		}
		if ( dwP1Offset>0 )
		{
			CStr num("");
			num.SetNumericText(dwP1Offset);
			WriteASMLine(static_cast<DWORD>(ASMOp::ADDRAX4), num.GetStr());
		}
		WriteASMLine(static_cast<DWORD>(ASMOp::PUSHRAX), nullptr);
		RecordPendingCallArg(0, 1u);
		WriteASMComment("PUSH ADDRESS TO STACK", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::Call))
	{
		// Cut Full Param into DLL and COMMAND Strings#
		// GetLeft/RightOfPosition return new[] buffers; own them with unique_ptr<char[]>
		DWORD dwPos = pP1->FindFirstChar(',');
		std::unique_ptr<char[]> pDLLString(pP1->GetLeftOfPosition(dwPos));
		std::unique_ptr<char[]> pCommandString(pP1->GetRightOfPosition(dwPos));

		// Add To DLL&Command Table
		DWORD dwIndex = AddCommandToTable(pDLLString.get(), pCommandString.get());

		// Produce token Command Call token
		CStr tokenCommandStr("[");
		tokenCommandStr.AddNumericText(dwIndex);
		// x64 ABI marshalling: move the machine-stack arguments into
		// RCX/RDX/R8/R9 (+XMM0-3 for float/double), reserve the 32-byte
		// shadow space and align RSP to 16 bytes before the call.
		EmitCommandCallAbiSetup();
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVRBXIMM4), tokenCommandStr.GetStr());
		WriteASMLine(static_cast<DWORD>(ASMOp::CALLRBX), "");
		// Restore the pre-call stack pointer; the caller then pops the
		// arguments exactly as before.
		EmitCommandCallAbiTeardown(false);

		// Comment Details
		WriteASMComment("CALL", pDLLString.get(), pCommandString.get(), "");

	}
	if(dwTask==static_cast<DWORD>(ASMTask::PopRax))
	{
		WriteASMLine(static_cast<DWORD>(ASMOp::POPRAX), "");
		WriteASMComment("POP RAX FROM STACK", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::PopRbx))
	{
		WriteASMLine(static_cast<DWORD>(ASMOp::POPRBX), "");
		WriteASMComment("POP RBX FROM STACK", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::Unknown))
	{
		WriteASMLine(static_cast<DWORD>(ASMOp::UNKNOWN), "");
		WriteASMComment("NOT IMPLEMENTED", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::Condition))
	{
		if(pP1)
		{
			if(pP1->GetChar(0)=='@')
			{
				WriteASMXtoRAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);
				WriteASMLine(static_cast<DWORD>(ASMOp::CMPRAX4), "0");
			}
			else
			{
				// IMM
				if(pP1->GetValue()==0)
				{
					WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXIMM4), "0");
					WriteASMLine(static_cast<DWORD>(ASMOp::CMPRAX4), "0");
				}
				else
				{
					WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXIMM4), "1");
					WriteASMLine(static_cast<DWORD>(ASMOp::CMPRAX4), "0");
				}
			}
		}
		WriteASMComment("CONDITION COMPARE", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::ConditionData))
	{
		if(pP1)
		{
			if(pP1->GetChar(0)!='@')
			{
				// Create DWORD String for IMM
				DWORD dwDWORDRep=0;
				DWORD dwExtraDWORD=0;
				dwDWORDRep = pP1->GetDWORDRepresentation(dwP1Type, &dwExtraDWORD);
				CStr dword1Str("");
				dword1Str.SetDWORDNumericText(dwDWORDRep);			

				// IMM
				WriteASMLine(static_cast<DWORD>(ASMOp::CMPRAX4), dword1Str.GetStr());
			}
		}
		WriteASMComment("CONDITION COMPARE", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::CondJumpNE))
	{
		WriteASMLine(static_cast<DWORD>(ASMOp::JNE), pP1->GetStr());
		WriteASMComment("JUMP IF NOT EQUAL", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::CondJumpE))
	{
		WriteASMLine(static_cast<DWORD>(ASMOp::JE), pP1->GetStr());
		WriteASMComment("JUMP IF EQUAL", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::Jump))
	{
		WriteASMLine(static_cast<DWORD>(ASMOp::JMP), pP1->GetStr());
		WriteASMComment("DIRECT JUMP", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::JumpSubroutine))
	{
		WriteASMLine(static_cast<DWORD>(ASMOp::CALLMEM), pP1->GetStr());
		WriteASMComment("DIRECT SUBCALL", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::Return))
	{
		// Unscheduled RETs are dangerous=crash, so default is safe return
		if(1)
		{
			// Get RSP into RAX
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXRSP), "");
			// Get _RSP_ into RBX
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVRBXMEM4), "@$_RSP_");
			// Compare the values
			WriteASMLine(static_cast<DWORD>(ASMOp::CMPRAXRBX4), nullptr);
			// Jump over line that sets the flag
			WriteASMLine(static_cast<DWORD>(ASMOp::JNE), "5");
			// Line that exits the program due to illegal RETURN CALL
			WriteASMLine(static_cast<DWORD>(ASMOp::JMP), "$labelend");
			// oh and the actual return!
			WriteASMLine(static_cast<DWORD>(ASMOp::RET), "");
			// Comment for safe return
			WriteASMComment("SAFERETURN", "", "", "");
		}
		else
		{
			// Hard RET
			WriteASMLine(static_cast<DWORD>(ASMOp::RET), "");
			WriteASMComment("RETURN", "", "", "");
		}
	}
	if(dwTask==static_cast<DWORD>(ASMTask::PureReturn))
	{
		// Hard RET
		WriteASMLine(static_cast<DWORD>(ASMOp::RET), "");
		WriteASMComment("RETURN", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::AddRsp))
	{
		WriteASMLine(static_cast<DWORD>(ASMOp::ADDRSP), pP1->GetStr());
		WriteASMComment("ADD RSP", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::SubRsp))
	{
		WriteASMLine(static_cast<DWORD>(ASMOp::SUBRSP), pP1->GetStr());
		WriteASMComment("SUB RSP", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::StoreRsp))
	{
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVMEMRSP4), pP1->GetStr());
		WriteASMComment("STORE STACK IN MEM", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::RestoreRsp))
	{
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVRSPMEM4), pP1->GetStr());
		WriteASMComment("RESTORE STACK FROM MEM", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::PushRbp))
	{
		WriteASMLine(static_cast<DWORD>(ASMOp::PUSHRBP), "");
		WriteASMComment("PUSH RBP", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::PushRsp))
	{
		WriteASMLine(static_cast<DWORD>(ASMOp::PUSHRSP), "");
		WriteASMComment("PUSH RSP", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::PopRbp))
	{
		WriteASMLine(static_cast<DWORD>(ASMOp::POPRBP), "");
		WriteASMComment("POP RBP", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::MovRbpRsp))
	{
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVRBPRSP), "");
		WriteASMComment("MOV RBP RSP", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::MovRspRbp))
	{
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVRSPRBP), "");
		WriteASMComment("MOV RSP RBP", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::PushRegisters))
	{
		// x64 has no PUSHALL instruction: save the full Microsoft x64 ABI
		// callee-saved register set (RBX,RBP,RDI,RSI,R12-R15) individually.
		// Eight pushes keep RSP 16-byte aligned.
		WriteASMLine(static_cast<DWORD>(ASMOp::PUSHRBX), "");
		WriteASMLine(static_cast<DWORD>(ASMOp::PUSHRBP), "");
		WriteASMLine(static_cast<DWORD>(ASMOp::PUSHRDI), "");
		WriteASMLine(static_cast<DWORD>(ASMOp::PUSHRSI), "");
		WriteASMLine(static_cast<DWORD>(ASMOp::PUSHR12), "");
		WriteASMLine(static_cast<DWORD>(ASMOp::PUSHR13), "");
		WriteASMLine(static_cast<DWORD>(ASMOp::PUSHR14), "");
		WriteASMLine(static_cast<DWORD>(ASMOp::PUSHR15), "");
		WriteASMComment("PUSH CALLEE-SAVED REGISTERS", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::PopRegisters))
	{
		// Restore the callee-saved set in reverse order (x64 ABI).
		WriteASMLine(static_cast<DWORD>(ASMOp::POPR15), "");
		WriteASMLine(static_cast<DWORD>(ASMOp::POPR14), "");
		WriteASMLine(static_cast<DWORD>(ASMOp::POPR13), "");
		WriteASMLine(static_cast<DWORD>(ASMOp::POPR12), "");
		WriteASMLine(static_cast<DWORD>(ASMOp::POPRSI), "");
		WriteASMLine(static_cast<DWORD>(ASMOp::POPRDI), "");
		WriteASMLine(static_cast<DWORD>(ASMOp::POPRBP), "");
		WriteASMLine(static_cast<DWORD>(ASMOp::POPRBX), "");
		WriteASMComment("POP CALLEE-SAVED REGISTERS", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::ClearStack))
	{
		// Zero the local-variable stack area with native x64 string
		// instructions: REP STOSQ for full qwords, REP STOSB for the tail.
		DWORD dwTotalToClear = pP1->GetDWORDRepresentation(1, nullptr);
		DWORD dwQWORDSteps = dwTotalToClear/8;
		DWORD dwBytesLeft = dwTotalToClear-(dwQWORDSteps*8);
		if(dwQWORDSteps>0)
		{
			CStr iterations;
			iterations.SetNumericText(dwQWORDSteps);

			// SET RDI base (RSP) and RCX max
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVRDIRSP), "");
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVRCXIMM4), iterations.GetStr());

			// REP STOSQ zeroes [RDI], RCX qwords
			WriteASMLine(static_cast<DWORD>(ASMOp::REPSTOSQ), "");
		}
		if(dwBytesLeft>0)
		{
			// Advance RDI base to skip zeroed qword area
			CStr advance;
			advance.SetNumericText(dwQWORDSteps*8);
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVRDIRSP), "");
			WriteASMLine(static_cast<DWORD>(ASMOp::ADDRDIIMM), advance.GetStr());

			CStr iterations;
			iterations.SetNumericText(dwBytesLeft);

			// SET RCX max
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVRCXIMM4), iterations.GetStr());

			// REP STOSB zeroes the remaining tail bytes
			WriteASMLine(static_cast<DWORD>(ASMOp::REPSTOSB), "");
		}

		// Comment
		WriteASMComment("CLEAR STACK (REP STOS)", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::SetNoReturnIfRspLeak))
	{
		// Get RSP into RAX
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXRSP), "");

		// Get _RSP_ into RBX
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVRBXMEM4), pP1->GetStr());

		// Compare the values
		WriteASMLine(static_cast<DWORD>(ASMOp::CMPRAXRBX4), nullptr);

		// Jump over line that sets the flag
		WriteASMLine(static_cast<DWORD>(ASMOp::JE), "10");

		// Line that sets the flag to say 'no return'
		WriteASMLine2(static_cast<DWORD>(ASMOp::MOVMEMIMM4), "@$_ESC_", "3");

		// Comment
		WriteASMComment("FLAG NORETURN IF RSP<>STOREDRSP", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::DebugStatementHook)
	|| dwTask==static_cast<DWORD>(ASMTask::DebugJumpHook)
	|| dwTask==static_cast<DWORD>(ASMTask::DebugReturnHook))
	{
		if(dwTask!=static_cast<DWORD>(ASMTask::DebugReturnHook))
		{
			// Push Four Debug Items to Stack
			DWORD iProgID = 0;
			DWORD iLineNum = dwLine;
			DWORD iStartChar = dwP1Type;
			DWORD iEndChar = dwP2Type;
			CStr progStr("");
			CStr lineStr("");
			CStr startStr("");
			CStr endStr("");
			progStr.SetDWORDNumericText(iProgID);
			lineStr.SetDWORDNumericText(iLineNum);
			startStr.SetDWORDNumericText(iStartChar);
			endStr.SetDWORDNumericText(iEndChar);
			WriteASMLine1IMM(static_cast<DWORD>(ASMOp::PUSHIMM4), endStr.GetStr(), 2);
			WriteASMLine1IMM(static_cast<DWORD>(ASMOp::PUSHIMM4), startStr.GetStr(), 2);
			WriteASMLine1IMM(static_cast<DWORD>(ASMOp::PUSHIMM4), lineStr.GetStr(), 2);
			WriteASMLine1IMM(static_cast<DWORD>(ASMOp::PUSHIMM4), progStr.GetStr(), 2);
		}

		// Produce token Command Call token
		if(dwTask==static_cast<DWORD>(ASMTask::DebugStatementHook)) WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXIMM4), "[1");
		if(dwTask==static_cast<DWORD>(ASMTask::DebugJumpHook)) WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXIMM4), "[2");
		if(dwTask==static_cast<DWORD>(ASMTask::DebugReturnHook)) WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXIMM4), "[3");
		WriteASMLine(static_cast<DWORD>(ASMOp::CALLRAX), "");

		// Free stack items
		if(dwTask!=static_cast<DWORD>(ASMTask::DebugReturnHook))
		{
			WriteASMLine(static_cast<DWORD>(ASMOp::POPRBX), "");
			WriteASMLine(static_cast<DWORD>(ASMOp::POPRBX), "");
			WriteASMLine(static_cast<DWORD>(ASMOp::POPRBX), "");
			WriteASMLine(static_cast<DWORD>(ASMOp::POPRBX), "");
		}

		// Comment Details
		WriteASMComment("CALL", "Debug Hook", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::RuntimeErrorHook))
	{
		// Move Line Number to register for RTE trace
		CStr lineStr("");
		DWORD dwNeverZero = dwLine;
		if ( dwLine>0 )
		{
			lineStr.SetNumericText(dwNeverZero);
			WriteASMLine2(static_cast<DWORD>(ASMOp::MOVMEMIMM4), "@$_SLN_", lineStr.GetStr());
		}
		else
		{
			// do NOT erase _SLN_ value with zero (can use last valid one)
			// but STILL NEED to fill in an instruction here to preserve jump distances
			WriteASMLine2(static_cast<DWORD>(ASMOp::MOVMEMIMM4), "@$_ESC_", "1");
		}

		// Debug Mode requires break, not quit..
		if(g_DebugInfo.DebugModeOn())
		{
			// If runtime error DWORD is not zero, error occurred
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXMEM4), "@$_ERR_");
			WriteASMLine(static_cast<DWORD>(ASMOp::CMPRAX4), "0");
			WriteASMLine(static_cast<DWORD>(ASMOp::JE), "25");

			// Work out BREAK Position
			CStr data("");
			DWORD dwPosition=g_DebugInfo.GetLastBreakPoint();
			data.SetNumericText(dwPosition);
			WriteASMLine2(static_cast<DWORD>(ASMOp::MOVMEMIMM4), "@$_REK_", data.GetStr());

			// Set Escape value so Debugger is entered
			WriteASMLine2(static_cast<DWORD>(ASMOp::MOVMEMIMM4), "@$_ESC_", "1");

			// Jump To End of Program (it will skil quit because of breakvalue)
			WriteASMLine(static_cast<DWORD>(ASMOp::JMP), "$labelend");

			// Comment Details
			WriteASMComment("CALL", "Debug Runtime Error Hook", "", "");
		}
		else
		{
			// If runtime error DWORD is not zero, error occurred
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXMEM4), "@$_ERR_");
			WriteASMLine(static_cast<DWORD>(ASMOp::CMPRAX4), "0");
			WriteASMLine(static_cast<DWORD>(ASMOp::JNE), "$labelend");

			// Comment Details
			WriteASMComment("CALL", "Normal Runtime Error Hook", "", "");
		}
	}

	//
	// ASM SWITCHES
	//

	// If INC uses non-memory, must use ADD instead
	// Stack-owned IMM value (RAII); aliased into pP2 and consumed by the ASM section below
	CStr localStrForSwitch;
	if((dwTask==static_cast<DWORD>(ASMTask::IncVar) && dwP1Mode!=static_cast<DWORD>(ParamMode::Mem))
	|| (dwTask==static_cast<DWORD>(ASMTask::DecVar) && dwP1Mode!=static_cast<DWORD>(ParamMode::Mem)))
	{
		if(dwTask==static_cast<DWORD>(ASMTask::IncVar)) dwTask=static_cast<DWORD>(ASMTask::Add);
		if(dwTask==static_cast<DWORD>(ASMTask::DecVar)) dwTask=static_cast<DWORD>(ASMTask::Sub);

		// P2 becomes an IMM=1 value
		dwP2Mode=static_cast<DWORD>(ParamMode::Imm);
		localStrForSwitch.SetText("1");
		pP2=&localStrForSwitch;
		pP2Off=nullptr;
		dwP2Type=7;
		dwP2Offset=0;

		// P3 becomes result var
		dwP3Mode=dwP1Mode;
		pP3=pP1;
		pP3Off=pP1Off;
		dwP3Type=dwP1Type;
		dwP3Offset=dwP1Offset;
	}

	//
	// HARDCODED ASM COMMANDS
	//

	if(dwTask==static_cast<DWORD>(ASMTask::Add) || dwTask==static_cast<DWORD>(ASMTask::Sub)
	|| dwTask==static_cast<DWORD>(ASMTask::Mul) || dwTask==static_cast<DWORD>(ASMTask::Div) || dwTask==static_cast<DWORD>(ASMTask::Mod)
	|| dwTask==static_cast<DWORD>(ASMTask::And) || dwTask==static_cast<DWORD>(ASMTask::Or) || dwTask==static_cast<DWORD>(ASMTask::Not)
	|| dwTask==static_cast<DWORD>(ASMTask::Shr) || dwTask==static_cast<DWORD>(ASMTask::Shl) || dwTask==static_cast<DWORD>(ASMTask::Xor)
	|| dwTask==static_cast<DWORD>(ASMTask::BitNot))
	{
		// MATH Machine Code
		if(dwP1Type==3 || dwP1Type==8 || dwP1Type==9)
		{
			// cannot hard code strings and doubles (yet)
//			WriteASMComment("!CANNOT! HARDMATH none ints", "", "", "");
			g_pErrorReport->SetError(dwLine, ERR_SYNTAX+50);
			return false;
		}
		else
		{
			// Treat FLOAT bitwize & logic as DWORD
			if(dwP1Type==2) dwP1Type=7;

			if(dwP2Mode==static_cast<DWORD>(ParamMode::Imm))
			{
				// Ensure IMM is DWORD
				DWORD dwExtraDWORD=0;
				DWORD dwDWORDRep = pP2->GetDWORDRepresentation(dwP2Type, &dwExtraDWORD);
				pP2->SetNumericText(dwDWORDRep);

				// mov rax,[a]
				WriteASMXtoRAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);

				switch(dwTask)
				{
					case static_cast<DWORD>(ASMTask::Add):
					{
						// add rax,imm
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::ADDRAX1),dwP1Type);
						DWORD dwIMMSize=dwCorrectASMCode-static_cast<DWORD>(ASMOp::ADDRAX1);
						WriteASMLine2IMM(dwCorrectASMCode, nullptr, pP2->GetStr(), dwIMMSize);
					}
					break;

					case static_cast<DWORD>(ASMTask::Sub):
					{
						// sub rax,imm
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::SUBRAX1),dwP1Type);
						DWORD dwIMMSize=dwCorrectASMCode-static_cast<DWORD>(ASMOp::SUBRAX1);
						WriteASMLine2IMM(dwCorrectASMCode, nullptr, pP2->GetStr(), dwIMMSize);
					}
					break;

					case static_cast<DWORD>(ASMTask::Mul):
					{
						// mov rbx,imm + mul rax,rbx
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVRBXIMM1),dwP1Type);
						DWORD dwIMMSize=dwCorrectASMCode-static_cast<DWORD>(ASMOp::MOVRBXIMM1);
						WriteASMLine2IMM(dwCorrectASMCode, nullptr, pP2->GetStr(), dwIMMSize);
						dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MULRAXRBX1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");
					}
					break;

					case static_cast<DWORD>(ASMTask::Div):
					case static_cast<DWORD>(ASMTask::Mod):
					{
						// mov rbx,imm + div rax,rbx
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVRBXIMM1),dwP1Type);
						DWORD dwIMMSize=dwCorrectASMCode-static_cast<DWORD>(ASMOp::MOVRBXIMM1);
						WriteASMLine2IMM(dwCorrectASMCode, nullptr, pP2->GetStr(), dwIMMSize);

						// leefix - 250604 - u54 - avoid division by zero with RT error
						WriteASMLine(static_cast<DWORD>(ASMOp::CMPRBX4), "0");
						WriteASMLine(static_cast<DWORD>(ASMOp::JNE), "15");

						// runtime error if not leaped over
						WriteASMLine2(static_cast<DWORD>(ASMOp::MOVMEMIMM4), "@$_ERR_", "119");
						WriteASMLine(static_cast<DWORD>(ASMOp::JMP), "3");

						// actual division
						WriteASMLine(static_cast<DWORD>(ASMOp::CQO), "");
						dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::DIVRAXRBX1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");

						// mod takes only the remainder
						if(dwTask==static_cast<DWORD>(ASMTask::Mod))
						{
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVRAXRDX1),dwP1Type);
							WriteASMLine(dwCorrectASMCode, "");
						}
					}
					break;

					case static_cast<DWORD>(ASMTask::And):
					{
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::ANDRAX1),dwP1Type);
						DWORD dwIMMSize=dwCorrectASMCode-static_cast<DWORD>(ASMOp::ANDRAX1);
						WriteASMLine2IMM(dwCorrectASMCode, nullptr, pP2->GetStr(), dwIMMSize);
					}
					break;

					case static_cast<DWORD>(ASMTask::Or):
					{
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::ORRAX1),dwP1Type);
						DWORD dwIMMSize=dwCorrectASMCode-static_cast<DWORD>(ASMOp::ORRAX1);
						WriteASMLine2IMM(dwCorrectASMCode, nullptr, pP2->GetStr(), dwIMMSize);
					}
					break;

					case static_cast<DWORD>(ASMTask::Not):
					{
						// NOT is a unary boolean operator. Emit a self-contained
						// logical normalization so compound IF/loop conditions work:
						//   CMP RAX, 0     ; ZF=1 when operand is logically false
						//   MOV RAX, 0     ; clear result, preserving flags
						//   SETE AL        ; AL=1 when operand==0, else 0
						WriteASMLine(static_cast<DWORD>(ASMOp::CMPRAX4), "0");
						WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXIMM4), "0");
						WriteASMLine(static_cast<DWORD>(ASMOp::SETE), "");
					}
					break;

					case static_cast<DWORD>(ASMTask::BitNot):
					{
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::NOTRAX1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");
					}
					break;


					case static_cast<DWORD>(ASMTask::Xor):
					{
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::XORRAX1),dwP1Type);
						DWORD dwIMMSize=dwCorrectASMCode-static_cast<DWORD>(ASMOp::XORRAX1);
						WriteASMLine2IMM(dwCorrectASMCode, nullptr, pP2->GetStr(), dwIMMSize);
					}
					break;

					case static_cast<DWORD>(ASMTask::Shl):
					{
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::SHLRAX1),dwP1Type);
						DWORD dwIMMSize=0;//can only be IMM8
						WriteASMLine2IMM(dwCorrectASMCode, nullptr, pP2->GetStr(), dwIMMSize);
					}
					break;

					case static_cast<DWORD>(ASMTask::Shr):
					{
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::SHRRAX1),dwP1Type);
						DWORD dwIMMSize=0;//can only be IMM8
						WriteASMLine2IMM(dwCorrectASMCode, nullptr, pP2->GetStr(), dwIMMSize);
					}
					break;
				}

				// mov [r],rax
				WriteASMRAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);
			}
			else
			{
				// Where DIV/MOD task, all 4bytes of RBX are CMP'd so clear RAX now
				if(dwTask==static_cast<DWORD>(ASMTask::Div) || dwTask==static_cast<DWORD>(ASMTask::Mod))
				{
					// and only if sub-4byte RAX op used
					if(dwP2Type==4 || dwP2Type==5 || dwP2Type==6)//bool,byte,word only
					{
						WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXIMM4), "0");
					}
				}

				// mov rax,[b]
				WriteASMXtoRAX(dwP2Mode, pP2, pP2Off, dwP2Type, dwP2Offset);

				// mov [b] to stack
				WriteASMLine(static_cast<DWORD>(ASMOp::PUSHRAX), "");

				// mov rax,[a]
				WriteASMXtoRAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);

				// put [b] into RBX from stack
				WriteASMLine(static_cast<DWORD>(ASMOp::POPRBX), "");

				switch(dwTask)
				{
					case static_cast<DWORD>(ASMTask::Add):
					{
						// add rax,rbx
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::ADDRAXRBX1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");
					}
					break;

					case static_cast<DWORD>(ASMTask::Sub):
					{
						// sub rax,rbx
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::SUBRAXRBX1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");
					}
					break;

					case static_cast<DWORD>(ASMTask::Mul):
					{
						// mul rax,rbx
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MULRAXRBX1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");
					}
					break;

					case static_cast<DWORD>(ASMTask::Div):
					case static_cast<DWORD>(ASMTask::Mod):
					{
						// avoid divide by zero
// leefix - 350604 - old way was silent skip, new way is runtime error
//						WriteASMLine(static_cast<DWORD>(ASMOp::CMPRBX4), "0");
//						WriteASMLine(static_cast<DWORD>(ASMOp::JE), "3");
						WriteASMLine(static_cast<DWORD>(ASMOp::CMPRBX4), "0");
						WriteASMLine(static_cast<DWORD>(ASMOp::JNE), "15");

						// runtime error if not leaped over
						WriteASMLine2(static_cast<DWORD>(ASMOp::MOVMEMIMM4), "@$_ERR_", "119");
						WriteASMLine(static_cast<DWORD>(ASMOp::JMP), "3");
						
						// div rax,rbx
						WriteASMLine(static_cast<DWORD>(ASMOp::CQO), "");
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::DIVRAXRBX1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");

						// mod takes only the remainder
						if(dwTask==static_cast<DWORD>(ASMTask::Mod))
						{
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVRAXRDX1),dwP1Type);
							WriteASMLine(dwCorrectASMCode, "");
						}
					}
					break;

					case static_cast<DWORD>(ASMTask::And):
					{
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::ANDRAXRBX1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");
					}
					break;

					case static_cast<DWORD>(ASMTask::Or):
					{
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::ORRAXRBX1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");
					}
					break;

					case static_cast<DWORD>(ASMTask::Not):
					{
						// Same value-local normalization as the P2-immediate path:
						// CMP RAX,0 / MOV RAX,0 / SETE AL.
						WriteASMLine(static_cast<DWORD>(ASMOp::CMPRAX4), "0");
						WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXIMM4), "0");
						WriteASMLine(static_cast<DWORD>(ASMOp::SETE), "");
					}
					break;

					case static_cast<DWORD>(ASMTask::BitNot):
					{
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::NOTRAX1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");
					}
					break;

					case static_cast<DWORD>(ASMTask::Xor):
					{
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::XORRAXRBX1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");
					}
					break;

					case static_cast<DWORD>(ASMTask::Shl):
					{
						// mov rbx to CL (rcx byte part)
						WriteASMLine(static_cast<DWORD>(ASMOp::MOVRCXRBX4), "");

						// do shift 0-31 limit
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::SHLRAXCLC1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");
					}
					break;

					case static_cast<DWORD>(ASMTask::Shr):
					{
						// mov rbx to CL (rcx byte part)
						WriteASMLine(static_cast<DWORD>(ASMOp::MOVRCXRBX4), "");

						// do shift 0-31 limit
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::SHRRAXCLC1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");
					}
					break;
				}

				// mov [r],rax
				WriteASMRAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);
			}

			// Comment
			if(dwTask==static_cast<DWORD>(ASMTask::Add)) WriteASMComment("ADD", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::Sub)) WriteASMComment("SUB", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::Mul)) WriteASMComment("MUL", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::Div)) WriteASMComment("DIV", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::Mod)) WriteASMComment("MOD", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::And)) WriteASMComment("AND", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::Or))  WriteASMComment("OR", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::Not)) WriteASMComment("NOT", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::Xor)) WriteASMComment("XOR", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::BitNot)) WriteASMComment("BIT NOT", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::Shl)) WriteASMComment("SHL", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::Shr)) WriteASMComment("SHR", "", "", "");
		}
	}

	if(dwTask==static_cast<DWORD>(ASMTask::Equal) || dwTask==static_cast<DWORD>(ASMTask::NotEqual)
	|| dwTask==static_cast<DWORD>(ASMTask::Greater) || dwTask==static_cast<DWORD>(ASMTask::GreaterEqual)
	|| dwTask==static_cast<DWORD>(ASMTask::Less) || dwTask==static_cast<DWORD>(ASMTask::LessEqual) )
	{
		// COMPARE Machine Code
		if(dwP1Type==3 || dwP1Type==8 || dwP1Type==9)
		{
			// cannot hard code floats and doubles (yet)
//			WriteASMComment("!CANNOT! HARDCOMPARE none ints", "", "", "");
			g_pErrorReport->SetError(dwLine, ERR_SYNTAX+50);
			return false;
		}
		else
		{
			// Treat FLOAT bitwize & logic as DWORD
			if(dwP1Type==2) dwP1Type=7;

			if(dwP2Mode==static_cast<DWORD>(ParamMode::Imm))
			{
				// Ensure IMM is DWORD
				DWORD dwExtraDWORD=0;
				DWORD dwDWORDRep = pP2->GetDWORDRepresentation(dwP2Type, &dwExtraDWORD);
				pP2->SetNumericText(dwDWORDRep);

				// mov rax,imm
				DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVRAXIMM1),dwP1Type);
				DWORD dwIMMSize=dwCorrectASMCode-static_cast<DWORD>(ASMOp::MOVRAXIMM1);
				WriteASMLine2IMM(dwCorrectASMCode, nullptr, pP2->GetStr(), dwIMMSize);
			}
			else
			{
				// mov rax,[b]
				WriteASMXtoRAX(dwP2Mode, pP2, pP2Off, dwP2Type, dwP2Offset);
			}

			// push [b] to stack
			WriteASMLine(static_cast<DWORD>(ASMOp::PUSHRAX), "");

			// mov rax,[a]
			WriteASMXtoRAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);

			// mov rbx,rax
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVRBXRAX4), "");

			// pop [b] from stack to RDX
			WriteASMLine(static_cast<DWORD>(ASMOp::POPRDX), "");

			// cmp and setcc
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXIMM4), "0");
			DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::CMPRDXRBX1),dwP1Type);
			WriteASMLine(dwCorrectASMCode, "");
			switch(dwTask)
			{
				case static_cast<DWORD>(ASMTask::Equal): WriteASMLine(static_cast<DWORD>(ASMOp::SETE), ""); break;
				case static_cast<DWORD>(ASMTask::NotEqual): WriteASMLine(static_cast<DWORD>(ASMOp::SETNE), ""); break;
				case static_cast<DWORD>(ASMTask::Greater): WriteASMLine(static_cast<DWORD>(ASMOp::SETG), ""); break;
				case static_cast<DWORD>(ASMTask::GreaterEqual): WriteASMLine(static_cast<DWORD>(ASMOp::SETGE), ""); break;
				case static_cast<DWORD>(ASMTask::Less): WriteASMLine(static_cast<DWORD>(ASMOp::SETL), ""); break;
				case static_cast<DWORD>(ASMTask::LessEqual): WriteASMLine(static_cast<DWORD>(ASMOp::SETLE), ""); break;
			}

			// mov [r],rax
			WriteASMRAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);
		}

		// Comment
		if(dwTask==static_cast<DWORD>(ASMTask::Equal)) WriteASMComment("EQUAL", "", "", "");
		if(dwTask==static_cast<DWORD>(ASMTask::NotEqual)) WriteASMComment("NOTEQUAL", "", "", "");
		if(dwTask==static_cast<DWORD>(ASMTask::Greater)) WriteASMComment("GREATER", "", "", "");
		if(dwTask==static_cast<DWORD>(ASMTask::GreaterEqual)) WriteASMComment("GREATEREQUAL", "", "", "");
		if(dwTask==static_cast<DWORD>(ASMTask::Less)) WriteASMComment("LESS", "", "", "");
		if(dwTask==static_cast<DWORD>(ASMTask::LessEqual)) WriteASMComment("LESSEQUAL", "", "", "");
	}

	if(dwTask==static_cast<DWORD>(ASMTask::IncVar))
	{
		// INC Machine Code
		if(dwP1Type==3 || dwP1Type==8 || dwP1Type==9)
		{
//			WriteASMComment("!CANNOT! INC VAR", "", "", "");
			g_pErrorReport->SetError(dwLine, ERR_SYNTAX+51);
			return false;
		}
		else
		{
			// Treat FLOAT as DWORD (replace with float ADD)
			if(dwP1Type==2) dwP1Type=7;

			if(dwP1Mode==static_cast<DWORD>(ParamMode::Mem))
			{
				DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::INCMEM1),dwP1Type);
				WriteASMLine(dwCorrectASMCode, pP1->GetStr());
			}
			WriteASMComment("INC VAR", "", "", "");
		}
	}
	if(dwTask==static_cast<DWORD>(ASMTask::DecVar))
	{
		if(dwP1Type==3 || dwP1Type==8 || dwP1Type==9)
		{
//			WriteASMComment("!CANNOT! DEC VAR", "", "", "");
			g_pErrorReport->SetError(dwLine, ERR_SYNTAX+51);
			return false;
		}
		else
		{
			// Treat FLOAT as DWORD (replace with float SUB)
			if(dwP1Type==2) dwP1Type=7;

			// DEC Machine Code
			if(dwP1Mode==static_cast<DWORD>(ParamMode::Mem))
			{
				DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::DECMEM1),dwP1Type);
				WriteASMLine(dwCorrectASMCode, pP1->GetStr());
			}
			WriteASMComment("DEC VAR", "", "", "");
		}
	}
	if(dwTask==static_cast<DWORD>(ASMTask::BreakpointResume))
	{
		// Check if breakpoint active
		WriteASMCheckBreakPointVar();

		// LEAP-FORWARDS Marker OpCode to skip breakpoint-resume
		WriteASMLeapMarkerJump(static_cast<DWORD>(ASMOp::JE), 0);

//		Cannot Resume in deep nest of stack - new m/c means stack data useless
//		// Restore stack as it was before we left the program
//		WriteASMCall(m_dwLineNumber, "dbprocore.dll", "?StackSnapshotRestore@@YAKXZ");
	
//		// When return, RAX holds the stack-ptr-adjustment value
//		WriteASMLine(static_cast<DWORD>(ASMOp::SUBRSPRAX), nullptr);

//		// Reload registers with all values as it was before we left
//		WriteASMLine(static_cast<DWORD>(ASMOp::POPRBX), "");

		// Jump to breakpoint
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVRBXMEM4), "@$_REK_");
		WriteASMLine2(static_cast<DWORD>(ASMOp::MOVMEMIMM4), "@$_REK_", "0");
		WriteASMLine(static_cast<DWORD>(ASMOp::JMPRBX), 0);

		// Complete LEAP-FORWARD Marker (jump here if skip breakpoint resume)
		g_pASMWriter->WriteASMLeapMarkerEnd(0);

		// Comment on this task
		WriteASMComment("BREAKPOINT RESUME", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::PushInternalArrayIndex))
	{
		// Find Array location (in RAX)
		if(dwP1Mode==static_cast<DWORD>(ParamMode::MemArr)) WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXMEM4), pP1->GetStr());
		if(dwP1Mode==static_cast<DWORD>(ParamMode::RbpArr)) WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXRBP4), pP1->GetStr()+2);

		/* old code crashes when safe array switched off and A() used instead of A( ) when A was not DIMMED
		// If Array Check Active
		if(GetArrayCheckFlag())
		{
			// Make Sure Array Exists
			WriteASMLine(static_cast<DWORD>(ASMOp::CMPRAX4), "0");

			// Leap Marker OpCode
			WriteASMLeapMarkerJump(static_cast<DWORD>(ASMOp::JE), 4);
		}

		// Get array index from -4 location within array header - put in RDX
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVRDXRAXOFF4), "-4");

		// Copy Push RDX to stack
		WriteASMLine(static_cast<DWORD>(ASMOp::PUSHRDX), "");

		// If Array Check Active
		if(GetArrayCheckFlag())
		{
			// Complete Leap Marker (so we jump here)
			WriteASMLeapMarkerEnd(4);
		}
		*/

		// Force a check of the array as we NEED it to find the array index (safe arrays or no)
		WriteASMLine(static_cast<DWORD>(ASMOp::CMPRAX4), "0");
		WriteASMLeapMarkerJump(static_cast<DWORD>(ASMOp::JE), 4);
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVRDXRAXOFF4), "-4");
		WriteASMLine(static_cast<DWORD>(ASMOp::PUSHRDX), "");
		WriteASMLeapMarkerEnd(4);

		// Comment on this task
		WriteASMComment("PUSH INT. ARRAY INDEX TO STACK", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::CalcArrayOffset))
	{
		// Find the array header in RAX.
		if(dwP2Mode==static_cast<DWORD>(ParamMode::MemArr))
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXMEM4), pP2->GetStr());
		if(dwP2Mode==static_cast<DWORD>(ParamMode::RbpArr))
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXRBP4), pP2->GetStr()+2);

		if(GetArrayCheckFlag())
		{
			WriteASMLine(static_cast<DWORD>(ASMOp::CMPRAX4), "0");
			WriteASMLeapMarkerJump(static_cast<DWORD>(ASMOp::JE), 4);
		}

		// The first subscript is the initial linear index. Each subsequent
		// subscript is multiplied by its dimension stride from the array header.
		WriteASMLine(static_cast<DWORD>(ASMOp::POPRBX), "");
		const auto dimensionCount = static_cast<int>(dwP1Offset);
		for(int dimension = 0; dimension < dimensionCount - 1; ++dimension)
		{
			CStr headerOffset;
			headerOffset.SetNumericText(-56 + (dimension * 4));
			WriteASMLine(static_cast<DWORD>(ASMOp::POPRDX), "");
			WriteASMLine(
				static_cast<DWORD>(ASMOp::MULRDXRAXOFF4),
				headerOffset.GetStr());
			WriteASMLine(static_cast<DWORD>(ASMOp::ADDRBXRDX4), "");
		}

		WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXRBX4), "");
		WriteASMRAXtoX(dwP1Mode, pP1, nullptr, 7, 0);

		if(GetArrayCheckFlag())
		{
			WriteASMLeapMarkerEnd(4);
		}
		WriteASMComment("CALCULATE ARRAY OFFSET", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::PushUdt))
	{
		// UDT address to RAX

// did not account for pasing UCTs from with user functions
//		WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXIMM4), pP1->GetStr());
		if ( dwP1Mode==static_cast<DWORD>(ParamMode::Mem) )
		{
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXIMM4), pP1->GetStr());
		}
		if ( dwP1Mode==static_cast<DWORD>(ParamMode::Rbp) )
		{
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXRBP), nullptr );
			WriteASMLine(static_cast<DWORD>(ASMOp::ADDRAX4), pP1->GetStr()+2 );
		}
		if ( dwP1Offset>0 )
		{
			CStr num;
			num.SetNumericText(dwP1Offset);
			WriteASMLine(static_cast<DWORD>(ASMOp::ADDRAX4), num.GetStr());
		}

		// Advance RAX to end of UDT data (UDT Size)
		WriteASMLine(static_cast<DWORD>(ASMOp::ADDRAX4), pP2->GetStr());

		// advance to last data element
		for ( DWORD n=0; n<dwP2Offset; n++)
		{
			// decrement udtptr
			WriteASMLine(static_cast<DWORD>(ASMOp::SUBRAX4), "4");

			// push udtptr to stack
			WriteASMLine(static_cast<DWORD>(ASMOp::PUSHFROMRAX), nullptr);
		}

		// Record the UDT as a single integer-shaped argument spanning all of
		// its stack slots so later argument offsets stay accurate. (By-value
		// UDT command parameters are outside the conformance path.)
		RecordPendingCallArg(1001, dwP2Offset > 0 ? dwP2Offset : 1u);

		// Comment on this task
		WriteASMComment("PUSH UDT TO STACK", "", "", "");
	}

	return true;
}

bool CASMWriter::WriteASMLine(DWORD dwOp, LPCSTR pOpData)
{
	// DBM Code
	CStr strDBMLine(256);
	strDBMLine.SetNumericText(m_dwLineNumber);
	strDBMLine.AddText(" ");
	strDBMLine.AddText(m_ASMDebugStrings[dwOp].c_str());
	strDBMLine.AddText(" ");
	strDBMLine.AddText(pOpData);
	if(g_pDBMWriter->OutputDBM(&strDBMLine)==false) return false;

	// ASM Code
	if(m_bASMOpData[dwOp]==true)
		CreateASMMiddle(m_iASMPreOp[dwOp], m_iASMOp1[dwOp], m_iASMOp2[dwOp], pOpData, m_iASMOp3[dwOp]);
	else
		CreateASMMiddle(m_iASMPreOp[dwOp], m_iASMOp1[dwOp], m_iASMOp2[dwOp], "", m_iASMOp3[dwOp]);

	// Complete
	return true;
}

bool CASMWriter::WriteASMLine2(DWORD dwOp, LPCSTR pOpData, LPCSTR pOpData2)
{
	// DBM Code
	CStr strDBMLine(256);
	strDBMLine.SetNumericText(m_dwLineNumber);
	strDBMLine.AddText(" ");
	strDBMLine.AddText(m_ASMDebugStrings[dwOp].c_str());
	strDBMLine.AddText(" ");
	strDBMLine.AddText(pOpData);
	strDBMLine.AddText(", ");
	strDBMLine.AddText(pOpData2);
	if(g_pDBMWriter->OutputDBM(&strDBMLine)==false) return false;

	// ASM Code
	CreateASMMiddleCore(m_iASMPreOp[dwOp], m_iASMOp1[dwOp], m_iASMOp2[dwOp], pOpData, pOpData2, false, 0, m_iASMOp3[dwOp]);

	// Complete
	return true;
}

bool CASMWriter::WriteASMLine1IMM(DWORD dwOp, LPCSTR pOpData, DWORD dwSizeIMM)
{
	// DBM Code
	CStr strDBMLine(256);
	strDBMLine.SetNumericText(m_dwLineNumber);
	strDBMLine.AddText(" ");
	strDBMLine.AddText(m_ASMDebugStrings[dwOp].c_str());
	strDBMLine.AddText(" ");
	strDBMLine.AddText(pOpData);
	if(g_pDBMWriter->OutputDBM(&strDBMLine)==false) return false;

	// ASM Code
	CreateASMMiddleCore(m_iASMPreOp[dwOp], m_iASMOp1[dwOp], m_iASMOp2[dwOp], pOpData, nullptr, true, dwSizeIMM, m_iASMOp3[dwOp]);

	// Complete
	return true;
}

bool CASMWriter::WriteASMLine2IMM(DWORD dwOp, LPCSTR pOpData, LPCSTR pOpData2, DWORD dwSizeIMM)
{
	// DBM Code
	CStr strDBMLine(256);
	strDBMLine.SetNumericText(m_dwLineNumber);
	strDBMLine.AddText(" ");
	strDBMLine.AddText(m_ASMDebugStrings[dwOp].c_str());
	strDBMLine.AddText(" ");
	strDBMLine.AddText(pOpData);
	strDBMLine.AddText(", ");
	strDBMLine.AddText(pOpData2);
	if(g_pDBMWriter->OutputDBM(&strDBMLine)==false) return false;

	// ASM Code
	CreateASMMiddleCore(m_iASMPreOp[dwOp], m_iASMOp1[dwOp], m_iASMOp2[dwOp], pOpData, pOpData2, true, dwSizeIMM, m_iASMOp3[dwOp]);

	// Complete
	return true;
}

bool CASMWriter::WriteASMComment(LPCSTR pTitle, LPCSTR pC1, LPCSTR pC2, LPCSTR pC3)
{
	// Ensure Comments appear at same horiz position
	DWORD dwNumOfLineChars = g_pDBMWriter->EatCarriageReturn();
	DWORD dwAdvance = 30-dwNumOfLineChars;
	if(dwNumOfLineChars>30) dwAdvance=0;

	// DBM Code
	CStr strDBMLine("");
	for(DWORD n=0; n<dwAdvance; n++) strDBMLine.AddText(" ");
	strDBMLine.AddText("; ");
	strDBMLine.AddText(pTitle);
	strDBMLine.AddText(" ");
	if(pC1)
	{
		strDBMLine.AddText(pC1);
		strDBMLine.AddText(" ");
	}
	if(pC2)
	{
		strDBMLine.AddText(pC2);
		strDBMLine.AddText(" ");
	}
	if(pC3)
	{
		strDBMLine.AddText(pC3);
	}
	if(g_pDBMWriter->OutputDBM(&strDBMLine)==false) return false;

	// Complete
	return true;
}

bool CASMWriter::WriteASMLeapMarkerTop(void)
{
	return m_leapManager.WriteASMLeapMarkerTop(this);
}

bool CASMWriter::WriteASMLineLeapToTop(DWORD dwOp)
{
	return m_leapManager.WriteASMLineLeapToTop(dwOp, this);
}

bool CASMWriter::WriteASMLeapMarkerJumpToTop(void)
{
	return m_leapManager.WriteASMLeapMarkerJumpToTop(this);
}

bool CASMWriter::WriteASMLineLeap(DWORD dwOp, DWORD di)
{
	return m_leapManager.WriteASMLineLeap(dwOp, di, this);
}

bool CASMWriter::WriteASMLeapMarkerJump(DWORD dwOp, DWORD di)
{
	return m_leapManager.WriteASMLeapMarkerJump(dwOp, di, this);
}

bool CASMWriter::WriteASMLeapMarkerJumpNotEqual(DWORD di)
{
	return m_leapManager.WriteASMLeapMarkerJumpNotEqual(di, this);
}

bool CASMWriter::WriteASMLeapForwardMarker(void)
{
	return m_leapManager.WriteASMLeapForwardMarker(this);
}

bool CASMWriter::WriteASMLeapMarkerEnd(DWORD di)
{
	return m_leapManager.WriteASMLeapMarkerEnd(di, this);
}

bool CASMWriter::WriteASMCheckBreakPointVar(void)
{
	WriteASMLine(static_cast<DWORD>(ASMOp::MOVRAXMEM4), "@$_REK_");
	WriteASMLine(static_cast<DWORD>(ASMOp::CMPRAX4), "0");
	return true;
}

bool CASMWriter::WriteASMForceEscapeAtCodeBREAK(void)
{
	// Force an escape by setting ec to 1
	WriteASMLine2(static_cast<DWORD>(ASMOp::MOVMEMIMM4), "@$_ESC_", "1");

	// Complete
	return true;
}

void CASMWriter::SetBreakPointValue(void)
{
	WriteASMLine2(static_cast<DWORD>(ASMOp::MOVMEMIMM4), "@$_REK_", "1");
}

DWORD CASMWriter::AddCommandToTable(LPCSTR pDLLString, LPCSTR pCommandString)
{
	// Skip non-DLL commands
	if(pDLLString==nullptr) return g_pStatementList->GetDLLIndexCounter();
	if(strlen(pDLLString)<2) return g_pStatementList->GetDLLIndexCounter();

	// Record DLL As Actually Being Used
	DWORD dwIndex = g_pStatementList->GetDLLIndexCounter() + 1;
	if(g_pDLLTable->AddUniqueString(pDLLString+1, &dwIndex))
		g_pStatementList->IncDLLIndexCounter(1);

	// Record Command As Actually Being Used
	CStr rawCommandString;
	rawCommandString.SetNumericText(dwIndex);
	rawCommandString.AddText(",");
	rawCommandString.AddText(pCommandString+1);
	dwIndex = g_pStatementList->GetCommandIndexCounter() + 1;
	if(g_pCommandTable->AddUniqueString(rawCommandString.GetStr(), &dwIndex))
		g_pStatementList->IncCommandIndexCounter(1);

	return dwIndex;
}

