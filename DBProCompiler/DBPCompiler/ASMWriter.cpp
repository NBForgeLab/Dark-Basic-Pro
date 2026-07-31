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

#include <DB3Time.h>

#include <memory>

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
	m_bOneOffCondToggle=false;

	// Work Variables
	m_dwLineNumber=0;

	// Reset ASM Code Database
	for(DWORD i=0; i<ASMMAXCOUNT; i++)
	{
		m_iASMPreOp[i]=0;
		m_iASMOp1[i]=0;
		m_iASMOp2[i]=0;
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
	// Default ASM Codes for ASMWriting
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXIMM1),	"MOV EAX IMM1",		-1,		0xB0+0,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXIMM2),	"MOV EAX IMM2",		0x66,	0xB8+0,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXIMM4),	"MOV EAX IMM4",		-1,		0xB8+0,	-1,		true);

	DefineASM(static_cast<DWORD>(ASMOp::MOVEBXIMM1),	"MOV EBX IMM1",		-1,		0xB0+3,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBXIMM2),	"MOV EBX IMM2",		0x66,	0xB8+3,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBXIMM4),	"MOV EBX IMM4",		-1,		0xB8+3,	-1,		true);
	
	DefineASM(static_cast<DWORD>(ASMOp::MOVEDXIMM4),	"MOV EDX IMM4",		-1,		0xB8+2,	-1,		true);

	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXMEM1),	"MOV EAX MEM1",		-1,		0xA0,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXMEM2),	"MOV EAX MEM2",		0x66,	0xA1,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXMEM4),	"MOV EAX MEM4",		-1,		0xA1,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVMEMEAX1),	"MOV MEM1 EAX",		-1,		0xA2,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVMEMEAX2),	"MOV MEM2 EAX",		0x66,	0xA3,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVMEMEAX4),	"MOV MEM4 EAX",		-1,		0xA3,	-1,		true);

	DefineASM(static_cast<DWORD>(ASMOp::MOVECXOFFEAX1),"MOV [ECX+A] EAX1",	-1,		0x88,	0x81,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVECXOFFEAX2),"MOV [ECX+A] EAX2",	0x66,	0x89,	0x81,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVECXOFFEAX4),"MOV [ECX+A] EAX4",	-1,		0x89,	0x81,	true);

	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXOFFECX1),"MOV [EAX+A] ECX1",	-1,		0x88,	0x88,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXOFFECX2),"MOV [EAX+A] ECX2",	0x66,	0x89,	0x88,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXOFFECX4),"MOV [EAX+A] ECX4",	-1,		0x89,	0x88,	true);
	
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXECXREL1),"MOV [EAX] ECX1",	-1,		0x88,	0x08,	false);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXECXREL2),"MOV [EAX] ECX2",	0x66,	0x89,	0x08,	false);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXECXREL4),"MOV [EAX] ECX4",	-1,		0x89,	0x08,	false);
	
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXEAXREL1),"MOV EAX [EAX1]",	-1,		0x8A,	0x00,	false);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXEAXREL2),"MOV EAX [EAX2]",	0x66,	0x8B,	0x00,	false);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXEAXREL4),"MOV EAX [EAX4]",	-1,		0x8B,	0x00,	false);
	
	DefineASM(static_cast<DWORD>(ASMOp::MOVECXEAXOFF1),"MOV ECX1 [EAX+A]",	-1,		0x8A,	0x88,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVECXEAXOFF2),"MOV ECX2 [EAX+A]",	0x66,	0x8B,	0x88,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVECXEAXOFF4),"MOV ECX4 [EAX+A]",	-1,		0x8B,	0x88,	true);
	
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXECXOFF1),"MOV EAX1 [ECX+A]",	-1,		0x8A,	0x81,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXECXOFF2),"MOV EAX2 [ECX+A]",	0x66,	0x8B,	0x81,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXECXOFF4),"MOV EAX4 [ECX+A]",	-1,		0x8B,	0x81,	true);
	
	DefineASM(static_cast<DWORD>(ASMOp::MOVEDXEAXOFF1),"MOV EDX1 [EAX+A]",	-1,		0x8A,	0x90,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEDXEAXOFF2),"MOV EDX2 [EAX+A]",	0x66,	0x8B,	0x90,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEDXEAXOFF4),"MOV EDX4 [EAX+A]",	-1,		0x8B,	0x90,	true);

	DefineASM(static_cast<DWORD>(ASMOp::MULEDXEAXOFF4),"MUL EDX4 [EAX+A]",	0x0F,	0xAF,	0x90,	true);

	DefineASM(static_cast<DWORD>(ASMOp::MOVEBXEAXOFF1),"MOV EBX1 [EAX+A]",	-1,		0x8A,	0x98,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBXEAXOFF2),"MOV EBX2 [EAX+A]",	0x66,	0x8B,	0x98,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBXEAXOFF4),"MOV EBX4 [EAX+A]",	-1,		0x8B,	0x98,	true);

	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXEBP1),	"MOV EAX1 [EBP+A]",	-1,		0x8A,	0x85,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXEBP2),	"MOV EAX2 [EBP+A]",	0x66,	0x8B,	0x85,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXEBP4),	"MOV EAX4 [EBP+A]",	-1,		0x8B,	0x85,	true);
	
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBXEBP1),	"MOV EBX1 [EBP+A]",	-1,		0x8A,	0x9D,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBXEBP2),	"MOV EBX2 [EBP+A]",	0x66,	0x8B,	0x9D,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBXEBP4),	"MOV EBX4 [EBP+A]",	-1,		0x8B,	0x9D,	true);
	
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBPEAX1),	"MOV [EBP+A] EAX1",	-1,		0x88,	0x85,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBPEAX2),	"MOV [EBP+A] EAX2",	0x66,	0x89,	0x85,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBPEAX4),	"MOV [EBP+A] EAX4",	-1,		0x89,	0x85,	true);
	
	DefineASM(static_cast<DWORD>(ASMOp::MOVEDXEAX4),	"MOV EDX EAX",		-1,		0x8B,	0xD0,	false);
	DefineASM(static_cast<DWORD>(ASMOp::MOVECXIMM4),	"MOV ECX IMM4",		-1,		0xB8+1, -1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVECXEAX4),	"MOV ECX EAX",		-1,		0x8B,	0xC8,	false);
	DefineASM(static_cast<DWORD>(ASMOp::MOVECXEDX4),	"MOV ECX EDX",		-1,		0x8B,	0xCA,	false);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEDXECX4),	"MOV EDX ECX",		-1,		0x8B,	0xD1,	false);

	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXEBX4),	"MOV EAX EBX",		-1,		0x8B,	0xC3,	false);

	DefineASM(static_cast<DWORD>(ASMOp::MOVECXEBX1),	"MOV ECX EBX1",		-1,		0x8A,	0xCB,	false);
	DefineASM(static_cast<DWORD>(ASMOp::MOVECXEBX2),	"MOV ECX EBX2",		-1,		0x8B,	0xCB,	false);
	DefineASM(static_cast<DWORD>(ASMOp::MOVECXEBX4),	"MOV ECX EBX4",		-1,		0x8B,	0xCB,	false);
	
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXECX1),	"MOV EAX ECX1",		-1,		0x8A,	0xC1,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXECX2),	"MOV EAX ECX2",		0x66,	0x8B,	0xC1,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXECX4),	"MOV EAX ECX4",		-1,		0x8B,	0xC1,	true);

	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXEDX1),	"MOV EAX EDX1",		-1,		0x8A,	0xC2,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXEDX2),	"MOV EAX EDX2",		0x66,	0x8B,	0xC2,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXEDX4),	"MOV EAX EDX4",		-1,		0x8B,	0xC2,	true);

	DefineASM(static_cast<DWORD>(ASMOp::MOVEBXEAX1),	"MOV EBX EAX1",		-1,		0x8A,	0xD8,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBXEAX2),	"MOV EBX EAX2",		0x66,	0x8B,	0xD8,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBXEAX4),	"MOV EBX EAX4",		-1,		0x8B,	0xD8,	true);

	DefineASM(static_cast<DWORD>(ASMOp::ADDEAXEBX1),	"ADD EAX EBX1",		-1,		0x00,	0xD8,	false);
	DefineASM(static_cast<DWORD>(ASMOp::ADDEAXEBX2),	"ADD EAX EBX2",		0x66,	0x01,	0xD8,	false);
	DefineASM(static_cast<DWORD>(ASMOp::ADDEAXEBX4),	"ADD EAX EBX4",		-1,		0x01,	0xD8,	false);

	DefineASM(static_cast<DWORD>(ASMOp::ADDEAXECX4),	"ADD EAX ECX4",		-1,		0x01,	0xC8,	false);

	DefineASM(static_cast<DWORD>(ASMOp::ADDESP),		"ADD ESP",			-1,		0x81,	0xC4,	true);
	DefineASM(static_cast<DWORD>(ASMOp::SUBESP),		"SUB ESP",			-1,		0x81,	0xEC,	true);
	DefineASM(static_cast<DWORD>(ASMOp::SUBESPEAX),	"SUB ESP EAX",		-1,		0x29,	0xC4,	false);

	DefineASM(static_cast<DWORD>(ASMOp::PUSHEBP),		"PUSH EBP",			-1,		0x55,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::POPEBP),		"POP EBP",			-1,		0x5D,	-1,		true);
	
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBPESP),	"MOV EBP ESP",		-1,		0x89,	0xE5,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVESPEBP),	"MOV ESP EBP",		-1,		0x89,	0xEC,	true);

	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXESP),	"MOV EAX ESP",		-1,		0x89,	0xE0,	false);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXEBP),	"MOV EAX EBP",		-1,		0x89,	0xE8,	false);

	DefineASM(static_cast<DWORD>(ASMOp::MOVMEMIMM1),	"MOV MEM IMM1",		-1,		0xC6,	0x05,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVMEMIMM2),	"MOV MEM IMM2",		0x66,	0xC7,	0x05,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVMEMIMM4),	"MOV MEM IMM4",		-1,		0xC7,	0x05,	true);

	DefineASM(static_cast<DWORD>(ASMOp::MOVEBPIMM1),	"MOV [EBP+A] IMM1",	-1,		0xC6,	0x85,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBPIMM2),	"MOV [EBP+A] IMM2",	0x66,	0xC7,	0x85,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBPIMM4),	"MOV [EBP+A] IMM4",	-1,		0xC7,	0x85,	true);
	
	DefineASM(static_cast<DWORD>(ASMOp::RELMOVEAXEDX1),"REL MOV [AX1] DX1",-1,		0x88,	0x10,	true);
	DefineASM(static_cast<DWORD>(ASMOp::RELMOVEAXEDX2),"REL MOV [AX2] DX2",0x66,	0x89,	0x10,	true);
	DefineASM(static_cast<DWORD>(ASMOp::RELMOVEAXEDX4),"REL MOV [EAX] EDX",-1,		0x89,	0x10,	true);

	DefineASM(static_cast<DWORD>(ASMOp::RELMOVEAXREDX1),"REL MOV AX1 [DX1]",-1,	0x8A,	0x02,	true);
	DefineASM(static_cast<DWORD>(ASMOp::RELMOVEAXREDX2),"REL MOV AX2 [DX2]",0x66,	0x8B,	0x02,	true);
	DefineASM(static_cast<DWORD>(ASMOp::RELMOVEAXREDX4),"REL MOV EAX [EDX]",-1,	0x8B,	0x02,	true);

	DefineASM(static_cast<DWORD>(ASMOp::MOVMEMST08),	"FSTP [MEM] ST08",	-1,		0xDD,	0x1D,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVST0MEM8),	"FLD ST08 [MEM]",	-1,		0xDD,	0x05,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEBPST08),	"FSTP [EBP+A] ST08",-1,		0xDD,	0x9D,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVST0EBP8),	"FLD ST08 [EBP+A]",	-1,		0xDD,	0x85,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXST08),	"FSTP [EAX+A] ST08",-1,		0xDD,	0x98,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVST0EAX8),	"FLD ST08 [EAX+A]",	-1,		0xDD,	0x80,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVECXOFFST08),"FSTP [ECX+A] ST08",-1,		0xDD,	0x99,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVST0ECXOFF8),"FLD ST08 [ECX+A]",	-1,		0xDD,	0x81,	true);
	
	DefineASM(static_cast<DWORD>(ASMOp::PUSHEAX),		"PUSH EAX",			-1,		0x50+0,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHEDX),		"PUSH EDX",			-1,		0x50+2,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHEBX),		"PUSH EBX",			-1,		0x50+3,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHESP),		"PUSH ESP",			-1,		0x50+4,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHECX),		"PUSH ECX",			-1,		0x50+1,	-1,		true);

	DefineASM(static_cast<DWORD>(ASMOp::PUSHRELEAX1),	"PUSH REL AX1",		-1,		0xFF,	0x30,	true);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHRELEAX2),	"PUSH REL AX2",		-1,		0xFF,	0x30,	true);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHRELEAX4),	"PUSH REL EAX",		-1,		0xFF,	0x30,	true);

	DefineASM(static_cast<DWORD>(ASMOp::PUSHEBP4),		"PUSH [EBP+A]",		-1,		0xFF,	0xB5,	true);
	DefineASM(static_cast<DWORD>(ASMOp::PUSHFROMEAX),	"PUSH [EAX]",		-1,		0xFF,	0x30,	false);

	DefineASM(static_cast<DWORD>(ASMOp::CALLEAX),		"CALL EAX",			-1,		0xFF,	0xD0,	false);
	DefineASM(static_cast<DWORD>(ASMOp::CALLEBX),		"CALL EBX",			-1,		0xFF,	0xD3,	false);
	DefineASM(static_cast<DWORD>(ASMOp::CALLMEM),		"CALL MEM",			-1,		0xE8,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::CALLABS),		"CALL REL",			-1,		0xE8,	-1,		true);
	
	DefineASM(static_cast<DWORD>(ASMOp::POPEAX),		"POP EAX",			-1,		0x58,	-1,		false);
	DefineASM(static_cast<DWORD>(ASMOp::POPEBX),		"POP EBX",			-1,		0x5B,	-1,		false);
	DefineASM(static_cast<DWORD>(ASMOp::RET),			"RET",				-1,		0xC3,	-1,		false);
	DefineASM(static_cast<DWORD>(ASMOp::POPEDX),		"POP EDX",			-1,		0x5A,	-1,		false);
	DefineASM(static_cast<DWORD>(ASMOp::POPECX),		"POP ECX",			-1,		0x59,	-1,		false);

	DefineASM(static_cast<DWORD>(ASMOp::UNKNOWN),		"???",				-1,		-1,		-1,		false);

	DefineASM(static_cast<DWORD>(ASMOp::CMPEAX1),		"CMP EAX1",			-1,		0x3C,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::CMPEAX2),		"CMP EAX2",			0x66,	0x3D,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::CMPEAX4),		"CMP EAX4",			-1,		0x3D,	-1,		true);

	DefineASM(static_cast<DWORD>(ASMOp::CMPEBX1),		"CMP EBX1",			-1,		0x80,	0xFB,	true);
	DefineASM(static_cast<DWORD>(ASMOp::CMPEBX2),		"CMP EBX2",			0x66,	0x81,	0xFB,	true);
	DefineASM(static_cast<DWORD>(ASMOp::CMPEBX4),		"CMP EBX4",			-1,		0x81,	0xFB,	true);
	
	DefineASM(static_cast<DWORD>(ASMOp::CMPGREEDXEBX),	"CMP EDX EBX4",		-1,		0x3B,	0xDA,	false);

	DefineASM(static_cast<DWORD>(ASMOp::CMPEDXEBX1),	"CMP EDX EBX1",		-1,		0x3A,	0xDA,	false);
	DefineASM(static_cast<DWORD>(ASMOp::CMPEDXEBX2),	"CMP EDX EBX2",		0x66,	0x3B,	0xDA,	false);
	DefineASM(static_cast<DWORD>(ASMOp::CMPEDXEBX4),	"CMP EDX EBX4",		-1,		0x3B,	0xDA,	false);

	DefineASM(static_cast<DWORD>(ASMOp::CMPEAXEBX4),	"CMP EAX EBX4",		-1,		0x3B,	0xD8,	false);

	DefineASM(static_cast<DWORD>(ASMOp::SETE),			"SETE EAX",			0x0F,	0x94,	0xC0,	false);
	DefineASM(static_cast<DWORD>(ASMOp::SETNE),		"SETNE EAX",		0x0F,	0x95,	0xC0,	false);
	DefineASM(static_cast<DWORD>(ASMOp::SETG),			"SETG EAX",			0x0F,	0x9F,	0xC0,	false);
	DefineASM(static_cast<DWORD>(ASMOp::SETGE),		"SETGE EAX",		0x0F,	0x9D,	0xC0,	false);
	DefineASM(static_cast<DWORD>(ASMOp::SETL),			"SETL EAX",			0x0F,	0x9C,	0xC0,	false);
	DefineASM(static_cast<DWORD>(ASMOp::SETLE),		"SETLE EAX",		0x0F,	0x9E,	0xC0,	false);

	DefineASM(static_cast<DWORD>(ASMOp::JMP),			"JMP",				-1,		0xE9,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::JNE),			"JNE",				-1,		0x0F,	0x85,	true);
	DefineASM(static_cast<DWORD>(ASMOp::JE),			"JE",				-1,		0x0F,	0x84,	true);
	DefineASM(static_cast<DWORD>(ASMOp::JMPREL),		"JMP REL",			-1,		0xFF,	0x25,	true);
	DefineASM(static_cast<DWORD>(ASMOp::JMPEBX),		"JMP EBX",			-1,		0xFF,	0xE3,	true);

	DefineASM(static_cast<DWORD>(ASMOp::JGE),			"JGE",				-1,		0x0F,	0x8D,	true);
	DefineASM(static_cast<DWORD>(ASMOp::JLE),			"JLE",				-1,		0x0F,	0x8E,	true);

	DefineASM(static_cast<DWORD>(ASMOp::MOVMEMESP4),	"MOV MEM4 ESP",		-1,		0x89,	0x25,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVESPMEM4),	"MOV ESP MEM4",		-1,		0x8B,	0x25,	true);

	DefineASM(static_cast<DWORD>(ASMOp::MOVEBXMEM4),	"MOV EBX MEM4",		-1,		0x8B,	0x1D,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVMEMEBX4),	"MOV MEM EBX4",		-1,		0x89,	0x1D,	true);

	DefineASM(static_cast<DWORD>(ASMOp::MOVEAXSIB4),	"MOV EAX SIB[EAX:EBX*4]",	0x8B,	0x04,	0x98,	false);

	DefineASM(static_cast<DWORD>(ASMOp::MOVSIB4IMM1),	"MOV SIB[EAX:ECX*1],IMM1",	0xC6,	0x04,	0x08,	true);
	DefineASM(static_cast<DWORD>(ASMOp::MOVSIB4IMM4),	"MOV SIB[EAX:ECX*4],IMM4",	0xC7,	0x04,	0x88,	true);

	DefineASM(static_cast<DWORD>(ASMOp::PUSHAD),		"PUSH REGISTERS",	-1,		0x60,	-1,		false);
	DefineASM(static_cast<DWORD>(ASMOp::POPAD),		"POP REGISTERS",	-1,		0x61,	-1,		false);

	DefineASM(static_cast<DWORD>(ASMOp::LOOP),			"LOOP ECX",			-1,		0xE2,	-1,		true);
	
	DefineASM(static_cast<DWORD>(ASMOp::PUSHIMM4),		"PUSH IMM4",		-1,		0x68,	-1,		true);

	DefineASM(static_cast<DWORD>(ASMOp::INCMEM1),		"INC MEM1",			-1,		0xFE,	0x05,	true);
	DefineASM(static_cast<DWORD>(ASMOp::INCMEM2),		"INC MEM2",			0x66,	0xFF,	0x05,	true);
	DefineASM(static_cast<DWORD>(ASMOp::INCMEM4),		"INC MEM4",			-1,		0xFF,	0x05,	true);
	DefineASM(static_cast<DWORD>(ASMOp::DECMEM1),		"DEC MEM1",			-1,		0xFE,	0x0D,	true);
	DefineASM(static_cast<DWORD>(ASMOp::DECMEM2),		"DEC MEM2",			0x66,	0xFF,	0x0D,	true);
	DefineASM(static_cast<DWORD>(ASMOp::DECMEM4),		"DEC MEM4",			-1,		0xFF,	0x0D,	true);

	DefineASM(static_cast<DWORD>(ASMOp::ADDEAX1),		"ADD EAX IMM1",		-1,		0x04,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::ADDEAX2),		"ADD EAX IMM2",		0x66,	0x05,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::ADDEAX4),		"ADD EAX IMM4",		-1,		0x05,	-1,		true);
	
	DefineASM(static_cast<DWORD>(ASMOp::SUBEAX1),		"SUB EAX IMM1",		-1,		0x2C,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::SUBEAX2),		"SUB EAX IMM2",		0x66,	0x2D,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::SUBEAX4),		"SUB EAX IMM4",		-1,		0x2D,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::SUBEAXEBX1),	"SUB EAX EBX1",		-1,		0x28,	0xD8,	false);
	DefineASM(static_cast<DWORD>(ASMOp::SUBEAXEBX2),	"SUB EAX EBX2",		0x66,	0x29,	0xD8,	false);
	DefineASM(static_cast<DWORD>(ASMOp::SUBEAXEBX4),	"SUB EAX EBX4",		-1,		0x29,	0xD8,	false);

	DefineASM(static_cast<DWORD>(ASMOp::MULEAXEBX1),	"IMUL EAX EBX1",	-1,		0xF6,	0xEB,	false);
	DefineASM(static_cast<DWORD>(ASMOp::MULEAXEBX2),	"IMUL EAX EBX2",	0x66,	0xF7,	0xEB,	false);
	DefineASM(static_cast<DWORD>(ASMOp::MULEAXEBX4),	"IMUL EAX EBX4",	-1,		0xF7,	0xEB,	false);

	DefineASM(static_cast<DWORD>(ASMOp::CDQ),			"CDQ",				-1,		0x99,	-1,		false);

	DefineASM(static_cast<DWORD>(ASMOp::DIVEAXEBX1),	"IDIV EAX EBX1",	-1,		0xF6,	0xFB,	false);
	DefineASM(static_cast<DWORD>(ASMOp::DIVEAXEBX2),	"IDIV EAX EBX2",	0x66,	0xF7,	0xFB,	false);
	DefineASM(static_cast<DWORD>(ASMOp::DIVEAXEBX4),	"IDIV EAX EBX4",	-1,		0xF7,	0xFB,	false);

	DefineASM(static_cast<DWORD>(ASMOp::ANDEAX1),		"AND EAX IMM1",		-1,		0x24,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::ANDEAX2),		"AND EAX IMM2",		0x66,	0x25,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::ANDEAX4),		"AND EAX IMM4",		-1,		0x25,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::ANDEAXEBX1),	"AND EAX EBX1",		-1,		0x20,	0xD8,	false);
	DefineASM(static_cast<DWORD>(ASMOp::ANDEAXEBX2),	"AND EAX EBX2",		0x66,	0x21,	0xD8,	false);
	DefineASM(static_cast<DWORD>(ASMOp::ANDEAXEBX4),	"AND EAX EBX4",		-1,		0x21,	0xD8,	false);

	DefineASM(static_cast<DWORD>(ASMOp::OREAX1),		"OR EAX IMM1",		-1,		0x0C,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::OREAX2),		"OR EAX IMM2",		0x66,	0x0D,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::OREAX4),		"OR EAX IMM4",		-1,		0x0D,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::OREAXEBX1),	"OR EAX EBX1",		-1,		0x08,	0xD8,	false);
	DefineASM(static_cast<DWORD>(ASMOp::OREAXEBX2),	"OR EAX EBX2",		0x66,	0x09,	0xD8,	false);
	DefineASM(static_cast<DWORD>(ASMOp::OREAXEBX4),	"OR EAX EBX4",		-1,		0x09,	0xD8,	false);

	DefineASM(static_cast<DWORD>(ASMOp::NOTEAX1),		"NOT EAX1",			-1,		0xF6,	0xD0,	false);
	DefineASM(static_cast<DWORD>(ASMOp::NOTEAX2),		"NOT EAX2",			0x66,	0xF7,	0xD0,	false);
	DefineASM(static_cast<DWORD>(ASMOp::NOTEAX4),		"NOT EAX4",			-1,		0xF7,	0xD0,	false);

	DefineASM(static_cast<DWORD>(ASMOp::XOREAX1),		"XOR EAX IMM1",		-1,		0x34,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::XOREAX2),		"XOR EAX IMM2",		0x66,	0x35,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::XOREAX4),		"XOR EAX IMM4",		-1,		0x35,	-1,		true);
	DefineASM(static_cast<DWORD>(ASMOp::XOREAXEBX1),	"XOR EAX EBX1",		-1,		0x30,	0xD8,	false);
	DefineASM(static_cast<DWORD>(ASMOp::XOREAXEBX2),	"XOR EAX EBX2",		0x66,	0x31,	0xD8,	false);
	DefineASM(static_cast<DWORD>(ASMOp::XOREAXEBX4),	"XOR EAX EBX4",		-1,		0x31,	0xD8,	false);
	
	DefineASM(static_cast<DWORD>(ASMOp::SHLEAX1),		"SHL EAX1 IMM1",	-1,		0xC0,	0xE0,	true);
	DefineASM(static_cast<DWORD>(ASMOp::SHLEAX2),		"SHL EAX2 IMM2",	0x66,	0xC1,	0xE0,	true);
	DefineASM(static_cast<DWORD>(ASMOp::SHLEAX4),		"SHL EAX4 IMM4",	-1,		0xC1,	0xE0,	true);
	DefineASM(static_cast<DWORD>(ASMOp::SHLEAXCLC1),	"SHL EAX1 CL",		-1,		0xD2,	0xE0,	false);
	DefineASM(static_cast<DWORD>(ASMOp::SHLEAXCLC2),	"SHL EAX2 CL",		0x66,	0xD3,	0xE0,	false);
	DefineASM(static_cast<DWORD>(ASMOp::SHLEAXCLC4),	"SHL EAX4 CL",		-1,		0xD3,	0xE0,	false);

	DefineASM(static_cast<DWORD>(ASMOp::SHREAX1),		"SHR EAX1 IMM1",	-1,		0xC0,	0xE8,	true);
	DefineASM(static_cast<DWORD>(ASMOp::SHREAX2),		"SHR EAX2 IMM2",	0x66,	0xC1,	0xE8,	true);
	DefineASM(static_cast<DWORD>(ASMOp::SHREAX4),		"SHR EAX4 IMM4",	-1,		0xC1,	0xE8,	true);
	DefineASM(static_cast<DWORD>(ASMOp::SHREAXCLC1),	"SHR EAX1 CL",		-1,		0xD2,	0xE8,	false);
	DefineASM(static_cast<DWORD>(ASMOp::SHREAXCLC2),	"SHR EAX2 CL",		0x66,	0xD3,	0xE8,	false);
	DefineASM(static_cast<DWORD>(ASMOp::SHREAXCLC4),	"SHR EAX4 CL",		-1,		0xD3,	0xE8,	false);

	DefineASM(static_cast<DWORD>(ASMOp::MULECXEDX4),	"IMUL ECX EDX4",	0x0F,	0xAF,	0x0A,	false);
	DefineASM(static_cast<DWORD>(ASMOp::ADDEBXEDX4),	"ADD EBX EDX4",		-1,		0x03,	0xDA,	false);
}

