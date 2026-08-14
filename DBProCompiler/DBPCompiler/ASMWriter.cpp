// ASMWriter.cpp: implementation of the CASMWriter class.
//
//////////////////////////////////////////////////////////////////////

// Includes
#include "ParserHeader.h"
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
extern DWORD g_dwEscapeValueMem;
extern DWORD g_dwBreakOutPosition;
extern LPSTR g_pVarSpaceAddressInUse;
extern DWORD g_dwVarSpaceSizeInUse;
extern GDI_RetVoidParamVoidPFN g_CORE_SyncRefresh;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CASMWriter::CASMWriter()
{
	// Reference Tracking
	m_referenceTracker.Reset();

	// Work Variables
	m_dwLineNumber=0;

	// Reset ASM Code Database (structured descriptor table)
	m_asmoOpcodeDefs.resize(ASMMAXCOUNT);

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
	// Default ASM Codes for ASMWriting
	// Every entry declares its operand data encoding explicitly so the
	// emitter can produce x64-native slots (Abs64, ImmOrAddr, PtrIndirect).
DefineASM(static_cast<DWORD>(ASMOp::MOVEAXIMM1), "MOV EAX IMM1", -1, 0xB0+0, -1, DataEncoding::Imm8, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXIMM2), "MOV EAX IMM2", 0x66, 0xB8+0, -1, DataEncoding::Imm16, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXIMM4), "MOV EAX IMM4", -1, 0xB8+0, -1, DataEncoding::ImmOrAddr, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBXIMM1), "MOV EBX IMM1", -1, 0xB0+3, -1, DataEncoding::Imm8, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBXIMM2), "MOV EBX IMM2", 0x66, 0xB8+3, -1, DataEncoding::Imm16, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBXIMM4), "MOV EBX IMM4", -1, 0xB8+3, -1, DataEncoding::ImmOrAddr, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEDXIMM4), "MOV EDX IMM4", -1, 0xB8+2, -1, DataEncoding::ImmOrAddr, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXMEM1), "MOV EAX MEM1", -1, 0xA0, -1, DataEncoding::Abs64, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXMEM2), "MOV EAX MEM2", 0x66, 0xA1, -1, DataEncoding::Abs64, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXMEM4), "MOV EAX MEM4", -1, 0xA1, -1, DataEncoding::Abs64, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVMEMEAX1), "MOV MEM1 EAX", -1, 0xA2, -1, DataEncoding::Abs64, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVMEMEAX2), "MOV MEM2 EAX", 0x66, 0xA3, -1, DataEncoding::Abs64, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVMEMEAX4), "MOV MEM4 EAX", -1, 0xA3, -1, DataEncoding::Abs64, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVECXOFFEAX1), "MOV [ECX+A] EAX1", -1, 0x88, 0x81, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVECXOFFEAX2), "MOV [ECX+A] EAX2", 0x66, 0x89, 0x81, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVECXOFFEAX4), "MOV [ECX+A] EAX4", -1, 0x89, 0x81, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXOFFECX1), "MOV [EAX+A] ECX1", -1, 0x88, 0x88, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXOFFECX2), "MOV [EAX+A] ECX2", 0x66, 0x89, 0x88, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXOFFECX4), "MOV [EAX+A] ECX4", -1, 0x89, 0x88, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXECXREL1), "MOV [EAX] ECX1", -1, 0x88, 0x08, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXECXREL2), "MOV [EAX] ECX2", 0x66, 0x89, 0x08, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXECXREL4), "MOV [EAX] ECX4", -1, 0x89, 0x08, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXEAXREL1), "MOV EAX [EAX1]", -1, 0x8A, 0x00, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXEAXREL2), "MOV EAX [EAX2]", 0x66, 0x8B, 0x00, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXEAXREL4), "MOV EAX [EAX4]", -1, 0x8B, 0x00, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVECXEAXOFF1), "MOV ECX1 [EAX+A]", -1, 0x8A, 0x88, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVECXEAXOFF2), "MOV ECX2 [EAX+A]", 0x66, 0x8B, 0x88, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVECXEAXOFF4), "MOV ECX4 [EAX+A]", -1, 0x8B, 0x88, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXECXOFF1), "MOV EAX1 [ECX+A]", -1, 0x8A, 0x81, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXECXOFF2), "MOV EAX2 [ECX+A]", 0x66, 0x8B, 0x81, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXECXOFF4), "MOV EAX4 [ECX+A]", -1, 0x8B, 0x81, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEDXEAXOFF1), "MOV EDX1 [EAX+A]", -1, 0x8A, 0x90, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEDXEAXOFF2), "MOV EDX2 [EAX+A]", 0x66, 0x8B, 0x90, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEDXEAXOFF4), "MOV EDX4 [EAX+A]", -1, 0x8B, 0x90, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MULEDXEAXOFF4), "MUL EDX4 [EAX+A]", 0x0F, 0xAF, 0x90, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBXEAXOFF1), "MOV EBX1 [EAX+A]", -1, 0x8A, 0x98, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBXEAXOFF2), "MOV EBX2 [EAX+A]", 0x66, 0x8B, 0x98, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBXEAXOFF4), "MOV EBX4 [EAX+A]", -1, 0x8B, 0x98, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXEBP1), "MOV EAX1 [EBP+A]", -1, 0x8A, 0x85, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXEBP2), "MOV EAX2 [EBP+A]", 0x66, 0x8B, 0x85, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXEBP4), "MOV EAX4 [EBP+A]", -1, 0x8B, 0x85, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBXEBP1), "MOV EBX1 [EBP+A]", -1, 0x8A, 0x9D, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBXEBP2), "MOV EBX2 [EBP+A]", 0x66, 0x8B, 0x9D, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBXEBP4), "MOV EBX4 [EBP+A]", -1, 0x8B, 0x9D, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBPEAX1), "MOV [EBP+A] EAX1", -1, 0x88, 0x85, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBPEAX2), "MOV [EBP+A] EAX2", 0x66, 0x89, 0x85, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBPEAX4), "MOV [EBP+A] EAX4", -1, 0x89, 0x85, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEDXEAX4), "MOV EDX EAX", -1, 0x8B, 0xD0, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVECXIMM4), "MOV ECX IMM4", -1, 0xB8+1, -1, DataEncoding::ImmOrAddr, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVECXEAX4), "MOV ECX EAX", -1, 0x8B, 0xC8, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVECXEDX4), "MOV ECX EDX", -1, 0x8B, 0xCA, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEDXECX4), "MOV EDX ECX", -1, 0x8B, 0xD1, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXEBX4), "MOV EAX EBX", -1, 0x8B, 0xC3, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVECXEBX1), "MOV ECX EBX1", -1, 0x8A, 0xCB, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVECXEBX2), "MOV ECX EBX2", -1, 0x8B, 0xCB, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVECXEBX4), "MOV ECX EBX4", -1, 0x8B, 0xCB, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXECX1), "MOV EAX ECX1", -1, 0x8A, 0xC1, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXECX2), "MOV EAX ECX2", 0x66, 0x8B, 0xC1, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXECX4), "MOV EAX ECX4", -1, 0x8B, 0xC1, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXEDX1), "MOV EAX EDX1", -1, 0x8A, 0xC2, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXEDX2), "MOV EAX EDX2", 0x66, 0x8B, 0xC2, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXEDX4), "MOV EAX EDX4", -1, 0x8B, 0xC2, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBXEAX1), "MOV EBX EAX1", -1, 0x8A, 0xD8, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBXEAX2), "MOV EBX EAX2", 0x66, 0x8B, 0xD8, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBXEAX4), "MOV EBX EAX4", -1, 0x8B, 0xD8, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::ADDEAXEBX1), "ADD EAX EBX1", -1, 0x00, 0xD8, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::ADDEAXEBX2), "ADD EAX EBX2", 0x66, 0x01, 0xD8, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::ADDEAXEBX4), "ADD EAX EBX4", -1, 0x01, 0xD8, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::ADDEAXECX4), "ADD EAX ECX4", -1, 0x01, 0xC8, DataEncoding::None, DataEncoding::None);
	// 64-bit frame ops: ADD/SUB ESP address the full RSP, so both carry REX.W.
	DefineASM(static_cast<DWORD>(ASMOp::ADDESP), "ADD ESP", -1, 0x81, 0xC4, DataEncoding::Imm32, DataEncoding::None, OpcodeExpansion::RexW);
	DefineASM(static_cast<DWORD>(ASMOp::SUBESP), "SUB ESP", -1, 0x81, 0xEC, DataEncoding::Imm32, DataEncoding::None, OpcodeExpansion::RexW);
	DefineASM(static_cast<DWORD>(ASMOp::SUBESPEAX), "SUB ESP EAX", -1, 0x29, 0xC4, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHEBP), "PUSH EBP", -1, 0x55, -1, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::POPEBP), "POP EBP", -1, 0x5D, -1, DataEncoding::None, DataEncoding::None);
	// 64-bit frame base: REX.W so EBP/RSP are full-width (MOV RBP,RSP / MOV RSP,RBP).
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBPESP), "MOV EBP ESP", -1, 0x89, 0xE5, DataEncoding::None, DataEncoding::None, OpcodeExpansion::RexW);
	DefineASM(static_cast<DWORD>(ASMOp::MOVESPEBP), "MOV ESP EBP", -1, 0x89, 0xEC, DataEncoding::None, DataEncoding::None, OpcodeExpansion::RexW);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXESP), "MOV EAX ESP", -1, 0x89, 0xE0, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXEBP), "MOV EAX EBP", -1, 0x89, 0xE8, DataEncoding::None, DataEncoding::None);
	// Wave 5: full-width (REX.W) string-pointer moves. The REX.W prefix makes
	// the moffs forms 64-bit data moves (48 A1/A3) and the modrm forms 64-bit
	// operand moves (48 8B/89). Abs64 keeps the 8-byte moffs slot; Imm32 keeps
	// the 4-byte disp32 slot.
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXMEM8), "MOV RAX MEM8", -1, 0xA1, -1, DataEncoding::Abs64, DataEncoding::None, OpcodeExpansion::RexW);
	DefineASM(static_cast<DWORD>(ASMOp::MOVMEMEAX8), "MOV MEM8 RAX", -1, 0xA3, -1, DataEncoding::Abs64, DataEncoding::None, OpcodeExpansion::RexW);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXEBP8), "MOV RAX8 [EBP+A]", -1, 0x8B, 0x85, DataEncoding::Imm32, DataEncoding::None, OpcodeExpansion::RexW);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBPEAX8), "MOV [EBP+A] RAX8", -1, 0x89, 0x85, DataEncoding::Imm32, DataEncoding::None, OpcodeExpansion::RexW);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXECXOFF8), "MOV RAX8 [ECX+A]", -1, 0x8B, 0x81, DataEncoding::Imm32, DataEncoding::None, OpcodeExpansion::RexW);
	DefineASM(static_cast<DWORD>(ASMOp::MOVECXOFFEAX8), "MOV [ECX+A] RAX8", -1, 0x89, 0x81, DataEncoding::Imm32, DataEncoding::None, OpcodeExpansion::RexW);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXECXREL8), "MOV RAX8 [ECX]", -1, 0x8B, 0x08, DataEncoding::None, DataEncoding::None, OpcodeExpansion::RexW);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXEAXREL8), "MOV RAX8 [EAX]", -1, 0x8B, 0x00, DataEncoding::None, DataEncoding::None, OpcodeExpansion::RexW);
	DefineASM(static_cast<DWORD>(ASMOp::MOVECXEAX8), "MOV RCX RAX", -1, 0x8B, 0xC8, DataEncoding::None, DataEncoding::None, OpcodeExpansion::RexW);
	// Wave 6: runtime array ABI — 8-byte ref table (SIB ×8) and QWORD
	// string-array element access (REX.W on the modrm/SIB forms).
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXSIB8), "MOV RAX SIB[RAX:RBX*8]", 0x8B, 0x04, 0xD8, DataEncoding::None, DataEncoding::None, OpcodeExpansion::RexW);
	DefineASM(static_cast<DWORD>(ASMOp::MOVECXEAXOFF8), "MOV RCX8 [RAX+A]", -1, 0x8B, 0x88, DataEncoding::Imm32, DataEncoding::None, OpcodeExpansion::RexW);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXECX8), "MOV RAX RCX8", -1, 0x8B, 0xC1, DataEncoding::None, DataEncoding::None, OpcodeExpansion::RexW);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXOFFECX8), "MOV [RAX+A] RCX8", -1, 0x89, 0x88, DataEncoding::Imm32, DataEncoding::None, OpcodeExpansion::RexW);
	DefineASM(static_cast<DWORD>(ASMOp::MOVMEMIMM1), "MOV MEM IMM1", -1, 0xC6, 0x05, DataEncoding::PtrIndirect, DataEncoding::Imm8);
	DefineASM(static_cast<DWORD>(ASMOp::MOVMEMIMM2), "MOV MEM IMM2", 0x66, 0xC7, 0x05, DataEncoding::PtrIndirect, DataEncoding::Imm16);
	DefineASM(static_cast<DWORD>(ASMOp::MOVMEMIMM4), "MOV MEM IMM4", -1, 0xC7, 0x05, DataEncoding::PtrIndirect, DataEncoding::Imm32);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBPIMM1), "MOV [EBP+A] IMM1", -1, 0xC6, 0x85, DataEncoding::Imm8, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBPIMM2), "MOV [EBP+A] IMM2", 0x66, 0xC7, 0x85, DataEncoding::Imm16, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBPIMM4), "MOV [EBP+A] IMM4", -1, 0xC7, 0x85, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::RELMOVEAXEDX1), "REL MOV [AX1] DX1", -1, 0x88, 0x10, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::RELMOVEAXEDX2), "REL MOV [AX2] DX2", 0x66, 0x89, 0x10, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::RELMOVEAXEDX4), "REL MOV [EAX] EDX", -1, 0x89, 0x10, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::RELMOVEAXREDX1), "REL MOV AX1 [DX1]", -1, 0x8A, 0x02, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::RELMOVEAXREDX2), "REL MOV AX2 [DX2]", 0x66, 0x8B, 0x02, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::RELMOVEAXREDX4), "REL MOV EAX [EDX]", -1, 0x8B, 0x02, DataEncoding::None, DataEncoding::None);
	// Wave 8: x87 FLD/FSTP -> SSE2 MOVSD XMM0 (same enum slots, new bytes).
	DefineASM(static_cast<DWORD>(ASMOp::MOVSDMEMXMM0), "MOVSD [MEM] XMM0", 0xF2, 0x0F, 0x11, DataEncoding::PtrIndirect, DataEncoding::None, OpcodeExpansion::None, 0x03);
	DefineASM(static_cast<DWORD>(ASMOp::MOVSDXMM0MEM), "MOVSD XMM0 [MEM]", 0xF2, 0x0F, 0x10, DataEncoding::PtrIndirect, DataEncoding::None, OpcodeExpansion::None, 0x03);
	DefineASM(static_cast<DWORD>(ASMOp::MOVSDEBPXMM0), "MOVSD [RBP+A] XMM0", 0xF2, 0x0F, 0x11, DataEncoding::Imm32, DataEncoding::None, OpcodeExpansion::None, 0x85);
	DefineASM(static_cast<DWORD>(ASMOp::MOVSDXMM0EBP), "MOVSD XMM0 [RBP+A]", 0xF2, 0x0F, 0x10, DataEncoding::Imm32, DataEncoding::None, OpcodeExpansion::None, 0x85);
	DefineASM(static_cast<DWORD>(ASMOp::MOVSDEAXXMM0), "MOVSD [RAX+A] XMM0", 0xF2, 0x0F, 0x11, DataEncoding::Imm32, DataEncoding::None, OpcodeExpansion::None, 0x80);
	DefineASM(static_cast<DWORD>(ASMOp::MOVSDXMM0EAX), "MOVSD XMM0 [RAX+A]", 0xF2, 0x0F, 0x10, DataEncoding::Imm32, DataEncoding::None, OpcodeExpansion::None, 0x80);
	DefineASM(static_cast<DWORD>(ASMOp::MOVSDECXOFFXMM0), "MOVSD [RCX+A] XMM0", 0xF2, 0x0F, 0x11, DataEncoding::Imm32, DataEncoding::None, OpcodeExpansion::None, 0x81);
	DefineASM(static_cast<DWORD>(ASMOp::MOVSDXMM0ECXOFF), "MOVSD XMM0 [RCX+A]", 0xF2, 0x0F, 0x10, DataEncoding::Imm32, DataEncoding::None, OpcodeExpansion::None, 0x81);
	// Wave 8: SSE2 float (MOVSS XMM0) memory forms mirroring the MOVSD family.
	DefineASM(static_cast<DWORD>(ASMOp::MOVSSXMM0MEM), "MOVSS XMM0 [MEM]", 0xF3, 0x0F, 0x10, DataEncoding::PtrIndirect, DataEncoding::None, OpcodeExpansion::None, 0x03);
	DefineASM(static_cast<DWORD>(ASMOp::MOVSSMEMXMM0), "MOVSS [MEM] XMM0", 0xF3, 0x0F, 0x11, DataEncoding::PtrIndirect, DataEncoding::None, OpcodeExpansion::None, 0x03);
	DefineASM(static_cast<DWORD>(ASMOp::MOVSSXMM0EBP), "MOVSS XMM0 [RBP+A]", 0xF3, 0x0F, 0x10, DataEncoding::Imm32, DataEncoding::None, OpcodeExpansion::None, 0x85);
	DefineASM(static_cast<DWORD>(ASMOp::MOVSSEBPXMM0), "MOVSS [RBP+A] XMM0", 0xF3, 0x0F, 0x11, DataEncoding::Imm32, DataEncoding::None, OpcodeExpansion::None, 0x85);
	DefineASM(static_cast<DWORD>(ASMOp::MOVSSXMM0EAX), "MOVSS XMM0 [RAX+A]", 0xF3, 0x0F, 0x10, DataEncoding::Imm32, DataEncoding::None, OpcodeExpansion::None, 0x80);
	DefineASM(static_cast<DWORD>(ASMOp::MOVSSEAXXMM0), "MOVSS [RAX+A] XMM0", 0xF3, 0x0F, 0x11, DataEncoding::Imm32, DataEncoding::None, OpcodeExpansion::None, 0x80);
	DefineASM(static_cast<DWORD>(ASMOp::MOVSSXMM0ECXOFF), "MOVSS XMM0 [RCX+A]", 0xF3, 0x0F, 0x10, DataEncoding::Imm32, DataEncoding::None, OpcodeExpansion::None, 0x81);
	DefineASM(static_cast<DWORD>(ASMOp::MOVSSECXOFFXMM0), "MOVSS [RCX+A] XMM0", 0xF3, 0x0F, 0x11, DataEncoding::Imm32, DataEncoding::None, OpcodeExpansion::None, 0x81);
	// Wave 8: SSE2 reg-reg moves / arithmetic / compares / conversions.
	DefineASM(static_cast<DWORD>(ASMOp::MOVSDXMM1XMM0), "MOVSD XMM1 XMM0", 0xF2, 0x0F, 0x10, DataEncoding::None, DataEncoding::None, OpcodeExpansion::None, 0xC8);
	DefineASM(static_cast<DWORD>(ASMOp::MOVSSXMM1XMM0), "MOVSS XMM1 XMM0", 0xF3, 0x0F, 0x10, DataEncoding::None, DataEncoding::None, OpcodeExpansion::None, 0xC8);
	DefineASM(static_cast<DWORD>(ASMOp::ADDSDXMM0XMM1), "ADDSD XMM0 XMM1", 0xF2, 0x0F, 0x58, DataEncoding::None, DataEncoding::None, OpcodeExpansion::None, 0xC1);
	DefineASM(static_cast<DWORD>(ASMOp::SUBSDXMM0XMM1), "SUBSD XMM0 XMM1", 0xF2, 0x0F, 0x5C, DataEncoding::None, DataEncoding::None, OpcodeExpansion::None, 0xC1);
	DefineASM(static_cast<DWORD>(ASMOp::MULSDXMM0XMM1), "MULSD XMM0 XMM1", 0xF2, 0x0F, 0x59, DataEncoding::None, DataEncoding::None, OpcodeExpansion::None, 0xC1);
	DefineASM(static_cast<DWORD>(ASMOp::DIVSDXMM0XMM1), "DIVSD XMM0 XMM1", 0xF2, 0x0F, 0x5E, DataEncoding::None, DataEncoding::None, OpcodeExpansion::None, 0xC1);
	DefineASM(static_cast<DWORD>(ASMOp::ADDSSXMM0XMM1), "ADDSS XMM0 XMM1", 0xF3, 0x0F, 0x58, DataEncoding::None, DataEncoding::None, OpcodeExpansion::None, 0xC1);
	DefineASM(static_cast<DWORD>(ASMOp::SUBSSXMM0XMM1), "SUBSS XMM0 XMM1", 0xF3, 0x0F, 0x5C, DataEncoding::None, DataEncoding::None, OpcodeExpansion::None, 0xC1);
	DefineASM(static_cast<DWORD>(ASMOp::MULSSXMM0XMM1), "MULSS XMM0 XMM1", 0xF3, 0x0F, 0x59, DataEncoding::None, DataEncoding::None, OpcodeExpansion::None, 0xC1);
	DefineASM(static_cast<DWORD>(ASMOp::DIVSSXMM0XMM1), "DIVSS XMM0 XMM1", 0xF3, 0x0F, 0x5E, DataEncoding::None, DataEncoding::None, OpcodeExpansion::None, 0xC1);
	DefineASM(static_cast<DWORD>(ASMOp::UCOMISDXMM0XMM1), "UCOMISD XMM0 XMM1", 0x66, 0x0F, 0x2E, DataEncoding::None, DataEncoding::None, OpcodeExpansion::None, 0xC1);
	DefineASM(static_cast<DWORD>(ASMOp::UCOMISSXMM0XMM1), "UCOMISS XMM0 XMM1", -1, 0x0F, 0x2E, DataEncoding::None, DataEncoding::None, OpcodeExpansion::None, 0xC1);
	DefineASM(static_cast<DWORD>(ASMOp::MOVDXMM0EAX), "MOVD XMM0 EAX", 0x66, 0x0F, 0x6E, DataEncoding::None, DataEncoding::None, OpcodeExpansion::None, 0xC0);
	DefineASM(static_cast<DWORD>(ASMOp::CVTSI2SDXMM0EAX), "CVTSI2SD XMM0 EAX", 0xF2, 0x0F, 0x2A, DataEncoding::None, DataEncoding::None, OpcodeExpansion::None, 0xC0);
	DefineASM(static_cast<DWORD>(ASMOp::CVTSI2SSXMM0EAX), "CVTSI2SS XMM0 EAX", 0xF3, 0x0F, 0x2A, DataEncoding::None, DataEncoding::None, OpcodeExpansion::None, 0xC0);
	DefineASM(static_cast<DWORD>(ASMOp::CVTTSD2SIEAXXMM0), "CVTTSD2SI EAX XMM0", 0xF2, 0x0F, 0x2C, DataEncoding::None, DataEncoding::None, OpcodeExpansion::None, 0xC0);
	DefineASM(static_cast<DWORD>(ASMOp::CVTTSS2SIEAXXMM0), "CVTTSS2SI EAX XMM0", 0xF3, 0x0F, 0x2C, DataEncoding::None, DataEncoding::None, OpcodeExpansion::None, 0xC0);
	// Wave 16: 64-bit CVT forms — the legacy F2/F3 prefix must precede REX.W.
	DefineASM(static_cast<DWORD>(ASMOp::CVTTSS2SIRAXXMM0), "CVTTSS2SI RAX XMM0", 0xF3, 0x0F, 0x2C, DataEncoding::None, DataEncoding::None, OpcodeExpansion::RexWAfterPrefix, 0xC0);
	DefineASM(static_cast<DWORD>(ASMOp::CVTTSD2SIRAXXMM0), "CVTTSD2SI RAX XMM0", 0xF2, 0x0F, 0x2C, DataEncoding::None, DataEncoding::None, OpcodeExpansion::RexWAfterPrefix, 0xC0);
	DefineASM(static_cast<DWORD>(ASMOp::CVTSI2SSXMM0RAX), "CVTSI2SS XMM0 RAX", 0xF3, 0x0F, 0x2A, DataEncoding::None, DataEncoding::None, OpcodeExpansion::RexWAfterPrefix, 0xC0);
	DefineASM(static_cast<DWORD>(ASMOp::CVTSI2SDXMM0RAX), "CVTSI2SD XMM0 RAX", 0xF2, 0x0F, 0x2A, DataEncoding::None, DataEncoding::None, OpcodeExpansion::RexWAfterPrefix, 0xC0);
	DefineASM(static_cast<DWORD>(ASMOp::CVTSD2SSXMM0XMM0), "CVTSD2SS XMM0 XMM0", 0xF2, 0x0F, 0x5A, DataEncoding::None, DataEncoding::None, OpcodeExpansion::None, 0xC0);
	DefineASM(static_cast<DWORD>(ASMOp::CVTSS2SDXMM0XMM0), "CVTSS2SD XMM0 XMM0", 0xF3, 0x0F, 0x5A, DataEncoding::None, DataEncoding::None, OpcodeExpansion::None, 0xC0);
	// Wave 19: register-form zero-extension (MOVZX r32, r/m8/r/m16) used
	// after a width-sized MOV AL/AX load — the loader leaves the upper bits
	// of EAX intact, so widening casts extend explicitly.
	DefineASM(static_cast<DWORD>(ASMOp::MOVZXEAXAL), "MOVZX EAX AL", -1, 0x0F, 0xB6, DataEncoding::None, DataEncoding::None, OpcodeExpansion::None, 0xC0);
	DefineASM(static_cast<DWORD>(ASMOp::MOVZXEAXAX), "MOVZX EAX AX", -1, 0x0F, 0xB7, DataEncoding::None, DataEncoding::None, OpcodeExpansion::None, 0xC0);
	DefineASM(static_cast<DWORD>(ASMOp::SETA), "SETA AL", 0x0F, 0x97, 0xC0, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SETAE), "SETAE AL", 0x0F, 0x93, 0xC0, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SETB), "SETB AL", 0x0F, 0x92, 0xC0, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SETBE), "SETBE AL", 0x0F, 0x96, 0xC0, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SETP), "SETP AL", 0x0F, 0x9A, 0xC0, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SETNP), "SETNP AL", 0x0F, 0x9B, 0xC0, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::ANDALAH), "AND AL AH", -1, 0x20, 0xE0, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::ORALAH), "OR AL AH", -1, 0x08, 0xE0, DataEncoding::None, DataEncoding::None);
	// Wave 8b: int64 (type 9) arithmetic — full-width REG64 (REX.W forms).
	DefineASM(static_cast<DWORD>(ASMOp::ADDEAXEBX8), "ADD RAX RBX8", -1, 0x01, 0xD8, DataEncoding::None, DataEncoding::None, OpcodeExpansion::RexW);
	DefineASM(static_cast<DWORD>(ASMOp::SUBEAXEBX8), "SUB RAX RBX8", -1, 0x29, 0xD8, DataEncoding::None, DataEncoding::None, OpcodeExpansion::RexW);
	DefineASM(static_cast<DWORD>(ASMOp::MULEAXEBX8), "IMUL RAX RBX8", 0x0F, 0xAF, 0xD8, DataEncoding::None, DataEncoding::None, OpcodeExpansion::RexW);
	DefineASM(static_cast<DWORD>(ASMOp::DIVEAXEBX8), "IDIV RBX8", -1, 0xF7, 0xFB, DataEncoding::None, DataEncoding::None, OpcodeExpansion::RexW);
	DefineASM(static_cast<DWORD>(ASMOp::CQO), "CQO", -1, 0x99, -1, DataEncoding::None, DataEncoding::None, OpcodeExpansion::RexW);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXEDX8), "MOV RAX RDX8", -1, 0x8B, 0xC2, DataEncoding::None, DataEncoding::None, OpcodeExpansion::RexW);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBXRAX8), "MOV RBX RAX8", -1, 0x8B, 0xD8, DataEncoding::None, DataEncoding::None, OpcodeExpansion::RexW);
	DefineASM(static_cast<DWORD>(ASMOp::CMPEDXEBX8), "CMP RDX RBX8", -1, 0x3B, 0xDA, DataEncoding::None, DataEncoding::None, OpcodeExpansion::RexW);
	DefineASM(static_cast<DWORD>(ASMOp::MOVSXDRAXEAX), "MOVSXD RAX EAX", -1, 0x63, 0xC0, DataEncoding::None, DataEncoding::None, OpcodeExpansion::RexW);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHEAX), "PUSH EAX", -1, 0x50+0, -1, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHEDX), "PUSH EDX", -1, 0x50+2, -1, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHEBX), "PUSH EBX", -1, 0x50+3, -1, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHESP), "PUSH ESP", -1, 0x50+4, -1, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHECX), "PUSH ECX", -1, 0x50+1, -1, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHRELEAX1), "PUSH REL AX1", -1, 0xFF, 0x30, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHRELEAX2), "PUSH REL AX2", -1, 0xFF, 0x30, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHRELEAX4), "PUSH REL EAX", -1, 0xFF, 0x30, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHEBP4), "PUSH [EBP+A]", -1, 0xFF, 0xB5, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHFROMEAX), "PUSH [EAX]", -1, 0xFF, 0x30, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::CALLEAX), "CALL EAX", -1, 0xFF, 0xD0, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::CALLEBX), "CALL EBX", -1, 0xFF, 0xD3, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::CALLMEM), "CALL MEM", -1, 0xE8, -1, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::CALLABS), "CALL REL", -1, 0xE8, -1, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::POPEAX), "POP EAX", -1, 0x58, -1, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::POPEBX), "POP EBX", -1, 0x5B, -1, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::RET), "RET", -1, 0xC3, -1, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::POPEDX), "POP EDX", -1, 0x5A, -1, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::POPECX), "POP ECX", -1, 0x59, -1, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::UNKNOWN), "???", -1, -1, -1, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::CMPEAX1), "CMP EAX1", -1, 0x3C, -1, DataEncoding::Imm8, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::CMPEAX2), "CMP EAX2", 0x66, 0x3D, -1, DataEncoding::Imm16, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::CMPEAX4), "CMP EAX4", -1, 0x3D, -1, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::CMPEBX1), "CMP EBX1", -1, 0x80, 0xFB, DataEncoding::Imm8, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::CMPEBX2), "CMP EBX2", 0x66, 0x81, 0xFB, DataEncoding::Imm16, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::CMPEBX4), "CMP EBX4", -1, 0x81, 0xFB, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::CMPGREEDXEBX), "CMP EDX EBX4", -1, 0x3B, 0xDA, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::CMPEDXEBX1), "CMP EDX EBX1", -1, 0x3A, 0xDA, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::CMPEDXEBX2), "CMP EDX EBX2", 0x66, 0x3B, 0xDA, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::CMPEDXEBX4), "CMP EDX EBX4", -1, 0x3B, 0xDA, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::CMPEAXEBX4), "CMP EAX EBX4", -1, 0x3B, 0xD8, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SETE), "SETE EAX", 0x0F, 0x94, 0xC0, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SETNE), "SETNE EAX", 0x0F, 0x95, 0xC0, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SETG), "SETG EAX", 0x0F, 0x9F, 0xC0, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SETGE), "SETGE EAX", 0x0F, 0x9D, 0xC0, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SETL), "SETL EAX", 0x0F, 0x9C, 0xC0, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SETLE), "SETLE EAX", 0x0F, 0x9E, 0xC0, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::JMP), "JMP", -1, 0xE9, -1, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::JNE), "JNE", -1, 0x0F, 0x85, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::JE), "JE", -1, 0x0F, 0x84, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::JMPREL), "JMP REL", -1, 0xFF, 0x25, DataEncoding::PtrIndirect, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::JMPEBX), "JMP EBX", -1, 0xFF, 0xE3, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::JGE), "JGE", -1, 0x0F, 0x8D, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::JLE), "JLE", -1, 0x0F, 0x8E, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVMEMESP4), "MOV MEM4 ESP", -1, 0x89, 0x25, DataEncoding::PtrIndirect, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVESPMEM4), "MOV ESP MEM4", -1, 0x8B, 0x25, DataEncoding::PtrIndirect, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBXMEM4), "MOV EBX MEM4", -1, 0x8B, 0x1D, DataEncoding::PtrIndirect, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVMEMEBX4), "MOV MEM EBX4", -1, 0x89, 0x1D, DataEncoding::PtrIndirect, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXSIB4), "MOV EAX SIB[EAX:EBX*4]", 0x8B, 0x04, 0x98, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVSIB4IMM1), "MOV SIB[EAX:ECX*1],IMM1", 0xC6, 0x04, 0x08, DataEncoding::Imm8, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MOVSIB4IMM4), "MOV SIB[EAX:ECX*4],IMM4", 0xC7, 0x04, 0x88, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHAD), "PUSH REGISTERS", -1, 0x60, -1, DataEncoding::None, DataEncoding::None, OpcodeExpansion::PushAll);
	DefineASM(static_cast<DWORD>(ASMOp::POPAD), "POP REGISTERS", -1, 0x61, -1, DataEncoding::None, DataEncoding::None, OpcodeExpansion::PopAll);
	DefineASM(static_cast<DWORD>(ASMOp::LOOP), "LOOP ECX", -1, 0xE2, -1, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHIMM4), "PUSH IMM4", -1, 0x68, -1, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::INCMEM1), "INC MEM1", -1, 0xFE, 0x05, DataEncoding::PtrIndirect, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::INCMEM2), "INC MEM2", 0x66, 0xFF, 0x05, DataEncoding::PtrIndirect, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::INCMEM4), "INC MEM4", -1, 0xFF, 0x05, DataEncoding::PtrIndirect, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::DECMEM1), "DEC MEM1", -1, 0xFE, 0x0D, DataEncoding::PtrIndirect, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::DECMEM2), "DEC MEM2", 0x66, 0xFF, 0x0D, DataEncoding::PtrIndirect, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::DECMEM4), "DEC MEM4", -1, 0xFF, 0x0D, DataEncoding::PtrIndirect, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::ADDEAX1), "ADD EAX IMM1", -1, 0x04, -1, DataEncoding::Imm8, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::ADDEAX2), "ADD EAX IMM2", 0x66, 0x05, -1, DataEncoding::Imm16, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::ADDEAX4), "ADD EAX IMM4", -1, 0x05, -1, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SUBEAX1), "SUB EAX IMM1", -1, 0x2C, -1, DataEncoding::Imm8, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SUBEAX2), "SUB EAX IMM2", 0x66, 0x2D, -1, DataEncoding::Imm16, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SUBEAX4), "SUB EAX IMM4", -1, 0x2D, -1, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SUBEAXEBX1), "SUB EAX EBX1", -1, 0x28, 0xD8, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SUBEAXEBX2), "SUB EAX EBX2", 0x66, 0x29, 0xD8, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SUBEAXEBX4), "SUB EAX EBX4", -1, 0x29, 0xD8, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MULEAXEBX1), "IMUL EAX EBX1", -1, 0xF6, 0xEB, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MULEAXEBX2), "IMUL EAX EBX2", 0x66, 0xF7, 0xEB, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MULEAXEBX4), "IMUL EAX EBX4", -1, 0xF7, 0xEB, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::CDQ), "CDQ", -1, 0x99, -1, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::DIVEAXEBX1), "IDIV EAX EBX1", -1, 0xF6, 0xFB, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::DIVEAXEBX2), "IDIV EAX EBX2", 0x66, 0xF7, 0xFB, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::DIVEAXEBX4), "IDIV EAX EBX4", -1, 0xF7, 0xFB, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::ANDEAX1), "AND EAX IMM1", -1, 0x24, -1, DataEncoding::Imm8, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::ANDEAX2), "AND EAX IMM2", 0x66, 0x25, -1, DataEncoding::Imm16, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::ANDEAX4), "AND EAX IMM4", -1, 0x25, -1, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::ANDEAXEBX1), "AND EAX EBX1", -1, 0x20, 0xD8, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::ANDEAXEBX2), "AND EAX EBX2", 0x66, 0x21, 0xD8, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::ANDEAXEBX4), "AND EAX EBX4", -1, 0x21, 0xD8, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::OREAX1), "OR EAX IMM1", -1, 0x0C, -1, DataEncoding::Imm8, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::OREAX2), "OR EAX IMM2", 0x66, 0x0D, -1, DataEncoding::Imm16, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::OREAX4), "OR EAX IMM4", -1, 0x0D, -1, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::OREAXEBX1), "OR EAX EBX1", -1, 0x08, 0xD8, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::OREAXEBX2), "OR EAX EBX2", 0x66, 0x09, 0xD8, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::OREAXEBX4), "OR EAX EBX4", -1, 0x09, 0xD8, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::NOTEAX1), "NOT EAX1", -1, 0xF6, 0xD0, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::NOTEAX2), "NOT EAX2", 0x66, 0xF7, 0xD0, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::NOTEAX4), "NOT EAX4", -1, 0xF7, 0xD0, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::XOREAX1), "XOR EAX IMM1", -1, 0x34, -1, DataEncoding::Imm8, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::XOREAX2), "XOR EAX IMM2", 0x66, 0x35, -1, DataEncoding::Imm16, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::XOREAX4), "XOR EAX IMM4", -1, 0x35, -1, DataEncoding::Imm32, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::XOREAXEBX1), "XOR EAX EBX1", -1, 0x30, 0xD8, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::XOREAXEBX2), "XOR EAX EBX2", 0x66, 0x31, 0xD8, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::XOREAXEBX4), "XOR EAX EBX4", -1, 0x31, 0xD8, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SHLEAX1), "SHL EAX1 IMM1", -1, 0xC0, 0xE0, DataEncoding::Imm8, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SHLEAX2), "SHL EAX2 IMM2", 0x66, 0xC1, 0xE0, DataEncoding::Imm8, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SHLEAX4), "SHL EAX4 IMM4", -1, 0xC1, 0xE0, DataEncoding::Imm8, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SHLEAXCLC1), "SHL EAX1 CL", -1, 0xD2, 0xE0, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SHLEAXCLC2), "SHL EAX2 CL", 0x66, 0xD3, 0xE0, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SHLEAXCLC4), "SHL EAX4 CL", -1, 0xD3, 0xE0, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SHREAX1), "SHR EAX1 IMM1", -1, 0xC0, 0xE8, DataEncoding::Imm8, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SHREAX2), "SHR EAX2 IMM2", 0x66, 0xC1, 0xE8, DataEncoding::Imm8, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SHREAX4), "SHR EAX4 IMM4", -1, 0xC1, 0xE8, DataEncoding::Imm8, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SHREAXCLC1), "SHR EAX1 CL", -1, 0xD2, 0xE8, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SHREAXCLC2), "SHR EAX2 CL", 0x66, 0xD3, 0xE8, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::SHREAXCLC4), "SHR EAX4 CL", -1, 0xD3, 0xE8, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::MULECXEDX4), "IMUL ECX EDX4", 0x0F, 0xAF, 0x0A, DataEncoding::None, DataEncoding::None);
	DefineASM(static_cast<DWORD>(ASMOp::ADDEBXEDX4), "ADD EBX EDX4", -1, 0x03, 0xDA, DataEncoding::None, DataEncoding::None);

}