void CASMWriter::DefineASM(DWORD dwASMCode, LPSTR pDebugStr, int iPreOp, int iOp1, int iOp2, bool bOpData)
{
	// Store Debug String for ASM Code
	m_ASMDebugStrings[dwASMCode] = pDebugStr;

	// Store OpCodes for ASM Code
	m_iASMPreOp[dwASMCode]=iPreOp;
	m_iASMOp1[dwASMCode]=iOp1;
	m_iASMOp2[dwASMCode]=iOp2;

	// Store OpData Flag for ASM Code
	m_bASMOpData[dwASMCode]=bOpData;
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

bool CASMWriter::CreateASMMiddle(int iPreOpCode, int iOpCode1, int iOpCode2, LPSTR lpOpData)
{
	return CreateASMMiddleCore(iPreOpCode, iOpCode1, iOpCode2, lpOpData, NULL, false, 0);
}

bool CASMWriter::CreateASMMiddleCore(int iPreOpCode, int iOpCode1, int iOpCode2, LPSTR lpOpData, LPSTR lpOpData2, bool bSecondOpDataIsIMM, DWORD dwSecondOpDataIMMSize)
{
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

	// Write Optional OpData 1 and 2
	for(DWORD n=0; n<2; n++)
	{
		LPSTR pData=lpOpData;
		if(n==1) pData=lpOpData2;
		if(pData)
		{
			if(strcmp(pData, "")!=NULL)
			{
				// Ensure reference array always large enough for new reference
				CheckAndExpandREFMemory();

				// REF or IMM
				if(bSecondOpDataIsIMM==true && n==1)
				{
					// WRITE IMM INTO MC
					DWORD dwDataAsDWORD = (DWORD)_atoi64(pData);
					// Convert legacy size code (0=1byte, 1=2bytes, 2=4bytes) to actual byte size
					DWORD dwByteSize = (dwSecondOpDataIMMSize == 0) ? 1 : (dwSecondOpDataIMMSize == 1) ? 2 : 4;
					m_machineCodeBuffer.WriteDWORD(dwDataAsDWORD, dwByteSize);
				}
				else
				{
					// Record Reference Position & Label
					char* pStr = new char[strlen(pData)+1];
					strcpy_s(pStr, strlen(pData)+1, pData);
					CStr cleanStr(pStr);
					cleanStr.EatEdgeSpacesandTabs(NULL);
					strcpy_s(pStr, strlen(pData)+1, cleanStr.GetStr());

					DWORD MCBBytePos = m_machineCodeBuffer.GetCurrentMCPosition();
					m_referenceTracker.AddReference(MCBBytePos, static_cast<DWORD>(reinterpret_cast<uintptr_t>(pStr)));

					// WRITE BLANK(XXXX) INTO MB
					m_machineCodeBuffer.WriteDWORD((DWORD)0xFFFFFFFF, 4);
				}
			}
		}
		if(lpOpData2==NULL) break;
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

	// Validate PE header alignment requirements via m_peBuilder
	if (!m_peBuilder.ValidatePEHeaderRequirements(0x400000, 4096, 512))
	{
		g_pErrorReport->AddErrorString("Failed to 'PrepareEXE' : Invalid PE Header Requirements");
		return false;
	}

	// Toggled if init/run fails..
	bool bResult=true;

	// The end of the machine code (if any)
	if(bGotNewCode==true)
	{
		// Apply new machine code to EXEData
		LPSTR pProgramEnd = m_machineCodeBuffer.GetMachineBlock();
		DWORD dwProgramSizeBytes = pProgramEnd-m_machineCodeBuffer.GetProgramStart();

		// Machine Code Block (MCB)
		if(UpdateMCB(dwProgramSizeBytes)==false)
		{
			g_pErrorReport->AddErrorString("Failed to 'PrepareEXE' : 1");
			return false;
		}

		// MCB Reference Data
		if(UpdateMCBRefData()==false)
		{
			g_pErrorReport->AddErrorString("Failed to 'PrepareEXE' : 2");
			return false;
		}

		// DLL Data
		if(UpdateDLLData()==false)
		{
			g_pErrorReport->AddErrorString("Failed to 'PrepareEXE' : 3");
			return false;
		}

		// Command Data
		if(UpdateCommandData()==false)
		{
			g_pErrorReport->AddErrorString("Failed to 'PrepareEXE' : 4");
			return false;
		}

		// Strings Data
		if(UpdateStringData()==false)
		{
			g_pErrorReport->AddErrorString("Failed to 'PrepareEXE' : 5");
			return false;
		}

		// Data Data
		if(UpdateDataData()==false)
		{
			g_pErrorReport->AddErrorString("Failed to 'PrepareEXE' : 6");
			return false;
		}

		// Dynamic Variables Data
		if(UpdateDynamicData()==false)
		{
			g_pErrorReport->AddErrorString("Failed to 'PrepareEXE' : 7");
			return false;
		}

		// U71 - Structures Data (for getting struct pattern at runtime)
		if(UpdateStructurePatternData()==false)
		{
			g_pErrorReport->AddErrorString("Failed to 'PrepareEXE' : 8");
			return false;
		}
	}

	if(g_pDBPCompiler->ValidateRuntimeBundle(
		g_pEXE->m_dwUsertypeStringPatternQuantity)==false)
	{
		return false;
	}

	// New Var and Data Space Sizes
	g_pEXE->m_dwVariableSpaceSize = g_pStatementList->GetVarOffsetCounter();
	g_pEXE->m_dwDataSpaceSize = g_pStatementList->GetDataIndexCounter()*10;

	// Full or Debug Mode
	if(g_DebugInfo.DebugModeOn())
	{
		// Find Debugger to send to
		HWND hWnd = FindWindowW(NULL, L"DBProDebugger");
		if(hWnd==NULL && CDebuggerInterface::IsInternalDebuggerActive()==true)
		{
			// Internal Debugger has been termined, so terminate App too...
			g_dwEscapeValueMem=2;
		}
		else
		{
			if(hWnd==NULL)
			{
				// If EXE for debug is absolute, using long range debug, so switch to correct dir
				if(pEXEFilename[1]==':')
				{
					// leefix - 240603 - u54 - this means we are running %TEMP\TEMP.EXE
					// and so we need to restore to the actual project exe directory (as we are in TEMP at the mo)
					// BUT when debug runs, the above exe does not point to project folder
					// so rely on the code from media path below
//					CStr* pPathOnly = new CStr("");
//					pPathOnly->SetText(pEXEFilename);
//					pPathOnly->TrimToPathOnly();
//					chdir(pPathOnly->GetStr());
//					SAFE_DELETE(pPathOnly);

// leefix - 240604 - u54 - removed as it seemed to do nothing, and even corrupted m_pEXEFilename
//					// use temp exe in temp folder (absolute path to temp exe)
//					strcpy(pEXEFilename, g_pDBPCompiler->GetInternalFile(PATH_TEMPFOLDER));
//					strcat(pEXEFilename, "\\temp.exe");
				}
			
				// Switch to Compiler Folder First
				char pStoreDir[_MAX_PATH];
				getcwd(pStoreDir, _MAX_PATH);

				// Change Directory now
				chdir(g_pDBPCompiler->GetInternalFile(PATH_ROOTPATH));

				// Start Debugger Window
				STARTUPINFO si;
				ZeroMemory(&si, sizeof(STARTUPINFO));
				si.cb=sizeof(STARTUPINFO);
				ZeroMemory(&CDebuggerInterface::GetDebuggerProcessInfo(), sizeof(PROCESS_INFORMATION));
				std::wstring wCmdLine = TextConvert::UTF8ToUTF16(g_pDBPCompiler->GetInternalFile(PATH_DEBUGGERFILE));
				if(CreateProcessW(	NULL, &wCmdLine[0],
									NULL, NULL, false,
									NORMAL_PRIORITY_CLASS,
									NULL, NULL,	&si, &CDebuggerInterface::GetDebuggerProcessInfo()))
				{
					// Wait until Debugger fully loaded
					WaitForInputIdle(CDebuggerInterface::GetDebuggerProcessInfo().hProcess, 5000);
					CDebuggerInterface::SetInternalDebuggerActive(true);
				}

				// Restore previous directory
				chdir(pStoreDir);

// leefix - 210504 - seemed this could mess up EXE current directory 
// leefix - 240604 - restored as running debug mode removes CWD info!
				// Media Root Folder is project folder so switch to it
				std::unique_ptr<char[]> pMediaRoot(g_pDBPCompiler->GetProjectMediaRoot());
				if(pMediaRoot)
					if(strcmp(pMediaRoot.get(),"")!=NULL)
						chdir(pMediaRoot.get());
			}

			// Transfer Compiler Info to Debugger
			LPSTR pData = g_DebugInfo.GetProgramPtr();
			CDebuggerInterface::HideAnyHiddenCode(pData, g_DebugInfo.GetProgramSize());
			CDebuggerInterface::SendDataToDebugger(1, pData, g_DebugInfo.GetProgramSize());
			DWORD dwDataSize=0;
			std::unique_ptr<char[]> pVarGlobalData(MakeVarDataForTransfer(&dwDataSize));
			CDebuggerInterface::SendDataToDebugger(2, pVarGlobalData.get(), dwDataSize);
			LPSTR pNameData = g_pDBPCompiler->GetProgramName();
			CDebuggerInterface::SendDataToDebugger(3, pNameData, strlen(pNameData));

			// Report Any Runtime Errors from previous run..
			ReportAnyErrorsToCLI();

			// Prepare Pointers to Debug Hook Functions
			bResult=true;
			LPVOID pDHookS = (LPVOID)DebugHookStatementFunctionCall;
			LPVOID pDHookJ = (LPVOID)DebugHookJumpFunctionCall;
			LPVOID pDHookR = (LPVOID)DebugHookReturnFunctionCall;
			LPSTR pReturnError = NULL;
			std::unique_ptr<char[]> pReturnErrorOwner;

			// Critical start info
			g_pEXE->m_UnpackFolderName = g_pDBPCompiler->GetInternalFile(PATH_PLUGINSFOLDER);
			g_pEXE->m_dwEncryptionKey=0;
			g_pEXE->StartInfo(const_cast<LPSTR>(g_pEXE->m_UnpackFolderName.c_str()), g_pEXE->m_dwEncryptionKey);

			// Main Program Run or MiniProgram Run
			if(bParsingMainProgram)
			{
				// MAIN PROGRAM RUN (MAIN-RUN)

				// Init The EXE in Debug Mode
				bResult=g_pEXE->InitDebug(g_DebugInfo.GetInstance(), pDHookS, pDHookJ, pDHookR, bResult, &pReturnError, NULL, false);

				// Store Variable Space Address In Global (for use by VarHook above)
				g_pVarSpaceAddressInUse = g_pEXE->m_pVariableSpace;
				g_dwVarSpaceSizeInUse = g_pEXE->m_dwVariableSpaceSize;

				// Run The EXE
				bResult=g_pEXE->Run(bResult);
			}
			else
			{
				// MINI PROGRAM RUN (MINI-RUN)

				// Mini-Init (only inits new stuff)
				bResult=g_pEXE->InitMini(pDHookS, pDHookJ, pDHookR, bResult, &pReturnError);

				// Aquire New Variable Allocation
				g_pVarSpaceAddressInUse = g_pEXE->m_pVariableSpace;
				g_dwVarSpaceSizeInUse = g_pEXE->m_dwVariableSpaceSize;

				// Only run CLItext if it produced valid code
				if(bGotNewCode==true)
				{
					// Store breakpoint position from mast run break
					DWORD dwStoreBreakPointBeforeRunCLICode = g_dwBreakOutPosition;

					// Ensure CLI run is full speed
					g_dwEscapeValueMem = 0;

					// Run The EXE for CLICode (remove breakpoint influence for min-prog)
					g_dwBreakOutPosition = 0;
					bResult=g_pEXE->RunFrom(bResult, g_pEXE->m_dwStartOfMiniMC);

					// Restore breakpoint position from mast run break
					g_dwBreakOutPosition = dwStoreBreakPointBeforeRunCLICode;
				}

				// Update BreakPoint with new memblock ptr for return jump (if any)
				if(g_dwBreakOutPosition>0)
				{
					g_dwBreakOutPosition = g_dwBreakOutPosition + (DWORD)g_pEXE->m_pMachineCodeBlock;
				}

				// Update screen (using SYNC) as CLI may not do this..
				g_CORE_SyncRefresh(); g_CORE_SyncRefresh();

				// Ensure CLI runs is debug mode!
				g_dwEscapeValueMem = 1;

				// Report Any Runtime Errors from CLI run..
				ReportAnyErrorsToCLI();

				// Run The Main EXE Again
				bResult=g_pEXE->Run(bResult);
			}

			// If special escape value set, broke from program whilst data still on stacl (cannot return to program)
			if(g_dwEscapeValueMem==3)
			{
				// ESP in and ESP out was not equal - means broke from function..
				g_dwEscapeValueMem=1;
				g_pEXE->m_dwRuntimeErrorDWORD=1;
				g_pEXE->m_dwRuntimeErrorLineDWORD=0;
			}

			// Free Debugger When Done (when quit program)
			if(CDebuggerInterface::IsInternalDebuggerActive()==true)
			{
				if(g_dwEscapeValueMem==2)
				{
					DWORD uExitCode=0;
					GetExitCodeProcess(CDebuggerInterface::GetDebuggerProcessInfo().hProcess, &uExitCode);
					TerminateProcess(CDebuggerInterface::GetDebuggerProcessInfo().hProcess,  (UINT)uExitCode );
				}
			}

			// Free Usages
			pReturnErrorOwner.reset(pReturnError);
		}
	}
	else
	{
		// Progress Reporting Tool
		g_pErrorReport->ProgressReport("Linker now at line ",g_pErrorReport->GetPerc(0));

		// Save EXE Block
		g_pEXE->Save(g_pDBPCompiler->GetInternalFile(PATH_TEMPEXBFILE));

		// Plugins Root Folder (same as project folder)
		char pEffectsRoot[_MAX_PATH];
		char pPluginsRoot[_MAX_PATH];
		const auto* runtimeBundle = g_pDBPCompiler->GetResolvedRuntimeBundle();
		if(runtimeBundle==NULL)
		{
			g_pErrorReport->AddErrorString("DBP3002: Runtime bundle was not resolved before packaging.");
			return false;
		}
		auto effectsRoot = runtimeBundle->effectsDirectory.string() + "\\";
		auto pluginsRoot = runtimeBundle->pluginsDirectory.string() + "\\";
		auto userPluginsRoot =
			runtimeBundle->userPluginsDirectory.string() + "\\";
		auto licensedPluginsRoot =
			runtimeBundle->licensedPluginsDirectory.string() + "\\";
		strcpy_s(pEffectsRoot, effectsRoot.c_str());
		strcpy_s(pPluginsRoot, pluginsRoot.c_str());

		// Extra Plugins Folders
		char pUserPluginsRoot[_MAX_PATH];
		char pLicensedPluginsRoot[_MAX_PATH];
		strcpy_s(pUserPluginsRoot, userPluginsRoot.c_str());
		strcpy_s(
			pLicensedPluginsRoot,
			licensedPluginsRoot.c_str());

		// Media Root Folder
		std::unique_ptr<char[]> pMediaRootOwner(g_pDBPCompiler->GetProjectMediaRoot());
		if(pMediaRootOwner==NULL)
		{
			// No Media Root Folder so use current directory)
			pMediaRootOwner.reset(new char[_MAX_PATH]);
			getcwd(pMediaRootOwner.get(),_MAX_PATH);
		}
		LPSTR pMediaRoot = pMediaRootOwner.get();
		DWORD dwCutOutRoot=strlen(pMediaRoot);

		// Create EXECUTABLE Here
		CFileBuilder CFBuilder;
		CFBuilder.SetPackageKeyFile(
			g_pDBPCompiler->GetPackageKeyFile());

		// Pass All Filenames for System Files & Media
		CFBuilder.NewFileTable();

		// Virtual EXB for Program Code
		CFBuilder.AddFile(g_pDBPCompiler->GetInternalFile(PATH_TEMPEXBFILE), "_virtual.dat");
	
		// Add DLLs to Build
		bool bFXFilesRequired = false;
		if ( CDebuggerInterface::ShouldExternaliseDLLs()==false )
		{
			// only if not entirely excluding DLLs
			for(DWORD dll=0; dll<g_pEXE->m_dwNumberOfDLLs; dll++)
			{
				// DLL To Add To Table
				LPSTR pDLLName = (LPSTR)g_pEXE->m_pDLLFilenameArray[dll];

				// Add Actual File to Table
				char pAbsPathToDLL[_MAX_PATH];
				if(stricmp(pDLLName, "dbprocore.dll")==NULL)
					strcpy_s(pAbsPathToDLL, runtimeBundle->corePath.string().c_str());
				else
				{
					snprintf(pAbsPathToDLL, sizeof(pAbsPathToDLL), "%s%s", pPluginsRoot, pDLLName);
				}

				const bool isCore = stricmp(pDLLName, "dbprocore.dll")==NULL;
				if(isCore && !g_pDBPCompiler->FileExists(pAbsPathToDLL))
				{
					g_pErrorReport->AddErrorString(
						"DBP3002: The validated runtime DBProCore.dll is no longer available.");
					return false;
				}

				// Core is fail-closed. Other product plugins retain legacy fallback.
				if(g_pDBPCompiler->FileExists(pAbsPathToDLL))
				{
					CFBuilder.AddFile(pAbsPathToDLL, pDLLName);
				}
				else if(!isCore)
				{
					// Get DLL from user folder (if exists - some DLLs can be removed without fault, ie Conv3DS)
					snprintf(pAbsPathToDLL, sizeof(pAbsPathToDLL), "%s%s", pUserPluginsRoot, pDLLName);
					if(g_pDBPCompiler->FileExists(pAbsPathToDLL))
					{
						CFBuilder.AddFile(pAbsPathToDLL, pDLLName);
					}
					else
					{
						// leefix - 120104 - DLL can be in licensed folder also
						snprintf(pAbsPathToDLL, sizeof(pAbsPathToDLL), "%s%s", pLicensedPluginsRoot, pDLLName);
						if(g_pDBPCompiler->FileExists(pAbsPathToDLL))
						{
							CFBuilder.AddFile(pAbsPathToDLL, pDLLName);
						}
					}
				}

				// If DLL is Basic3D, need FX files too
				if ( stricmp ( pDLLName, "dbprobasic3ddebug.dll" )==NULL ) bFXFilesRequired = true;
			}
		}

		// Check if Icon Specified256 colour icon specified
		std::unique_ptr<char[]> pIncludesReplacementIconFile(g_pDBPCompiler->GetProjectField("icon1"));
		if(pIncludesReplacementIconFile)
			if(strcmp(pIncludesReplacementIconFile.get(), "")==NULL)
				pIncludesReplacementIconFile.reset();

		// All MEDIA required (from project file)
		if(g_pDBPCompiler->m_bInternalMediaState==true)
		{
			DWORD media=1;
			while(media<65535)
			{
				// Get Media File
				char mediafieldname[256];
				snprintf(mediafieldname, sizeof(mediafieldname), "media%d", media);
				std::unique_ptr<char[]> pMediaFileName(g_pDBPCompiler->GetProjectField(mediafieldname));
				if(pMediaFileName)
				{
					if(strcmp(pMediaFileName.get(),"")==NULL)
					{
						break;
					}
				}
				if(pMediaFileName==NULL) break;

				// if pMediaFileName is absolute
				bool bUseMedia=true;
				if(pMediaFileName[1]==':')
				{
					if(_strnicmp(pMediaFileName.get(), pMediaRoot, strlen(pMediaRoot))==NULL)
					{
						// remove media path from it
						strrev(pMediaFileName.get());
						pMediaFileName[strlen(pMediaRoot)]=0;
						strrev(pMediaFileName.get());
					}
					else
					{
						// media must be in media root - otherwise ignore
						pMediaFileName.reset();
						bUseMedia=false;
					}
				}
				if(bUseMedia)
				{
					// Check if media has wildcards (more than one file)
					bool bHadWildcards=false;
					for(DWORD d=0; d<strlen(pMediaFileName.get()); d++)
						if(pMediaFileName[d]=='*')
							bHadWildcards=true;

					// What is the relative path from the project root onwards
					char pMediaPath[_MAX_PATH];
					char pAbsPathToMedia[_MAX_PATH];
					if(bHadWildcards==false)
					{
						// Add Actual File to Table
						snprintf(pMediaPath, sizeof(pMediaPath), "media\\%s", pMediaFileName.get());
						snprintf(pAbsPathToMedia, sizeof(pAbsPathToMedia), "%s%s", pMediaRoot, pMediaFileName.get());
						CFBuilder.AddFile(pAbsPathToMedia, pMediaPath);
					}
					else
					{
						// Add Wildcard Files to Table
						CFBuilder.AddWildcardFiles(pMediaRoot, pMediaFileName.get());
					}

					// Only overwrite if icon.ico (priority override icon)
					if(stricmp(pMediaFileName.get(), "icon.ico")==NULL)
					{
						// Found better ICO file
						pIncludesReplacementIconFile = std::move(pMediaFileName);
					}
				}

				// Next Media index
				media++;
			}
		}

		// Icon file used for window/taskbar
		if(pIncludesReplacementIconFile)
		{
			// Media includes an icon
			bool bUseMedia=true;
			char pAbsPathToMediaIcon[_MAX_PATH];
			if(pIncludesReplacementIconFile[1]==':')
			{
				if(_strnicmp(pIncludesReplacementIconFile.get(), pMediaRoot, strlen(pMediaRoot))==NULL)
				{
					// remove media path from it
					snprintf(pAbsPathToMediaIcon, sizeof(pAbsPathToMediaIcon), "%s", pIncludesReplacementIconFile.get());
				}
				else
				{
					// media must be in media root - otherwise ignore
					pIncludesReplacementIconFile.reset();
					bUseMedia=false;
				}
			}
			else
			{
				snprintf(pAbsPathToMediaIcon, sizeof(pAbsPathToMediaIcon), "%s%s", pMediaRoot, pIncludesReplacementIconFile.get());
			}
			if(bUseMedia)
			{
				// Make an ICO file from BMP
				char pWorkIcon[_MAX_PATH];
				DWORD dwLength=strlen(pAbsPathToMediaIcon);
				if(_strnicmp(pAbsPathToMediaIcon+dwLength-4, ".bmp", 4)==NULL)
				{
					// BMP to ICO
					snprintf(pWorkIcon, sizeof(pWorkIcon), "%s", pPluginsRoot);
					CFBuilder.MakeICOFromBMP(pAbsPathToMediaIcon, pWorkIcon);
					snprintf(pWorkIcon, sizeof(pWorkIcon), "%sworkicon.ico", pPluginsRoot);
				}
				else
				{
					// Must be ICO
					snprintf(pWorkIcon, sizeof(pWorkIcon), "%s", pAbsPathToMediaIcon);
				}

				// Add Actual File to Table
				CFBuilder.AddFile(pWorkIcon, "icon.ico");
				pIncludesReplacementIconFile.reset();
			}
		}
		else
		{
			// Add Actual File to Table
			char pAbsPathToIcon[_MAX_PATH];
			snprintf(pAbsPathToIcon, sizeof(pAbsPathToIcon), "%s%s", pPluginsRoot, "icon.ico");
			CFBuilder.AddFile(pAbsPathToIcon, "icon.ico");
		}

		// Cursor files used for application window
		for ( DWORD cindex=0; cindex<32; cindex++)
		{
			char pFieldName[_MAX_PATH];
			char pFinalFileName[_MAX_PATH];
			if ( cindex==0 )
			{
				snprintf(pFieldName, sizeof(pFieldName), "%s", "cursorarrow");
				snprintf(pFinalFileName, sizeof(pFinalFileName), "%s", "arrow.cur");
			}
			if ( cindex==1 )
			{
				snprintf(pFieldName, sizeof(pFieldName), "%s", "cursorwait");
				snprintf(pFinalFileName, sizeof(pFinalFileName), "%s", "hourglass.cur");
			}
			if ( cindex>=2 )
			{
				snprintf(pFieldName, sizeof(pFieldName), "pointer%d", cindex);
				snprintf(pFinalFileName, sizeof(pFinalFileName), "pointer%d.cur", cindex);
			}

			std::unique_ptr<char[]> pCursorFilename(g_pDBPCompiler->GetProjectField(pFieldName));
			if(pCursorFilename)
			{
				if(strcmp(pCursorFilename.get(), "")>0)
				{
					// Media includes an icon
					bool bUseMedia=true;
					char pAbsPathToMediaCur[_MAX_PATH];
					if(pCursorFilename[1]==':')
					{
						if(_strnicmp(pCursorFilename.get(), pMediaRoot, strlen(pMediaRoot))==NULL)
						{
							// remove media path from it
							snprintf(pAbsPathToMediaCur, sizeof(pAbsPathToMediaCur), "%s", pCursorFilename.get());
						}
						else
						{
							// media must be in media root - otherwise ignore
							pCursorFilename.reset();
							bUseMedia=false;
						}
					}
					else
					{
						snprintf(pAbsPathToMediaCur, sizeof(pAbsPathToMediaCur), "%s%s", pMediaRoot, pCursorFilename.get());
					}
					if(bUseMedia)
					{
						// Make an CUR file from BMP
						char pWorkCursor[_MAX_PATH];
						DWORD dwLength=strlen(pAbsPathToMediaCur);
						if(_strnicmp(pAbsPathToMediaCur+dwLength-4, ".bmp", 4)==NULL)
						{
							// BMP to CUR
							snprintf(pWorkCursor, sizeof(pWorkCursor), "%sworkcursor%d.cur", pPluginsRoot, cindex);
							CFBuilder.MakeCURFromBMP(pAbsPathToMediaCur, pWorkCursor);
						}
						else
						{
							// Must be CUR
							snprintf(pWorkCursor, sizeof(pWorkCursor), "%s", pAbsPathToMediaCur);
						}

						// Add Actual File to Table
						CFBuilder.AddFile(pWorkCursor, pFinalFileName);
					}
				}
			}
		}

		// Add FX Files when appropriate
		if ( bFXFilesRequired==true )
		{
			char pFXName[_MAX_PATH];
			char pFXPathAndName[_MAX_PATH];
			for ( int i=0; i<6; i++)
			{
				const char* pFXLiteral = nullptr;
				if ( i==0 ) pFXLiteral = "bump.fx";
				if ( i==1 ) pFXLiteral = "cartoon.fx";
				if ( i==2 ) pFXLiteral = "rainbow.fx";
				if ( i==3 ) pFXLiteral = "stencilshadow.fx";
				if ( i==4 ) pFXLiteral = "stencilshadowbone.fx";
				if ( i==5 ) pFXLiteral = "quad.fx";
				snprintf(pFXName, sizeof(pFXName), "%s", pFXLiteral);
				snprintf(pFXPathAndName, sizeof(pFXPathAndName), "%s%s", pEffectsRoot, pFXLiteral);
				CFBuilder.AddFile(pFXPathAndName, pFXName);
			}
		}

		// Progress Reporting Tool
		g_pErrorReport->ProgressReport("Linker now at line ",g_pErrorReport->GetPerc(10));

		// Make The EXE
		if(!CFBuilder.MakeEXE(
				pEXEFilename,
				g_pDBPCompiler->m_bEncryptionState,
				NULL))
		{
			FreeMachineBlock();
			return false;
		}

		// Progress Reporting Tool
		g_pErrorReport->ProgressReport("Linker now at line ",g_pErrorReport->GetPerc(50));

		// Ensure EXE created okay
		if(CFBuilder.HasStagedExecutable())
		{
			// Produce Absolute Paths to replacement-files resources
			char pAbsResourceFile[_MAX_PATH];
			CFBuilder.NewFileTable();

			// Get stringdatas (max 24 chars as appendages takes up to 32 chars)
			std::unique_ptr<char[]> pOrigAppName(g_pDBPCompiler->GetProjectField("app title"));
			char pAppName[33];
			memset ( pAppName, 0, 33 );
			if(pOrigAppName)
			{
				DWORD dwSourceSize = 0;
				if ( pOrigAppName ) dwSourceSize = strlen(pOrigAppName.get());
				if ( dwSourceSize > 24 ) dwSourceSize=24;
				memcpy(pAppName, pOrigAppName.get(), dwSourceSize);
			}
			else
				snprintf(pAppName, sizeof(pAppName), "%s", "DBPro Application");

			pAppName[24]=0;

			char pExeName[33];
			memset ( pExeName, 0, 33 );
			if(pEXEFilename)
			{
				DWORD dwSourceSize = strlen(pEXEFilename);
				if ( dwSourceSize > 24 ) dwSourceSize=24;
				memcpy(pExeName, pEXEFilename, dwSourceSize);
			}
			else
				snprintf(pExeName, sizeof(pExeName), "%s", "executable.exe");

			pExeName[24]=0;

			// [0-9] version info (strings not files)
			for(int iIndex=0; iIndex<=9; iIndex++)
			{
				// Get ver info
				std::unique_ptr<char[]> pStr;
				if(iIndex==0) pStr.reset(g_pDBPCompiler->GetProjectField("VerComments"));
				if(iIndex==1) pStr.reset(g_pDBPCompiler->GetProjectField("VerCompany"));
				if(iIndex==2) pStr.reset(g_pDBPCompiler->GetProjectField("VerFileDesc"));
				if(iIndex==3) pStr.reset(g_pDBPCompiler->GetProjectField("VerFileNumber"));
				if(iIndex==4) pStr.reset(g_pDBPCompiler->GetProjectField("VerInternal"));
				if(iIndex==5) pStr.reset(g_pDBPCompiler->GetProjectField("VerCopyright"));
				if(iIndex==6) pStr.reset(g_pDBPCompiler->GetProjectField("VerTrademark"));
				if(iIndex==7) pStr.reset(g_pDBPCompiler->GetProjectField("VerFilename"));
				if(iIndex==8) pStr.reset(g_pDBPCompiler->GetProjectField("VerProduct"));
				if(iIndex==9) pStr.reset(g_pDBPCompiler->GetProjectField("VerProductNumber"));

				// Add to filetable [0-9]
				if(iIndex==3 || iIndex==9)
				{
					char pFillStr[11];
					memset(pFillStr, 0, 11);
					if(pStr==NULL)
					{
						if(iIndex==3) snprintf(pFillStr, sizeof(pFillStr), "%s", "V1.0");
						if(iIndex==9) snprintf(pFillStr, sizeof(pFillStr), "%s", "V1.0");
					}
					else
					{
						DWORD dwSourceSize = 0;
						if ( pStr ) dwSourceSize = strlen(pStr.get());
						if ( dwSourceSize > 10 ) dwSourceSize=10;
						memcpy(pFillStr, pStr.get(), dwSourceSize);
						pFillStr[10]=0;
					}
					CFBuilder.AddFile(pFillStr,"");
				}
				else
				{
					char pFillStr[33];
					memset(pFillStr, 0, 33);
					if(pStr==NULL)
					{
						// Copy to fill string
						if(iIndex==0) snprintf(pFillStr, sizeof(pFillStr), "%s", pAppName);
						if(iIndex==1)
						{
							snprintf(pFillStr, sizeof(pFillStr), "%s Ltd", pAppName);
						}
						if(iIndex==2) snprintf(pFillStr, sizeof(pFillStr), "%s", pAppName);
						if(iIndex==4) snprintf(pFillStr, sizeof(pFillStr), "%s", pAppName);
						if(iIndex==5)
						{
							snprintf(pFillStr, sizeof(pFillStr), "(C) %s Ltd", pAppName);
						}
						if(iIndex==6)
						{
							snprintf(pFillStr, sizeof(pFillStr), "%s", "");
						}
						if(iIndex==7)
						{
							snprintf(pFillStr, sizeof(pFillStr), "%s", pExeName);
						}
						if(iIndex==8)
						{
							snprintf(pFillStr, sizeof(pFillStr), "%s", pAppName);
						}
					}
					else
					{
						DWORD dwSourceSize = 0;
						if ( pStr ) dwSourceSize = strlen(pStr.get());
						if ( dwSourceSize > 32 ) dwSourceSize=32;
						memcpy(pFillStr, pStr.get(), dwSourceSize);
						pFillStr[32]=0;
					}
					CFBuilder.AddFile(pFillStr,"");
				}
			}

			// [10] 32x32 icon
			snprintf(pAbsResourceFile, sizeof(pAbsResourceFile), "%s", "");
			std::unique_ptr<char[]> pIconFilename(g_pDBPCompiler->GetProjectField("icon1"));
			if(pIconFilename==NULL) pIconFilename.reset(g_pDBPCompiler->GetProjectField("icon1"));
			if(pIconFilename)
			{
				if(pIconFilename[1]!=':')
					snprintf(pAbsResourceFile, sizeof(pAbsResourceFile), "%s%s", pMediaRoot, pIconFilename.get());
				else
					snprintf(pAbsResourceFile, sizeof(pAbsResourceFile), "%s", pIconFilename.get());
			}
			else
			{
				snprintf(pAbsResourceFile, sizeof(pAbsResourceFile), "%s%s", pPluginsRoot, "icon.ico");
			}
			CFBuilder.AddFile(pAbsResourceFile,"");

			// [11] 16x16 icon
			snprintf(pAbsResourceFile, sizeof(pAbsResourceFile), "%s", "");
			pIconFilename.reset(g_pDBPCompiler->GetProjectField("icon2"));
			if(pIconFilename==NULL) pIconFilename.reset(g_pDBPCompiler->GetProjectField("icon1"));
			if(pIconFilename)
			{
				if(pIconFilename[1]!=':')
					snprintf(pAbsResourceFile, sizeof(pAbsResourceFile), "%s%s", pMediaRoot, pIconFilename.get());
				else
					snprintf(pAbsResourceFile, sizeof(pAbsResourceFile), "%s", pIconFilename.get());
			}
			else
			{
				snprintf(pAbsResourceFile, sizeof(pAbsResourceFile), "%s%s", pPluginsRoot, "icon.ico");
			}
			CFBuilder.AddFile(pAbsResourceFile,"");

			// Progress Reporting Tool
			g_pErrorReport->ProgressReport("Linker now at line ",g_pErrorReport->GetPerc(60));

			// Change The EXE with new resources
			if(!CFBuilder.ChangeEXE(pEXEFilename, pPluginsRoot))
			{
				FreeMachineBlock();
				return false;
			}

			// Kind Of Executable Determines if(installer or not)
			DWORD KindOfExecutable=0;
			if(g_pDBPCompiler->GetEXEInstallerState())
			{
				KindOfExecutable=1;
			}

			// Progress Reporting Tool
			g_pErrorReport->ProgressReport("Linker now at line ",g_pErrorReport->GetPerc(100));

			// Publish the authenticated sidecar and descriptor only after all
			// PE resource customization has completed.
			if(!CFBuilder.FinalizePackage(
					pEXEFilename,
					KindOfExecutable))
			{
				FreeMachineBlock();
				return false;
			}
		}
		else
		{
			// EXE Not Created
			FreeMachineBlock();

			// Report failure
			g_pErrorReport->AddErrorString("Failed to 'PrepareEXE' : EXE Filename not exist");

			// return as failure
			return false;
		}
	}

	// Free Usages
	FreeMachineBlock();

	// Complete
	return bResult;
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
	return m_referenceTracker.UpdateMCBRefData(g_pEXE);
}

bool CASMWriter::UpdateDLLData(void)
{
	db3::CProfile<> prof("CASMWriter::UpdateDLLData");

	DWORD dwDLLIndex = 0;
	if(g_pEXE->m_pDLLIndexArray==NULL)
	{
		// Create Array(s)
		DWORD dwDLLCount = g_pStatementList->GetDLLIndexCounter();
		LPSTR pNewArray1 = (LPSTR)g_pEXE->CreateArray(dwDLLCount);
		uintptr_t* pNewArray2 = g_pEXE->CreatePtrArray(dwDLLCount);
		LPSTR pNewArray3 = (LPSTR)g_pEXE->CreateArray(dwDLLCount);

		// Update pointers
		g_pEXE->m_dwNumberOfDLLs = dwDLLCount;
		g_pEXE->m_pDLLIndexArray = (DWORD*)pNewArray1;
		g_pEXE->m_pDLLFilenameArray = pNewArray2;
		g_pEXE->m_pDLLLoadedAlreadyArray = (DWORD*)pNewArray3;
	}
	else
	{
		// Count New DLLs
		DWORD dwNewDLLs=0;
		CDataTable* pStringEntry = g_pDLLTable->GetNext();
		while(pStringEntry)
		{
			if(pStringEntry->GetAddedToEXEData()==false) dwNewDLLs++;
			pStringEntry=pStringEntry->GetNext();
		}

		// Add To Array(s)
		DWORD dwOldSize = g_pEXE->m_dwNumberOfDLLs;
		LPSTR pOldArray1 = (LPSTR)g_pEXE->m_pDLLIndexArray;
		uintptr_t* pOldArray2 = g_pEXE->m_pDLLFilenameArray;
		LPSTR pOldArray3 = (LPSTR)g_pEXE->m_pDLLLoadedAlreadyArray;
		DWORD dwNewSize = dwOldSize + dwNewDLLs;
		if(dwNewSize>dwOldSize)
		{
			// Only DLLs can wind up with less in the table than before!
			LPSTR pNewArray1 = (LPSTR)g_pEXE->CreateArray(dwNewSize);
			uintptr_t* pNewArray2 = g_pEXE->CreatePtrArray(dwNewSize);
			LPSTR pNewArray3 = (LPSTR)g_pEXE->CreateArray(dwNewSize);

			// Fill New Array with Old+New, then delete Old
			memcpy(pNewArray1, pOldArray1, dwOldSize*sizeof(DWORD));
			memcpy(pNewArray2, pOldArray2, dwOldSize*sizeof(uintptr_t));
			memcpy(pNewArray3, pOldArray3, dwOldSize*sizeof(DWORD));
			std::unique_ptr<DWORD[]> pOldArray1Owner((DWORD*)pOldArray1);
			std::unique_ptr<uintptr_t[]> pOldArray2Owner(pOldArray2);
			std::unique_ptr<DWORD[]> pOldArray3Owner((DWORD*)pOldArray3);

			// Update pointers
			g_pEXE->m_dwNumberOfDLLs = dwNewSize;
			g_pEXE->m_pDLLIndexArray = (DWORD*)pNewArray1;
			g_pEXE->m_pDLLFilenameArray = pNewArray2;
			g_pEXE->m_pDLLLoadedAlreadyArray = (DWORD*)pNewArray3;
		}

		// Start Adding From this index
		dwDLLIndex=dwOldSize;
	}

	// Always make sure dbprocore.dll is first
	for(DWORD pass=0; pass<2; pass++)
	{
		CDataTable* pStringEntry = g_pDLLTable->GetNext();
		while(pStringEntry)
		{
			if(pStringEntry->GetAddedToEXEData()==false)
			{
				// String golding DLL Name
				LPSTR pStringData = pStringEntry->GetString()->GetStr();

				// Ensure it is added in right order
				bool bAddThisString=false;
				if(pass==0 && stricmp("dbprocore.dll", pStringData)==NULL) bAddThisString=true;
				if(pass==1 && stricmp("dbprocore.dll", pStringData)!=NULL) bAddThisString=true;
				if(bAddThisString)
				{
					// Add DLL to DLLEXEData
					pStringEntry->SetAddedToEXEData(true);

					// Ensure it is uniqur in list
					bool bNotGot=true;
					for(DWORD n=0; n<g_pEXE->m_dwNumberOfDLLs; n++)
					{
						LPSTR pCompareWith=(LPSTR)(g_pEXE->m_pDLLFilenameArray[n]);
						if(pCompareWith)
							if(stricmp(pCompareWith, pStringData)==NULL) bNotGot=false;
					}

					// If not got, add to list
					if(bNotGot)
					{
						// Create Dynamic String
						char* pDynamicString = new char[strlen(pStringData)+1];
						strcpy_s(pDynamicString, strlen(pStringData)+1, pStringData);

						// EXEData from Table
						g_pEXE->m_pDLLIndexArray[dwDLLIndex]=pStringEntry->GetIndex();
						g_pEXE->m_pDLLFilenameArray[dwDLLIndex]=(DWORD)pDynamicString;
						dwDLLIndex++;
					}
				}
			}
			pStringEntry=pStringEntry->GetNext();
		}
	}

	// Complete
	return true;
}

bool CASMWriter::UpdateCommandData(void)
{
	db3::CProfile<> prof("CASMWriter::UpdateCommandData");

	if(g_pEXE->m_pCommandDLLIdArray==NULL)
	{
		// Create Array(s)
		DWORD dwNewSize = g_pStatementList->GetCommandIndexCounter();
		LPSTR pNewArray1 = (LPSTR)g_pEXE->CreateArray(dwNewSize);
		uintptr_t* pNewArray2 = g_pEXE->CreatePtrArray(dwNewSize);

		// Update pointers
		g_pEXE->m_dwNumberOfCommands = dwNewSize;
		g_pEXE->m_pCommandDLLIdArray = (DWORD*)pNewArray1;
		g_pEXE->m_pCommandDLLCallArray = pNewArray2;
	}
	else
	{
		// Add To Array(s)
		DWORD dwOldSize = g_pEXE->m_dwNumberOfCommands;
		LPSTR pOldArray1 = (LPSTR)g_pEXE->m_pCommandDLLIdArray;
		uintptr_t* pOldArray2 = g_pEXE->m_pCommandDLLCallArray;
		DWORD dwNewSize = g_pStatementList->GetCommandIndexCounter();
		LPSTR pNewArray1 = (LPSTR)g_pEXE->CreateArray(dwNewSize);
		uintptr_t* pNewArray2 = g_pEXE->CreatePtrArray(dwNewSize);

		// Fill New Array with Old+New, then delete Old
		memcpy(pNewArray1, pOldArray1, dwOldSize*sizeof(DWORD));
		memcpy(pNewArray2, pOldArray2, dwOldSize*sizeof(uintptr_t));
		std::unique_ptr<DWORD[]> pOldArray1Owner((DWORD*)pOldArray1);
		std::unique_ptr<uintptr_t[]> pOldArray2Owner(pOldArray2);

		// Update pointers
		g_pEXE->m_dwNumberOfCommands = dwNewSize;
		g_pEXE->m_pCommandDLLIdArray = (DWORD*)pNewArray1;
		g_pEXE->m_pCommandDLLCallArray = pNewArray2;
	}

	// Assumes commands are added in sequential order
	CDataTable* pStringEntry = g_pCommandTable->GetNext();
	for(DWORD c=0; c<g_pEXE->m_dwNumberOfCommands; c++)
	{
		if(pStringEntry->GetAddedToEXEData()==false)
		{
			// Add to EXEData
			pStringEntry->SetAddedToEXEData(true);

			// Cut String between DLL value and command string
			LPSTR pLeft = "???";
			LPSTR pRight = "???";
			std::unique_ptr<char[]> pLeftOwner, pRightOwner;
			if(pStringEntry)
			{
				CStr* pStr = pStringEntry->GetString();
				DWORD dwPos = pStr->FindFirstChar(',');
				pLeftOwner.reset(pStr->GetLeftOfPosition(dwPos));
				pRightOwner.reset(pStr->GetRightOfPosition(dwPos+1));
				pLeft = pLeftOwner.get();
				pRight = pRightOwner.get();
			}

			// Create Dynamic String
			char* pDynamicString = new char[strlen(pRight)+1];
			strcpy_s(pDynamicString, strlen(pRight)+1, pRight);

			// EXEData from Table
			g_pEXE->m_pCommandDLLIdArray[c]=atoi(pLeft);
			g_pEXE->m_pCommandDLLCallArray[c]=(uintptr_t)pDynamicString;
		}

		// Next String
		if(pStringEntry) pStringEntry = pStringEntry->GetNext();
	}

	// Complete
	return true;
}

bool CASMWriter::UpdateStringData(void)
{
	db3::CProfile<> prof("CASMWriter::UpdateStringData");

	if(g_pEXE->m_pStringsArray==NULL)
	{
		// Create Array(s)
		DWORD dwNewSize = g_pStatementList->GetStringIndexCounter();
		uintptr_t* pNewArray = g_pEXE->CreatePtrArray(dwNewSize);

		// Update pointers
		g_pEXE->m_dwNumberOfStrings = dwNewSize;
		g_pEXE->m_pStringsArray = pNewArray;
	}
	else
	{
		// Add To Array(s)
		DWORD dwOldSize = g_pEXE->m_dwNumberOfStrings;
		std::unique_ptr<uintptr_t[]> pOldArray(g_pEXE->m_pStringsArray);
		DWORD dwNewSize = g_pStatementList->GetStringIndexCounter();
		uintptr_t* pNewArray = g_pEXE->CreatePtrArray(dwNewSize);

		// Fill New Array with Old+New, then delete Old
		memcpy(pNewArray, pOldArray.get(), dwOldSize*sizeof(uintptr_t));

		// Update pointers
		g_pEXE->m_dwNumberOfStrings = dwNewSize;
		g_pEXE->m_pStringsArray = pNewArray;
	}

	// Strings Data (assumed added in sequential order)
	CDataTable* pStringEntry = g_pStringTable->GetNext();
	for(DWORD s=0; s<g_pEXE->m_dwNumberOfStrings; s++)
	{
		if(pStringEntry->GetAddedToEXEData()==false)
		{
			// Add to EXEData
			pStringEntry->SetAddedToEXEData(true);

			// Get Data
			LPSTR pStringData=NULL;
			CStr noSpeechMarks;
			if(pStringEntry)
			{
				// Ensure no speech marks wrap literal string data (stack-owned, RAII)
				noSpeechMarks.SetText(pStringEntry->GetString()->GetStr());
				noSpeechMarks.EatSpeechMarks();
				pStringData = noSpeechMarks.GetStr();
			}
			else
				pStringData = "???";

			// Create Dynamic String
			char* pDynamicString = new char[strlen(pStringData)+1];
			strcpy_s(pDynamicString, strlen(pStringData)+1, pStringData);

			// EXEData from Table
			g_pEXE->m_pStringsArray[s]=(DWORD)pDynamicString;
		}

		// Next String
		if(pStringEntry) pStringEntry = pStringEntry->GetNext();
	}

	// Complete
	return true;
}

bool CASMWriter::UpdateDataData(void)
{
	db3::CProfile<> prof("CASMWriter::UpdateDataData");

	if(g_pEXE->m_pDataArray==NULL)
	{
		// Create Array(s)
		DWORD dwNewSize = g_pStatementList->GetDataIndexCounter();
		LPSTR pNewArray1 = (LPSTR)new char[dwNewSize*10];
		uintptr_t* pNewArray2 = g_pEXE->CreatePtrArray(dwNewSize);

		// Update pointers
		g_pEXE->m_dwNumberOfDataItems = dwNewSize;
		g_pEXE->m_pDataArray = pNewArray1;
		g_pEXE->m_pDataStringsArray = pNewArray2;
	}
	else
	{
		// Add To Array(s)
		DWORD dwOldSize = g_pEXE->m_dwNumberOfDataItems;
		LPSTR pOldArray1 = (LPSTR)g_pEXE->m_pDataArray;
		uintptr_t* pOldArray2 = g_pEXE->m_pDataStringsArray;
		DWORD dwNewSize = g_pStatementList->GetDataIndexCounter();
		LPSTR pNewArray1 = (LPSTR)new char[dwNewSize*10];
		uintptr_t* pNewArray2 = g_pEXE->CreatePtrArray(dwNewSize);

		// Fill New Array with Old+New, then delete Old
		memcpy(pNewArray1, pOldArray1, dwOldSize*10);
		memcpy(pNewArray2, pOldArray2, dwOldSize*sizeof(uintptr_t));
		std::unique_ptr<char[]> pOldArray1Owner(pOldArray1);
		std::unique_ptr<uintptr_t[]> pOldArray2Owner(pOldArray2);

		// Update pointers
		g_pEXE->m_dwNumberOfDataItems = dwNewSize;
		g_pEXE->m_pDataArray = pNewArray1;
		g_pEXE->m_pDataStringsArray = pNewArray2;
	}

	// Data Data (assumed added in sequential order)
	CDataTable* pDataEntry = g_pDataTable->GetNext();
	for(DWORD d=0; d<g_pEXE->m_dwNumberOfDataItems*10; d+=10)
	{
		if(pDataEntry->GetAddedToEXEData()==false)
		{
			// Add to EXEData
			pDataEntry->SetAddedToEXEData(true);

			// Get Type of Data Item
			DWORD dwType = pDataEntry->GetType();
			g_pEXE->m_pDataArray[d+0] = (unsigned char)dwType;

			// Byte 2 is reserved
			g_pEXE->m_pDataArray[d+1] = 0;

			// Set Data Item
			if(dwType==1)
			{
				// EXEData from Table
				*(double*)&g_pEXE->m_pDataArray[d+2] = (double)pDataEntry->GetNumeric();
			}
			if(dwType==2)
			{
				// Data Item as String
				LPSTR pDataItem=NULL;
				if(pDataEntry)
					pDataItem = pDataEntry->GetString()->GetStr();
				else
					pDataItem = "???";

				// Create Dynamic String
				char* pDynamicString = new char[strlen(pDataItem)+1];
				strcpy_s(pDynamicString, strlen(pDataItem)+1, pDataItem);

				// EXEData from Table
				DWORD dwStrIndex=d/10;
				*(DWORD*)&g_pEXE->m_pDataArray[d+2] = (DWORD)dwStrIndex;
				g_pEXE->m_pDataStringsArray[dwStrIndex]=(uintptr_t)pDynamicString;
			}
		}

		// Next String
		if(pDataEntry) pDataEntry = pDataEntry->GetNext();
	}

	// Complete
	return true;
}

bool CASMWriter::UpdateDynamicData(void)
{
	db3::CProfile<> prof("CASMWriter::UpdateDynamicData");

	// Complete Dynamic Array Refresh
	std::unique_ptr<DWORD[]> oldDynamicVars(g_pEXE->m_pDynamicVarsArray);
	std::unique_ptr<DWORD[]> oldDynamicVarsType(g_pEXE->m_pDynamicVarsArrayType);
	g_pEXE->m_pDynamicVarsArray = NULL;
	g_pEXE->m_pDynamicVarsArrayType = NULL;

	// Dynamic Variables (Strings and Arrays, etc)
	DWORD dwDynamicVarsCounter=0;
	for(short pass=0; pass<=1; pass++)
	{
		// Second pass creates array
		if(pass==1)
		{
			g_pEXE->m_pDynamicVarsArray = g_pEXE->CreateArray(g_pEXE->m_dwDynamicVarsQuantity);
			g_pEXE->m_pDynamicVarsArrayType = g_pEXE->CreateArray(g_pEXE->m_dwDynamicVarsQuantity);
		}

		// Scan all variables for dyanmic ones
		dwDynamicVarsCounter=0;
		CVarTable* pVarEntry = g_pVarTable;
		while(pVarEntry)
		{
			// Only main program vars
			if(strcmp(pVarEntry->GetVarScope()->GetStr(),"")==NULL)
			{
				if(pVarEntry->GetArrFlag()==1 || pVarEntry->GetVarTypeValue()==3)
				{
					if(pass==1)
					{
						g_pEXE->m_pDynamicVarsArray[dwDynamicVarsCounter]=pVarEntry->GetOffsetValue();
						g_pEXE->m_pDynamicVarsArrayType[dwDynamicVarsCounter]=pVarEntry->GetArrFlag();
					}
					dwDynamicVarsCounter++;
				}
			}
			pVarEntry=pVarEntry->GetNext();
		}

		// Now I know how many are dynamic..second pass to come
		if(pass==0)
		{
			g_pEXE->m_dwDynamicVarsQuantity = dwDynamicVarsCounter;
		}
	}
	
	// Complete
	return true;
}


void CASMWriter::UpdateStructurePatternDataRec ( LPSTR pPattern, CDeclaration* pDecMain )
{
	while(pDecMain)
	{
		// If find string item, add offset to array
		LPSTR pTypeLetter = "-";
		LPSTR pFullString = pDecMain->GetType()->GetStr();
		if ( stricmp ( "integer", pFullString )==NULL )			pTypeLetter = "L";
		if ( stricmp ( "float", pFullString )==NULL )			pTypeLetter = "F";
		if ( stricmp ( "string", pFullString )==NULL )			pTypeLetter = "S";
		if ( stricmp ( "boolean", pFullString )==NULL )			pTypeLetter = "B";
		if ( stricmp ( "byte", pFullString )==NULL )			pTypeLetter = "Y";
		if ( stricmp ( "word", pFullString )==NULL )			pTypeLetter = "W";
		if ( stricmp ( "dword", pFullString )==NULL )			pTypeLetter = "D";
		if ( stricmp ( "double float", pFullString )==NULL )	pTypeLetter = "O";
		if ( stricmp ( "double integer", pFullString )==NULL )	pTypeLetter = "R";

		// If another type, traverse that too
		CStructTable* pStruct = g_pStructTable->DoesTypeEvenExist(pDecMain->GetType()->GetStr());
		if(pStruct)
		{
			CDeclaration* pDeeperDec = pStruct->GetDecChain();
			if(pDeeperDec)
			{
				UpdateStructurePatternDataRec ( pPattern, pDeeperDec );
				pTypeLetter = "";
			}
		}

		// commit data type to pattern string
		strcat ( pPattern, pTypeLetter );

		// Next dec item
		pDecMain = pDecMain->GetNext();
	}
}

bool CASMWriter::UpdateStructurePatternData(void)
{
	db3::CProfile<> prof("CASMWriter::UpdateStructurePatternData");

	// go through all structures and create a full datatype pattern for each
	g_pEXE->m_dwUsertypeStringPatternQuantity = 0;
	std::unique_ptr<char[]> oldPatternArray(g_pEXE->m_pUsertypeStringPatternArray);
	g_pEXE->m_pUsertypeStringPatternArray = NULL;

	// first pass count structures, second pass create and fill arrays
	for(short pass=0; pass<=1; pass++)
	{
		// Second pass creates array
		if(pass==1)
		{
			if ( g_pEXE->m_dwUsertypeStringPatternQuantity > 0 )
			{
				g_pEXE->m_pUsertypeStringPatternArray = new char[g_pEXE->m_dwUsertypeStringPatternQuantity];
				strcpy ( (LPSTR)g_pEXE->m_pUsertypeStringPatternArray, "" );
			}
		}

		// Scan all variables for dyanmic ones
		DWORD dwCounter=0;
		CStructTable* pEntry = g_pStructTable;
		while(pEntry)
		{
			if(pEntry->GetDecChain())
			{
				char pPattern[512];
				LPSTR pTypeName = pEntry->GetTypeName()->GetStr();
				strcpy ( pPattern, pTypeName );
				strcat ( pPattern, ":" );
				char num[32];
				wsprintf ( num, "%d", g_pStructTable->FindIndex(pTypeName) );
				strcat ( pPattern, num );
				strcat ( pPattern, ":" );
				UpdateStructurePatternDataRec ( pPattern, pEntry->GetDecChain() );
				strcat ( pPattern, ":" );
				if(pass==1) strcat ( (LPSTR)g_pEXE->m_pUsertypeStringPatternArray, pPattern );
				dwCounter+=strlen(pPattern)+1;
			}
			pEntry=pEntry->GetNext();
		}

		// Now I know how many
		if(pass==0) g_pEXE->m_dwUsertypeStringPatternQuantity = dwCounter;
	}
	
	// Complete
	return true;
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
	LPSTR pData = NULL;
	DWORD dwSizeOfData = 0;
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
					LPSTR pStringInMemory=(LPSTR)*(DWORD*)(g_pVarSpaceAddressInUse+dwOffset);
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

		// First DWORD is Size Of VarSpace Block
		*((DWORD*)pPtr) = g_dwVarSpaceSizeInUse; pPtr+=4;

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
					*((LPSTR)pPtr) = 1; pPtr+=1;

					// Store offset to string
					DWORD dwOffset=pCurrent->GetOffsetValue();
					*((DWORD*)pPtr) = dwOffset; pPtr+=4;

					// Locate string if any in memory at offset position
					LPSTR pStringInMemory=(LPSTR)*(DWORD*)(g_pVarSpaceAddressInUse+dwOffset);

					// Store length and contents of string in memory
					DWORD dwLengthOfString=0;
					if(pStringInMemory) dwLengthOfString=strlen(pStringInMemory);
					*((DWORD*)pPtr) = dwLengthOfString; pPtr+=4;
					for(DWORD t=0; t<dwLengthOfString; t++)
						*(pPtr++) = pStringInMemory[t];
				}
			}
			pCurrent=pCurrent->GetNext();
		}
		*((LPSTR)pPtr) = 0;
		pPtr+=1;
	}

	// Return Data Address
	*pdwDataSize=dwSizeOfData;
	return pData;
}

void CASMWriter::TraverseDecForPattern(DWORD dwBaseOffset, short pass, DWORD* dwPatternArrayCounter, DWORD* dwSizeOfUserTypePattern, CDeclaration* pDecMain)
{
	/* old code - not used - pre 2007
	while(pDecMain)
	{
		// If find string item, add offset to array
		DWORD dwOffset=dwBaseOffset + pDecMain->GetOffset();
		if(pDecMain->GetType()->CheckChars(0,5,"STRING"))
		{
			// Store string offset in pattern
			if(pass==1) g_pEXE->m_pUsertypeStringPatternArray[*dwPatternArrayCounter] = dwOffset;
			(*dwPatternArrayCounter)++;

			// Increase size of this pattern
			(*dwSizeOfUserTypePattern)++;
		}

		// If another type, traverse that too
		CStructTable* pStruct = g_pStructTable->DoesTypeEvenExist(pDecMain->GetType()->GetStr());
		if(pStruct)
		{
			CDeclaration* pDeeperDec = pStruct->GetDecChain();
			if(pDeeperDec)
			{
				TraverseDecForPattern(dwOffset, pass, dwPatternArrayCounter, dwSizeOfUserTypePattern, pDeeperDec);
			}
		}

		// Next dec item
		pDecMain = pDecMain->GetNext();
	}
	*/
}