void CASMWriter::DefineASM(DWORD dwASMCode, const char* pDebugStr, int iPreOp, int iOp1, int iOp2, DataEncoding data1, DataEncoding data2, OpcodeExpansion expansion, int iModRm)
{
	if (dwASMCode >= m_asmoOpcodeDefs.size())
		return;

	ASMOpcodeDef& def = m_asmoOpcodeDefs[dwASMCode];
	def.name = pDebugStr;
	def.preOp = iPreOp;
	def.op1 = iOp1;
	def.op2 = iOp2;
	def.modrm = iModRm;
	def.data1 = data1;
	def.data2 = data2;
	def.expansion = expansion;
}

const ASMOpcodeDef& CASMWriter::GetASMOpcodeDef(DWORD dwASMCode) const noexcept
{
	static const ASMOpcodeDef s_undefined;
	if (dwASMCode >= m_asmoOpcodeDefs.size())
		return s_undefined;
	return m_asmoOpcodeDefs[dwASMCode];
}

bool CASMWriter::CreateASMHeader(void)
{
	// Create Empty MC Block via machine code buffer
	m_machineCodeBuffer.Initialize(1024);

	// Prepare RefData
	m_referenceTracker.Reset();

	// Seed the x64 stack/alignment tracker: the program is entered through a
	// C call, so RSP%16==8 at entry (the 7-register prologue lands the body at 0).
	ResetStackTracking();

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

bool CASMWriter::CreateASMMiddle(int iPreOpCode, int iOpCode1, int iOpCode2, LPSTR lpOpData)
{
	// Raw byte emitter used by CLeapMarkerManager (precomputed numeric offsets)
	// and by low-level tests. The single data field is a 4-byte value slot.
	if(m_machineCodeBuffer.GetProgramStart()==NULL || m_machineCodeBuffer.GetMachineBlock()==NULL)
	{
		if(g_pErrorReport)
			g_pErrorReport->AddErrorString(
				"DBP2001: code emission attempted before backend initialization.");
		return false;
	}
	DBP_TRACE("Generated instruction: preOp={}, op1={}, op2={}", iPreOpCode, iOpCode1, iOpCode2);

	// Check and expand if MCB too small
	CheckAndExpandMCBMemory();

	// Write OpCode(s)
	if(iPreOpCode!=-1)
		m_machineCodeBuffer.WriteByte(iPreOpCode);
	if(iOpCode1!=-1)
		m_machineCodeBuffer.WriteByte(iOpCode1);
	if(iOpCode2!=-1)
		m_machineCodeBuffer.WriteByte(iOpCode2);

	// Write Optional OpData (4-byte value slot)
	if(lpOpData && strcmp(lpOpData, "")!=0)
	{
		// Ensure reference array always large enough for new reference
		CheckAndExpandREFMemory();

		// Record a value-owned reference label. Host pointers must never
		// leak into the serialized target reference representation.
		CStr cleanStr(lpOpData);
		cleanStr.EatEdgeSpacesandTabs(NULL);

		DWORD MCBBytePos = m_machineCodeBuffer.GetCurrentMCPosition();
		m_referenceTracker.AddReference(MCBBytePos, cleanStr.GetStr());

		// WRITE BLANK(XXXX) INTO MB
		m_machineCodeBuffer.WriteDWORD((DWORD)0xFFFFFFFF, 4);
	}

	// Complete
	return true;
}

bool CASMWriter::CreateASMMiddleCore(DWORD dwASMCode, LPSTR lpOpData, LPSTR lpOpData2, bool bSecondOpDataIsIMM, DWORD dwSecondOpDataIMMSize)
{
	if(m_machineCodeBuffer.GetProgramStart()==NULL || m_machineCodeBuffer.GetMachineBlock()==NULL)
	{
		if(g_pErrorReport)
			g_pErrorReport->AddErrorString(
				"DBP2001: code emission attempted before backend initialization.");
		return false;
	}

	const ASMOpcodeDef& def = GetASMOpcodeDef(dwASMCode);
	DBP_TRACE("Generated instruction: op={}, name={}", dwASMCode, def.name ? def.name : "?");

	// Whole-instruction expansions that have no direct x64 encoding.
	// PUSHAD/POPAD do not exist in 64-bit mode; emit explicit register
	// save/restore sequences (one-byte pushes/pops, no REX needed).
	if (def.expansion == OpcodeExpansion::PushAll)
	{
		CheckAndExpandMCBMemory();
		static const uint8_t pushes[] = { 0x50, 0x53, 0x51, 0x52, 0x56, 0x57, 0x55 };
		for (const uint8_t b : pushes)
			m_machineCodeBuffer.WriteByte(b);
		ApplyStackDelta(56, false); // 7 x 8-byte register saves
		return true;
	}
	if (def.expansion == OpcodeExpansion::PopAll)
	{
		CheckAndExpandMCBMemory();
		static const uint8_t pops[] = { 0x5D, 0x5F, 0x5E, 0x5A, 0x59, 0x5B, 0x58 };
		for (const uint8_t b : pops)
			m_machineCodeBuffer.WriteByte(b);
		ApplyStackDelta(-56, false); // 7 x 8-byte register restores
		return true;
	}

	// x64 stack/alignment tracking for the call convention (PUSH/POP/SUBESP/
	// ADDESP deltas; runtime RSP moves poison the alignment knowledge).
	TrackStackForOpcode(dwASMCode, lpOpData);

	// Check and expand if MCB too small
	CheckAndExpandMCBMemory();

	const bool hasData1 = def.data1 != DataEncoding::None;
	const bool hasData2 = def.data2 != DataEncoding::None;
	const bool hasData1Text = lpOpData != nullptr && strcmp(lpOpData, "") != 0;
	const bool hasData2Text = lpOpData2 != nullptr && strcmp(lpOpData2, "") != 0;

	// WriteASMLine2IMM(op, NULL, imm, size): the immediate is written into
	// the instruction's own data1 slot (TaskEmitter loads constants this way).
	const bool immFillsData1 = bSecondOpDataIsIMM && !hasData1Text && hasData2Text;

	if (immFillsData1)
	{
		// OpCode(s) first, then the immediate value bytes.
		if (def.preOp != -1)
			m_machineCodeBuffer.WriteByte(def.preOp);
		if (def.op1 != -1)
			m_machineCodeBuffer.WriteByte(def.op1);
		if (def.op2 != -1)
			m_machineCodeBuffer.WriteByte(def.op2);

		DWORD dwDataAsDWORD = (DWORD)_atoi64(lpOpData2);
		DWORD dwByteSize = (dwSecondOpDataIMMSize == 0) ? 1 : (dwSecondOpDataIMMSize == 1) ? 2 : 4;
		m_machineCodeBuffer.WriteDWORD(dwDataAsDWORD, dwByteSize);
	}
	else if (def.data1 == DataEncoding::PtrIndirect)
	{
		// Absolute [disp32] memory operand. In 64-bit mode modrm rm=101 means
		// RIP-relative, so embed the address as a true 64-bit immediate and
		// access through RBX: 48 BB <imm64> + opcode with modrm rm 101 -> 011.
		if (hasData1Text)
			EmitAddressSlotIntoRbx(lpOpData);
		else
			EmitAddressSlotIntoRbx("0");

		if (def.preOp != -1)
			m_machineCodeBuffer.WriteByte(def.preOp);
		if (def.op1 != -1)
			m_machineCodeBuffer.WriteByte(def.op1);
		if (def.modrm != -1)
		{
			// SSE2 forms: op2 carries the last opcode byte, modrm is explicit.
			if (def.op2 != -1)
				m_machineCodeBuffer.WriteByte(def.op2);
			m_machineCodeBuffer.WriteByte((def.modrm & 0xF8) | 3); // rm 101 -> RBX(011)
		}
		else if (def.op2 != -1)
			m_machineCodeBuffer.WriteByte((def.op2 & 0xF8) | 3); // rm 101 -> RBX(011)
	}
	else if (def.data1 == DataEncoding::ImmOrAddr)
	{
		// MOV r, imm: values keep the 4-byte imm32 slot; addresses become
		// 48 B8+rd imm64 so the pointer never truncates.
		if (hasData1Text)
			EmitMovRegImmSlot(def.op1, lpOpData);
		else if (def.op1 != -1)
			m_machineCodeBuffer.WriteByte(def.op1);
	}
	else
	{
		// Write OpCode(s)
		if (def.expansion == OpcodeExpansion::RexW)
			m_machineCodeBuffer.WriteByte(0x48); // 64-bit operand (frame ops)
		if (def.expansion == OpcodeExpansion::RexWAfterPrefix)
		{
			// 64-bit CVT forms: the legacy F2/F3/66 prefix must come before
			// the REX byte (F3 48 0F 2C C0 = CVTTSS2SI RAX,XMM0).
			if (def.preOp != -1)
				m_machineCodeBuffer.WriteByte(def.preOp);
			m_machineCodeBuffer.WriteByte(0x48); // REX.W
			if (def.op1 != -1)
				m_machineCodeBuffer.WriteByte(def.op1);
			if (def.op2 != -1)
				m_machineCodeBuffer.WriteByte(def.op2);
			if (def.modrm != -1)
				m_machineCodeBuffer.WriteByte(def.modrm);
		}
		else
		{
			if (def.preOp != -1)
				m_machineCodeBuffer.WriteByte(def.preOp);
			if (def.op1 != -1)
				m_machineCodeBuffer.WriteByte(def.op1);
			if (def.op2 != -1)
				m_machineCodeBuffer.WriteByte(def.op2);
			// SSE2 forms with an explicit ModRM byte (F2 0F 10 85 <disp32> ...).
			if (def.modrm != -1)
				m_machineCodeBuffer.WriteByte(def.modrm);
		}

		// Write Optional OpData 1 (value or 8-byte address slot)
		if (hasData1 && hasData1Text)
			EmitDataSlot(def.data1, lpOpData);
	}

	// Write Optional OpData 2 (only MOVMEMIMM* uses a second field)
	if (hasData2)
	{
		if (bSecondOpDataIsIMM)
		{
			if (hasData2Text)
			{
				// WRITE IMM INTO MC
				DWORD dwDataAsDWORD = (DWORD)_atoi64(lpOpData2);
				// Convert legacy size code (0=1byte, 1=2bytes, 2=4bytes) to actual byte size
				DWORD dwByteSize = (dwSecondOpDataIMMSize == 0) ? 1 : (dwSecondOpDataIMMSize == 1) ? 2 : 4;
				m_machineCodeBuffer.WriteDWORD(dwDataAsDWORD, dwByteSize);
			}
		}
		else if (hasData2Text)
		{
			// Reference label: 4-byte value slot (existing semantics)
			EmitDataSlot(DataEncoding::Imm32, lpOpData2);
		}
	}

	// Complete
	return true;
}

void CASMWriter::EmitDataSlot(DataEncoding encoding, LPSTR pData)
{
	// Ensure reference array always large enough for new reference
	CheckAndExpandREFMemory();

	CStr cleanStr(pData);
	cleanStr.EatEdgeSpacesandTabs(NULL);

	const DWORD slotPos = m_machineCodeBuffer.GetCurrentMCPosition();
	m_referenceTracker.AddReference(slotPos, cleanStr.GetStr());

	if (encoding == DataEncoding::Abs64)
	{
		// moffs A0-A3: the 64-bit mode address field is a full 8-byte slot.
		for (int i = 0; i < 8; ++i)
			m_machineCodeBuffer.WriteByte(0);
		return;
	}

	// Imm8/Imm16/Imm32: 4-byte value slot (legacy reference semantics;
	// byte/word forms are only ever emitted through the IMM path which
	// writes the exact value bytes).
	m_machineCodeBuffer.WriteDWORD((DWORD)0xFFFFFFFF, 4);
}

void CASMWriter::EmitMovRegImmSlot(int op1, LPSTR pData)
{
	// Ensure reference array always large enough for new reference
	CheckAndExpandREFMemory();

	CStr cleanStr(pData);
	cleanStr.EatEdgeSpacesandTabs(NULL);

	const auto parsed = ParseReferenceLabel(cleanStr.GetStr());
	const bool isAddress = parsed.has_value() &&
		(parsed->kind == ReferenceKind::Command ||
		 parsed->kind == ReferenceKind::StringLiteral ||
		 parsed->kind == ReferenceKind::Variable ||
		 parsed->kind == ReferenceKind::DataLabel);

	if (isAddress && sizeof(void*) == 8)
	{
		// 48 B8+rd imm64: embed the address without truncation.
		const DWORD slotPos = m_machineCodeBuffer.GetCurrentMCPosition();
		m_machineCodeBuffer.WriteByte(0x48); // REX.W
		m_machineCodeBuffer.WriteByte(static_cast<uint8_t>(op1));
		m_referenceTracker.AddReference(slotPos + 2, cleanStr.GetStr());
		for (int i = 0; i < 8; ++i)
			m_machineCodeBuffer.WriteByte(0);
	}
	else
	{
		// 32-bit value: B8+rd imm32 (zero-extending).
		const DWORD slotPos = m_machineCodeBuffer.GetCurrentMCPosition();
		m_machineCodeBuffer.WriteByte(static_cast<uint8_t>(op1));
		m_referenceTracker.AddReference(slotPos + 1, cleanStr.GetStr());
		m_machineCodeBuffer.WriteDWORD((DWORD)0xFFFFFFFF, 4);
	}
}

void CASMWriter::EmitAddressSlotIntoRbx(LPSTR pData)
{
	// 48 BB <imm64>: MOV RBX, imm64 (address embedded as a true 64-bit value)
	CheckAndExpandREFMemory();

	CStr cleanStr(pData);
	cleanStr.EatEdgeSpacesandTabs(NULL);

	const DWORD slotPos = m_machineCodeBuffer.GetCurrentMCPosition();
	m_machineCodeBuffer.WriteByte(0x48); // REX.W
	m_machineCodeBuffer.WriteByte(0xBB); // MOV RBX, imm64
	m_referenceTracker.AddReference(slotPos + 2, cleanStr.GetStr());
	for (int i = 0; i < 8; ++i)
		m_machineCodeBuffer.WriteByte(0);
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
			lResult=0;//EAX needs to be zero
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
	DWORD dwRTError=g_pEXE->m_dwRuntimeErrorDWORD;
	DWORD dwRTErrorLine=g_pEXE->m_dwRuntimeErrorLineDWORD;
	if(dwRTError>0)
	{
		// Report error
		char lpReturnError[1024];
		LPSTR pRuntimeErrorString = NULL;
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

	if(g_pEXE->m_pMachineCodeBlock==NULL)
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
		if (_stricmp(label->GetName()->GetStr(), "$labelend") != 0)
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

void CASMWriter::UpdateStructurePatternDataRec(LPSTR pPattern, CDeclaration* pDecMain)
{
}

LPSTR CASMWriter::MakeVarDataForTransfer(DWORD *pdwDataSize)
{
	LPSTR pData = NULL;
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
	LPSTR pData = NULL;
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
	// Clear ASM Code Database (descriptors auto-clear; names are static strings)
	m_asmoOpcodeDefs.clear();
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

bool CASMWriter::WriteASMCall(DWORD dwLine, LPSTR pDLL, LPSTR pDecoratedName)
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

void CASMWriter::CalculateArrayOffsetInEBX ( CStr* pPIndex )
{
	// Locate Array Element (EBX)
	if(pPIndex)
	{
		// If empty, it must use internal index (list system)
		if(pPIndex->Length()==0)
		{
			// Internal unified list index
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEBXEAXOFF4), "-4");
		}
		else
		{
			/* moved to mathop to calc-array-offset
			// Always have first dimension D1
			WriteASMLine(static_cast<DWORD>(ASMOp::POPEBX), "");

			// Loop through subsequent dimensions D2-D9
			int iCount=0;
			int iCountMax = (DWORD)(pNumSubscriptsOnStack->GetValue())-1;
			while(iCount<iCountMax)
			{
				CStr* pValue = new CStr("");
				int iHeaderOffset = (-56)+(iCount*4);
				pValue->SetNumericText(iHeaderOffset);
				WriteASMLine(static_cast<DWORD>(ASMOp::POPEDX), "");
				WriteASMLine(static_cast<DWORD>(ASMOp::MULEDXEAXOFF4), pValue->GetStr());
				WriteASMLine(static_cast<DWORD>(ASMOp::ADDEBXEDX4), "");
				SAFE_DELETE(pValue);
				iCount++;
			}
			*/
			if(pPIndex->GetChar(0)=='@')
			{
				if(pPIndex->GetChar(1)==':')
				{
					DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEBXEBP1),1);
					WriteASMLine(dwCorrectASMCode, (pPIndex->GetStr()+2));	
				}
				else
				{
					WriteASMLine(static_cast<DWORD>(ASMOp::MOVEBXMEM4), pPIndex->GetStr());
				}
			}
			else
			{
				// Normal specified index
				WriteASMLine(static_cast<DWORD>(ASMOp::MOVEBXIMM4), pPIndex->GetStr());
			}
		}

		// If Array Check Active
		if(GetArrayCheckFlag())
		{
			// Perform array bounds (for user subscripts) check and leap over
			WriteASMLine(static_cast<DWORD>(ASMOp::PUSHEDX), "");//added 300305 - to stop EDX overwritten as it can store DOUBLE compoent!
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEDXEAXOFF4), "-16");
			WriteASMLine(static_cast<DWORD>(ASMOp::CMPGREEDXEBX), "");
			WriteASMLine(static_cast<DWORD>(ASMOp::POPEDX), "");//added 300305 - to stop EDX overwritten as it can store DOUBLE compoent!
			WriteASMLeapMarkerJump(static_cast<DWORD>(ASMOp::JGE), 2);
			WriteASMLine(static_cast<DWORD>(ASMOp::CMPEBX4), "-1");
			WriteASMLeapMarkerJump(static_cast<DWORD>(ASMOp::JLE), 3);
		}
	}
	else
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVEBXIMM4), "0");
}