void CASMWriter::FreeMachineBlock(void)
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
	DWORD dwAddressSizeCode = m_taskEmitter.DetermineASMCall(dwASMCodeAsAByte, dwTypeValue);
	return dwASMCodeAsAByte + dwAddressSizeCode;
}

DWORD CASMWriter::DetermineASMCallForREL(DWORD dwASMCodeAsAByte, DWORD dwTypeValue)
{
	DWORD dwAddressSizeCode=0;
	switch(dwTypeValue)
	{
		case 104 :	dwAddressSizeCode=0;	break;	// RELATIVE ADDRESS TO A BYTE
		case 105 :	dwAddressSizeCode=0;	break;	// RELATIVE ADDRESS TO A BYTE
		case 106 :	dwAddressSizeCode=1;	break;	// RELATIVE ADDRESS TO A WORD
		case 101 :	
		case 102 :	
		case 103 :	
		case 107 :	dwAddressSizeCode=2;	break;	// RELATIVE ADDRESS TO A DWORD
		case 108 :	dwAddressSizeCode=3;	break;	// RELATIVE ADDRESS TO A DWORDx2
		case 109 :	dwAddressSizeCode=3;	break;	// RELATIVE ADDRESS TO A DWORDx2
		default:	dwAddressSizeCode=2;	break;	// RELATIVE ADDRESS TO A DWORD
	}
	return dwASMCodeAsAByte+dwAddressSizeCode;
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
	if(pP)
	{
		if(pP->GetChar(0)=='@')
		{
			if(pP->GetChar(1)==':')
			{
				if(dwPType==1001)
					return static_cast<DWORD>(ParamMode::Ebp);
				else
				{
					if((dwPType>100 && dwPType<=199) || dwPType==1101)
						return static_cast<DWORD>(ParamMode::EbpArr);
					else
					{
						if(dwPType>200 && dwPType<=299)
							return static_cast<DWORD>(ParamMode::EbpRel);
						else
						{
							if(dwPOffset>0)
								return static_cast<DWORD>(ParamMode::EbpOff);
							else
								return static_cast<DWORD>(ParamMode::Ebp);
						}
					}
				}
			}
			else
			{
				if(dwPType==1001)
					return static_cast<DWORD>(ParamMode::Mem);
				else
				{
					if((dwPType>100 && dwPType<=199) || dwPType==1101)
						return static_cast<DWORD>(ParamMode::MemArr);
					else
					{
						if(dwPType>200 && dwPType<=299)
							return static_cast<DWORD>(ParamMode::MemRel);
						else
						{
							if(dwPOffset>0)
								return static_cast<DWORD>(ParamMode::MemOff);
							else
								return static_cast<DWORD>(ParamMode::Mem);
						}
					}
				}
			}
		}
		else
			return static_cast<DWORD>(ParamMode::Imm);
	}
	return static_cast<DWORD>(ParamMode::None);
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
	// Temp vars
	DWORD dwCorrectASMCode1=0;
	DWORD dwCorrectASMCode2=0;

	// Create offset strings
	CStr offset1Str("");
	CStr* pOffset1Str = &offset1Str;
	pOffset1Str->SetNumericText( dwPOffset );
	CStr offset2Str("");
	CStr* pOffset2Str = &offset2Str;
	pOffset2Str->SetNumericText( dwPOffset + 4 );

	// If Array Check Active
	if(GetArrayCheckFlag())
	{
		// Make Sure Array Exists
		WriteASMLine(static_cast<DWORD>(ASMOp::CMPEAX4), "0");

		// Leap Marker OpCode
		WriteASMLeapMarkerJump(static_cast<DWORD>(ASMOp::JE), 1);
	}

	// Locate Array Element
	CalculateArrayOffsetInEBX(pOffset);

	WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXSIB4), "");

	// Read From Array
	switch(dwPType-100)
	{
		case 1001:	// UDT ARRAY - Place EAX at Location of UDT Array Element
					// EAX already pointing to array data index, just need to add dataoffset (.field part)
					WriteASMLine(static_cast<DWORD>(ASMOp::ADDEAX4), pOffset1Str->GetStr());
					
// leefix - 170403 - odd code, writes to ECX twice - replaced with above
//					dwCorrectASMCode1=DetermineASMCallForREL(static_cast<DWORD>(ASMOp::MOVECXEAXOFF1),dwPType);
//					WriteASMLine(dwCorrectASMCode1, pOffset1Str->GetStr());
//					WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXEAX4), "");
					break;

		case 8:		// MEMARR to ST08
					WriteASMLine(static_cast<DWORD>(ASMOp::MOVST0EAX8), pOffset1Str->GetStr());
					break;

		case 9:		// MEMARR to EAX/EDX
					WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXEAXOFF4), pOffset2Str->GetStr());
					WriteASMLine(static_cast<DWORD>(ASMOp::MOVEDXECX4), "");
					WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXEAXOFF4), pOffset1Str->GetStr());
					WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXECX4), "");
					break;

		default:	// MEMARR to EAX
					dwCorrectASMCode1=DetermineASMCallForREL(static_cast<DWORD>(ASMOp::MOVECXEAXOFF1),dwPType);
					dwCorrectASMCode2=DetermineASMCallForREL(static_cast<DWORD>(ASMOp::MOVEAXECX1),dwPType);
					WriteASMLine(dwCorrectASMCode1, pOffset1Str->GetStr());
					WriteASMLine(dwCorrectASMCode2, "");
					break;
	}

	// If Array Check Active
	if(GetArrayCheckFlag())
	{
		// leeadd - 300305 - Leap size depends on (runtimeerror hook activity)
		int iLeapSize = 10; //10 is from this code below (exc. rutimeerrorhook)
		if(g_DebugInfo.DebugModeOn())
			iLeapSize+=51;
		else
			iLeapSize+=26;

		// leap over error handling code for arrays (10 bytes)
		char leapstring[32];
		itoa ( iLeapSize, leapstring, 10 );
		WriteASMLine(static_cast<DWORD>(ASMOp::JMP), leapstring);

		// Complete Leap Marker (so we jump here)
		WriteASMLeapMarkerEnd(1);
		WriteASMLeapMarkerEnd(2);
		WriteASMLeapMarkerEnd(3);

		// the array error (1=not there/2+3-out of bounds), else continue after this (+10)
		WriteASMLine2(static_cast<DWORD>(ASMOp::MOVMEMIMM4), "@$_ERR_", "118");

		// leefix - 300305 - ensure array errors reported immediately
		WriteASMTaskCoreP2(m_dwLineNumber, static_cast<DWORD>(ASMTask::RuntimeErrorHook), NULL, 0, NULL, 0);
	}
}