void CASMWriter::WriteASMARRtoEAX(DWORD dwMode, CStr* pP, CStr* pOffset, DWORD dwPType, DWORD dwPOffset)
{
	m_taskEmitter.WriteASMARRtoEAX(this, dwMode, pP, pOffset, dwPType, dwPOffset);
}

void CASMWriter::WriteASMXtoEAX(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset)
{
	m_taskEmitter.WriteASMXtoEAX(this, dwMode, pP, pPIndex, dwPType, dwPOffset);
}

void CASMWriter::WriteASMEAXtoARR(DWORD dwMode, CStr* pP, CStr* pOffset, DWORD dwPType, DWORD dwPOffset)
{
	m_taskEmitter.WriteASMEAXtoARR(this, dwMode, pP, pOffset, dwPType, dwPOffset);
}

void CASMWriter::WriteASMEAXtoX(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset)
{
	m_taskEmitter.WriteASMEAXtoX(this, dwMode, pP, pPIndex, dwPType, dwPOffset);
}

bool CASMWriter::WriteASMTaskP1(DWORD dwLine, DWORD dwTask, CResultData* pP1Result)
{
	m_taskEmitter.IncrementTaskCount();
	DWORD dwP1Type = 0;
	CStr* pP1Str = NULL;
	DWORD dwP1Offset = 0;
	CStr* pP1OffsetStr = NULL;
	if(pP1Result)
	{
		dwP1Type = pP1Result->m_dwType;
		pP1Str = pP1Result->m_pStringToken.get();
		dwP1Offset = pP1Result->m_dwDataOffset;
		pP1OffsetStr = pP1Result->m_pAdditionalOffset.get();
	}
	return WriteASMTaskCore(dwLine, dwTask, pP1Str, pP1OffsetStr, dwP1Type, dwP1Offset, NULL, NULL, 0, 0);
}