void CASMWriter::WriteASMXtoEAX(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset)
{
	DWORD dwCorrectASMCode = 0;
	int iOffset = 0;
	DWORD dwDWORDRep=0;
	DWORD dwExtraDWORD=0;
	DWORD dwIMMSize=0;

	// Stack-owned formatting temporaries (RAII); non-owning pointers alias them
	CStr doubleStr, dword1Str, dword2Str, offset1Str, offset2Str, temp1Str, temp2Str;
	CStr* pDoubleStr = &doubleStr;
	CStr* pDWORD1Str = &dword1Str;
	CStr* pDWORD2Str = &dword2Str;
	CStr* pOffset1Str = &offset1Str;
	CStr* pOffset2Str = &offset2Str;
	CStr* pTemp1Str = &temp1Str;
	CStr* pTemp2Str = &temp2Str;

	switch(dwMode)
	{
		case static_cast<DWORD>(ParamMode::Imm):	// IMM to EAX

			dwExtraDWORD=0;
			dwDWORDRep = pP->GetDWORDRepresentation(dwPType, &dwExtraDWORD);
			pDWORD1Str->SetDWORDNumericText(dwDWORDRep);
			pDWORD2Str->SetDWORDNumericText(dwExtraDWORD);
			switch(dwPType)
			{
				case 8:		// IMM to ST08
							pTemp1Str->SetText("@$_TEMPA_");
							pTemp2Str->SetText("@$_TEMPB_");
							WriteASMLine2IMM(static_cast<DWORD>(ASMOp::MOVMEMIMM4), pTemp1Str->GetStr(), pDWORD1Str->GetStr(),2);
							WriteASMLine2IMM(static_cast<DWORD>(ASMOp::MOVMEMIMM4), pTemp2Str->GetStr(), pDWORD2Str->GetStr(),2);
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVST0MEM8), pTemp1Str->GetStr());
							break;

				case 9:		// IMM to EAX/EDX
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEDXIMM4), pDWORD2Str->GetStr());
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXIMM4), pDWORD1Str->GetStr());
							break;

				case 3:		// IMMSTR to EAX
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXIMM4), pP->GetStr());
							break;

				case 20:	// DATA LABEL POINTER to EAX
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXIMM4), pP->GetStr());
							break;

				default:	// IMM to EAX
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXIMM1),dwPType);
							dwIMMSize=dwCorrectASMCode-static_cast<DWORD>(ASMOp::MOVEAXIMM1);
							WriteASMLine2IMM(dwCorrectASMCode, NULL, pDWORD1Str->GetStr(), dwIMMSize);
							break;
			}
			break;
			
		case static_cast<DWORD>(ParamMode::Mem):	// MEM to EAX

			pDoubleStr->SetText("+");
			pDoubleStr->AddText(pP->GetStr());
			switch(dwPType)
			{
				case 8:		// MEM to ST08
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVST0MEM8), pP->GetStr());
							break;

				case 9:		// MEM to EAX/EDX
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXMEM4), pDoubleStr->GetStr());
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEDXEAX4), "");
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXMEM4), pP->GetStr());
							break;

				default:	// MEM to EAX
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXMEM1),dwPType);
							WriteASMLine(dwCorrectASMCode, pP->GetStr());
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::MemOff):	// MEMOFF to EAX

			pOffset1Str->SetDWORDNumericText(dwPOffset);
			pOffset2Str->SetDWORDNumericText(dwPOffset+4);
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXIMM4), pP->GetStr());
			switch(dwPType)
			{
				case 8:		// ST08 to MEM
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVST0ECXOFF8), pOffset1Str->GetStr());
							break;

				case 9:		// EAX/EDX to MEM
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXECXOFF4), pOffset2Str->GetStr());
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEDXEAX4), "");
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXECXOFF4), pOffset1Str->GetStr());
							break;
	
				default:	// EAX to MEM
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXECXOFF1),dwPType);
							WriteASMLine(dwCorrectASMCode, pOffset1Str->GetStr());
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::Ebp):	// EBP to EAX

			pDoubleStr->SetText((pP->GetStr()+2));
			iOffset=(int)pDoubleStr->GetValue();
			//if(iOffset<0) iOffset+=4;
			iOffset+=4;//leefix-230603-double must +4!
			pDoubleStr->SetNumericText(iOffset);
			switch(dwPType)
			{
				case 8:		// EBP to ST08
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVST0EBP8), (pP->GetStr()+2));
							break;

				case 9:		// EBP to EAX/EDX
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXEBP4), pDoubleStr->GetStr());
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEDXEAX4), "");
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXEBP4), (pP->GetStr()+2));
							break;

				default:	// EBP to EAX
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXEBP1),dwPType);
							WriteASMLine(dwCorrectASMCode, (pP->GetStr()+2));	
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::EbpOff):	// EBP-OFFSET to EAX

			pOffset2Str->SetText((pP->GetStr()+2));
			iOffset=(int)pOffset2Str->GetValue();
			pOffset1Str->SetNumericText( iOffset + dwPOffset );
			//if(iOffset<0) iOffset+=4;
			iOffset+=4;//leefix-230603-double must +4!
			pOffset2Str->SetNumericText( iOffset + dwPOffset );
			switch(dwPType)
			{
				case 8:		// EBP to ST08
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVST0EBP8), pOffset1Str->GetStr());
							break;

				case 9:		// EBP to EAX/EDX
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXEBP4), pOffset2Str->GetStr());
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEDXEAX4), "");
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXEBP4), pOffset1Str->GetStr());
							break;

				default:	// EBP to EAX
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXEBP1),dwPType);
							WriteASMLine(dwCorrectASMCode, pOffset1Str->GetStr());	
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::MemArr):	// MEMARR to EAX

			// Find Array location
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXMEM4), pP->GetStr());

			// Copy Array Data to EAX
			WriteASMARRtoEAX(dwMode, pP, pPIndex, dwPType, dwPOffset);

			break;

		case static_cast<DWORD>(ParamMode::EbpArr):	// EBPARR to EAX

			// Find Array location
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXEBP4), (pP->GetStr()+2));

			// Copy Array Data to EAX
			WriteASMARRtoEAX(dwMode, pP, pPIndex, dwPType, dwPOffset);

			break;

		case static_cast<DWORD>(ParamMode::MemRel):	// [MEM] to EAX

			pDoubleStr->SetText("+");
			pDoubleStr->AddText(pP->GetStr());
			switch(dwPType)
			{
				case 8:		// MEM to ST08
							break;

				case 9:		// MEM to EAX/EDX
							break;

				default:	// MEM to EAX
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXMEM1),dwPType);
							WriteASMLine(dwCorrectASMCode, pP->GetStr());
							// leefix - 280305 - passing the memaddr as the offset is very odd,
							// changed it to use the actual data ofset passed in, possible typo
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXEAXREL1),dwPType);
							WriteASMLine(dwCorrectASMCode, "");
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::EbpRel):	// [MEM] to EAX (that is, value pointed by EBP+x to EAX)

			switch(dwPType)
			{
				case 8:		// MEM to ST08
							break;

				case 9:		// MEM to EAX/EDX
							break;

				default:	// MEM to EAX
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXEBP1),dwPType);
							WriteASMLine(dwCorrectASMCode, (pP->GetStr()+2));
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXEAXREL1),dwPType);
// 280305 - as above fix	WriteASMLine(dwCorrectASMCode, pP->GetStr());
							WriteASMLine(dwCorrectASMCode, "");
							break;
			}
			break;
	}
}