bool CASMWriter::WriteASMTaskP2(DWORD dwLine, DWORD dwTask, CResultData* pP1Result, CResultData* pP2Result)
{
	DWORD dwP1Type = 0;
	CStr* pP1Str = NULL;
	DWORD dwP1Offset = 0;
	CStr* pP1OffsetStr = NULL;
	DWORD dwP2Type = 0;
	CStr* pP2Str = NULL;
	DWORD dwP2Offset = 0;
	CStr* pP2OffsetStr = NULL;
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
	CStr* pP1Str = NULL;
	DWORD dwP1Offset = 0;
	CStr* pP1OffsetStr = NULL;
	DWORD dwP2Type = 0;
	CStr* pP2Str = NULL;
	DWORD dwP2Offset = 0;
	CStr* pP2OffsetStr = NULL;
	DWORD dwP3Type = 0;
	CStr* pP3Str = NULL;
	DWORD dwP3Offset = 0;
	CStr* pP3OffsetStr = NULL;
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
	return WriteASMTaskCore(dwLine, dwTask, pP1, NULL, dwP1Type, 0, NULL, NULL, 0, 0);
}

bool CASMWriter::WriteASMTaskCoreP2(DWORD dwLine, DWORD dwTask, CStr* pP1, DWORD dwP1Type, CStr* pP2, DWORD dwP2Type)
{
	return WriteASMTaskCore(dwLine, dwTask, pP1, NULL, dwP1Type, 0, pP2, NULL, dwP2Type, 0);
}

bool CASMWriter::WriteASMTaskCore(DWORD dwLine, DWORD dwTask, CStr* pP1, CStr* pP1Off, DWORD dwP1Type, DWORD dwP1Offset, CStr* pP2, CStr* pP2Off, DWORD dwP2Type, DWORD dwP2Offset)
{
	return WriteASMTaskCore(dwLine, dwTask, pP1, pP1Off, dwP1Type, dwP1Offset, pP2, pP2Off, dwP2Type, dwP2Offset,NULL,NULL,0,0);
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

	// Batches of ASM Ops to perform a single task
	if(dwTask==static_cast<DWORD>(ASMTask::AssignToEax))
	{
		WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);
		WriteASMComment("ASSIGN X TO EAX", "", "", "");
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
				WriteASMXtoEAX(dwP2Mode, pP2, pP2Off, dwP2Type, dwP2Offset);
				WriteASMEAXtoX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);
				WriteASMComment("ASSIGN X TO X", "", "", "");
//			}
		}
		else
		{
			WriteASMEAXtoX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);
			WriteASMComment("ASSIGN EAX TO X", "", "", "");
		}
	}
	if(dwTask==static_cast<DWORD>(ASMTask::Push))
	{
		if(pP1)
		{
			if(_strnicmp(pP1->GetStr(),"fs@",3)==NULL)
			{
				WriteASMComment("PUSH TO STACK", "", "", "");
			}
		}
		WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);
		WriteASMEAXtoX(static_cast<DWORD>(ParamMode::Stack), NULL, NULL, dwP1Type, dwP1Offset);
		// x64 call ABI: record the pushed slot type(s) for the upcoming Call.
		RecordPendingArg(dwP1Type);
		WriteASMComment("PUSH TO STACK", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::PushAddress))
	{
		// leefix - include dataofsfet if any
		if ( dwP1Mode==static_cast<DWORD>(ParamMode::Mem) )
		{
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXIMM4), pP1->GetStr());
		}
		if ( dwP1Mode==static_cast<DWORD>(ParamMode::Ebp) )
		{
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXEBP), NULL );
			WriteASMLine(static_cast<DWORD>(ASMOp::ADDEAX4), pP1->GetStr()+2 );
		}
		if ( dwP1Offset>0 )
		{
			CStr num("");
			num.SetNumericText(dwP1Offset);
			WriteASMLine(static_cast<DWORD>(ASMOp::ADDEAX4), num.GetStr());
		}
		WriteASMLine(static_cast<DWORD>(ASMOp::PUSHEAX), NULL);
		// x64 call ABI: an address is an integer-class pointer slot.
		RecordPendingArg(7);
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

		// Write current ESP into ErrorLineDWORD if third party DLLs to check that they have not been tampered with
		bool bProtectedByESPDetection = false;
		if ( AddProtectionToSelectedDLLs ( pDLLString.get() ) ) 
		{
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEBXMEM4), "@$_SLN_");
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVMEMEBX4), "@$_TEMPA_");
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVMEMESP4), "@$_SLN_");
			bProtectedByESPDetection=true;
		}

		// Microsoft x64 call convention: RCX/RDX/R8/R9 + XMM0-3, 32-byte shadow
		// space, 16-byte RSP alignment at the CALL, caller cleanup consumed
		// here (the trailing cleanup pops are suppressed). Frames containing
		// UDT-by-value args, or whose RSP alignment is unknown, keep the
		// legacy bytes so the caller's pops remain balanced.
		const bool bLegacyCall = m_bPendingFramePoisoned || !m_bRSPAlignmentKnown;
		if ( bLegacyCall )
		{
			CStr tokenCommandStr("[");
			tokenCommandStr.AddNumericText(dwIndex);
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEBXIMM4), tokenCommandStr.GetStr());
			WriteASMLine(static_cast<DWORD>(ASMOp::CALLEBX), "");
		}
		else
		{
			EmitX64CallFrame(dwIndex);
		}

		// Comment Details
		WriteASMComment("CALL", pDLLString.get(), pCommandString.get(), "");

		// Restore SLN after CALL for RTE tracing
		if ( bProtectedByESPDetection ) 
		{
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEBXMEM4), "@$_TEMPA_");
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVMEMEBX4), "@$_SLN_");
		}
	}
	if(dwTask==static_cast<DWORD>(ASMTask::PopEax))
	{
		// x64 call ABI: the Call task consumes the frame, so the caller's
		// cleanup pops are suppressed (a pop with no pending cleanup still
		// emits, preserving value-stack uses).
		if ( m_iPendingCleanupPops > 0 )
		{
			m_iPendingCleanupPops--;
		}
		else
		{
			WriteASMLine(static_cast<DWORD>(ASMOp::POPEAX), "");
		}
		WriteASMComment("POP EAX FROM STACK", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::PopEbx))
	{
		// x64 call ABI: same suppression as PopEax (the cleanup pops follow the
		// consumed frame; see the Call handler).
		if ( m_iPendingCleanupPops > 0 )
		{
			m_iPendingCleanupPops--;
		}
		else
		{
			WriteASMLine(static_cast<DWORD>(ASMOp::POPEBX), "");
		}
		WriteASMComment("POP EBX FROM STACK", "", "", "");
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
				WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);
				WriteASMLine(static_cast<DWORD>(ASMOp::CMPEAX4), "0");
			}
			else
			{
				// IMM
				if(pP1->GetValue()==0)
				{
					WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXIMM4), "0");
					WriteASMLine(static_cast<DWORD>(ASMOp::CMPEAX4), "0");
				}
				else
				{
					WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXIMM4), "1");
					WriteASMLine(static_cast<DWORD>(ASMOp::CMPEAX4), "0");
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
				WriteASMLine(static_cast<DWORD>(ASMOp::CMPEAX4), dword1Str.GetStr());
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
		// User-function calls keep the stack convention (wave 4 frames); their
		// pushes must not leak into the next DLL call's pending frame.
		ResetPendingFrame();
		WriteASMLine(static_cast<DWORD>(ASMOp::CALLMEM), pP1->GetStr());
		WriteASMComment("DIRECT SUBCALL", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::Return))
	{
		// Function boundary: no pending call frame crosses a RET.
		ResetPendingFrame();
		// Unscheduled RETs are dangerous=crash, so default is safe return
		if(1)
		{
			// Get ESP into EAX
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXESP), "");
			// Get _ESP_ into EBX
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEBXMEM4), "@$_ESP_");
			// Compare the values
			WriteASMLine(static_cast<DWORD>(ASMOp::CMPEAXEBX4), NULL);
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
	if(dwTask==static_cast<DWORD>(ASMTask::AddEsp))
	{
		WriteASMLine(static_cast<DWORD>(ASMOp::ADDESP), pP1->GetStr());
		WriteASMComment("ADD ESP", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::SubEsp))
	{
		WriteASMLine(static_cast<DWORD>(ASMOp::SUBESP), pP1->GetStr());
		WriteASMComment("SUB ESP", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::StoreEsp))
	{
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVMEMESP4), pP1->GetStr());
		WriteASMComment("STORE STACK IN MEM", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::RestoreEsp))
	{
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVESPMEM4), pP1->GetStr());
		WriteASMComment("RESTORE STACK FROM MEM", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::PushEbp))
	{
		WriteASMLine(static_cast<DWORD>(ASMOp::PUSHEBP), "");
		WriteASMComment("PUSH EBP", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::PushEsp))
	{
		WriteASMLine(static_cast<DWORD>(ASMOp::PUSHESP), "");
		WriteASMComment("PUSH ESP", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::PopEbp))
	{
		WriteASMLine(static_cast<DWORD>(ASMOp::POPEBP), "");
		WriteASMComment("POP EBP", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::MovBpEsp))
	{
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVEBPESP), "");
		WriteASMComment("MOV EBP ESP", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::MovSpEbp))
	{
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVESPEBP), "");
		WriteASMComment("MOV ESP EBP", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::PushRegisters))
	{
		// Program prologue boundary: no pending call frame crosses it.
		ResetPendingFrame();
		WriteASMLine(static_cast<DWORD>(ASMOp::PUSHAD), "");
		WriteASMComment("PUSH REGISTERS", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::PopRegisters))
	{
		ResetPendingFrame();
		WriteASMLine(static_cast<DWORD>(ASMOp::POPAD), "");
		WriteASMComment("POP REGISTERS", "", "", "");
	}	
	if(dwTask==static_cast<DWORD>(ASMTask::ClearStack))
	{
		// No pending call frame crosses a stack clear.
		ResetPendingFrame();

		// x64: zero [RSP, RSP+count) exactly with REP STOSB. The legacy
		// SIB[EAX:ECX*4]+LOOP structure couples the loop counter to the
		// addressing index (and a truncated EAX base), which cannot clear a
		// region in bounds on any architecture; REP STOSB uses independent
		// counter (ECX) and destination (EDI) registers and handles any byte
		// count. EDI is part of the saved register file (wave-3 prologue),
		// and ClearStack only ever runs inside the function prologue before
		// any body code, so clobbering it is safe.
		DWORD dwTotalToClear = pP1->GetDWORDRepresentation(1, NULL);

		CheckAndExpandMCBMemory();
		EmitRawByte(0x48); EmitRawByte(0x89); EmitRawByte(0xE0); // MOV RAX,RSP
		EmitRawByte(0x33); EmitRawByte(0xC0);                    // XOR EAX,EAX
		EmitRawByte(0x48); EmitRawByte(0x8B); EmitRawByte(0xFC); // MOV RDI,RSP
		EmitRawByte(0xB9);                                       // MOV ECX,imm32
		m_machineCodeBuffer.WriteDWORD(dwTotalToClear, 4);
		EmitRawByte(0xFC);                                       // CLD
		EmitRawByte(0xF3); EmitRawByte(0xAA);                    // REP STOSB

		// Comment
		WriteASMComment("CLEAR STACK", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::SetNoReturnIfEspLeak))
	{
		// Get ESP into EAX
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXESP), "");

		// Get _ESP_ into EBX
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVEBXMEM4), pP1->GetStr());

		// Compare the values
		WriteASMLine(static_cast<DWORD>(ASMOp::CMPEAXEBX4), NULL);

		// Jump over line that sets the flag
		WriteASMLine(static_cast<DWORD>(ASMOp::JE), "10");

		// Line that sets the flag to say 'no return'
		WriteASMLine2(static_cast<DWORD>(ASMOp::MOVMEMIMM4), "@$_ESC_", "3");

		// Comment
		WriteASMComment("FLAG NORETURN IF ESP<>STOREDESP", "", "", "");
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
		if(dwTask==static_cast<DWORD>(ASMTask::DebugStatementHook)) WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXIMM4), "[1");
		if(dwTask==static_cast<DWORD>(ASMTask::DebugJumpHook)) WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXIMM4), "[2");
		if(dwTask==static_cast<DWORD>(ASMTask::DebugReturnHook)) WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXIMM4), "[3");
		WriteASMLine(static_cast<DWORD>(ASMOp::CALLEAX), "");

		// Free stack items
		if(dwTask!=static_cast<DWORD>(ASMTask::DebugReturnHook))
		{
			WriteASMLine(static_cast<DWORD>(ASMOp::POPEBX), "");
			WriteASMLine(static_cast<DWORD>(ASMOp::POPEBX), "");
			WriteASMLine(static_cast<DWORD>(ASMOp::POPEBX), "");
			WriteASMLine(static_cast<DWORD>(ASMOp::POPEBX), "");
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
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXMEM4), "@$_ERR_");
			WriteASMLine(static_cast<DWORD>(ASMOp::CMPEAX4), "0");
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
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXMEM4), "@$_ERR_");
			WriteASMLine(static_cast<DWORD>(ASMOp::CMPEAX4), "0");
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
		pP2Off=NULL;
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
		// Wave 8: SSE2 float/double arithmetic — XMM0 op XMM1 -> XMM0.
		// Float-class types are {2,102} (MOVSS) and {8,108} (MOVSD).
		const bool bFloatMath = (dwTask==static_cast<DWORD>(ASMTask::Add)
			|| dwTask==static_cast<DWORD>(ASMTask::Sub)
			|| dwTask==static_cast<DWORD>(ASMTask::Mul)
			|| dwTask==static_cast<DWORD>(ASMTask::Div))
			&& (dwP1Type==2 || dwP1Type==102 || dwP1Type==8 || dwP1Type==108);
		if(bFloatMath)
		{
			const bool bDoubleClass = (dwP1Type==8 || dwP1Type==108);

			// Load operand B into XMM1 first (preserves A-B / A/B order).
			WriteASMXtoEAX(dwP2Mode, pP2, pP2Off, dwP2Type, dwP2Offset);
			if(bDoubleClass)
				WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDXMM1XMM0), "");
			else
			{
				WriteASMLine(static_cast<DWORD>(ASMOp::MOVDXMM0EAX), "");
				WriteASMLine(static_cast<DWORD>(ASMOp::MOVSSXMM1XMM0), "");
			}

			// Load operand A into XMM0.
			WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);
			if(!bDoubleClass)
				WriteASMLine(static_cast<DWORD>(ASMOp::MOVDXMM0EAX), "");

			// The operation itself.
			if(dwTask==static_cast<DWORD>(ASMTask::Add))
				WriteASMLine(static_cast<DWORD>(bDoubleClass ? ASMOp::ADDSDXMM0XMM1 : ASMOp::ADDSSXMM0XMM1), "");
			if(dwTask==static_cast<DWORD>(ASMTask::Sub))
				WriteASMLine(static_cast<DWORD>(bDoubleClass ? ASMOp::SUBSDXMM0XMM1 : ASMOp::SUBSSXMM0XMM1), "");
			if(dwTask==static_cast<DWORD>(ASMTask::Mul))
				WriteASMLine(static_cast<DWORD>(bDoubleClass ? ASMOp::MULSDXMM0XMM1 : ASMOp::MULSSXMM0XMM1), "");
			if(dwTask==static_cast<DWORD>(ASMTask::Div))
				WriteASMLine(static_cast<DWORD>(bDoubleClass ? ASMOp::DIVSDXMM0XMM1 : ASMOp::DIVSSXMM0XMM1), "");

			// Store the result. Doubles store XMM0 directly (MOVSD forms);
			// floats spill to the 4-byte temp then reuse the EAX store paths.
			if(bDoubleClass)
				WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);
			else
			{
				WriteASMLine(static_cast<DWORD>(ASMOp::MOVSSMEMXMM0), "@$_TEMPA_");
				WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXMEM4), "@$_TEMPA_");
				WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);
			}

			if(dwTask==static_cast<DWORD>(ASMTask::Add)) WriteASMComment("ADD", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::Sub)) WriteASMComment("SUB", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::Mul)) WriteASMComment("MUL", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::Div)) WriteASMComment("DIV", "", "", "");
			return true;
		}

		// Wave 21: float modulo via the CRT fmod primitive (msvcrt.dll) — the
		// ModFFF DLL row is `if(B==0) return 0; float(fmod((double)A,(double)B))`.
		// The guard tests the sign-stripped divisor bits so +0.0 and -0.0 both
		// return 0.0f while NaN divisors (nonzero mantissa) still reach fmod —
		// exactly the C `fValueB==0` semantics.
		if(dwTask==static_cast<DWORD>(ASMTask::Mod) && (dwP1Type==2 || dwP1Type==102))
		{
			CStr tempA("@$_TEMPA_"), tempB("@$_TEMPB_");

			// Guard: B == ±0.0f -> store 0.0f directly (leap markers 5/6 are
			// unused by the control-flow/array machinery).
			WriteASMXtoEAX(dwP2Mode, pP2, pP2Off, dwP2Type, dwP2Offset); // EAX = B bits
			WriteASMLine2IMM(static_cast<DWORD>(ASMOp::ANDEAX4), NULL, "2147483647", 2); // AND EAX,0x7FFFFFFF
			WriteASMLeapMarkerJump(static_cast<DWORD>(ASMOp::JNE), 5);
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXIMM4), "0");
			WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);
			WriteASMLeapMarkerJump(static_cast<DWORD>(ASMOp::JMP), 6);
			WriteASMLeapMarkerEnd(5);

			// Widen both operands to double and spill.
			WriteASMXtoEAX(dwP2Mode, pP2, pP2Off, dwP2Type, dwP2Offset);
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVDXMM0EAX), "");
			WriteASMLine(static_cast<DWORD>(ASMOp::CVTSS2SDXMM0XMM0), "");
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDMEMXMM0), tempB.GetStr());
			WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVDXMM0EAX), "");
			WriteASMLine(static_cast<DWORD>(ASMOp::CVTSS2SDXMM0XMM0), "");
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDMEMXMM0), tempA.GetStr());

			// TEMPA = fmod(TEMPA, TEMPB)
			EmitBinaryTranscendentalCall("fmod", tempA.GetStr(), tempB.GetStr());

			// P3 = (float)TEMPA (wave-8 spill-through-temp contract).
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDXMM0MEM), tempA.GetStr());
			WriteASMLine(static_cast<DWORD>(ASMOp::CVTSD2SSXMM0XMM0), "");
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVSSMEMXMM0), "@$_TEMPA_");
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXMEM4), "@$_TEMPA_");
			WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);
			WriteASMLeapMarkerEnd(6);
			WriteASMComment("MOD FLOAT (FMOD)", "", "", "");
			return true;
		}

		// Wave 8b: int64 (type 9) arithmetic — full-width REG64. Both operands
		// load as single 8-byte moves (WriteASMXtoEAX now emits MOVEAXMEM8 for
		// type 9); B rides RBX, A rides RAX, and the operation is a REX.W
		// instruction in place (CQO+IDIV for division, MOV RAX,RDX for the
		// remainder). P3 stores back through the 8-byte WriteASMEAXtoX path.
		const bool bInt64Math = (dwP1Type==9 || dwP1Type==109)
			&& (dwTask==static_cast<DWORD>(ASMTask::Add)
			|| dwTask==static_cast<DWORD>(ASMTask::Sub)
			|| dwTask==static_cast<DWORD>(ASMTask::Mul)
			|| dwTask==static_cast<DWORD>(ASMTask::Div)
			|| dwTask==static_cast<DWORD>(ASMTask::Mod));
		if(bInt64Math)
		{
			// Load operand B into RBX first (preserves A-B / A/B order).
			WriteASMXtoEAX(dwP2Mode, pP2, pP2Off, dwP2Type, dwP2Offset);
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEBXRAX8), "");

			// Load operand A into RAX.
			WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);

			// Div/Mod guard: zero divisor -> runtime error 119 (same shape as
			// the int path; zero is zero in any width).
			if(dwTask==static_cast<DWORD>(ASMTask::Div) || dwTask==static_cast<DWORD>(ASMTask::Mod))
			{
				WriteASMLine(static_cast<DWORD>(ASMOp::CMPEBX4), "0");
				WriteASMLine(static_cast<DWORD>(ASMOp::JNE), "15");
				WriteASMLine2(static_cast<DWORD>(ASMOp::MOVMEMIMM4), "@$_ERR_", "119");
				WriteASMLine(static_cast<DWORD>(ASMOp::JMP), "3");
				WriteASMLine(static_cast<DWORD>(ASMOp::CQO), "");
				WriteASMLine(static_cast<DWORD>(ASMOp::DIVEAXEBX8), "");
				if(dwTask==static_cast<DWORD>(ASMTask::Mod))
					WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXEDX8), "");
			}
			else
			{
				// The operation itself.
				if(dwTask==static_cast<DWORD>(ASMTask::Add))
					WriteASMLine(static_cast<DWORD>(ASMOp::ADDEAXEBX8), "");
				if(dwTask==static_cast<DWORD>(ASMTask::Sub))
					WriteASMLine(static_cast<DWORD>(ASMOp::SUBEAXEBX8), "");
				if(dwTask==static_cast<DWORD>(ASMTask::Mul))
					WriteASMLine(static_cast<DWORD>(ASMOp::MULEAXEBX8), "");
			}

			// Store the result (8-byte store via the type-9 path).
			WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);

			if(dwTask==static_cast<DWORD>(ASMTask::Add)) WriteASMComment("ADD", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::Sub)) WriteASMComment("SUB", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::Mul)) WriteASMComment("MUL", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::Div)) WriteASMComment("DIV", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::Mod)) WriteASMComment("MOD", "", "", "");
			return true;
		}

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

				// mov eax,[a]
				WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);

				switch(dwTask)
				{
					case static_cast<DWORD>(ASMTask::Add):
					{
						// add eax,imm
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::ADDEAX1),dwP1Type);
						DWORD dwIMMSize=dwCorrectASMCode-static_cast<DWORD>(ASMOp::ADDEAX1);
						WriteASMLine2IMM(dwCorrectASMCode, NULL, pP2->GetStr(), dwIMMSize);
					}
					break;

					case static_cast<DWORD>(ASMTask::Sub):
					{
						// sub eax,imm
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::SUBEAX1),dwP1Type);
						DWORD dwIMMSize=dwCorrectASMCode-static_cast<DWORD>(ASMOp::SUBEAX1);
						WriteASMLine2IMM(dwCorrectASMCode, NULL, pP2->GetStr(), dwIMMSize);
					}
					break;

					case static_cast<DWORD>(ASMTask::Mul):
					{
						// mov ebx,imm + mul eax,ebx
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEBXIMM1),dwP1Type);
						DWORD dwIMMSize=dwCorrectASMCode-static_cast<DWORD>(ASMOp::MOVEBXIMM1);
						WriteASMLine2IMM(dwCorrectASMCode, NULL, pP2->GetStr(), dwIMMSize);
						dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MULEAXEBX1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");
					}
					break;

					case static_cast<DWORD>(ASMTask::Div):
					case static_cast<DWORD>(ASMTask::Mod):
					{
						// mov ebx,imm + div eax,ebx
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEBXIMM1),dwP1Type);
						DWORD dwIMMSize=dwCorrectASMCode-static_cast<DWORD>(ASMOp::MOVEBXIMM1);
						WriteASMLine2IMM(dwCorrectASMCode, NULL, pP2->GetStr(), dwIMMSize);

						// leefix - 250604 - u54 - avoid division by zero with RT error
						WriteASMLine(static_cast<DWORD>(ASMOp::CMPEBX4), "0");
						WriteASMLine(static_cast<DWORD>(ASMOp::JNE), "15");

						// runtime error if not leaped over
						WriteASMLine2(static_cast<DWORD>(ASMOp::MOVMEMIMM4), "@$_ERR_", "119");
						WriteASMLine(static_cast<DWORD>(ASMOp::JMP), "3");

						// actual division
						WriteASMLine(static_cast<DWORD>(ASMOp::CDQ), "");
						dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::DIVEAXEBX1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");

						// mod takes only the remainder
						if(dwTask==static_cast<DWORD>(ASMTask::Mod))
						{
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXEDX1),dwP1Type);
							WriteASMLine(dwCorrectASMCode, "");
						}
					}
					break;

					case static_cast<DWORD>(ASMTask::And):
					{
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::ANDEAX1),dwP1Type);
						DWORD dwIMMSize=dwCorrectASMCode-static_cast<DWORD>(ASMOp::ANDEAX1);
						WriteASMLine2IMM(dwCorrectASMCode, NULL, pP2->GetStr(), dwIMMSize);
					}
					break;

					case static_cast<DWORD>(ASMTask::Or):
					{
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::OREAX1),dwP1Type);
						DWORD dwIMMSize=dwCorrectASMCode-static_cast<DWORD>(ASMOp::OREAX1);
						WriteASMLine2IMM(dwCorrectASMCode, NULL, pP2->GetStr(), dwIMMSize);
					}
					break;

					case static_cast<DWORD>(ASMTask::Not):
					{
						// NOT is a unary boolean operator. Emit a self-contained
						// logical normalization so compound IF/loop conditions work:
						//   CMP EAX, 0     ; ZF=1 when operand is logically false
						//   MOV EAX, 0     ; clear result, preserving flags
						//   SETE AL        ; AL=1 when operand==0, else 0
						WriteASMLine(static_cast<DWORD>(ASMOp::CMPEAX4), "0");
						WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXIMM4), "0");
						WriteASMLine(static_cast<DWORD>(ASMOp::SETE), "");
					}
					break;

					case static_cast<DWORD>(ASMTask::BitNot):
					{
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::NOTEAX1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");
					}
					break;


					case static_cast<DWORD>(ASMTask::Xor):
					{
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::XOREAX1),dwP1Type);
						DWORD dwIMMSize=dwCorrectASMCode-static_cast<DWORD>(ASMOp::XOREAX1);
						WriteASMLine2IMM(dwCorrectASMCode, NULL, pP2->GetStr(), dwIMMSize);
					}
					break;

					case static_cast<DWORD>(ASMTask::Shl):
					{
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::SHLEAX1),dwP1Type);
						DWORD dwIMMSize=0;//can only be IMM8
						WriteASMLine2IMM(dwCorrectASMCode, NULL, pP2->GetStr(), dwIMMSize);
					}
					break;

					case static_cast<DWORD>(ASMTask::Shr):
					{
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::SHREAX1),dwP1Type);
						DWORD dwIMMSize=0;//can only be IMM8
						WriteASMLine2IMM(dwCorrectASMCode, NULL, pP2->GetStr(), dwIMMSize);
					}
					break;
				}

				// mov [r],eax
				WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);
			}
			else
			{
				// Where DIV/MOD task, all 4bytes of EBX are CMP'd so clear EAX now
				if(dwTask==static_cast<DWORD>(ASMTask::Div) || dwTask==static_cast<DWORD>(ASMTask::Mod))
				{
					// and only if sub-4byte EAX op used
					if(dwP2Type==4 || dwP2Type==5 || dwP2Type==6)//bool,byte,word only
					{
						WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXIMM4), "0");
					}
				}

				// mov eax,[b]
				WriteASMXtoEAX(dwP2Mode, pP2, pP2Off, dwP2Type, dwP2Offset);

				// mov [b] to stack
				WriteASMLine(static_cast<DWORD>(ASMOp::PUSHEAX), "");

				// mov eax,[a]
				WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);

				// put [b] into EBX from stack
				WriteASMLine(static_cast<DWORD>(ASMOp::POPEBX), "");

				switch(dwTask)
				{
					case static_cast<DWORD>(ASMTask::Add):
					{
						// add eax,ebx
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::ADDEAXEBX1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");
					}
					break;

					case static_cast<DWORD>(ASMTask::Sub):
					{
						// sub eax,ebx
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::SUBEAXEBX1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");
					}
					break;

					case static_cast<DWORD>(ASMTask::Mul):
					{
						// mul eax,ebx
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MULEAXEBX1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");
					}
					break;

					case static_cast<DWORD>(ASMTask::Div):
					case static_cast<DWORD>(ASMTask::Mod):
					{
						// avoid divide by zero
// leefix - 350604 - old way was silent skip, new way is runtime error
//						WriteASMLine(static_cast<DWORD>(ASMOp::CMPEBX4), "0");
//						WriteASMLine(static_cast<DWORD>(ASMOp::JE), "3");
						WriteASMLine(static_cast<DWORD>(ASMOp::CMPEBX4), "0");
						WriteASMLine(static_cast<DWORD>(ASMOp::JNE), "15");

						// runtime error if not leaped over
						WriteASMLine2(static_cast<DWORD>(ASMOp::MOVMEMIMM4), "@$_ERR_", "119");
						WriteASMLine(static_cast<DWORD>(ASMOp::JMP), "3");
						
						// div eax,ebx
						WriteASMLine(static_cast<DWORD>(ASMOp::CDQ), "");
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::DIVEAXEBX1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");

						// mod takes only the remainder
						if(dwTask==static_cast<DWORD>(ASMTask::Mod))
						{
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXEDX1),dwP1Type);
							WriteASMLine(dwCorrectASMCode, "");
						}
					}
					break;

					case static_cast<DWORD>(ASMTask::And):
					{
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::ANDEAXEBX1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");
					}
					break;

					case static_cast<DWORD>(ASMTask::Or):
					{
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::OREAXEBX1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");
					}
					break;

					case static_cast<DWORD>(ASMTask::Not):
					{
						// Same value-local normalization as the P2-immediate path:
						// CMP EAX,0 / MOV EAX,0 / SETE AL.
						WriteASMLine(static_cast<DWORD>(ASMOp::CMPEAX4), "0");
						WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXIMM4), "0");
						WriteASMLine(static_cast<DWORD>(ASMOp::SETE), "");
					}
					break;

					case static_cast<DWORD>(ASMTask::BitNot):
					{
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::NOTEAX1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");
					}
					break;

					case static_cast<DWORD>(ASMTask::Xor):
					{
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::XOREAXEBX1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");
					}
					break;

					case static_cast<DWORD>(ASMTask::Shl):
					{
						// mov ebx to CL (ecx byte part)
						WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXEBX4), "");

						// do shift 0-31 limit
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::SHLEAXCLC1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");
					}
					break;

					case static_cast<DWORD>(ASMTask::Shr):
					{
						// mov ebx to CL (ecx byte part)
						WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXEBX4), "");

						// do shift 0-31 limit
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::SHREAXCLC1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");
					}
					break;
				}

				// mov [r],eax
				WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);
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
		// Wave 8: SSE2 float/double compare — UCOMIS* + SETcc. UCOMIS* flags
		// are subtraction-like (CF=below, ZF=equal, PF=unordered). Ordered
		// predicates (SETB/SETBE/SETA/SETAE) ignore the unordered bit so NaN
		// yields false; EQ/NE/LT/LE mask it explicitly (SET* AL; SETNP/SETP AH;
		// AND/OR AL,AH) so NaN != NaN and NaN !< x hold.
		const bool bFloatCompare = (dwP1Type==2 || dwP1Type==102 || dwP1Type==8 || dwP1Type==108);
		if(bFloatCompare)
		{
			const bool bDoubleClass = (dwP1Type==8 || dwP1Type==108);

			// Load B into XMM1.
			WriteASMXtoEAX(dwP2Mode, pP2, pP2Off, dwP2Type, dwP2Offset);
			if(bDoubleClass)
				WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDXMM1XMM0), "");
			else
			{
				WriteASMLine(static_cast<DWORD>(ASMOp::MOVDXMM0EAX), "");
				WriteASMLine(static_cast<DWORD>(ASMOp::MOVSSXMM1XMM0), "");
			}

			// Load A into XMM0.
			WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);
			if(!bDoubleClass)
				WriteASMLine(static_cast<DWORD>(ASMOp::MOVDXMM0EAX), "");

			// MOV EAX,0 clears the result without touching the flags.
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXIMM4), "0");
			WriteASMLine(static_cast<DWORD>(bDoubleClass ? ASMOp::UCOMISDXMM0XMM1 : ASMOp::UCOMISSXMM0XMM1), "");

			switch(dwTask)
			{
				case static_cast<DWORD>(ASMTask::Equal):
					WriteASMLine(static_cast<DWORD>(ASMOp::SETE), "");
					WriteASMLine(static_cast<DWORD>(ASMOp::SETNP), "");
					WriteASMLine(static_cast<DWORD>(ASMOp::ANDALAH), "");
					break;
				case static_cast<DWORD>(ASMTask::NotEqual):
					WriteASMLine(static_cast<DWORD>(ASMOp::SETNE), "");
					WriteASMLine(static_cast<DWORD>(ASMOp::SETP), "");
					WriteASMLine(static_cast<DWORD>(ASMOp::ORALAH), "");
					break;
				case static_cast<DWORD>(ASMTask::Greater):
					WriteASMLine(static_cast<DWORD>(ASMOp::SETA), "");
					break;
				case static_cast<DWORD>(ASMTask::GreaterEqual):
					WriteASMLine(static_cast<DWORD>(ASMOp::SETAE), "");
					break;
				case static_cast<DWORD>(ASMTask::Less):
					WriteASMLine(static_cast<DWORD>(ASMOp::SETB), "");
					WriteASMLine(static_cast<DWORD>(ASMOp::SETNP), "");
					WriteASMLine(static_cast<DWORD>(ASMOp::ANDALAH), "");
					break;
				case static_cast<DWORD>(ASMTask::LessEqual):
					WriteASMLine(static_cast<DWORD>(ASMOp::SETBE), "");
					WriteASMLine(static_cast<DWORD>(ASMOp::SETNP), "");
					WriteASMLine(static_cast<DWORD>(ASMOp::ANDALAH), "");
					break;
			}

			// Store the 0/1 result (comparisons always return integer).
			WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);

			if(dwTask==static_cast<DWORD>(ASMTask::Equal)) WriteASMComment("EQUAL", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::NotEqual)) WriteASMComment("NOTEQUAL", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::Greater)) WriteASMComment("GREATER", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::GreaterEqual)) WriteASMComment("GREATEREQUAL", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::Less)) WriteASMComment("LESS", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::LessEqual)) WriteASMComment("LESSEQUAL", "", "", "");
			return true;
		}

		// Wave 8b: int64 (type 9) compare — full-width REG64. Mirrors the int
		// path's operand order exactly (P2 -> RDX via stack, P1 -> RBX,
		// CMP RDX,RBX) so SETcc semantics are unchanged; only the register
		// width and the 8-byte loads differ.
		const bool bInt64Compare = (dwP1Type==9 || dwP1Type==109)
			&& (dwTask==static_cast<DWORD>(ASMTask::Equal)
			|| dwTask==static_cast<DWORD>(ASMTask::NotEqual)
			|| dwTask==static_cast<DWORD>(ASMTask::Greater)
			|| dwTask==static_cast<DWORD>(ASMTask::GreaterEqual)
			|| dwTask==static_cast<DWORD>(ASMTask::Less)
			|| dwTask==static_cast<DWORD>(ASMTask::LessEqual));
		if(bInt64Compare)
		{
			// mov rax,[b] (P2, 8-byte) -> push to stack
			WriteASMXtoEAX(dwP2Mode, pP2, pP2Off, dwP2Type, dwP2Offset);
			WriteASMLine(static_cast<DWORD>(ASMOp::PUSHEAX), "");

			// mov rax,[a] (P1, 8-byte) -> RBX
			WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEBXRAX8), "");

			// pop [b] from stack to RDX
			WriteASMLine(static_cast<DWORD>(ASMOp::POPEDX), "");

			// cmp and setcc
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXIMM4), "0");
			WriteASMLine(static_cast<DWORD>(ASMOp::CMPEDXEBX8), "");
			switch(dwTask)
			{
				case static_cast<DWORD>(ASMTask::Equal): WriteASMLine(static_cast<DWORD>(ASMOp::SETE), ""); break;
				case static_cast<DWORD>(ASMTask::NotEqual): WriteASMLine(static_cast<DWORD>(ASMOp::SETNE), ""); break;
				case static_cast<DWORD>(ASMTask::Greater): WriteASMLine(static_cast<DWORD>(ASMOp::SETG), ""); break;
				case static_cast<DWORD>(ASMTask::GreaterEqual): WriteASMLine(static_cast<DWORD>(ASMOp::SETGE), ""); break;
				case static_cast<DWORD>(ASMTask::Less): WriteASMLine(static_cast<DWORD>(ASMOp::SETL), ""); break;
				case static_cast<DWORD>(ASMTask::LessEqual): WriteASMLine(static_cast<DWORD>(ASMOp::SETLE), ""); break;
			}

			// mov [r],eax (comparisons always return integer)
			WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);

			if(dwTask==static_cast<DWORD>(ASMTask::Equal)) WriteASMComment("EQUAL", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::NotEqual)) WriteASMComment("NOTEQUAL", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::Greater)) WriteASMComment("GREATER", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::GreaterEqual)) WriteASMComment("GREATEREQUAL", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::Less)) WriteASMComment("LESS", "", "", "");
			if(dwTask==static_cast<DWORD>(ASMTask::LessEqual)) WriteASMComment("LESSEQUAL", "", "", "");
			return true;
		}

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

				// mov eax,imm
				DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXIMM1),dwP1Type);
				DWORD dwIMMSize=dwCorrectASMCode-static_cast<DWORD>(ASMOp::MOVEAXIMM1);
				WriteASMLine2IMM(dwCorrectASMCode, NULL, pP2->GetStr(), dwIMMSize);
			}
			else
			{
				// mov eax,[b]
				WriteASMXtoEAX(dwP2Mode, pP2, pP2Off, dwP2Type, dwP2Offset);
			}

			// push [b] to stack
			WriteASMLine(static_cast<DWORD>(ASMOp::PUSHEAX), "");

			// mov eax,[a]
			WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);

			// mov ebx,eax
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEBXEAX4), "");

			// pop [b] from stack to EDX
			WriteASMLine(static_cast<DWORD>(ASMOp::POPEDX), "");

			// cmp and setcc
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXIMM4), "0");
			DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::CMPEDXEBX1),dwP1Type);
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

			// mov [r],eax
			WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);
		}

		// Comment
		if(dwTask==static_cast<DWORD>(ASMTask::Equal)) WriteASMComment("EQUAL", "", "", "");
		if(dwTask==static_cast<DWORD>(ASMTask::NotEqual)) WriteASMComment("NOTEQUAL", "", "", "");
		if(dwTask==static_cast<DWORD>(ASMTask::Greater)) WriteASMComment("GREATER", "", "", "");
		if(dwTask==static_cast<DWORD>(ASMTask::GreaterEqual)) WriteASMComment("GREATEREQUAL", "", "", "");
		if(dwTask==static_cast<DWORD>(ASMTask::Less)) WriteASMComment("LESS", "", "", "");
		if(dwTask==static_cast<DWORD>(ASMTask::LessEqual)) WriteASMComment("LESSEQUAL", "", "", "");
	}

	if(dwTask==static_cast<DWORD>(ASMTask::CastIntToFloat) || dwTask==static_cast<DWORD>(ASMTask::CastIntToDouble)
	|| dwTask==static_cast<DWORD>(ASMTask::CastFloatToInt) || dwTask==static_cast<DWORD>(ASMTask::CastFloatToDouble)
	|| dwTask==static_cast<DWORD>(ASMTask::CastDoubleToInt) || dwTask==static_cast<DWORD>(ASMTask::CastDoubleToFloat))
	{
		// Wave 8: SSE2 int<->float conversions. P1 is the source value; P3 the
		// result location (its type drives the store width).
		switch(dwTask)
		{
			case static_cast<DWORD>(ASMTask::CastIntToFloat):
				WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);
				WriteASMLine(static_cast<DWORD>(ASMOp::CVTSI2SSXMM0EAX), "");
				WriteASMLine(static_cast<DWORD>(ASMOp::MOVSSMEMXMM0), "@$_TEMPA_");
				WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXMEM4), "@$_TEMPA_");
				WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);
				break;
			case static_cast<DWORD>(ASMTask::CastIntToDouble):
				WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);
				WriteASMLine(static_cast<DWORD>(ASMOp::CVTSI2SDXMM0EAX), "");
				WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);
				break;
			case static_cast<DWORD>(ASMTask::CastFloatToInt):
				WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);
				WriteASMLine(static_cast<DWORD>(ASMOp::MOVDXMM0EAX), "");
				WriteASMLine(static_cast<DWORD>(ASMOp::CVTTSS2SIEAXXMM0), "");
				WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);
				break;
			case static_cast<DWORD>(ASMTask::CastFloatToDouble):
				WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);
				WriteASMLine(static_cast<DWORD>(ASMOp::MOVDXMM0EAX), "");
				WriteASMLine(static_cast<DWORD>(ASMOp::CVTSS2SDXMM0XMM0), "");
				WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);
				break;
			case static_cast<DWORD>(ASMTask::CastDoubleToInt):
				WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);
				WriteASMLine(static_cast<DWORD>(ASMOp::CVTTSD2SIEAXXMM0), "");
				WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);
				break;
			case static_cast<DWORD>(ASMTask::CastDoubleToFloat):
				WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);
				WriteASMLine(static_cast<DWORD>(ASMOp::CVTSD2SSXMM0XMM0), "");
				WriteASMLine(static_cast<DWORD>(ASMOp::MOVSSMEMXMM0), "@$_TEMPA_");
				WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXMEM4), "@$_TEMPA_");
				WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);
				break;
		}
		WriteASMComment("CAST", "", "", "");
	}

	// Wave 15: integer-family widening to int64 (REG64, no DLL). P1 is the
	// source value; P3 the int64 result location.
	if(dwTask==static_cast<DWORD>(ASMTask::CastIntToInt64))
	{
		// int -> int64: sign-extend (matches ?CastLtoR@@ semantics).
		WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVSXDRAXEAX), "");
		WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);
		WriteASMComment("CAST INT TO INT64", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::CastDwordToInt64))
	{
		// dword/byte/word -> int64: writing EAX zero-extends to RAX; a 107
		// (address-of) source is already read at full QWORD width (wave 14).
		// No conversion instruction needed — just load and store 8 bytes.
		WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);
		WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);
		WriteASMComment("CAST DWORD TO INT64", "", "", "");
	}

	// Wave 16: int64 <-> float/double conversions (SSE2 CVT* with REX.W;
	// legacy F2/F3 prefix precedes the REX byte).
	if(dwTask==static_cast<DWORD>(ASMTask::CastFloatToInt64))
	{
		// float -> int64: F3 48 0F 2C C0 (CVTTSS2SI RAX,XMM0).
		WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset); // float bits in EAX
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVDXMM0EAX), "");
		WriteASMLine(static_cast<DWORD>(ASMOp::CVTTSS2SIRAXXMM0), "");
		WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset); // 8-byte store
		WriteASMComment("CAST FLOAT TO INT64", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::CastDoubleToInt64))
	{
		// double -> int64: F2 48 0F 2C C0 (CVTTSD2SI RAX,XMM0).
		WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset); // double in XMM0
		WriteASMLine(static_cast<DWORD>(ASMOp::CVTTSD2SIRAXXMM0), "");
		WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset); // 8-byte store
		WriteASMComment("CAST DOUBLE TO INT64", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::CastInt64ToLower))
	{
		// int64 -> int/dword/byte/word: full-width load into RAX, then a
		// truncating store at the target width (WriteASMEAXtoX follows P3).
		WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);
		WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);
		WriteASMComment("CAST INT64 TO LOWER", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::CastInt64ToFloat))
	{
		// int64 -> float: F3 48 0F 2A C0 (CVTSI2SS XMM0,RAX); float results
		// round-trip through the temp slot (same contract as wave 8).
		WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset); // int64 in RAX
		WriteASMLine(static_cast<DWORD>(ASMOp::CVTSI2SSXMM0RAX), "");
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVSSMEMXMM0), "@$_TEMPA_");
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXMEM4), "@$_TEMPA_");
		WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset); // 4-byte store
		WriteASMComment("CAST INT64 TO FLOAT", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::CastInt64ToDouble))
	{
		// int64 -> double: F2 48 0F 2A C0 (CVTSI2SD XMM0,RAX); the double
		// store path emits MOVSD [dst],XMM0 from XMM0 (wave 8 contract).
		WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset); // int64 in RAX
		WriteASMLine(static_cast<DWORD>(ASMOp::CVTSI2SDXMM0RAX), "");
		WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset); // 8-byte store
		WriteASMComment("CAST INT64 TO DOUBLE", "", "", "");
	}

	// Wave 17: Power (x^y) built from exp/log primitives — no dbprocore call.
	// x^y = exp(y * log(x)) computed in double precision using the CRT exp/log
	// primitives (msvcrt.dll) resolved through the command table. The double
	// intermediate preserves the DLL's own semantics ((float)pow((double)a,(double)b)
	// and (int)pow((long double)a,(long double)b)); int results truncate.
	// Wave 20 extends the same block to the rest of the family (B/Y/W/D/O/R)
	// with MOVZX for unsigned byte/word sources, REX.W conversions for int64,
	// and the (float) round-trip the PowerOOO DLL applies before storing.
	if(dwTask==static_cast<DWORD>(ASMTask::Power))
	{
		CStr tempA("@$_TEMPA_"), tempB("@$_TEMPB_");

		// Widen one operand to double in XMM0 and spill it to its temp slot.
		//   float:         MOVD XMM0,EAX + CVTSS2SD
		//   byte/word:     MOVZX (unsigned 0-255 / 0-65535) + CVTSI2SD XMM0,EAX
		//   int64:         the load already produced RAX -> CVTSI2SD XMM0,RAX
		//   int/dword:     CVTSI2SD XMM0,EAX
		//   double:        already in XMM0
		const auto widenToDouble = [this](CStr* pP, CStr* pPOff, DWORD dwType,
		                                  DWORD dwOffset, DWORD dwMode,
		                                  LPSTR pTemp) {
			WriteASMXtoEAX(dwMode, pP, pPOff, dwType, dwOffset);
			const bool bFloat  = (dwType==2 || dwType==102);
			const bool bDouble = (dwType==8 || dwType==108);
			const bool bInt64  = (dwType==9 || dwType==109);
			const bool bWord   = (dwType==6 || dwType==106);
			const bool bByte   = (dwType==4 || dwType==5 || dwType==104 || dwType==105);
			if(bFloat)
			{
				WriteASMLine(static_cast<DWORD>(ASMOp::MOVDXMM0EAX), "");
				WriteASMLine(static_cast<DWORD>(ASMOp::CVTSS2SDXMM0XMM0), "");
			}
			else if(bByte || bWord)
			{
				WriteASMLine(static_cast<DWORD>(bWord ? ASMOp::MOVZXEAXAX : ASMOp::MOVZXEAXAL), "");
				WriteASMLine(static_cast<DWORD>(ASMOp::CVTSI2SDXMM0EAX), "");
			}
			else if(bInt64)
				WriteASMLine(static_cast<DWORD>(ASMOp::CVTSI2SDXMM0RAX), "");
			else if(!bDouble)
				WriteASMLine(static_cast<DWORD>(ASMOp::CVTSI2SDXMM0EAX), "");
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDMEMXMM0), pTemp);
		};

		// TEMPA = (double)x ; TEMPB = (double)y.
		widenToDouble(pP1, pP1Off, dwP1Type, dwP1Offset, dwP1Mode, tempA.GetStr());
		widenToDouble(pP2, pP2Off, dwP2Type, dwP2Offset, dwP2Mode, tempB.GetStr());
		// TEMPA = log(TEMPA)
		EmitTranscendentalCall("log", tempA.GetStr());

		// TEMPA = TEMPA * TEMPB  (MULSD XMM0,XMM1)
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDXMM0MEM), tempA.GetStr());
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDXMM1XMM0), "");
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDXMM0MEM), tempB.GetStr());
		WriteASMLine(static_cast<DWORD>(ASMOp::MULSDXMM0XMM1), "");
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDMEMXMM0), tempA.GetStr());

		// TEMPA = exp(TEMPA)
		EmitTranscendentalCall("exp", tempA.GetStr());

		// P3 = (target-type) TEMPA
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDXMM0MEM), tempA.GetStr());
		if(dwP3Type==2 || dwP3Type==102)
		{
			// float result: narrow and spill through the 4-byte temp.
			WriteASMLine(static_cast<DWORD>(ASMOp::CVTSD2SSXMM0XMM0), "");
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVSSMEMXMM0), "@$_TEMPA_");
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXMEM4), "@$_TEMPA_");
			WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);
		}
		else if(dwP3Type==8 || dwP3Type==108)
		{
			// double result: round through float precision — the PowerOOO DLL
			// is double result = (float)pow(a,b) — then the store path emits
			// MOVSD [dst],XMM0.
			WriteASMLine(static_cast<DWORD>(ASMOp::CVTSD2SSXMM0XMM0), "");
			WriteASMLine(static_cast<DWORD>(ASMOp::CVTSS2SDXMM0XMM0), "");
			WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);
		}
		else if(dwP3Type==9 || dwP3Type==109)
		{
			// int64 result: REX.W truncating conversion (matches (LONGLONG)
			// cast of the truncated double), then an 8-byte store.
			WriteASMLine(static_cast<DWORD>(ASMOp::CVTTSD2SIRAXXMM0), "");
			WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);
		}
		else
		{
			// integer-family result (L/D/B/Y/W): truncating double->int, store
			// at width — the low-byte/word store matches the (unsigned char)/
			// (WORD)/(DWORD) truncation of the truncated int (wave-18 pattern).
			WriteASMLine(static_cast<DWORD>(ASMOp::CVTTSD2SIEAXXMM0), "");
			WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);
		}
		WriteASMComment("POWER (EXP/LOG)", "", "", "");
	}

	// Wave 18: narrowing casts to byte/word/dword (no dbprocore).
	if(dwTask==static_cast<DWORD>(ASMTask::CastToNarrow))
	{
		// Integer-family source (L/D) -> byte/word/dword target: the store
		// width does the truncation (MOV [dst],AL / AX / EAX) — exact match
		// for the (unsigned char)/(WORD)/(DWORD) C++ casts; L->D / D->L are
		// same-width 4-byte moves.
		WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);
		WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);
		WriteASMComment("CAST TO NARROW", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::CastFloatToNarrow))
	{
		// Float-family source (F/O) -> byte/word target: the truncating
		// CVTTSS2SI/CVTTSD2SI (wave-8 pattern) converts to int, then the
		// target-width store truncates the low byte/word — exact match for
		// the (unsigned char)/(WORD) casts of float/double values.
		const bool bDoubleClass = (dwP1Type==8 || dwP1Type==108);
		WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);
		if(!bDoubleClass)
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVDXMM0EAX), "");
		WriteASMLine(static_cast<DWORD>(bDoubleClass ? ASMOp::CVTTSD2SIEAXXMM0 : ASMOp::CVTTSS2SIEAXXMM0), "");
		WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);
		WriteASMComment("CAST FLOAT TO NARROW", "", "", "");
	}

	// Wave 19: widening casts from byte/word (no dbprocore). B/Y/W sources
	// are unsigned (the Y rows share the B DLL entries); the emitter loads
	// them at width with MOV AL/AX which preserves the upper bits of EAX,
	// so an explicit MOVZX precedes the wider store or CVT* conversion.
	if(dwTask==static_cast<DWORD>(ASMTask::CastWiden))
	{
		// B/Y/W -> L/W/D: width load, MOVZX to 32 bits, target-width store.
		const bool bWordSource = (dwP1Type==6 || dwP1Type==106);
		WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);
		WriteASMLine(static_cast<DWORD>(bWordSource ? ASMOp::MOVZXEAXAX : ASMOp::MOVZXEAXAL), "");
		WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);
		WriteASMComment("CAST WIDEN", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::CastWidenToFloat))
	{
		// B/Y/W -> F/O: width load, MOVZX, then the wave-8 CVTSI2SS/CVTSI2SD
		// conversion; float results round-trip through the 4-byte temp slot
		// (same contract as CastIntToFloat).
		const bool bWordSource = (dwP1Type==6 || dwP1Type==106);
		WriteASMXtoEAX(dwP1Mode, pP1, pP1Off, dwP1Type, dwP1Offset);
		WriteASMLine(static_cast<DWORD>(bWordSource ? ASMOp::MOVZXEAXAX : ASMOp::MOVZXEAXAL), "");
		if(dwP3Type==2 || dwP3Type==102)
		{
			WriteASMLine(static_cast<DWORD>(ASMOp::CVTSI2SSXMM0EAX), "");
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVSSMEMXMM0), "@$_TEMPA_");
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXMEM4), "@$_TEMPA_");
		}
		else
		{
			// double target: the store path emits MOVSD [dst],XMM0 directly.
			WriteASMLine(static_cast<DWORD>(ASMOp::CVTSI2SDXMM0EAX), "");
		}
		WriteASMEAXtoX(dwP3Mode, pP3, pP3Off, dwP3Type, dwP3Offset);
		WriteASMComment("CAST WIDEN TO FLOAT", "", "", "");
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
	