void CASMWriter::WriteASMEAXtoARR(DWORD dwMode, CStr* pP, CStr* pOffset, DWORD dwPType, DWORD dwPOffset)
{
	// Temp vars
	DWORD dwCorrectASMCode=0;

	// Calculate Offset Strings
	CStr offset1Str("");
	CStr* pOffset1Str = &offset1Str;
	pOffset1Str->SetNumericText( dwPOffset );
	CStr offset2Str("");
	CStr* pOffset2Str = &offset2Str;
	pOffset2Str->SetNumericText( dwPOffset+4 );

	// If Array Check Active
	if(GetArrayCheckFlag())
	{
		// Make Sure Array Exists
		WriteASMLine(static_cast<DWORD>(ASMOp::CMPEAX4), "0");

		// Leap Marker OpCode
		WriteASMLeapMarkerJump(static_cast<DWORD>(ASMOp::JE), 1);
	}

	// Locate Array Element (EBX)
	CalculateArrayOffsetInEBX(pOffset);

	// Perform extraction of data to EAX
	WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXSIB4), "");

	// Write To Element
	switch(dwPType-100)
	{
		case 1001:	// UDT ARRAY
//					dwCorrectASMCode=DetermineASMCallForREL(static_cast<DWORD>(ASMOp::MOVEAXOFFECX1),dwPType);
//					WriteASMLine(dwCorrectASMCode, pOffset1Str->GetStr());
					WriteASMLine(static_cast<DWORD>(ASMOp::ADDEAXECX4), "");
					break;

		case 8:		// ST08 to MEMARR
					WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXST08), pOffset1Str->GetStr());
					break;

		case 9:		// EAX/EDX to MEMARR
					WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXOFFECX4), pOffset1Str->GetStr());
					WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXEDX4), "");
					WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXOFFECX4), pOffset2Str->GetStr());
					break;

		default:	// EAX to MEMARR
					dwCorrectASMCode=DetermineASMCallForREL(static_cast<DWORD>(ASMOp::MOVEAXOFFECX1),dwPType);
					WriteASMLine(dwCorrectASMCode, pOffset1Str->GetStr());
					break;
	}

	// If Array Check Active
	if(GetArrayCheckFlag())
	{
		// leeadd - 300305 - Leap size depends on (runtimeerror hook activity)
		int iLeapSize = 10; //10 is from this code below (exc. rutimeerrorhook)
		if(g_DebugInfo.DebugModeOn())
			iLeapSize+=51;
		else
			iLeapSize+=26;

		// leap over error handling code for arrays (10 bytes)
		char leapstring[32];
		itoa ( iLeapSize, leapstring, 10 );
		WriteASMLine(static_cast<DWORD>(ASMOp::JMP), leapstring);

		// Complete Leap Marker (so we jump here)
		WriteASMLeapMarkerEnd(1);
		WriteASMLeapMarkerEnd(2);
		WriteASMLeapMarkerEnd(3);

		// the array error (1=not there/2+3-out of bounds), else continue after this (+10)
		WriteASMLine2(static_cast<DWORD>(ASMOp::MOVMEMIMM4), "@$_ERR_", "118");

		// leefix - 300305 - ensure array errors reported immediately
		WriteASMTaskCoreP2(m_dwLineNumber, static_cast<DWORD>(ASMTask::RuntimeErrorHook), NULL, 0, NULL, 0);
	}
}