//		// When return, EAX holds the stack-ptr-adjustment value
//		WriteASMLine(static_cast<DWORD>(ASMOp::SUBESPEAX), NULL);

//		// Reload registers with all values as it was before we left
//		WriteASMLine(static_cast<DWORD>(ASMOp::POPAD), "");

		// Jump to breakpoint
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVEBXMEM4), "@$_REK_");
		WriteASMLine2(static_cast<DWORD>(ASMOp::MOVMEMIMM4), "@$_REK_", "0");
		WriteASMLine(static_cast<DWORD>(ASMOp::JMPEBX), 0);

		// Complete LEAP-FORWARD Marker (jump here if skip breakpoint resume)
		g_pASMWriter->WriteASMLeapMarkerEnd(0);

		// Comment on this task
		WriteASMComment("BREAKPOINT RESUME", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::PushInternalArrayIndex))
	{
		// Find Array location (in EAX)
		if(dwP1Mode==static_cast<DWORD>(ParamMode::MemArr)) WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXMEM4), pP1->GetStr());
		if(dwP1Mode==static_cast<DWORD>(ParamMode::EbpArr)) WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXEBP4), pP1->GetStr()+2);

		/* old code crashes when safe array switched off and A() used instead of A( ) when A was not DIMMED
		// If Array Check Active
		if(GetArrayCheckFlag())
		{
			// Make Sure Array Exists
			WriteASMLine(static_cast<DWORD>(ASMOp::CMPEAX4), "0");

			// Leap Marker OpCode
			WriteASMLeapMarkerJump(static_cast<DWORD>(ASMOp::JE), 4);
		}

		// Get array index from -4 location within array header - put in EDX
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVEDXEAXOFF4), "-4");

		// Copy Push EDX to stack
		WriteASMLine(static_cast<DWORD>(ASMOp::PUSHEDX), "");

		// If Array Check Active
		if(GetArrayCheckFlag())
		{
			// Complete Leap Marker (so we jump here)
			WriteASMLeapMarkerEnd(4);
		}
		*/

		// Force a check of the array as we NEED it to find the array index (safe arrays or no)
		WriteASMLine(static_cast<DWORD>(ASMOp::CMPEAX4), "0");
		WriteASMLeapMarkerJump(static_cast<DWORD>(ASMOp::JE), 4);
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVEDXEAXOFF4), "-4");
		WriteASMLine(static_cast<DWORD>(ASMOp::PUSHEDX), "");
		WriteASMLeapMarkerEnd(4);

		// Comment on this task
		WriteASMComment("PUSH INT. ARRAY INDEX TO STACK", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::CalcArrayOffset))
	{
		// Find the array header in EAX (8-byte address slot on x64).
		if(dwP2Mode==static_cast<DWORD>(ParamMode::MemArr))
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXMEM8), pP2->GetStr());
		if(dwP2Mode==static_cast<DWORD>(ParamMode::EbpArr))
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXEBP8), pP2->GetStr()+2);

		if(GetArrayCheckFlag())
		{
			WriteASMLine(static_cast<DWORD>(ASMOp::CMPEAX4), "0");
			WriteASMLeapMarkerJump(static_cast<DWORD>(ASMOp::JE), 4);
		}

		// The first subscript is the initial linear index. Each subsequent
		// subscript is multiplied by its dimension stride from the array header.
		WriteASMLine(static_cast<DWORD>(ASMOp::POPEBX), "");
		const auto dimensionCount = static_cast<int>(dwP1Offset);
		for(int dimension = 0; dimension < dimensionCount - 1; ++dimension)
		{
			CStr headerOffset;
			headerOffset.SetNumericText(-56 + (dimension * 4));
			WriteASMLine(static_cast<DWORD>(ASMOp::POPEDX), "");
			WriteASMLine(
				static_cast<DWORD>(ASMOp::MULEDXEAXOFF4),
				headerOffset.GetStr());
			WriteASMLine(static_cast<DWORD>(ASMOp::ADDEBXEDX4), "");
		}

		WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXEBX4), "");
		WriteASMEAXtoX(dwP1Mode, pP1, nullptr, 7, 0);

		if(GetArrayCheckFlag())
		{
			WriteASMLeapMarkerEnd(4);
		}
		WriteASMComment("CALCULATE ARRAY OFFSET", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::PushUdt))
	{
		// UDT-by-value args cannot cross the x64 register/shadow boundary yet
		// (variable-size frame); poison the frame so the Call falls back to the
		// legacy bytes and the caller's slot-based pops stay balanced.
		m_bPendingFramePoisoned = true;
		// UDT address to EAX

// did not account for pasing UCTs from with user functions
//		WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXIMM4), pP1->GetStr());
		if ( dwP1Mode==static_cast<DWORD>(ParamMode::Mem) )
		{
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXIMM4), pP1->GetStr());
		}
		if ( dwP1Mode==static_cast<DWORD>(ParamMode::Ebp) )
		{
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXEBP), NULL );
			WriteASMLine(static_cast<DWORD>(ASMOp::ADDEAX4), pP1->GetStr()+2 );
		}
		if ( dwP1Offset>0 )
		{
			CStr num;
			num.SetNumericText(dwP1Offset);
			WriteASMLine(static_cast<DWORD>(ASMOp::ADDEAX4), num.GetStr());
		}

		// Advance EAX to end of UDT data (UDT Size)
		WriteASMLine(static_cast<DWORD>(ASMOp::ADDEAX4), pP2->GetStr());

		// advance to last data element
		for ( DWORD n=0; n<dwP2Offset; n++)
		{
			// decrement udtptr
			WriteASMLine(static_cast<DWORD>(ASMOp::SUBEAX4), "4");

			// push udtptr to stack
			WriteASMLine(static_cast<DWORD>(ASMOp::PUSHFROMEAX), NULL);
		}

		// Comment on this task
		WriteASMComment("PUSH UDT TO STACK", "", "", "");
	}

	return true;
}

bool CASMWriter::WriteASMLine(DWORD dwOp, LPSTR pOpData)
{
	// DBM Code
	CStr strDBMLine(256);
	strDBMLine.SetNumericText(m_dwLineNumber);
	strDBMLine.AddText(" ");
	strDBMLine.AddText(const_cast<LPSTR>(GetASMOpcodeDef(dwOp).name ? GetASMOpcodeDef(dwOp).name : ""));
	strDBMLine.AddText(" ");
	strDBMLine.AddText(pOpData);
	if(g_pDBMWriter->OutputDBM(&strDBMLine)==false) return false;

	// ASM Code (descriptor-driven x64 emission)
	const ASMOpcodeDef& def = GetASMOpcodeDef(dwOp);
	const bool bHasData = def.data1 != DataEncoding::None || def.data2 != DataEncoding::None;
	CreateASMMiddleCore(dwOp, bHasData ? pOpData : nullptr, nullptr, false, 0);

	// Complete
	return true;
}

bool CASMWriter::WriteASMLine2(DWORD dwOp, LPSTR pOpData, LPSTR pOpData2)
{
	// DBM Code
	CStr strDBMLine(256);
	strDBMLine.SetNumericText(m_dwLineNumber);
	strDBMLine.AddText(" ");
	strDBMLine.AddText(const_cast<LPSTR>(GetASMOpcodeDef(dwOp).name ? GetASMOpcodeDef(dwOp).name : ""));
	strDBMLine.AddText(" ");
	strDBMLine.AddText(pOpData);
	strDBMLine.AddText(", ");
	strDBMLine.AddText(pOpData2);
	if(g_pDBMWriter->OutputDBM(&strDBMLine)==false) return false;

	// ASM Code (descriptor-driven x64 emission)
	CreateASMMiddleCore(dwOp, pOpData, pOpData2, false, 0);

	// Complete
	return true;
}

bool CASMWriter::WriteASMLine1IMM(DWORD dwOp, LPSTR pOpData, DWORD dwSizeIMM)
{
	// DBM Code
	CStr strDBMLine(256);
	strDBMLine.SetNumericText(m_dwLineNumber);
	strDBMLine.AddText(" ");
	strDBMLine.AddText(const_cast<LPSTR>(GetASMOpcodeDef(dwOp).name ? GetASMOpcodeDef(dwOp).name : ""));
	strDBMLine.AddText(" ");
	strDBMLine.AddText(pOpData);
	if(g_pDBMWriter->OutputDBM(&strDBMLine)==false) return false;

	// ASM Code (descriptor-driven x64 emission)
	CreateASMMiddleCore(dwOp, pOpData, nullptr, true, dwSizeIMM);

	// Complete
	return true;
}

bool CASMWriter::WriteASMLine2IMM(DWORD dwOp, LPSTR pOpData, LPSTR pOpData2, DWORD dwSizeIMM)
{
	// DBM Code
	CStr strDBMLine(256);
	strDBMLine.SetNumericText(m_dwLineNumber);
	strDBMLine.AddText(" ");
	strDBMLine.AddText(const_cast<LPSTR>(GetASMOpcodeDef(dwOp).name ? GetASMOpcodeDef(dwOp).name : ""));
	strDBMLine.AddText(" ");
	strDBMLine.AddText(pOpData);
	strDBMLine.AddText(", ");
	strDBMLine.AddText(pOpData2);
	if(g_pDBMWriter->OutputDBM(&strDBMLine)==false) return false;

	// ASM Code (descriptor-driven x64 emission)
	CreateASMMiddleCore(dwOp, pOpData, pOpData2, true, dwSizeIMM);

	// Complete
	return true;
}

bool CASMWriter::WriteASMComment(LPSTR pTitle, LPSTR pC1, LPSTR pC2, LPSTR pC3)
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
	WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXMEM4), "@$_REK_");
	WriteASMLine(static_cast<DWORD>(ASMOp::CMPEAX4), "0");
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

DWORD CASMWriter::AddCommandToTable(LPSTR pDLLString, LPSTR pCommandString)
{
	// Skip non-DLL commands
	if(pDLLString==NULL) return g_pStatementList->GetDLLIndexCounter();
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

bool CASMWriter::AddProtectionToSelectedDLLs(LPSTR pDLLString)
{
	// ANYTHING ending with "Master.DLL" is protected
	if ( strnicmp ( pDLLString + strlen ( pDLLString ) - 10, "Master.dll", 10 )==NULL )
		return true;
	else
		return false;
}

// ---------------------------------------------------------------------------
// x64 instruction emission helpers (in-place 64-bit backend of CASMWriter)
// ---------------------------------------------------------------------------

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

} // namespace

void CASMWriter::EmitByte(uint8_t b)
{
	m_codeBuffer.push_back(b);
}

void CASMWriter::EmitDword(uint32_t dw)
{
	m_codeBuffer.push_back(static_cast<uint8_t>(dw & 0xFF));
	m_codeBuffer.push_back(static_cast<uint8_t>((dw >> 8) & 0xFF));
	m_codeBuffer.push_back(static_cast<uint8_t>((dw >> 16) & 0xFF));
	m_codeBuffer.push_back(static_cast<uint8_t>((dw >> 24) & 0xFF));
}

void CASMWriter::EmitQword(uint64_t qw)
{
	EmitDword(static_cast<uint32_t>(qw & 0xFFFFFFFFULL));
	EmitDword(static_cast<uint32_t>((qw >> 32) & 0xFFFFFFFFULL));
}

void CASMWriter::EmitMovRegImm64(X64Register reg, uint64_t val)
{
	const uint8_t idx = GetRegIndex(reg);
	uint8_t rex = 0x48; // REX.W
	if (idx >= 8) rex |= 0x01; // REX.B
	EmitByte(rex);
	EmitByte(static_cast<uint8_t>(0xB8 + (idx & 7)));
	EmitQword(val);
}

void CASMWriter::EmitPushReg(X64Register reg)
{
	const uint8_t idx = GetRegIndex(reg);
	if (idx >= 8) EmitByte(0x41); // REX.B
	EmitByte(static_cast<uint8_t>(0x50 + (idx & 7)));
}

void CASMWriter::EmitPopReg(X64Register reg)
{
	const uint8_t idx = GetRegIndex(reg);
	if (idx >= 8) EmitByte(0x41); // REX.B
	EmitByte(static_cast<uint8_t>(0x58 + (idx & 7)));
}

void CASMWriter::EmitSubRegImm32(X64Register reg, uint32_t val)
{
	const uint8_t idx = GetRegIndex(reg);
	uint8_t rex = 0x48; // REX.W
	if (idx >= 8) rex |= 0x01; // REX.B
	EmitByte(rex);
	EmitByte(0x81); // SUB r/m64, imm32
	EmitByte(static_cast<uint8_t>(0xE8 | (idx & 7)));
	EmitDword(val);
}

void CASMWriter::EmitAddRegImm32(X64Register reg, uint32_t val)
{
	const uint8_t idx = GetRegIndex(reg);
	uint8_t rex = 0x48; // REX.W
	if (idx >= 8) rex |= 0x01; // REX.B
	EmitByte(rex);
	EmitByte(0x81); // ADD r/m64, imm32
	EmitByte(static_cast<uint8_t>(0xC0 | (idx & 7)));
	EmitDword(val);
}

void CASMWriter::EmitRet()
{
	EmitByte(0xC3);
}

void CASMWriter::EmitCmpRegReg(X64Register reg1, X64Register reg2)
{
	const uint8_t idx1 = GetRegIndex(reg1);
	const uint8_t idx2 = GetRegIndex(reg2);
	uint8_t rex = 0x48; // REX.W
	if (idx2 >= 8) rex |= 0x04; // REX.R
	if (idx1 >= 8) rex |= 0x01; // REX.B
	EmitByte(rex);
	EmitByte(0x39); // CMP r/m64, r64
	EmitByte(static_cast<uint8_t>(0xC0 | ((idx2 & 7) << 3) | (idx1 & 7)));
}

void CASMWriter::EmitTestRegReg(X64Register reg1, X64Register reg2)
{
	const uint8_t idx1 = GetRegIndex(reg1);
	const uint8_t idx2 = GetRegIndex(reg2);
	uint8_t rex = 0x48; // REX.W
	if (idx2 >= 8) rex |= 0x04; // REX.R
	if (idx1 >= 8) rex |= 0x01; // REX.B
	EmitByte(rex);
	EmitByte(0x85); // TEST r/m64, r64
	EmitByte(static_cast<uint8_t>(0xC0 | ((idx2 & 7) << 3) | (idx1 & 7)));
}