void CASMWriter::WriteASMEAXtoX(DWORD dwMode, CStr* pP, CStr* pPIndex, DWORD dwPType, DWORD dwPOffset)
{
	DWORD dwCorrectASMCode=0;
	int iOffset=0;

	// Stack-owned formatting temporaries (RAII); non-owning pointers alias them
	CStr doubleStr, offset1Str, offset2Str, temp1Str, temp2Str;
	CStr* pDoubleStr = &doubleStr;
	CStr* pOffset1Str = &offset1Str;
	CStr* pOffset2Str = &offset2Str;
	CStr* pTemp1Str = &temp1Str;
	CStr* pTemp2Str = &temp2Str;

	switch(dwMode)
	{
		case static_cast<DWORD>(ParamMode::Mem):		// EAX to MEM
			pDoubleStr->SetText("+");
			pDoubleStr->AddText(pP->GetStr());
			switch(dwPType)
			{
				case 8:		// ST08 to MEM
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVMEMST08), pP->GetStr());
							break;

				case 9:		// EAX/EDX to MEM
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVMEMEAX4), pP->GetStr());
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXEDX4), "");
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVMEMEAX4), pDoubleStr->GetStr());
							break;
	
				default:	// EAX to MEM
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVMEMEAX1),dwPType);
							WriteASMLine(dwCorrectASMCode, pP->GetStr());
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::MemOff):	// EAX to MEM-OFFSET
			pOffset1Str->SetDWORDNumericText(dwPOffset);
			pOffset2Str->SetDWORDNumericText(dwPOffset+4);
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXIMM4), pP->GetStr());
			switch(dwPType)
			{
				case 8:		// ST08 to MEMFF
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXOFFST08), pOffset1Str->GetStr());
							break;

				case 9:		// EAX/EDX to MEMFF
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXOFFEAX4), pOffset1Str->GetStr());
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXEDX4), "");
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXOFFEAX4), pOffset2Str->GetStr());
							break;
	
				default:	// EAX to MEMFF
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVECXOFFEAX1),dwPType);
							WriteASMLine(dwCorrectASMCode, pOffset1Str->GetStr());
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::Ebp):		// EAX to EBP

			pDoubleStr->SetText((pP->GetStr()+2));
			iOffset=(int)pDoubleStr->GetValue();
			//if(iOffset<0) iOffset+=4;
			iOffset+=4;//leefix-230603-double must +4!
			pDoubleStr->SetNumericText(iOffset);
			switch(dwPType)
			{
				case 8:		// ST08 to EBP
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEBPST08), (pP->GetStr()+2));
							break;

				case 9:		// EAX/EDX to EBP
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEBPEAX4), (pP->GetStr()+2));
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXEDX4), "");
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEBPEAX4), pDoubleStr->GetStr());
							break;

				default:	// EAX to EBP
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEBPEAX1),dwPType);
							WriteASMLine(dwCorrectASMCode, (pP->GetStr()+2));
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::EbpOff):	// EAX to EBP-OFFSET

			pOffset2Str->SetText((pP->GetStr()+2));
			iOffset=(int)pOffset2Str->GetValue();
			pOffset1Str->SetNumericText( iOffset + dwPOffset );
			//if(iOffset<0) iOffset+=4;
			iOffset+=4;//leefix-230603-double must +4!
			pOffset2Str->SetNumericText( iOffset + dwPOffset );
			switch(dwPType)
			{
				case 8:		// ST08 to EBP
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEBPST08), pOffset1Str->GetStr());
							break;

				case 9:		// EAX/EDX to EBP
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEBPEAX4), pOffset1Str->GetStr());
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXEDX4), "");
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEBPEAX4), pOffset2Str->GetStr());
							break;

				default:	// EAX to EBP
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEBPEAX1),dwPType);
							WriteASMLine(dwCorrectASMCode, pOffset1Str->GetStr());
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::MemArr):	// EAX to MEMARR

			// Store EAX safely in ECX
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXEAX4), "");

			// Find Location of Array
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXMEM4), pP->GetStr());

			// Store EAX in Array
			WriteASMEAXtoARR(dwMode, pP, pPIndex, dwPType, dwPOffset);

			break;

		case static_cast<DWORD>(ParamMode::EbpArr):	// EAX to EBPARR

			// Store EAX safely in ECX
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXEAX4), "");

			// Find Location of Array
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXEBP4), (pP->GetStr()+2));

			// Store EAX in Array
			WriteASMEAXtoARR(dwMode, pP, pPIndex, dwPType, dwPOffset);

			break;

		case static_cast<DWORD>(ParamMode::Stack):	// EAX to STACK

			switch(dwPType)
			{
				case 8:		
				case 108:	// ST08 to STACK
							pTemp1Str->SetText("@$_TEMPA_");
							pTemp2Str->SetText("@$_TEMPB_");
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVMEMST08), pTemp1Str->GetStr());
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXMEM4), pTemp2Str->GetStr());
							WriteASMLine(static_cast<DWORD>(ASMOp::PUSHEAX), "");
							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXMEM4), pTemp1Str->GetStr());
							WriteASMLine(static_cast<DWORD>(ASMOp::PUSHEAX), "");
							break;

				case 9:		
				case 109:	// EAX/EDX to STACK
							WriteASMLine(static_cast<DWORD>(ASMOp::PUSHEDX), "");
							WriteASMLine(static_cast<DWORD>(ASMOp::PUSHEAX), "");
							break;

				default:	// EAX to STACK
							WriteASMLine(static_cast<DWORD>(ASMOp::PUSHEAX), "");
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::MemRel):	// EAX to [MEM]

			// Store EAX (as using EAX to get seg-memaddress)
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXEAX4), "");

			pDoubleStr->SetText("+");
			pDoubleStr->AddText(pP->GetStr());
			switch(dwPType)
			{
				case 8:		// ST08 to MEM
//							WriteASMLine(static_cast<DWORD>(ASMOp::MOVMEMST08), pP->GetStr());
							break;

				case 9:		// EAX/EDX to MEM
//							WriteASMLine(static_cast<DWORD>(ASMOp::MOVMEMEAX4), pP->GetStr());
//							WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXEDX4), "");
//							WriteASMLine(static_cast<DWORD>(ASMOp::MOVMEMEAX4), pDoubleStr->GetStr());
							break;
	
				default:	// EAX to MEM
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXMEM1),dwPType);
							WriteASMLine(dwCorrectASMCode, pP->GetStr());
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXECXREL1),dwPType);
							WriteASMLine(dwCorrectASMCode, "");
							break;
			}
			break;

		case static_cast<DWORD>(ParamMode::EbpRel): 	// EAX to [MEM] (that is, the memory pointed to by EBP+x)

			// Store EAX (as using EAX to get seg-memaddress)
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXEAX4), "");

			switch(dwPType)
			{
				case 8:		// ST08 to MEM
							break;

				case 9:		// EAX/EDX to MEM
							break;
	
				default:	// EAX to MEM
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXEBP1),dwPType);
							WriteASMLine(dwCorrectASMCode, (pP->GetStr()+2));
							dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::MOVEAXECXREL1),dwPType);
							WriteASMLine(dwCorrectASMCode, "");
							break;
			}
			break;
	}
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
	// Assign Line for DBM Code
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

		// Produce token Command Call token
		CStr tokenCommandStr("[");
		tokenCommandStr.AddNumericText(dwIndex);
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVEBXIMM4), tokenCommandStr.GetStr());
		WriteASMLine(static_cast<DWORD>(ASMOp::CALLEBX), "");

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
		WriteASMLine(static_cast<DWORD>(ASMOp::POPEAX), "");
		WriteASMComment("POP EAX FROM STACK", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::PopEbx))
	{
		WriteASMLine(static_cast<DWORD>(ASMOp::POPEBX), "");
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
				if ( GetCondToggle() ) WriteASMLine ( static_cast<DWORD>(ASMOp::NOTEAX4), 0 ); // lee - 240306 - u6b4 - NOT it back for correct conditional operation
				WriteASMLine(static_cast<DWORD>(ASMOp::CMPEAX4), "0");
			}
			else
			{
				// IMM
				if(pP1->GetValue()==0)
				{
					WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXIMM4), "0");
					if ( GetCondToggle() ) WriteASMLine ( static_cast<DWORD>(ASMOp::NOTEAX4), 0 ); // lee - 240306 - u6b4 - NOT it back for correct conditional operation
					WriteASMLine(static_cast<DWORD>(ASMOp::CMPEAX4), "0");
				}
				else
				{
					WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXIMM4), "1");
					if ( GetCondToggle() ) WriteASMLine ( static_cast<DWORD>(ASMOp::NOTEAX4), 0 ); // lee - 240306 - u6b4 - NOT it back for correct conditional operation
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
		WriteASMLine(static_cast<DWORD>(ASMOp::CALLMEM), pP1->GetStr());
		WriteASMComment("DIRECT SUBCALL", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::Return))
	{
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
		WriteASMLine(static_cast<DWORD>(ASMOp::PUSHAD), "");
		WriteASMComment("PUSH REGISTERS", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::PopRegisters))
	{
		WriteASMLine(static_cast<DWORD>(ASMOp::POPAD), "");
		WriteASMComment("POP REGISTERS", "", "", "");
	}	
	if(dwTask==static_cast<DWORD>(ASMTask::ClearStack))
	{
		// SET EAX base
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXESP), "");

		// BATCHES OF DWORDS
		DWORD dwTotalToClear = pP1->GetDWORDRepresentation(1, NULL);
		DWORD dwDWORDSteps = dwTotalToClear/4;
		DWORD dwDWORDLeft = dwTotalToClear-(dwDWORDSteps*4);
		if(dwDWORDSteps>0)
		{
			CStr iterations;
			iterations.SetNumericText(dwDWORDSteps);

			// SET ECX max
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXIMM4), iterations.GetStr());

			// MOV SIB[EAX:ECX*4], 0
			WriteASMLine2IMM(static_cast<DWORD>(ASMOp::MOVSIB4IMM4), NULL, "0", 2);

			// LOOP BACK
			WriteASMLine2IMM(static_cast<DWORD>(ASMOp::LOOP), NULL, "-9", 0);
		}
		if(dwDWORDLeft>0)
		{
			// Advance EAX base to skip zero'd batch areas
			CStr advance;
			advance.SetNumericText(((dwDWORDSteps+1)*4)-1);
			WriteASMLine(static_cast<DWORD>(ASMOp::ADDEAX4), advance.GetStr());

			CStr iterations;
			iterations.SetNumericText(dwDWORDLeft);

			// SET ECX max
			WriteASMLine(static_cast<DWORD>(ASMOp::MOVECXIMM4), iterations.GetStr());

			// MOV SIB[EAX:ECX*4], 0
			WriteASMLine2IMM(static_cast<DWORD>(ASMOp::MOVSIB4IMM1), NULL, "0", 0);

			// LOOP BACK
			WriteASMLine2IMM(static_cast<DWORD>(ASMOp::LOOP), NULL, "-6", 0);
		}

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
						// LEEFIX - 041002 - NOT is a boolean task, so screen out all but first bit