void CASMWriter::EmitJmpRel32(int32_t relOffset)
{
	EmitByte(0xE9);
	EmitDword(static_cast<uint32_t>(relOffset));
}

void CASMWriter::EmitJneRel32(int32_t relOffset)
{
	EmitByte(0x0F);
	EmitByte(0x85);
	EmitDword(static_cast<uint32_t>(relOffset));
}

void CASMWriter::EmitJeRel32(int32_t relOffset)
{
	EmitByte(0x0F);
	EmitByte(0x84);
	EmitDword(static_cast<uint32_t>(relOffset));
}

void CASMWriter::EmitCallReg(X64Register reg)
{
	const uint8_t idx = GetRegIndex(reg);
	if (idx >= 8) EmitByte(0x41); // REX.B
	EmitByte(0xFF);
	EmitByte(static_cast<uint8_t>(0xD0 | (idx & 7)));
}

void CASMWriter::EmitNop()
{
	EmitByte(0x90);
}

void CASMWriter::EmitMovss(XMMRegister dst, XMMRegister src)
{
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

void CASMWriter::EmitAddss(XMMRegister dst, XMMRegister src)
{
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

void CASMWriter::EmitMulss(XMMRegister dst, XMMRegister src)
{
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

// ---------------------------------------------------------------------------
// Microsoft x64 calling convention (wave 3)
// ---------------------------------------------------------------------------

void CASMWriter::ResetStackTracking() noexcept
{
	// The machine block is entered through a C call, so RSP%16==8 at entry;
	// the 7-register prologue (PushRegisters) lands the program body at 0.
	m_iRSPMod16 = 8;
	m_iFrameDepth = 0;
	m_bRSPAlignmentKnown = true;
	m_pendingArgTypes.clear();
	m_iPendingCleanupPops = 0;
	m_bPendingFramePoisoned = false;
}

void CASMWriter::ApplyStackDelta(int iDeltaBytes, bool bPoisonAlignment) noexcept
{
	if (bPoisonAlignment)
	{
		m_bRSPAlignmentKnown = false;
		return;
	}
	m_iFrameDepth += iDeltaBytes;
	if (iDeltaBytes != 0)
	{
		int mod = (m_iRSPMod16 + iDeltaBytes) % 16;
		if (mod < 0) mod += 16;
		m_iRSPMod16 = mod;
	}
}

void CASMWriter::TrackStackForOpcode(DWORD dwASMCode, LPSTR lpOpData) noexcept
{
	switch (static_cast<ASMOp>(dwASMCode))
	{
		case ASMOp::PUSHEAX:
		case ASMOp::PUSHEDX:
		case ASMOp::PUSHEBX:
		case ASMOp::PUSHECX:
		case ASMOp::PUSHEBP:
		case ASMOp::PUSHESP:
		case ASMOp::PUSHFROMEAX:
			// 8-byte pushes in 64-bit mode (PUSH ESP pushes the post-decrement RSP).
			ApplyStackDelta(8, false);
			break;
		case ASMOp::POPEAX:
		case ASMOp::POPEBX:
		case ASMOp::POPEDX:
		case ASMOp::POPECX:
		case ASMOp::POPEBP:
			ApplyStackDelta(-8, false);
			// Pops remove the matching pushed slot type (array-offset subscript
			// consumption stays in lock-step with the pending call frame).
			PopPendingArgSlot();
			break;
		case ASMOp::SUBESP:
			if (lpOpData) ApplyStackDelta(atoi(lpOpData), false);
			break;
		case ASMOp::ADDESP:
			if (lpOpData) ApplyStackDelta(-atoi(lpOpData), false);
			break;
		case ASMOp::SUBESPEAX:
		case ASMOp::MOVESPEBP:    // MOV ESP, EBP (frame restore)
		case ASMOp::MOVESPMEM4:   // MOV ESP, [mem] (RestoreEsp)
			// RSP moves by a runtime amount: alignment is no longer statically known.
			ApplyStackDelta(0, true);
			break;
		default:
			break;
	}
}

void CASMWriter::RecordPendingArg(DWORD dwTypeValue) noexcept
{
	// One type entry per pushed 8-byte slot (doubles/int64 push two).
	m_pendingArgTypes.push_back(dwTypeValue);
	if (IsDoubleSlotType(dwTypeValue))
		m_pendingArgTypes.push_back(dwTypeValue);
}

void CASMWriter::PopPendingArgSlot() noexcept
{
	if (!m_pendingArgTypes.empty())
		m_pendingArgTypes.pop_back();
}

void CASMWriter::ResetPendingFrame() noexcept
{
	m_pendingArgTypes.clear();
	m_iPendingCleanupPops = 0;
	m_bPendingFramePoisoned = false;
}

bool CASMWriter::IsDoubleSlotType(DWORD dwType) noexcept
{
	// Doubles (8/108) are FP-class and ride a single XMM register, but the
	// legacy emitter passes them as two 4-byte stack halves, so the frame
	// reassembles them. Int64 (9/109) became a single 8-byte slot in wave 8b
	// (the MS x64 ABI: __int64 rides one integer register), so it no longer
	// counts as double-slot.
	return dwType == 8 || dwType == 108;
}

bool CASMWriter::IsFloatClassType(DWORD dwType) noexcept
{
	return dwType == 2 || dwType == 102 || dwType == 8 || dwType == 108;
}

void CASMWriter::EmitRawByte(uint8_t b)
{
	CheckAndExpandMCBMemory();
	m_machineCodeBuffer.WriteByte(b);
}

// modrm/SIB/disp for [RSP + disp] addressing (SIB base=RSP requires SIB).
void CASMWriter::EmitRspModRmByte(uint8_t reg, int iDisp)
{
	if (iDisp >= -128 && iDisp <= 127)
	{
		EmitRawByte(static_cast<uint8_t>(0x40 | ((reg & 7) << 3) | 0x04)); // mod=01, rm=100
		EmitRawByte(0x24); // SIB: scale=0, index=none, base=RSP
		EmitRawByte(static_cast<uint8_t>(iDisp));
	}
	else
	{
		EmitRawByte(static_cast<uint8_t>(0x80 | ((reg & 7) << 3) | 0x04)); // mod=10, rm=100
		EmitRawByte(0x24);
		m_machineCodeBuffer.WriteDWORD(static_cast<DWORD>(iDisp), 4);
	}
}

void CASMWriter::EmitSubRspImm(int iBytes)
{
	if (iBytes >= -128 && iBytes <= 127)
	{
		EmitRawByte(0x48); EmitRawByte(0x83); EmitRawByte(0xEC);
		EmitRawByte(static_cast<uint8_t>(iBytes));
	}
	else
	{
		EmitRawByte(0x48); EmitRawByte(0x81); EmitRawByte(0xEC);
		m_machineCodeBuffer.WriteDWORD(static_cast<DWORD>(iBytes), 4);
	}
}

void CASMWriter::EmitAddRspImm(int iBytes)
{
	if (iBytes >= -128 && iBytes <= 127)
	{
		EmitRawByte(0x48); EmitRawByte(0x83); EmitRawByte(0xC4);
		EmitRawByte(static_cast<uint8_t>(iBytes));
	}
	else
	{
		EmitRawByte(0x48); EmitRawByte(0x81); EmitRawByte(0xC4);
		m_machineCodeBuffer.WriteDWORD(static_cast<DWORD>(iBytes), 4);
	}
}

void CASMWriter::EmitMovRegFromRspOffset(int iRegIndex, bool bRexR, int iDisp)
{
	EmitRawByte(bRexR ? 0x49 : 0x48); // REX.W (+ REX.R for R8/R9)
	EmitRawByte(0x8B);                // MOV r64, r/m64
	EmitRspModRmByte(static_cast<uint8_t>(iRegIndex), iDisp);
}

void CASMWriter::EmitMovRaxFromRspOffset(int iDisp)
{
	EmitRawByte(0x48); EmitRawByte(0x8B); // MOV RAX, r/m64
	EmitRspModRmByte(0, iDisp);
}

void CASMWriter::EmitMovRcxFromRspOffset(int iDisp)
{
	EmitRawByte(0x48); EmitRawByte(0x8B); // MOV RCX, r/m64
	EmitRspModRmByte(1, iDisp);
}

void CASMWriter::EmitMovToRspOffsetFromRax(int iDisp)
{
	EmitRawByte(0x48); EmitRawByte(0x89); // MOV r/m64, RAX
	EmitRspModRmByte(0, iDisp);
}

void CASMWriter::EmitMovEaxFromRspOffset(int iDisp)
{
	EmitRawByte(0x8B); // MOV EAX, r/m32 (no REX)
	EmitRspModRmByte(0, iDisp);
}

void CASMWriter::EmitMovToRspOffsetFromEax(int iDisp)
{
	EmitRawByte(0x89); // MOV r/m32, EAX
	EmitRspModRmByte(0, iDisp);
}

void CASMWriter::EmitMovssXmmFromRspOffset(int iXmmIndex, int iDisp)
{
	EmitRawByte(0xF3); EmitRawByte(0x0F); EmitRawByte(0x10); // MOVSS xmm, m32
	EmitRspModRmByte(static_cast<uint8_t>(iXmmIndex), iDisp);
}

void CASMWriter::EmitMovqXmmFromRspOffset(int iXmmIndex, int iDisp)
{
	EmitRawByte(0x66); EmitRawByte(0x48); EmitRawByte(0x0F); EmitRawByte(0x6E); // MOVQ xmm, m64
	EmitRspModRmByte(static_cast<uint8_t>(iXmmIndex), iDisp);
}

void CASMWriter::EmitMovqXmmFromRax(int iXmmIndex)
{
	EmitRawByte(0x66); EmitRawByte(0x48); EmitRawByte(0x0F); EmitRawByte(0x6E);
	EmitRawByte(static_cast<uint8_t>(0xC0 | ((iXmmIndex & 7) << 3))); // MOVQ xmm, rax
}

void CASMWriter::EmitShlRcxImm32()
{
	EmitRawByte(0x48); EmitRawByte(0xC1); EmitRawByte(0xE1); EmitRawByte(0x20);
}

void CASMWriter::EmitOrRaxRcx()
{
	EmitRawByte(0x48); EmitRawByte(0x09); EmitRawByte(0xC8);
}

void CASMWriter::EmitMovRcxFromRax()
{
	EmitRawByte(0x48); EmitRawByte(0x89); EmitRawByte(0xC1);
}

void CASMWriter::EmitX64CallFrame(DWORD dwCommandIndex)
{
	// Walk the pending slot list from the top: the last push is arg1. A
	// double/int64 spans two adjacent slots (low half at the lower address,
	// with the x64 zero-extension gap between the halves).
	struct PendingArg { DWORD type; int slots; int base; };
	std::vector<PendingArg> args;
	{
		const int total = static_cast<int>(m_pendingArgTypes.size());
		int i = total;
		while (i > 0)
		{
			const DWORD t = m_pendingArgTypes[static_cast<std::size_t>(i - 1)];
			const int slots = IsDoubleSlotType(t) ? 2 : 1;
			args.push_back({ t, slots, 8 * (total - i) });
			i -= slots;
		}
	}

	const DWORD dwNumArgs = static_cast<DWORD>(args.size());
	const int iStackArgs = (dwNumArgs > 4) ? static_cast<int>(dwNumArgs - 4) : 0;

	// 16-byte alignment at the CALL: F = 32 (shadow) + 8*stackArgs + pad,
	// with F ≡ RSP mod 16 so RSP-F is aligned. All quantities are
	// compile-time here because m_iRSPMod16 is statically tracked.
	int pad = (m_iRSPMod16 - (32 + 8 * iStackArgs)) % 16;
	if (pad < 0) pad += 16;
	const int iFrameBytes = 32 + 8 * iStackArgs + pad;

	EmitSubRspImm(iFrameBytes);
	ApplyStackDelta(iFrameBytes, false);

	for (DWORD k = 1; k <= dwNumArgs; ++k)
	{
		const PendingArg& a = args[static_cast<std::size_t>(k - 1)];
		const int iSrc = iFrameBytes + a.base;
		if (k <= 4)
		{
			// Register arguments (positional ABI slots).
			if (IsFloatClassType(a.type))
			{
				if (IsDoubleSlotType(a.type))
				{
					// Reassemble low@[RSP+src], high@[RSP+src+8] into XMM(k-1).
					EmitMovRaxFromRspOffset(iSrc);
					EmitMovRcxFromRspOffset(iSrc + 8);
					EmitShlRcxImm32();
					EmitOrRaxRcx();
					EmitMovqXmmFromRax(static_cast<int>(k - 1));
				}
				else
				{
					EmitMovssXmmFromRspOffset(static_cast<int>(k - 1), iSrc);
				}
			}
			else if (IsDoubleSlotType(a.type))
			{
				// int64: reassemble into an integer register.
				EmitMovRaxFromRspOffset(iSrc);
				EmitMovRcxFromRspOffset(iSrc + 8);
				EmitShlRcxImm32();
				EmitOrRaxRcx();
				EmitMovRcxFromRax();
			}
			else
			{
				// Integer/pointer: RCX, RDX, R8, R9.
				static const int kIntReg[4] = { 1, 2, 0, 1 };
				static const bool kRexR[4]  = { false, false, true, true };
				EmitMovRegFromRspOffset(kIntReg[k - 1], kRexR[k - 1], iSrc);
			}
		}
		else
		{
			// Stack arguments move above the shadow space: [RSP+32+8*(k-5)].
			const int iDest = 32 + 8 * (static_cast<int>(k) - 5);
			if (IsFloatClassType(a.type) && !IsDoubleSlotType(a.type))
			{
				// 4-byte float in the low half of the 8-byte stack slot.
				EmitMovEaxFromRspOffset(iSrc);
				EmitMovToRspOffsetFromEax(iDest);
			}
			else if (IsDoubleSlotType(a.type))
			{
				EmitMovRaxFromRspOffset(iSrc);
				EmitMovRcxFromRspOffset(iSrc + 8);
				EmitShlRcxImm32();
				EmitOrRaxRcx();
				EmitMovToRspOffsetFromRax(iDest);
			}
			else
			{
				EmitMovRaxFromRspOffset(iSrc);
				EmitMovToRspOffsetFromRax(iDest);
			}
		}
	}

	// Command load + call (MOV EBX,[index] expands to 48 BB <imm64> for the
	// command-address reference; CALL EBX is FF D3 in both modes).
	CStr tokenCommandStr("[");
	tokenCommandStr.AddNumericText(dwCommandIndex);
	WriteASMLine(static_cast<DWORD>(ASMOp::MOVEBXIMM4), tokenCommandStr.GetStr());
	WriteASMLine(static_cast<DWORD>(ASMOp::CALLEBX), "");

	EmitAddRspImm(iFrameBytes);
	ApplyStackDelta(-iFrameBytes, false);

	// The frame is consumed: record the caller's cleanup-pop count (one per
	// pushed slot) so the trailing PopEbx/PopEax tasks are suppressed, and
	// reset the pending state for the next call.
	m_iPendingCleanupPops = static_cast<DWORD>(m_pendingArgTypes.size());
	m_pendingArgTypes.clear();
	m_bPendingFramePoisoned = false;
}

void CASMWriter::EmitAlignedCrtCall(const char* pCommand)
{
	// Wave 17/21: the shared body of a fully-balanced direct call to a CRT
	// primitive (msvcrt exp/log/fmod). Unlike the value-stack Push/Call
	// machinery this touches no pending-arg state: the SUB/ADD frame pair
	// restores RSP exactly, so the emitter's stack tracking stays consistent
	// for the surrounding code.

	// Frame: 32-byte shadow space plus a pad that leaves RSP 16-aligned at
	// the CALL — identical formula to EmitX64CallFrame.
	int pad = (m_iRSPMod16 - 32) % 16;
	if (pad < 0) pad += 16;
	const int iFrameBytes = 32 + pad;
	EmitSubRspImm(iFrameBytes);
	ApplyStackDelta(iFrameBytes, false);

	// Resolve the primitive through the DLL/command tables (msvcrt.dll is a
	// guaranteed system DLL; exp/log/fmod are undecorated exports).
	CStr dllString("[msvcrt.dll");
	CStr cmdString(",");
	cmdString.AddText(const_cast<char*>(pCommand));
	const DWORD dwIndex = AddCommandToTable(dllString.GetStr(), cmdString.GetStr());

	CStr tokenCommandStr("[");
	tokenCommandStr.AddNumericText(dwIndex);
	WriteASMLine(static_cast<DWORD>(ASMOp::MOVEBXIMM4), tokenCommandStr.GetStr());
	WriteASMLine(static_cast<DWORD>(ASMOp::CALLEBX), "");

	EmitAddRspImm(iFrameBytes);
	ApplyStackDelta(-iFrameBytes, false);
}

void CASMWriter::EmitTranscendentalCall(const char* pCommand, const char* pTempSlot)
{
	// Wave 17: single-double-argument call (exp/log): load XMM0, call, store
	// the XMM0 result back into the temp slot.
	WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDXMM0MEM), const_cast<char*>(pTempSlot));
	EmitAlignedCrtCall(pCommand);
	WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDMEMXMM0), const_cast<char*>(pTempSlot));
}

void CASMWriter::EmitBinaryTranscendentalCall(const char* pCommand, const char* pTempA, const char* pTempB)
{
	// Wave 21: two-double-argument call (fmod): XMM0 = first arg (pTempA),
	// XMM1 = second arg (pTempB), result returns in XMM0 and is stored back
	// into pTempA.
	WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDXMM0MEM), const_cast<char*>(pTempB));
	WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDXMM1XMM0), "");
	WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDXMM0MEM), const_cast<char*>(pTempA));
	EmitAlignedCrtCall(pCommand);
	WriteASMLine(static_cast<DWORD>(ASMOp::MOVSDMEMXMM0), const_cast<char*>(pTempA));
}