//						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::NOTEAX1),dwP1Type);
//						WriteASMLine(dwCorrectASMCode, "");
//						dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::ANDEAX1),dwP1Type);
//						DWORD dwIMMSize=dwCorrectASMCode-static_cast<DWORD>(ASMOp::ANDEAX1);
//						WriteASMLine2IMM(dwCorrectASMCode, NULL, "1", dwIMMSize);

						// lee - 010306 - u60 - actually toggle condition which reads EAX to JNE
						SetCondToggle(true);

						// lee - 240306 - u6b4 - also, NOT as well for legacy support (above flag NOTs it back)
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::NOTEAX1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");
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
						// NOT is a boolean task, so screen out all but first bit
//						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::NOTEAX1),dwP1Type);
//						WriteASMLine(dwCorrectASMCode, "");

						// lee - 010306 - u60 - defer to witching JE to JNE below..
						SetCondToggle(true);

						// lee - 240306 - u6b4 - also, NOT as well for legacy support (above flag NOTs it back)
						DWORD dwCorrectASMCode=DetermineASMCall(static_cast<DWORD>(ASMOp::NOTEAX1),dwP1Type);
						WriteASMLine(dwCorrectASMCode, "");
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
		// Find Array location (in EAX)
		if(dwP2Mode==static_cast<DWORD>(ParamMode::MemArr)) WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXMEM4), pP2->GetStr());
		if(dwP2Mode==static_cast<DWORD>(ParamMode::EbpArr)) WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXEBP4), pP2->GetStr()+2);

		// If Array Check Active
		if(GetArrayCheckFlag())
		{
			// Make Sure Array Exists
			WriteASMLine(static_cast<DWORD>(ASMOp::CMPEAX4), "0");

			// Leap Marker OpCode
			WriteASMLeapMarkerJump(static_cast<DWORD>(ASMOp::JE), 4);
		}

		// Always have first dimension D1
		WriteASMLine(static_cast<DWORD>(ASMOp::POPEBX), "");

		// Loop through subsequent dimensions D2-D9 (dwP1Offset used as carrier)
		int iCount=0;
		int iCountMax = (DWORD)dwP1Offset-1;
		while(iCount<iCountMax)
		{
			CStr value;
			int iHeaderOffset = (-56)+(iCount*4);
			value.SetNumericText(iHeaderOffset);
			WriteASMLine(static_cast<DWORD>(ASMOp::POPEDX), "");
			WriteASMLine(static_cast<DWORD>(ASMOp::MULEDXEAXOFF4), value.GetStr());
			WriteASMLine(static_cast<DWORD>(ASMOp::ADDEBXEDX4), "");
			iCount++;
		}

		// Store EBX in Offset variable
		WriteASMLine(static_cast<DWORD>(ASMOp::MOVEAXEBX4), "");
		WriteASMEAXtoX(dwP1Mode, pP1, NULL, 7, 0);

		// If Array Check Active
		if(GetArrayCheckFlag())
		{
			// Complete Leap Marker (so we jump here)
			WriteASMLeapMarkerEnd(4);
		}

		// Comment on this task
		WriteASMComment("CALCULATE ARRAY OFFSET", "", "", "");
	}
	if(dwTask==static_cast<DWORD>(ASMTask::PushUdt))
	{
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
	strDBMLine.AddText(const_cast<LPSTR>(m_ASMDebugStrings[dwOp].c_str()));
	strDBMLine.AddText(" ");
	strDBMLine.AddText(pOpData);
	if(g_pDBMWriter->OutputDBM(&strDBMLine)==false) return false;

	// ASM Code
	if(m_bASMOpData[dwOp]==true)
		CreateASMMiddle(m_iASMPreOp[dwOp], m_iASMOp1[dwOp], m_iASMOp2[dwOp], pOpData);
	else
		CreateASMMiddle(m_iASMPreOp[dwOp], m_iASMOp1[dwOp], m_iASMOp2[dwOp], "");

	// Complete
	return true;
}

bool CASMWriter::WriteASMLine2(DWORD dwOp, LPSTR pOpData, LPSTR pOpData2)
{
	// DBM Code
	CStr strDBMLine(256);
	strDBMLine.SetNumericText(m_dwLineNumber);
	strDBMLine.AddText(" ");
	strDBMLine.AddText(const_cast<LPSTR>(m_ASMDebugStrings[dwOp].c_str()));
	strDBMLine.AddText(" ");
	strDBMLine.AddText(pOpData);
	strDBMLine.AddText(", ");
	strDBMLine.AddText(pOpData2);
	if(g_pDBMWriter->OutputDBM(&strDBMLine)==false) return false;

	// ASM Code
	CreateASMMiddleCore(m_iASMPreOp[dwOp], m_iASMOp1[dwOp], m_iASMOp2[dwOp], pOpData, pOpData2, false, 0);

	// Complete
	return true;
}

bool CASMWriter::WriteASMLine1IMM(DWORD dwOp, LPSTR pOpData, DWORD dwSizeIMM)
{
	// DBM Code
	CStr strDBMLine(256);
	strDBMLine.SetNumericText(m_dwLineNumber);
	strDBMLine.AddText(" ");
	strDBMLine.AddText(const_cast<LPSTR>(m_ASMDebugStrings[dwOp].c_str()));
	strDBMLine.AddText(" ");
	strDBMLine.AddText(pOpData);
	if(g_pDBMWriter->OutputDBM(&strDBMLine)==false) return false;

	// ASM Code
	CreateASMMiddleCore(m_iASMPreOp[dwOp], m_iASMOp1[dwOp], m_iASMOp2[dwOp], pOpData, NULL, true, dwSizeIMM);

	// Complete
	return true;
}

bool CASMWriter::WriteASMLine2IMM(DWORD dwOp, LPSTR pOpData, LPSTR pOpData2, DWORD dwSizeIMM)
{
	// DBM Code
	CStr strDBMLine(256);
	strDBMLine.SetNumericText(m_dwLineNumber);
	strDBMLine.AddText(" ");
	strDBMLine.AddText(const_cast<LPSTR>(m_ASMDebugStrings[dwOp].c_str()));
	strDBMLine.AddText(" ");
	strDBMLine.AddText(pOpData);
	strDBMLine.AddText(", ");
	strDBMLine.AddText(pOpData2);
	if(g_pDBMWriter->OutputDBM(&strDBMLine)==false) return false;

	// ASM Code
	CreateASMMiddleCore(m_iASMPreOp[dwOp], m_iASMOp1[dwOp], m_iASMOp2[dwOp], pOpData, pOpData2, true, dwSizeIMM);

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
