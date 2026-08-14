// InstructionTable.cpp: implementation of the CInstructionTable class.
//
//////////////////////////////////////////////////////////////////////


// Includes
#include "macros.h"
#include "InstructionTable.h"
#include "VarTable.h"
#include "direct.h"
#include "DBPCompiler.h"
#include "io.h"
#include "TextConvert.h"
#include <string>

// External Class Pointers
extern CVarTable* g_pVarTable;
extern CDBPCompiler* g_pDBPCompiler;

//
// Internal Support Functions
//

bool					m_bScanActive=false;
int						m_findex;
struct _finddata_t		m_filedata[200];
long					m_hInternalFile[200];
int						m_FileReturnValue[200];

void Init(void)
{
	m_findex=0;
	for(int n=0; n<199; n++)
	{
		m_hInternalFile[n]=NULL;
		m_FileReturnValue[n]=-1;
	}
}

void Free(void)
{
	for(int n=0; n<199; n++)
	{
		if(m_hInternalFile[n]) _findclose(m_hInternalFile[n]);
		m_FileReturnValue[n]=-1;
	}
}

void FFindCloseFile(void)
{
	_findclose(m_hInternalFile[m_findex]);
	m_hInternalFile[m_findex]=NULL;
}

void FFindFirstFile(LPSTR pExt)
{
	if(m_hInternalFile[m_findex]) FFindCloseFile();
	m_hInternalFile[m_findex] = _findfirst(pExt, &m_filedata[m_findex]);
	if(m_hInternalFile[m_findex]!=-1L)
	{
		// Success!
		m_FileReturnValue[m_findex]=0;
	}
}

int FGetFileReturnValue(void)
{
	return m_FileReturnValue[m_findex];
}

void FFindNextFile(void)
{
	m_FileReturnValue[m_findex] = _findnext(m_hInternalFile[m_findex], &m_filedata[m_findex]);
}

int FGetActualTypeValue(int flagvalue)
{
	if(flagvalue & _A_SUBDIR)
		return 1;
	else
		return 0;
}

bool FileExist(LPSTR pFilename)
{
	HANDLE hReadFile = CreateFileW(TextConvert::UTF8ToUTF16(pFilename).c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hReadFile!=INVALID_HANDLE_VALUE)
	{
		CloseHandle(hReadFile);
		return true;
	}
	else
		return false;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

#define ALLOWED_LOWER "abcdefghijklmnopqrstuvwxyz"
#define ALLOWED_UPPER "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define ALLOWED_ALPHA ALLOWED_LOWER ALLOWED_UPPER
#define ALLOWED_DIGIT "0123456789"
#define ALLOWED_ALNUM ALLOWED_ALPHA ALLOWED_DIGIT
#define ALLOWED_IDENT ALLOWED_ALNUM "_"

#define ALLOWED_DBINT ALLOWED_IDENT "+#$% "
#define ALLOWED_DBUSR ALLOWED_IDENT "#$%"

CInstructionTable::CInstructionTable()
#ifdef __AARON_INSTRPERF__
: m_InstructionMap (ALLOWED_DBINT, map_type::ECase::Insensitive, map_type::EOnDestruct::DeleteEntriesAndData),
  m_UserFunctionMap(ALLOWED_DBUSR, map_type::ECase::Insensitive, map_type::EOnDestruct::DeleteEntriesAndData)
#endif
{
	m_dwCurrentInternalID=1000;

#ifndef __AARON_INSTRPERF__
	m_pFirstInstructionEntry=NULL;
	m_pFirstUserFunctionEntry=NULL;
#endif

	// Clear instruction table array
	ZeroMemory(m_InternalInstructions, sizeof(m_InternalInstructions));
	ZeroMemory(m_InternalInstructRef, sizeof(m_InternalInstructRef));
}

CInstructionTable::~CInstructionTable()
{
#ifdef __AARON_INSTRPERF__
#else
	if(m_pFirstInstructionEntry) m_pFirstInstructionEntry->Free();
	if(m_pFirstUserFunctionEntry) m_pFirstUserFunctionEntry->Free();
#endif
}

bool CInstructionTable::DefineHardCodedCommand(void)
{
	// Bitwise
	AddBuildCommand("+mathint", "SHLLLL", "L", 1, 1, static_cast<DWORD>(InternalInstruction::ShiftLLLL), static_cast<DWORD>(BuildTask::Shl));
	AddBuildCommand("+mathint", "SHRLLL", "L", 1, 1, static_cast<DWORD>(InternalInstruction::ShiftRLLL), static_cast<DWORD>(BuildTask::Shr));
	AddBuildCommand("+mathint", "BITORLLL", "L", 1, 1, static_cast<DWORD>(InternalInstruction::BitOrLLL), static_cast<DWORD>(BuildTask::BitOr));
	AddBuildCommand("+mathint", "BITANDLLL", "L", 1, 1, static_cast<DWORD>(InternalInstruction::BitAndLLL), static_cast<DWORD>(BuildTask::BitAnd));
	AddBuildCommand("+mathint", "BITXORLLL", "L", 1, 1, static_cast<DWORD>(InternalInstruction::BitXorLLL), static_cast<DWORD>(BuildTask::BitXor));
	AddBuildCommand("+mathint", "BITNOTLLL", "L", 1, 1, static_cast<DWORD>(InternalInstruction::BitNotLLL), static_cast<DWORD>(BuildTask::BitNot));

	// Logic
	AddBuildCommand("+mathint", "ORLLL", "L", 1, 1, static_cast<DWORD>(InternalInstruction::OrLLL), static_cast<DWORD>(BuildTask::Or));
	AddBuildCommand("+mathint", "ANDLLL", "L", 1, 1, static_cast<DWORD>(InternalInstruction::AndLLL), static_cast<DWORD>(BuildTask::And));
	AddBuildCommand("+mathint", "NOTLLL", "L", 1, 1, static_cast<DWORD>(InternalInstruction::NotLLL), static_cast<DWORD>(BuildTask::Not));
	AddBuildCommand("+mathint", "CONDLLL", "LL", 1, 2, static_cast<DWORD>(InternalInstruction::EqualLLL), static_cast<DWORD>(BuildTask::Equal));
	AddBuildCommand("+mathint", "CONDLLL", "LL", 1, 2, static_cast<DWORD>(InternalInstruction::NotEqualLLL), static_cast<DWORD>(BuildTask::NotEqual));
	AddBuildCommand("+mathint", "CONDLLL", "LL", 1, 2, static_cast<DWORD>(InternalInstruction::GreaterLLL), static_cast<DWORD>(BuildTask::Greater));
	AddBuildCommand("+mathint", "CONDLLL", "LL", 1, 2, static_cast<DWORD>(InternalInstruction::GreaterEqualLLL), static_cast<DWORD>(BuildTask::GreaterEqual));
	AddBuildCommand("+mathint", "CONDLLL", "LL", 1, 2, static_cast<DWORD>(InternalInstruction::LessLLL), static_cast<DWORD>(BuildTask::Less));
	AddBuildCommand("+mathint", "CONDLLL", "LL", 1, 2, static_cast<DWORD>(InternalInstruction::LessEqualLLL), static_cast<DWORD>(BuildTask::LessEqual));

	// External Maths
	// Wave 17/20: every Power row is the emitter-built exp/log sequence
	// (ASMTask::Power); byte/boolean share the unsigned-char DLL semantics.
	AddBuildCommand("+math", "POWERLLL", "LL", 1, 2, static_cast<DWORD>(InternalInstruction::PowerLLL), static_cast<DWORD>(BuildTask::Power));
	AddBuildCommand("+math", "POWERBBB", "BB", 4, 2, static_cast<DWORD>(InternalInstruction::PowerBBB), static_cast<DWORD>(BuildTask::Power));
	AddBuildCommand("+math", "POWERYYY", "YY", 5, 2, static_cast<DWORD>(InternalInstruction::PowerYYY), static_cast<DWORD>(BuildTask::Power));
	AddBuildCommand("+math", "POWERWWW", "WW", 6, 2, static_cast<DWORD>(InternalInstruction::PowerWWW), static_cast<DWORD>(BuildTask::Power));
	AddBuildCommand("+math", "POWERDDD", "DD", 7, 2, static_cast<DWORD>(InternalInstruction::PowerDDD), static_cast<DWORD>(BuildTask::Power));

	// INT Math
	AddBuildCommand("+math", "MULLLL", "LL", 1, 2, static_cast<DWORD>(InternalInstruction::MulLLL), static_cast<DWORD>(BuildTask::Mul));
	AddBuildCommand("+math", "DIVLLL", "LL", 1, 2, static_cast<DWORD>(InternalInstruction::DivLLL), static_cast<DWORD>(BuildTask::Div));
	AddBuildCommand("+math", "ADDLLL", "LL", 1, 2, static_cast<DWORD>(InternalInstruction::AddLLL), static_cast<DWORD>(BuildTask::Add));
	AddBuildCommand("+math", "SUBLLL", "LL", 1, 2, static_cast<DWORD>(InternalInstruction::SubLLL), static_cast<DWORD>(BuildTask::Sub));
	AddBuildCommand("+math", "MODLLL", "LL", 1, 2, static_cast<DWORD>(InternalInstruction::ModLLL), static_cast<DWORD>(BuildTask::Mod));

	// BOOLEAN Math
	AddBuildCommand("+math", "MULBBB", "BB", 1, 2, static_cast<DWORD>(InternalInstruction::MulBBB), static_cast<DWORD>(BuildTask::Mul));
	AddBuildCommand("+math", "DIVBBB", "BB", 1, 2, static_cast<DWORD>(InternalInstruction::DivBBB), static_cast<DWORD>(BuildTask::Div));
	AddBuildCommand("+math", "ADDBBB", "BB", 1, 2, static_cast<DWORD>(InternalInstruction::AddBBB), static_cast<DWORD>(BuildTask::Add));
	AddBuildCommand("+math", "SUBBBB", "BB", 1, 2, static_cast<DWORD>(InternalInstruction::SubBBB), static_cast<DWORD>(BuildTask::Sub));
	AddBuildCommand("+math", "MODBBB", "BB", 1, 2, static_cast<DWORD>(InternalInstruction::ModBBB), static_cast<DWORD>(BuildTask::Mod));

	// BYTE Math
	AddBuildCommand("+math", "MULYYY", "YY", 1, 2, static_cast<DWORD>(InternalInstruction::MulYYY), static_cast<DWORD>(BuildTask::Mul));
	AddBuildCommand("+math", "DIVYYY", "YY", 1, 2, static_cast<DWORD>(InternalInstruction::DivYYY), static_cast<DWORD>(BuildTask::Div));
	AddBuildCommand("+math", "ADDYYY", "YY", 1, 2, static_cast<DWORD>(InternalInstruction::AddYYY), static_cast<DWORD>(BuildTask::Add));
	AddBuildCommand("+math", "SUBYYY", "YY", 1, 2, static_cast<DWORD>(InternalInstruction::SubYYY), static_cast<DWORD>(BuildTask::Sub));
	AddBuildCommand("+math", "MODYYY", "YY", 1, 2, static_cast<DWORD>(InternalInstruction::ModYYY), static_cast<DWORD>(BuildTask::Mod));

	// WORD Math
	AddBuildCommand("+math", "MULWWW", "WW", 1, 2, static_cast<DWORD>(InternalInstruction::MulWWW), static_cast<DWORD>(BuildTask::Mul));
	AddBuildCommand("+math", "DIVWWW", "WW", 1, 2, static_cast<DWORD>(InternalInstruction::DivWWW), static_cast<DWORD>(BuildTask::Div));
	AddBuildCommand("+math", "ADDWWW", "WW", 1, 2, static_cast<DWORD>(InternalInstruction::AddWWW), static_cast<DWORD>(BuildTask::Add));
	AddBuildCommand("+math", "SUBWWW", "WW", 1, 2, static_cast<DWORD>(InternalInstruction::SubWWW), static_cast<DWORD>(BuildTask::Sub));
	AddBuildCommand("+math", "MODWWW", "WW", 1, 2, static_cast<DWORD>(InternalInstruction::ModWWW), static_cast<DWORD>(BuildTask::Mod));

	// DWORD Math
	AddBuildCommand("+math", "MULDDD", "DD", 1, 2, static_cast<DWORD>(InternalInstruction::MulDDD), static_cast<DWORD>(BuildTask::Mul));
	AddBuildCommand("+math", "DIVDDD", "DD", 1, 2, static_cast<DWORD>(InternalInstruction::DivDDD), static_cast<DWORD>(BuildTask::Div));
	AddBuildCommand("+math", "ADDDDD", "DD", 1, 2, static_cast<DWORD>(InternalInstruction::AddDDD), static_cast<DWORD>(BuildTask::Add));
	AddBuildCommand("+math", "SUBDDD", "DD", 1, 2, static_cast<DWORD>(InternalInstruction::SubDDD), static_cast<DWORD>(BuildTask::Sub));
	AddBuildCommand("+math", "MODDDD", "DD", 1, 2, static_cast<DWORD>(InternalInstruction::ModDDD), static_cast<DWORD>(BuildTask::Mod));

	// Quantity INC and DEC
	AddBuildCommand("inc", "INCLL", "LL", 0, 2, static_cast<DWORD>(InternalInstruction::IncVar), static_cast<DWORD>(BuildTask::Inc));
	AddBuildCommand("inc", "INCFF", "FF", 0, 2, static_cast<DWORD>(InternalInstruction::IncVar), static_cast<DWORD>(BuildTask::IncAdd));
	AddBuildCommand("inc", "INCBB", "BB", 0, 2, static_cast<DWORD>(InternalInstruction::IncVar), static_cast<DWORD>(BuildTask::Inc));
	AddBuildCommand("inc", "INCYY", "YY", 0, 2, static_cast<DWORD>(InternalInstruction::IncVar), static_cast<DWORD>(BuildTask::Inc));
	AddBuildCommand("inc", "INCWW", "WW", 0, 2, static_cast<DWORD>(InternalInstruction::IncVar), static_cast<DWORD>(BuildTask::Inc));
	AddBuildCommand("inc", "INCDD", "DD", 0, 2, static_cast<DWORD>(InternalInstruction::IncVar), static_cast<DWORD>(BuildTask::Inc));
	AddBuildCommand("inc", "INCRR", "RR", 0, 2, static_cast<DWORD>(InternalInstruction::IncVar), static_cast<DWORD>(BuildTask::IncAdd));
	AddBuildCommand("inc", "INCOO", "OO", 0, 2, static_cast<DWORD>(InternalInstruction::IncVar), static_cast<DWORD>(BuildTask::IncAdd));
	AddBuildCommand("dec", "DECLL", "LL", 0, 2, static_cast<DWORD>(InternalInstruction::DecVar), static_cast<DWORD>(BuildTask::Dec));
	AddBuildCommand("dec", "DECFF", "FF", 0, 2, static_cast<DWORD>(InternalInstruction::DecVar), static_cast<DWORD>(BuildTask::DecAdd));
	AddBuildCommand("dec", "DECBB", "BB", 0, 2, static_cast<DWORD>(InternalInstruction::DecVar), static_cast<DWORD>(BuildTask::Dec));
	AddBuildCommand("dec", "DECYY", "YY", 0, 2, static_cast<DWORD>(InternalInstruction::DecVar), static_cast<DWORD>(BuildTask::Dec));
	AddBuildCommand("dec", "DECWW", "WW", 0, 2, static_cast<DWORD>(InternalInstruction::DecVar), static_cast<DWORD>(BuildTask::Dec));
	AddBuildCommand("dec", "DECDD", "DD", 0, 2, static_cast<DWORD>(InternalInstruction::DecVar), static_cast<DWORD>(BuildTask::Dec));
	AddBuildCommand("dec", "DECRR", "RR", 0, 2, static_cast<DWORD>(InternalInstruction::DecVar), static_cast<DWORD>(BuildTask::DecAdd));
	AddBuildCommand("dec", "DECOO", "OO", 0, 2, static_cast<DWORD>(InternalInstruction::DecVar), static_cast<DWORD>(BuildTask::DecAdd));

	// Singular INC and DEC
	AddBuildCommand("inc", "INCL", "L", 0, 1, static_cast<DWORD>(InternalInstruction::IncVar), static_cast<DWORD>(BuildTask::Inc));
	AddBuildCommand("inc", "INCF", "F", 0, 1, static_cast<DWORD>(InternalInstruction::IncVar), static_cast<DWORD>(BuildTask::IncAdd));
	AddBuildCommand("inc", "INCB", "B", 0, 1, static_cast<DWORD>(InternalInstruction::IncVar), static_cast<DWORD>(BuildTask::Inc));
	AddBuildCommand("inc", "INCY", "Y", 0, 1, static_cast<DWORD>(InternalInstruction::IncVar), static_cast<DWORD>(BuildTask::Inc));
	AddBuildCommand("inc", "INCW", "W", 0, 1, static_cast<DWORD>(InternalInstruction::IncVar), static_cast<DWORD>(BuildTask::Inc));
	AddBuildCommand("inc", "INCD", "D", 0, 1, static_cast<DWORD>(InternalInstruction::IncVar), static_cast<DWORD>(BuildTask::Inc));
	AddBuildCommand("inc", "INCR", "R", 0, 1, static_cast<DWORD>(InternalInstruction::IncVar), static_cast<DWORD>(BuildTask::IncAdd));
	AddBuildCommand("inc", "INCO", "O", 0, 1, static_cast<DWORD>(InternalInstruction::IncVar), static_cast<DWORD>(BuildTask::IncAdd));
	AddBuildCommand("dec", "DECL", "L", 0, 1, static_cast<DWORD>(InternalInstruction::DecVar), static_cast<DWORD>(BuildTask::Dec));
	AddBuildCommand("dec", "DECF", "F", 0, 1, static_cast<DWORD>(InternalInstruction::DecVar), static_cast<DWORD>(BuildTask::DecAdd));
	AddBuildCommand("dec", "DECB", "B", 0, 1, static_cast<DWORD>(InternalInstruction::DecVar), static_cast<DWORD>(BuildTask::Dec));
	AddBuildCommand("dec", "DECY", "Y", 0, 1, static_cast<DWORD>(InternalInstruction::DecVar), static_cast<DWORD>(BuildTask::Dec));
	AddBuildCommand("dec", "DECW", "W", 0, 1, static_cast<DWORD>(InternalInstruction::DecVar), static_cast<DWORD>(BuildTask::Dec));
	AddBuildCommand("dec", "DECD", "D", 0, 1, static_cast<DWORD>(InternalInstruction::DecVar), static_cast<DWORD>(BuildTask::Dec));
	AddBuildCommand("dec", "DECR", "R", 0, 1, static_cast<DWORD>(InternalInstruction::DecVar), static_cast<DWORD>(BuildTask::DecAdd));
	AddBuildCommand("dec", "DECO", "O", 0, 1, static_cast<DWORD>(InternalInstruction::DecVar), static_cast<DWORD>(BuildTask::DecAdd));

	// Complete
	return true;
}

bool CInstructionTable::SetInternalInstructionDatabase(void)
{
#ifdef __AARON_INSTRPERF__
# define BLANKCMD " "
#else
# define BLANKCMD ""
#endif
	// Internal DBM that uses Hard Coded Machine Code
	AddBuildCommand("return", "RET", "", 0, 0, static_cast<DWORD>(InternalInstruction::Return), static_cast<DWORD>(BuildTask::Ret));
	AddBuildCommand(BLANKCMD, "PURERET", "", 0, 0, static_cast<DWORD>(InternalInstruction::PureReturn), static_cast<DWORD>(BuildTask::PureRet));
	AddBuildCommand("sync", "", "", 0, 0, static_cast<DWORD>(InternalInstruction::Sync), static_cast<DWORD>(BuildTask::Sync));
	AddBuildCommand("end", "", "", 0, 0, static_cast<DWORD>(InternalInstruction::End), static_cast<DWORD>(BuildTask::End));
	AddBuildCommand("enderror", "", "", 0, 0, static_cast<DWORD>(InternalInstruction::EndError), static_cast<DWORD>(BuildTask::EndError));
	AddBuildCommand(BLANKCMD, "STARTPROG", "", 0, 0, static_cast<DWORD>(InternalInstruction::StartProgram), static_cast<DWORD>(BuildTask::StartProgram));
	AddBuildCommand(BLANKCMD, "ENDPROG", "", 0, 0, static_cast<DWORD>(InternalInstruction::EndProgram), static_cast<DWORD>(BuildTask::EndProgramAndQuit));

	// Optimisations from Hard Coded ASM
	DefineHardCodedCommand();

	// Internal Commands (generated internally)
	AddCommandCore("+exitfunction", "", "", "", 0, 0, static_cast<DWORD>(InternalInstruction::UserFunctionExit), static_cast<DWORD>(BuildTask::UserFunctionExit));
	// Wave 7: the runtime array-pointer API widened to uintptr_t — the
	// decorated names now carry the _K (unsigned __int64) type.
	AddCommandCore("+allocate", "dbprocore.dll", "?DimDDD@@YA_K_KKKKKKKKKK@Z", "DDDDDDDDDD", 7, 11, static_cast<DWORD>(InternalInstruction::Alloc), 0);
	AddCommandCore("+deallocate", "dbprocore.dll", "?UnDimDD@@YA_K_K@Z", "D", 7, 1, static_cast<DWORD>(InternalInstruction::Free), 0);

	// Wave 11: the runtime list API (ArrayInsert/ArrayDelete/Queue/Stack)
	// widened to uintptr_t. The DBDLLCore.rc resource strings still carry the
	// 32-bit decorated names, so register the x64 names internally — the
	// internal DB loads before the DLL surface, so these win the friend chain.
	// The H param passes the array pointer as input (bPassArrayAsInput); the
	// star-commands write the returned (possibly reallocated) pointer back
	// through the first param (dwPlace=1), exactly like the .rc %H*% forms.
	AddCommandCore2("ARRAY INSERT AT TOP", "dbprocore.dll", "?ArrayInsertAtTop@@YA_K_K@Z", "H", 7, 1, static_cast<DWORD>(InternalInstruction::ArrayInsertTop), 0, 1, true, NULL, NULL);
	AddCommandCore2("ARRAY INSERT AT TOP", "dbprocore.dll", "?ArrayInsertAtTop@@YA_K_KH@Z", "HL", 7, 2, static_cast<DWORD>(InternalInstruction::ArrayInsertTop), 0, 1, true, NULL, NULL);
	AddCommandCore2("ARRAY INSERT AT BOTTOM", "dbprocore.dll", "?ArrayInsertAtBottom@@YA_K_K@Z", "H", 7, 1, static_cast<DWORD>(InternalInstruction::ArrayInsertBottom), 0, 1, true, NULL, NULL);
	AddCommandCore2("ARRAY INSERT AT BOTTOM", "dbprocore.dll", "?ArrayInsertAtBottom@@YA_K_KH@Z", "HL", 7, 2, static_cast<DWORD>(InternalInstruction::ArrayInsertBottom), 0, 1, true, NULL, NULL);
	AddCommandCore2("ARRAY INSERT AT ELEMENT", "dbprocore.dll", "?ArrayInsertAtElement@@YA_K_KH@Z", "HL", 7, 2, static_cast<DWORD>(InternalInstruction::ArrayInsertElement), 0, 1, true, NULL, NULL);
	AddCommandCore2("ARRAY DELETE ELEMENT", "dbprocore.dll", "?ArrayDeleteElement@@YAX_K@Z", "H", 0, 1, static_cast<DWORD>(InternalInstruction::ArrayDeleteElement), 0, 0, false, NULL, NULL);
	AddCommandCore2("ARRAY DELETE ELEMENT", "dbprocore.dll", "?ArrayDeleteElement@@YAX_KH@Z", "HL", 0, 2, static_cast<DWORD>(InternalInstruction::ArrayDeleteElement), 0, 0, false, NULL, NULL);
	AddCommandCore2("EMPTY ARRAY", "dbprocore.dll", "?EmptyArray@@YAX_K@Z", "H", 0, 1, static_cast<DWORD>(InternalInstruction::EmptyArray), 0, 0, false, NULL, NULL);
	AddCommandCore2("ADD TO QUEUE", "dbprocore.dll", "?AddToQueue@@YA_K_K@Z", "H", 7, 1, static_cast<DWORD>(InternalInstruction::AddToQueue), 0, 1, true, NULL, NULL);
	AddCommandCore2("REMOVE FROM QUEUE", "dbprocore.dll", "?RemoveFromQueue@@YAX_K@Z", "H", 0, 1, static_cast<DWORD>(InternalInstruction::RemoveFromQueue), 0, 0, false, NULL, NULL);
	AddCommandCore2("ADD TO STACK", "dbprocore.dll", "?PushToStack@@YA_K_K@Z", "H", 7, 1, static_cast<DWORD>(InternalInstruction::PushStack), 0, 1, true, NULL, NULL);
	AddCommandCore2("REMOVE FROM STACK", "dbprocore.dll", "?PopFromStack@@YAX_K@Z", "H", 0, 1, static_cast<DWORD>(InternalInstruction::PopStack), 0, 0, false, NULL, NULL);
	AddCommandCore("+assign", "", "MOVLL", "LL", 0, 0, static_cast<DWORD>(InternalInstruction::AssignLL), 0);
	AddCommandCore("+assign", "", "MOVFF", "FF", 0, 0, static_cast<DWORD>(InternalInstruction::AssignFF), 0);
	// x64: string addresses are uintptr_t (_K in MSVC x64 mangling), so the
	// runtime core must export the widened signatures.
	AddCommandCore("+assign", "dbprocore.dll", "?EquateSS@@YA_K_K_K@Z", "S", 3, 2, static_cast<DWORD>(InternalInstruction::AssignSS), 0);
	AddCommandCore("+free", "dbprocore.dll", "?FreeSS@@YA_K_K@Z", "S", 3, 1, static_cast<DWORD>(InternalInstruction::StrFree), 0);
	
	// Internal Assignments Commands
	AddCommandCore("+assign", "", "MOVBB", "BB", 0, 0, static_cast<DWORD>(InternalInstruction::AssignBB), 0);
	AddCommandCore("+assign", "", "MOVYY", "YY", 0, 0, static_cast<DWORD>(InternalInstruction::AssignYY), 0);
	AddCommandCore("+assign", "", "MOVWW", "WW", 0, 0, static_cast<DWORD>(InternalInstruction::AssignWW), 0);
	AddCommandCore("+assign", "", "MOVDD", "DD", 0, 0, static_cast<DWORD>(InternalInstruction::AssignDD), 0);
	AddCommandCore("+assign", "", "MOVOO", "OO", 0, 0, static_cast<DWORD>(InternalInstruction::AssignOO), 0);
	AddCommandCore("+assign", "", "MOVRR", "RR", 0, 0, static_cast<DWORD>(InternalInstruction::AssignRR), 0);
	AddCommandCore("+assign", "", "MOVPP", "PP", 0, 0, static_cast<DWORD>(InternalInstruction::AssignPP), 0);
	AddCommandCore("+relassign", "", "MOVREL_LL", "mL", 0, 0, static_cast<DWORD>(InternalInstruction::RelAssignLL), 0);
	AddCommandCore("+relassign", "", "MOVREL_FF", "gF", 0, 0, static_cast<DWORD>(InternalInstruction::RelAssignFF), 0);
	AddCommandCore("+relassign", "", "MOVREL_SS", "tS", 0, 0, static_cast<DWORD>(InternalInstruction::RelAssignSS), 0);
	AddCommandCore("+relassign", "", "MOVREL_BB", "cB", 0, 0, static_cast<DWORD>(InternalInstruction::RelAssignBB), 0);
	AddCommandCore("+relassign", "", "MOVREL_YY", "zY", 0, 0, static_cast<DWORD>(InternalInstruction::RelAssignYY), 0);
	AddCommandCore("+relassign", "", "MOVREL_WW", "xW", 0, 0, static_cast<DWORD>(InternalInstruction::RelAssignWW), 0);
	AddCommandCore("+relassign", "", "MOVREL_DD", "eD", 0, 0, static_cast<DWORD>(InternalInstruction::RelAssignDD), 0);
	AddCommandCore("+relassign", "", "MOVREL_OO", "uO", 0, 0, static_cast<DWORD>(InternalInstruction::RelAssignOO), 0);
	AddCommandCore("+relassign", "", "MOVREL_RR", "vR", 0, 0, static_cast<DWORD>(InternalInstruction::RelAssignRR), 0);

	AddCommandCore("+udtassign", "", "MOVUDT", "DD", 0, 0, static_cast<DWORD>(InternalInstruction::AssignUdt), static_cast<DWORD>(BuildTask::CopyUdt));

	// Internal Pointer Math Commands (what are the es for)
//	AddCommandCore("+mathptr", "dbprocore.dll", "?PowerDDD@@YAKKK@Z", "eD", 7, 2, static_cast<DWORD>(InternalInstruction::PowerDDD), 0);
//	AddCommandCore("+mathptr", "dbprocore.dll", "?MulDDD@@YAKKK@Z", "eD", 7, 2, static_cast<DWORD>(InternalInstruction::MulDDD), 0);
//	AddCommandCore("+mathptr", "dbprocore.dll", "?DivDDD@@YAKKK@Z", "eD", 7, 2, static_cast<DWORD>(InternalInstruction::DivDDD), 0);
//	AddCommandCore("+mathptr", "dbprocore.dll", "?AddDDD@@YAKKK@Z", "eD", 7, 2, static_cast<DWORD>(InternalInstruction::AddDDD), 0);
//	AddCommandCore("+mathptr", "dbprocore.dll", "?SubDDD@@YAKKK@Z", "eD", 7, 2, static_cast<DWORD>(InternalInstruction::SubDDD), 0);
//	AddCommandCore("+mathptr", "dbprocore.dll", "?EqualDDD@@YAKKK@Z", "eD", 7, 2, static_cast<DWORD>(InternalInstruction::EqualDDD), 0);
//	AddCommandCore("+mathptr", "dbprocore.dll", "?GreaterDDD@@YAKKK@Z", "eD", 7, 2, static_cast<DWORD>(InternalInstruction::GreaterDDD), 0);
//	AddCommandCore("+mathptr", "dbprocore.dll", "?LessDDD@@YAKKK@Z", "eD", 7, 2, static_cast<DWORD>(InternalInstruction::LessDDD), 0);
//	AddCommandCore("+mathptr", "dbprocore.dll", "?NotEqualDDD@@YAKKK@Z", "eD", 7, 2, static_cast<DWORD>(InternalInstruction::NotEqualDDD), 0);
//	AddCommandCore("+mathptr", "dbprocore.dll", "?GreaterEqualDDD@@YAKKK@Z", "eD", 7, 2, static_cast<DWORD>(InternalInstruction::GreaterEqualDDD), 0);
//	AddCommandCore("+mathptr", "dbprocore.dll", "?LessEqualDDD@@YAKKK@Z", "eD", 7, 2, static_cast<DWORD>(InternalInstruction::LessEqualDDD), 0);
	// lee - 240306 - u6b4 - reintroduced because 0xFF < 0x00 is not true (which INT will result in)
	AddCommandCore("+mathptr", "dbprocore.dll", "?EqualDDD@@YAKKK@Z", "eD", 7, 2, static_cast<DWORD>(InternalInstruction::EqualDDD), 0);
	AddCommandCore("+mathptr", "dbprocore.dll", "?GreaterDDD@@YAKKK@Z", "eD", 7, 2, static_cast<DWORD>(InternalInstruction::GreaterDDD), 0);
	AddCommandCore("+mathptr", "dbprocore.dll", "?LessDDD@@YAKKK@Z", "eD", 7, 2, static_cast<DWORD>(InternalInstruction::LessDDD), 0);
	AddCommandCore("+mathptr", "dbprocore.dll", "?NotEqualDDD@@YAKKK@Z", "eD", 7, 2, static_cast<DWORD>(InternalInstruction::NotEqualDDD), 0);
	AddCommandCore("+mathptr", "dbprocore.dll", "?GreaterEqualDDD@@YAKKK@Z", "eD", 7, 2, static_cast<DWORD>(InternalInstruction::GreaterEqualDDD), 0);
	AddCommandCore("+mathptr", "dbprocore.dll", "?LessEqualDDD@@YAKKK@Z", "eD", 7, 2, static_cast<DWORD>(InternalInstruction::LessEqualDDD), 0);

	// Internal Math Commands
	// Wave 8: float arithmetic/comparison is hardcoded SSE2 in the emitter
	// (Mod stays as a runtime call; Power is emitter-built via exp/log, wave 17).
	AddBuildCommand("+mathfloat", "POWERFFF", "FF", 1, 2, static_cast<DWORD>(InternalInstruction::PowerFFF), static_cast<DWORD>(BuildTask::Power));
	AddBuildCommand("+mathfloat", "MULFFF", "FF", 1, 2, static_cast<DWORD>(InternalInstruction::MulFFF), static_cast<DWORD>(BuildTask::Mul));
	AddBuildCommand("+mathfloat", "DIVFFF", "FF", 1, 2, static_cast<DWORD>(InternalInstruction::DivFFF), static_cast<DWORD>(BuildTask::Div));
	AddBuildCommand("+mathfloat", "ADDFFF", "FF", 1, 2, static_cast<DWORD>(InternalInstruction::AddFFF), static_cast<DWORD>(BuildTask::Add));
	AddBuildCommand("+mathfloat", "SUBFFF", "FF", 1, 2, static_cast<DWORD>(InternalInstruction::SubFFF), static_cast<DWORD>(BuildTask::Sub));
	// Wave 21: float mod is emitter-built via the CRT fmod primitive — the
	// last scalar-arithmetic row (integer MOD* use the IDIV remainder path).
	AddBuildCommand("+mathfloat", "MODFFF", "FF", 1, 2, static_cast<DWORD>(InternalInstruction::ModFFF), static_cast<DWORD>(BuildTask::Mod));

	// Internal Comparison Commands
	AddBuildCommand("+mathfloat", "EQUALFF", "FF", 1, 2, static_cast<DWORD>(InternalInstruction::EqualFF), static_cast<DWORD>(BuildTask::Equal));
	AddBuildCommand("+mathfloat", "GREATERFF", "FF", 1, 2, static_cast<DWORD>(InternalInstruction::GreaterLFF), static_cast<DWORD>(BuildTask::Greater));
	AddBuildCommand("+mathfloat", "LESSFF", "FF", 1, 2, static_cast<DWORD>(InternalInstruction::LessLFF), static_cast<DWORD>(BuildTask::Less));
	AddBuildCommand("+mathfloat", "NOTEQUALFF", "FF", 1, 2, static_cast<DWORD>(InternalInstruction::NotEqualLFF), static_cast<DWORD>(BuildTask::NotEqual));
	AddBuildCommand("+mathfloat", "GREATEREQUALFF", "FF", 1, 2, static_cast<DWORD>(InternalInstruction::GreaterEqualLFF), static_cast<DWORD>(BuildTask::GreaterEqual));
	AddBuildCommand("+mathfloat", "LESSEQUALFF", "FF", 1, 2, static_cast<DWORD>(InternalInstruction::LessEqualLFF), static_cast<DWORD>(BuildTask::LessEqual));

	// Internal Math Commands
//	AddCommandCore("+mathstr", "dbprocore.dll", "?AddSSS@@YAKKKK@Z", "SS", 3, 2, static_cast<DWORD>(InternalInstruction::AddSSS), 0);
//	AddCommandCore("+mathstr", "dbprocore.dll", "?EqualLSS@@YAKKK@Z", "SS", 3, 2, static_cast<DWORD>(InternalInstruction::EqualSS), 0);
//	AddCommandCore("+mathstr", "dbprocore.dll", "?GreaterLSS@@YAKKK@Z", "SS", 3, 2, static_cast<DWORD>(InternalInstruction::GreaterLSS), 0);
//	AddCommandCore("+mathstr", "dbprocore.dll", "?LessLSS@@YAKKK@Z", "SS", 3, 2, static_cast<DWORD>(InternalInstruction::LessLSS), 0);
//	AddCommandCore("+mathstr", "dbprocore.dll", "?NotEqualLSS@@YAKKK@Z", "SS", 3, 2, static_cast<DWORD>(InternalInstruction::NotEqualLSS), 0);
//	AddCommandCore("+mathstr", "dbprocore.dll", "?GreaterEqualLSS@@YAKKK@Z", "SS", 3, 2, static_cast<DWORD>(InternalInstruction::GreaterEqualLSS), 0);
//	AddCommandCore("+mathstr", "dbprocore.dll", "?LessEqualLSS@@YAKKK@Z", "SS", 3, 2, static_cast<DWORD>(InternalInstruction::LessEqualSS), 0);
	AddCommandCore("+mathstr", "dbprocore.dll", "?AddSSS@@YAKKKK@Z", "SS", 3, 2, static_cast<DWORD>(InternalInstruction::AddSSS), 0);
	AddCommandCore("+mathstr", "dbprocore.dll", "?EqualLSS@@YAKKK@Z", "SS", 1, 2, static_cast<DWORD>(InternalInstruction::EqualSS), 0);
	AddCommandCore("+mathstr", "dbprocore.dll", "?GreaterLSS@@YAKKK@Z", "SS", 1, 2, static_cast<DWORD>(InternalInstruction::GreaterLSS), 0);
	AddCommandCore("+mathstr", "dbprocore.dll", "?LessLSS@@YAKKK@Z", "SS", 1, 2, static_cast<DWORD>(InternalInstruction::LessLSS), 0);
	AddCommandCore("+mathstr", "dbprocore.dll", "?NotEqualLSS@@YAKKK@Z", "SS", 1, 2, static_cast<DWORD>(InternalInstruction::NotEqualLSS), 0);
	AddCommandCore("+mathstr", "dbprocore.dll", "?GreaterEqualLSS@@YAKKK@Z", "SS", 1, 2, static_cast<DWORD>(InternalInstruction::GreaterEqualLSS), 0);
	AddCommandCore("+mathstr", "dbprocore.dll", "?LessEqualLSS@@YAKKK@Z", "SS", 1, 2, static_cast<DWORD>(InternalInstruction::LessEqualSS), 0);

	// Internal Math Commands
	// Wave 8: double arithmetic/comparison is hardcoded SSE2 in the emitter;
	// wave 20 made PowerOOO native too (exp/log with the (float) round-trip).
	AddBuildCommand("+mathdoublef", "POWEROOO", "OO", 8, 2, static_cast<DWORD>(InternalInstruction::PowerOOO), static_cast<DWORD>(BuildTask::Power));
	AddBuildCommand("+mathdoublef", "MULOOO", "OO", 8, 2, static_cast<DWORD>(InternalInstruction::MulOOO), static_cast<DWORD>(BuildTask::Mul));
	AddBuildCommand("+mathdoublef", "DIVOOO", "OO", 8, 2, static_cast<DWORD>(InternalInstruction::DivOOO), static_cast<DWORD>(BuildTask::Div));
	AddBuildCommand("+mathdoublef", "ADDOOO", "OO", 8, 2, static_cast<DWORD>(InternalInstruction::AddOOO), static_cast<DWORD>(BuildTask::Add));
	AddBuildCommand("+mathdoublef", "SUBOOO", "OO", 8, 2, static_cast<DWORD>(InternalInstruction::SubOOO), static_cast<DWORD>(BuildTask::Sub));

	// Internal Comparison Commands
	AddBuildCommand("+mathdoublef", "EQUALOO", "OO", 1, 8, static_cast<DWORD>(InternalInstruction::EqualLOO), static_cast<DWORD>(BuildTask::Equal));
	AddBuildCommand("+mathdoublef", "GREATERO", "OO", 1, 8, static_cast<DWORD>(InternalInstruction::GreaterLOO), static_cast<DWORD>(BuildTask::Greater));
	AddBuildCommand("+mathdoublef", "LESSOO", "OO", 1, 8, static_cast<DWORD>(InternalInstruction::LessLOO), static_cast<DWORD>(BuildTask::Less));
	AddBuildCommand("+mathdoublef", "NOTEQUALOO", "OO", 1, 8, static_cast<DWORD>(InternalInstruction::NotEqualLOO), static_cast<DWORD>(BuildTask::NotEqual));
	AddBuildCommand("+mathdoublef", "GREATEREQUALOO", "OO", 1, 8, static_cast<DWORD>(InternalInstruction::GreaterEqualLOO), static_cast<DWORD>(BuildTask::GreaterEqual));
	AddBuildCommand("+mathdoublef", "LESSEQUALOO", "OO", 1, 8, static_cast<DWORD>(InternalInstruction::LessEqualOO), static_cast<DWORD>(BuildTask::LessEqual));

	// Internal Math Commands
	// Wave 8b: int64 arithmetic is hardcoded full-width REG64 in the emitter;
	// wave 20 made PowerRRR native too (REX.W exp/log with truncating store).
	AddBuildCommand("+mathdoublei", "POWERRRR", "RR", 9, 2, static_cast<DWORD>(InternalInstruction::PowerRRR), static_cast<DWORD>(BuildTask::Power));
	AddBuildCommand("+mathdoublei", "MULRRR", "RR", 9, 2, static_cast<DWORD>(InternalInstruction::MulRRR), static_cast<DWORD>(BuildTask::Mul));
	AddBuildCommand("+mathdoublei", "DIVRRR", "RR", 9, 2, static_cast<DWORD>(InternalInstruction::DivRRR), static_cast<DWORD>(BuildTask::Div));
	AddBuildCommand("+mathdoublei", "ADDRRR", "RR", 9, 2, static_cast<DWORD>(InternalInstruction::AddRRR), static_cast<DWORD>(BuildTask::Add));
	AddBuildCommand("+mathdoublei", "SUBRRR", "RR", 9, 2, static_cast<DWORD>(InternalInstruction::SubRRR), static_cast<DWORD>(BuildTask::Sub));
	AddBuildCommand("+mathdoublei", "MODRRR", "RR", 9, 2, static_cast<DWORD>(InternalInstruction::ModRRR), static_cast<DWORD>(BuildTask::Mod));

	// Internal Comparison Commands
	AddBuildCommand("+mathdoublei", "CONDRRR", "RR", 1, 2, static_cast<DWORD>(InternalInstruction::EqualLRR), static_cast<DWORD>(BuildTask::Equal));
	AddBuildCommand("+mathdoublei", "CONDRRR", "RR", 1, 2, static_cast<DWORD>(InternalInstruction::GreaterLRR), static_cast<DWORD>(BuildTask::Greater));
	AddBuildCommand("+mathdoublei", "CONDRRR", "RR", 1, 2, static_cast<DWORD>(InternalInstruction::LessLRR), static_cast<DWORD>(BuildTask::Less));
	AddBuildCommand("+mathdoublei", "CONDRRR", "RR", 1, 2, static_cast<DWORD>(InternalInstruction::NotEqualLRR), static_cast<DWORD>(BuildTask::NotEqual));
	AddBuildCommand("+mathdoublei", "CONDRRR", "RR", 1, 2, static_cast<DWORD>(InternalInstruction::GreaterEqualLRR), static_cast<DWORD>(BuildTask::GreaterEqual));
	AddBuildCommand("+mathdoublei", "CONDRRR", "RR", 1, 2, static_cast<DWORD>(InternalInstruction::LessEqualRR), static_cast<DWORD>(BuildTask::LessEqual));

	// Cast Instructions
	// Wave 8: the float-family casts (int<->float/double, dword<->float/double)
	// are hardcoded SSE2 CVT* instructions in the emitter; the rest stay DLL.
	AddBuildCommand("+cast", "CASTLTOF", "L", 1, 1, static_cast<DWORD>(InternalInstruction::CastLToF), static_cast<DWORD>(BuildTask::CastIntToFloat));
	AddBuildCommand("+cast", "CASTLTOB", "L", 1, 1, static_cast<DWORD>(InternalInstruction::CastLToB), static_cast<DWORD>(BuildTask::CastToNarrow));
	AddBuildCommand("+cast", "CASTLTOY", "L", 1, 1, static_cast<DWORD>(InternalInstruction::CastLToY), static_cast<DWORD>(BuildTask::CastToNarrow));
	AddBuildCommand("+cast", "CASTLTOW", "L", 1, 1, static_cast<DWORD>(InternalInstruction::CastLToW), static_cast<DWORD>(BuildTask::CastToNarrow));
	AddBuildCommand("+cast", "CASTLTOD", "L", 1, 1, static_cast<DWORD>(InternalInstruction::CastLToD), static_cast<DWORD>(BuildTask::CastToNarrow));
	AddBuildCommand("+cast", "CASTLTOO", "L", 1, 1, static_cast<DWORD>(InternalInstruction::CastLToO), static_cast<DWORD>(BuildTask::CastIntToDouble));
	// Wave 15: int -> int64 widening is native MOVSXD RAX,EAX, not a DLL call.
	AddBuildCommand("+cast", "CASTLTOR", "L", 1, 1, static_cast<DWORD>(InternalInstruction::CastLToR), static_cast<DWORD>(BuildTask::CastIntToInt64));
	AddBuildCommand("+cast", "CASTFTOL", "F", 1, 1, static_cast<DWORD>(InternalInstruction::CastFTOL), static_cast<DWORD>(BuildTask::CastFloatToInt));
	AddBuildCommand("+cast", "CASTFTOB", "F", 1, 1, static_cast<DWORD>(InternalInstruction::CastFTOB), static_cast<DWORD>(BuildTask::CastFloatToNarrow));
	AddBuildCommand("+cast", "CASTFTOY", "F", 1, 1, static_cast<DWORD>(InternalInstruction::CastFTOY), static_cast<DWORD>(BuildTask::CastFloatToNarrow));
	AddBuildCommand("+cast", "CASTFTOW", "F", 1, 1, static_cast<DWORD>(InternalInstruction::CastFTOW), static_cast<DWORD>(BuildTask::CastFloatToNarrow));
	AddBuildCommand("+cast", "CASTFTOD", "F", 1, 1, static_cast<DWORD>(InternalInstruction::CastFTOD), static_cast<DWORD>(BuildTask::CastFloatToInt));
	AddBuildCommand("+cast", "CASTFTOO", "F", 1, 1, static_cast<DWORD>(InternalInstruction::CastFTOO), static_cast<DWORD>(BuildTask::CastFloatToDouble));
	AddBuildCommand("+cast", "CASTFTOR", "F", 1, 1, static_cast<DWORD>(InternalInstruction::CastFTOR), static_cast<DWORD>(BuildTask::CastFloatToInt64));
	// Wave 19: the widening casts (B/Y/W sources) are emitter-native MOVZX
	// zero-extensions; W->B/Y stay truncations via CastToNarrow. B/Y share
	// the unsigned-char semantics of the DLL entries they replace.
	AddBuildCommand("+cast", "CASTBTOL", "B", 1, 1, static_cast<DWORD>(InternalInstruction::CastBTOL), static_cast<DWORD>(BuildTask::CastWiden));
	AddBuildCommand("+cast", "CASTBTOF", "B", 1, 1, static_cast<DWORD>(InternalInstruction::CastBTOF), static_cast<DWORD>(BuildTask::CastWidenToFloat));
	AddBuildCommand("+cast", "CASTBTOW", "B", 1, 1, static_cast<DWORD>(InternalInstruction::CastBTOW), static_cast<DWORD>(BuildTask::CastWiden));
	AddBuildCommand("+cast", "CASTBTOD", "B", 1, 1, static_cast<DWORD>(InternalInstruction::CastBTOD), static_cast<DWORD>(BuildTask::CastWiden));
	AddBuildCommand("+cast", "CASTBTOO", "B", 1, 1, static_cast<DWORD>(InternalInstruction::CastBTOO), static_cast<DWORD>(BuildTask::CastWidenToFloat));
	AddBuildCommand("+cast", "CASTBTOR", "B", 1, 1, static_cast<DWORD>(InternalInstruction::CastBTOR), static_cast<DWORD>(BuildTask::CastDwordToInt64));
	AddBuildCommand("+cast", "CASTYTOL", "Y", 1, 1, static_cast<DWORD>(InternalInstruction::CastYTOL), static_cast<DWORD>(BuildTask::CastWiden));
	AddBuildCommand("+cast", "CASTYTOF", "Y", 1, 1, static_cast<DWORD>(InternalInstruction::CastYTOF), static_cast<DWORD>(BuildTask::CastWidenToFloat));
	AddBuildCommand("+cast", "CASTYTOW", "Y", 1, 1, static_cast<DWORD>(InternalInstruction::CastYTOW), static_cast<DWORD>(BuildTask::CastWiden));
	AddBuildCommand("+cast", "CASTYTOD", "Y", 1, 1, static_cast<DWORD>(InternalInstruction::CastYTOD), static_cast<DWORD>(BuildTask::CastWiden));
	AddBuildCommand("+cast", "CASTYTOO", "Y", 1, 1, static_cast<DWORD>(InternalInstruction::CastYTOO), static_cast<DWORD>(BuildTask::CastWidenToFloat));
	AddBuildCommand("+cast", "CASTYTOR", "Y", 1, 1, static_cast<DWORD>(InternalInstruction::CastYTOR), static_cast<DWORD>(BuildTask::CastDwordToInt64));
	AddBuildCommand("+cast", "CASTWTOL", "W", 1, 1, static_cast<DWORD>(InternalInstruction::CastWTOL), static_cast<DWORD>(BuildTask::CastWiden));
	AddBuildCommand("+cast", "CASTWTOF", "W", 1, 1, static_cast<DWORD>(InternalInstruction::CastWTOF), static_cast<DWORD>(BuildTask::CastWidenToFloat));
	AddBuildCommand("+cast", "CASTWTOB", "W", 1, 1, static_cast<DWORD>(InternalInstruction::CastWTOB), static_cast<DWORD>(BuildTask::CastToNarrow));
	AddBuildCommand("+cast", "CASTWTOY", "W", 1, 1, static_cast<DWORD>(InternalInstruction::CastWTOY), static_cast<DWORD>(BuildTask::CastToNarrow));
	AddBuildCommand("+cast", "CASTWTOD", "W", 1, 1, static_cast<DWORD>(InternalInstruction::CastWTOD), static_cast<DWORD>(BuildTask::CastWiden));
	AddBuildCommand("+cast", "CASTWTOO", "W", 1, 1, static_cast<DWORD>(InternalInstruction::CastWTOO), static_cast<DWORD>(BuildTask::CastWidenToFloat));
	AddBuildCommand("+cast", "CASTWTOR", "W", 1, 1, static_cast<DWORD>(InternalInstruction::CastWTOR), static_cast<DWORD>(BuildTask::CastDwordToInt64));
	AddBuildCommand("+cast", "CASTDTOL", "D", 1, 1, static_cast<DWORD>(InternalInstruction::CastDTOL), static_cast<DWORD>(BuildTask::CastToNarrow));
	AddBuildCommand("+cast", "CASTDTOF", "D", 1, 1, static_cast<DWORD>(InternalInstruction::CastDTOF), static_cast<DWORD>(BuildTask::CastIntToFloat));
	AddBuildCommand("+cast", "CASTDTOB", "D", 1, 1, static_cast<DWORD>(InternalInstruction::CastDTOB), static_cast<DWORD>(BuildTask::CastToNarrow));
	AddBuildCommand("+cast", "CASTDTOY", "D", 1, 1, static_cast<DWORD>(InternalInstruction::CastDTOY), static_cast<DWORD>(BuildTask::CastToNarrow));
	AddBuildCommand("+cast", "CASTDTOW", "D", 1, 1, static_cast<DWORD>(InternalInstruction::CastDTOW), static_cast<DWORD>(BuildTask::CastToNarrow));
	AddBuildCommand("+cast", "CASTDTOO", "D", 1, 1, static_cast<DWORD>(InternalInstruction::CastDTOO), static_cast<DWORD>(BuildTask::CastIntToDouble));
	// Wave 15: dword/address -> int64 widening is native (zero-extension), not
	// a DLL call.
	AddBuildCommand("+cast", "CASTDTOR", "D", 1, 1, static_cast<DWORD>(InternalInstruction::CastDTOR), static_cast<DWORD>(BuildTask::CastDwordToInt64));
	AddBuildCommand("+cast", "CASTOTOL", "O", 1, 1, static_cast<DWORD>(InternalInstruction::CastOTOL), static_cast<DWORD>(BuildTask::CastDoubleToInt));
	AddBuildCommand("+cast", "CASTOTOF", "O", 1, 1, static_cast<DWORD>(InternalInstruction::CastOTOF), static_cast<DWORD>(BuildTask::CastDoubleToFloat));
	AddBuildCommand("+cast", "CASTOTOB", "O", 1, 1, static_cast<DWORD>(InternalInstruction::CastOTOB), static_cast<DWORD>(BuildTask::CastFloatToNarrow));
	AddBuildCommand("+cast", "CASTOTOY", "O", 1, 1, static_cast<DWORD>(InternalInstruction::CastOTOY), static_cast<DWORD>(BuildTask::CastFloatToNarrow));
	AddBuildCommand("+cast", "CASTOTOW", "O", 1, 1, static_cast<DWORD>(InternalInstruction::CastOTOW), static_cast<DWORD>(BuildTask::CastFloatToNarrow));
	AddBuildCommand("+cast", "CASTOTOD", "O", 1, 1, static_cast<DWORD>(InternalInstruction::CastOTOD), static_cast<DWORD>(BuildTask::CastDoubleToInt));
	AddBuildCommand("+cast", "CASTOTOR", "O", 1, 1, static_cast<DWORD>(InternalInstruction::CastOTOR), static_cast<DWORD>(BuildTask::CastDoubleToInt64));
	AddBuildCommand("+cast", "CASTRTOL", "R", 1, 1, static_cast<DWORD>(InternalInstruction::CastRTOL), static_cast<DWORD>(BuildTask::CastInt64ToLower));
	AddBuildCommand("+cast", "CASTRTOF", "R", 1, 1, static_cast<DWORD>(InternalInstruction::CastRTOF), static_cast<DWORD>(BuildTask::CastInt64ToFloat));
	AddBuildCommand("+cast", "CASTRTOB", "R", 1, 1, static_cast<DWORD>(InternalInstruction::CastRTOB), static_cast<DWORD>(BuildTask::CastInt64ToLower));
	AddBuildCommand("+cast", "CASTRTOY", "R", 1, 1, static_cast<DWORD>(InternalInstruction::CastRTOY), static_cast<DWORD>(BuildTask::CastInt64ToLower));
	AddBuildCommand("+cast", "CASTRTOW", "R", 1, 1, static_cast<DWORD>(InternalInstruction::CastRTOW), static_cast<DWORD>(BuildTask::CastInt64ToLower));
	AddBuildCommand("+cast", "CASTRTOD", "R", 1, 1, static_cast<DWORD>(InternalInstruction::CastRTOD), static_cast<DWORD>(BuildTask::CastInt64ToLower));
	AddBuildCommand("+cast", "CASTRTOO", "R", 1, 1, static_cast<DWORD>(InternalInstruction::CastRTOO), static_cast<DWORD>(BuildTask::CastInt64ToDouble));

	return true;
}

void CInstructionTable::ScanStart(void)
{
	// Initialise
	Init();

	// Start Scan
	FFindFirstFile("*.dll");

	// begin scan
	m_bScanActive=true;
}

void CInstructionTable::ScanStep(void)
{
	if(FGetFileReturnValue()!=-1)
	{
		// Item Details
		LPSTR pDLLFile = m_filedata[m_findex].name;
		DWORD dwType = m_filedata[m_findex].attrib;
		if(stricmp(pDLLFile,".")!=NULL && stricmp(pDLLFile, "..")!=NULL)
		{
			if(FGetActualTypeValue(dwType)==1)
			{
				// No folders
			}
			else
			{
				// Do File Thing
				std::string name(pDLLFile);
				name.resize(name.size()-4);
				LoadCommandsFromDLL(const_cast<LPSTR>(name.c_str()), pDLLFile);
			}
		}

		// Go to next one
		FFindNextFile();
	}
	else
	{
		// Determine quit state
		if(m_findex==0)
		{
			// Scan Over
			m_bScanActive=false;
		}
	}
}

void CInstructionTable::ScanEnd(void)
{
	// Free usages
	Free();
}

bool CInstructionTable::ScanPluginsForCommands(void)
{
	// Store current folder
	char path[_MAX_PATH];
	_getcwd(path, _MAX_PATH);
	g_pDBPCompiler->SetInternalFile(PATH_CURRENTFOLDER, path);


	// Switch to PLUGINS Folder
	_chdir(g_pDBPCompiler->GetInternalFile(PATH_PLUGINSFOLDER));

	/* When I compiled VS2012 (with VS 10.0 platform), got this error:
	   No user-defined-conversion operator available that can perform this conversion, or the operator cannot be called

	//
	//	TODO: Make each DLL load in parallel
	//	NOTE: This will probably be a good test for the work queue
	//

	// For each DLL found, attempt to add it as a commandDLL
	struct SEnv
	{
		const char *ca;
		const char *nm;
		CInstructionTable *it;
	};
	const SEnv parms[] =
	{
		"core","dbprocore.dll",this,
		"display","DBProSetupDebug.dll",this,
		"text","DBProTextDebug.dll",this,
		"image","DBProImageDebug.dll",this,
		"basic2d","DBProBasic2DDebug.dll",this,
		"sprite","DBProSpritesDebug.dll",this,
		"input","DBProInputDebug.dll",this,
		"system","DBProSystemDebug.dll",this,
		"file","DBProFileDebug.dll",this,
		"ftp","DBProFtpDebug.dll",this,
		"memblocks","DBProMemblocksDebug.dll",this,
		"animation","DBProAnimationDebug.dll",this,
		"bitmap","DBProBitmapDebug.dll",this,
		"multiplayer","DBProMultiplayerDebug.dll",this,
		"camera","DBProCameraDebug.dll",this,
		"light","DBProLightDebug.dll",this,
		"matrix","DBProMatrixDebug.dll",this,
		"basic3d","DBProBasic3DDebug.dll",this,
		"world","DBProWorld3DDebug.dll",this,
		"world","DBProLODTerrainDebug.dll",this,
		"3dmaths","DBProVectorsDebug.dll",this,
		"particles","DBProParticlesDebug.dll",this,
		"music","DBProMusicDebug.dll",this,
		"sound","DBProSoundDebug.dll",this,
		"basic3d","DBProCSGDebug.dll",this,
		"transforms","DBProTransformsDebug.dll",this
	};
	void(*load)(const SEnv *env) = [](const SEnv *env)->void
	{
		env->it->LoadCommandsFromDLL(const_cast<LPSTR>(env->ca), const_cast<LPSTR>(env->nm));
	};

	db3::CSignal sig;
	db3::uint p;
	for(p=0; p<sizeof(parms)/sizeof(parms[0]); p++)
	{
		//g_WorkQueue.Enqueue(load, &parms[p], &sig);
		load(&parms[p]);
	}

	//sig.Sync();
	*/

	// Read core commands from the selected core runtime. This is the same image
	// that packaging embeds, so decorated command names cannot drift.
	const auto* runtimeBundle = g_pDBPCompiler->GetResolvedRuntimeBundle();
	if(runtimeBundle == nullptr ||
		_chdir(runtimeBundle->pluginsDirectory.string().c_str()) != 0 ||
		!LoadCommandsFromDLL("core","dbprocore.dll"))
	{
		_chdir(g_pDBPCompiler->GetInternalFile(PATH_CURRENTFOLDER));
		return false;
	}
	_chdir(g_pDBPCompiler->GetInternalFile(PATH_PLUGINSFOLDER));

	// The remaining DLLs belong to the host product's command surface.
	LoadCommandsFromDLL("display","DBProSetupDebug.dll");
	LoadCommandsFromDLL("text","DBProTextDebug.dll");
	LoadCommandsFromDLL("image","DBProImageDebug.dll");
	LoadCommandsFromDLL("basic2d","DBProBasic2DDebug.dll");
	LoadCommandsFromDLL("sprite","DBProSpritesDebug.dll");
	LoadCommandsFromDLL("input","DBProInputDebug.dll");
	LoadCommandsFromDLL("system","DBProSystemDebug.dll");
	LoadCommandsFromDLL("file","DBProFileDebug.dll");
	LoadCommandsFromDLL("ftp","DBProFtpDebug.dll");
	LoadCommandsFromDLL("memblocks","DBProMemblocksDebug.dll");
	LoadCommandsFromDLL("animation","DBProAnimationDebug.dll");
	LoadCommandsFromDLL("bitmap","DBProBitmapDebug.dll");
	LoadCommandsFromDLL("multiplayer","DBProMultiplayerDebug.dll");
	LoadCommandsFromDLL("camera","DBProCameraDebug.dll");
	LoadCommandsFromDLL("light","DBProLightDebug.dll");
	LoadCommandsFromDLL("matrix","DBProMatrixDebug.dll");
	LoadCommandsFromDLL("basic3d","DBProBasic3DDebug.dll");
	LoadCommandsFromDLL("world","DBProWorld3DDebug.dll");
	LoadCommandsFromDLL("world","DBProLODTerrainDebug.dll");
	LoadCommandsFromDLL("3dmaths","DBProVectorsDebug.dll");
	LoadCommandsFromDLL("particles","DBProParticlesDebug.dll");
	LoadCommandsFromDLL("music","DBProMusicDebug.dll");
	LoadCommandsFromDLL("sound","DBProSoundDebug.dll");
	LoadCommandsFromDLL("basic3d","DBProCSGDebug.dll");
	LoadCommandsFromDLL("transforms","DBProTransformsDebug.dll");

	// Switch to PLUGINS-USER Folder
	_chdir(g_pDBPCompiler->GetInternalFile(PATH_PLUGINSUSERFOLDER));
	ScanStart();
	while(m_bScanActive) ScanStep();
	ScanEnd();

	// Switch to PLUGINS-LICENSED Folder
	_chdir(g_pDBPCompiler->GetInternalFile(PATH_PLUGINSLICENSEDFOLDER));
	ScanStart();
	while(m_bScanActive) ScanStep();
	ScanEnd();

	// Switch back to current folder
	_chdir(g_pDBPCompiler->GetInternalFile(PATH_CURRENTFOLDER));
	return true;
}

bool CInstructionTable::LoadInstructionDatabase(void)
{
	// Scan Available DLLs for Command Names (this will be a plugin traversal)
	return ScanPluginsForCommands();
}

bool CInstructionTable::AddCommand(LPSTR pName, LPSTR pDLL, LPSTR pDecoratedName, LPSTR pParamTypesString, DWORD resultp, DWORD pmax)
{
	return AddCommandCore2(pName, pDLL, pDecoratedName, pParamTypesString, resultp, pmax, 0, 0, 0, false, NULL, NULL);
}

bool CInstructionTable::AddUniqueCommand(LPSTR pName, LPSTR pDLL, LPSTR pDecoratedName, LPSTR pParamTypesString, DWORD resultp, DWORD pmax)
{
	// Only add if completely unique
	if(FindInstructionWithNameAndParams(pName, pParamTypesString)==false)
		return AddCommandCore2(pName, pDLL, pDecoratedName, pParamTypesString, resultp, pmax, 0, 0, 0, false, NULL, NULL);
	else
		return true;
}

bool CInstructionTable::AddBuildCommand(LPSTR pName, LPSTR pDesc, LPSTR pParamTypesString, DWORD resultp, DWORD pmax, DWORD dwInternalValueIndex, DWORD dwBuildID)
{
	return AddCommandCore2(pName, "", pDesc, pParamTypesString, resultp, pmax, dwInternalValueIndex, dwBuildID, 0, false, NULL, NULL);
}

bool CInstructionTable::AddCommandCore(LPSTR pName, LPSTR pDLL, LPSTR pDecoratedName, LPSTR pParamTypesString, DWORD resultp, DWORD pmax, DWORD dwInternalValueIndex, DWORD dwBuildID)
{
	return AddCommandCore2(pName, pDLL, pDecoratedName, pParamTypesString, resultp, pmax, dwInternalValueIndex, dwBuildID, 0, false, NULL, NULL);
}

bool CInstructionTable::AddCommandCore2(LPSTR pName, LPSTR pDLL, LPSTR pDecoratedName, LPSTR pParamTypesString, DWORD resultp, DWORD pmax, DWORD dwInternalValueIndex, DWORD dwBuildID, DWORD dwPlace, bool bPassArrayAsInput, CStr* pParamFullDesc, LPSTR* plpretStr )
{
	// Make Entry (RAII-owned until inserted into the table)
	auto pEntryOwner = std::make_unique<CInstructionTableEntry>();
	CInstructionTableEntry* pEntry = pEntryOwner.get();
	auto pStr = std::make_unique<CStr>(pName);
	auto pStrDLL = std::make_unique<CStr>(pDLL);
	auto pStrDecName = std::make_unique<CStr>(pDecoratedName);
	auto pStrParamTypes = std::make_unique<CStr>(pParamTypesString);

	DWORD dwCurrentID;

	dwCurrentID = static_cast<DWORD>(db3::atomic_inc(reinterpret_cast<db3::u32 *>(&m_dwCurrentInternalID)));

	// Ownership of the four CStr buffers transfers into the entry.
	pEntry->SetData(dwCurrentID, pStr.release(), pStrDLL.release(), pStrDecName.release(), pStrParamTypes.release(), resultp, pmax, dwInternalValueIndex, dwBuildID);
	pEntry->SetSpecialArrayParam(bPassArrayAsInput);

	// Set param full desc if available
	if(pParamFullDesc)
	{
		if(pParamFullDesc->Length()==0)
		{
			SAFE_DELETE(pParamFullDesc);
		}
		else
		{
			// Add to instruction as a value full param desc
			pEntry->SetFullParamDesc(pParamFullDesc);
		}
	}

#ifndef __AARON_INSTRPERF__
	// See if command has friends of same name (as they go together)
	CInstructionTableEntry* pLastFriendEntry = FindLastFriendOfName(pName);

	// Has a Friend - Insert Into Database
	if(pLastFriendEntry)
	{
		// If command is identical to friend, cannot add it
		if(pEntry->GetHardcoreInternalValue()==0)
		{
			if(pEntry->GetReturnParam()==pLastFriendEntry->GetReturnParam())
			{
				if(pEntry->GetDLL()->Length()>0 && pLastFriendEntry->GetDLL()->Length()>0)
				{
					if(pEntry->GetDecoratedName()->Length()>0 && pLastFriendEntry->GetDecoratedName()->Length()>0)
					{
						if(stricmp(pLastFriendEntry->GetName()->GetStr(), pEntry->GetName()->GetStr())==NULL)
						{
							if(stricmp(pLastFriendEntry->GetParamTypes()->GetStr(), pEntry->GetParamTypes()->GetStr())==NULL)
							{
								// if return string valid, fill with conflicting DLL/name
								if ( plpretStr )
								{
									// create and return in ptr
									*plpretStr = new char[512];
									wsprintf ( *plpretStr, "%s", pLastFriendEntry->GetDLL()->GetStr() );
								}
								// identical command, cannot have two the same
								SAFE_DELETE(pEntry);
								return false;
							}
						}
					}
				}
			}
		}

		{
			db3::CAutolock autolock(m_Lock);

			// Add unique command to instructions
			CInstructionTableEntry* pNextAfterLast = pLastFriendEntry->GetNext();
			if(pNextAfterLast)
				pNextAfterLast->Insert(pEntry);
			else
				pLastFriendEntry->Add(pEntry);
		}
	}
	else
	{
		db3::CAutolock autolock(m_Lock);

		// Newy - Add to Database
		if(m_pFirstInstructionEntry==NULL)
			m_pFirstInstructionEntry=pEntry;
		else
			m_pFirstInstructionEntry->Add(pEntry);
	}
#else
	{
		autolock_type autolock(m_InstructionMapLock);

		auto pFirst = m_InstructionMap.Lookup(pName);
		if (!pFirst)
			return false;

		if (pFirst->P != nullptr)
			pFirst->P->Add(pEntryOwner.release());
		else
			pFirst->P = pEntryOwner.release();

		//
		//	NOTE: Checks for duplicate commands will be performed after all of the other commands have been loaded. This
		//	      is to ensure that all duplicates are found (improves experience for plugin developers). This also
		//	      reduces the amount of time spent in the lock (which reduces waiting in other threads and therefore
		//	      improves throughput).
		//
	}
#endif

	// NOTE: Storing this doesn't look like it's necessary, but it might as well be done anyway
	// NOTE[20121123]: Storing this is somewhat necessary.
	if (!m_EntryArray.CheckSlot(static_cast<db3::uint>(dwCurrentID)))
		return false;

	m_EntryArray[dwCurrentID] = pEntry;

	// Store reference if internal
	if(dwInternalValueIndex>0)
	{
		SetIIValue(dwInternalValueIndex, dwCurrentID);
		SetRef(dwInternalValueIndex, pEntry);
	}

	// Set Place if any of return param in input param list
	pEntry->SetReturnParamPlace(dwPlace);

	return true;
}

bool CInstructionTable::AddUserFunction(LPSTR pName, DWORD resultp, LPSTR pParamTypesString, DWORD pmax, CDeclaration* pDecChain)
{
	// leefix-040803-Before confirm, check if name is a reserved word or function name
	if ( g_pStatementList->GetProgramStatements()->DetermineIfReservedWord ( pName ) ) return false;
	if ( g_pStatementList->GetProgramStatements()->DetermineIfFunctionName ( pName, false ) ) return false;

	// Increment ID
	DWORD dwCurrentID = static_cast<DWORD>(db3::atomic_inc(reinterpret_cast<db3::u32 *>(&m_dwCurrentInternalID)));

	// Make Entry (RAII-owned until inserted into the table)
	auto pEntryOwner = std::make_unique<CInstructionTableEntry>();
	CInstructionTableEntry* pEntry = pEntryOwner.get();
	auto pStr = std::make_unique<CStr>(pName);
	auto pStrID = std::make_unique<CStr>((DWORD)1);
	auto pStrParamTypes = std::make_unique<CStr>(pParamTypesString);

	pStrID->SetNumericText(dwCurrentID);
	// Ownership of the three CStr buffers transfers into the entry.
	pEntry->SetData(dwCurrentID, pStr.release(), NULL, pStrID.release(), pStrParamTypes.release(), resultp, pmax, 0, 0);

	// Ensure all user functions know about their userfunction structure
	if ( pDecChain ) pEntry->SetDecChain(pDecChain);

#ifdef __AARON_INSTRPERF__
	{
		autolock_type autolock(m_UserFunctionMapLock);

		auto entry = m_UserFunctionMap.Lookup(pName);
		if (!entry)
			return false;

		if (entry->P != nullptr)
			entry->P->Add(pEntryOwner.release());
		else
			entry->P = pEntryOwner.release();
	}

	if (!m_EntryArray.CheckSlot(static_cast<db3::uint>(dwCurrentID)))
		return false;

	m_EntryArray[dwCurrentID] = pEntry;
#else
	// Add to Database
	if(m_pFirstUserFunctionEntry==NULL)
		m_pFirstUserFunctionEntry=pEntry;
	else
		m_pFirstUserFunctionEntry->Add(pEntry);
#endif

	// success
	return true;
}

#if 0
inline bool IdentifierBreak(char c, bool bAcceptWhitespace)
{
	if (c=='\0' || (c<=' ' && bAcceptWhitespace))
		return true;

	if (c=='(' || c==':')
		return true;

	return false;
}
#endif
inline const char *AdvanceIdentifier(const char *pStringData, bool bAcceptWhitespace)
{
#if 0
	const char *p;

	p = pStringData;
	while(*p != '\0')
	{
		if ((*p<=' ' && bAcceptWhitespace) || *p=='(' || *p==':')
			return p;

		p++;
	}

	return p;
#else
# define DIFFER_ACCEPT 0
# if DIFFER_ACCEPT
	static bool accept_n[256];
	static bool accept_w[256];
	static bool didinit=false;
	bool *accept;
# else
	static bool accept[256];
	static bool didinit=false;
# endif
	const char *p;

	if (!didinit)
	{
		static db3::CLock lock;
		db3::CAutolock autolock(lock);

# if DIFFER_ACCEPT
		memset(accept_n, 0, sizeof(accept_n));
		memset(accept_w, 0, sizeof(accept_w));
# else
		memset(accept, 0, sizeof(accept));
# endif

# if DIFFER_ACCEPT
		for(p="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_#$%"; *p!='\0'; p++)
			accept_n[*p]=accept_w[*p] = true;

		for(p=" "; *p!='\0'; p++)
			accept_w[*p] = true;
# else
		for(p="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_#$%"; *p!='\0'; p++)
			accept[*p] = true;
# endif

		didinit = true;
	}

# if DIFFER_ACCEPT
	accept = bAcceptWhitespace ? &accept_w[0] : &accept_n[0];
# endif

	p = pStringData;
	while(*p != '\0')
	{
		if (!accept[*(db3::u8 *)p])
			return p;

		p++;
	}

	return p;
#endif
}

CInstructionTableEntry *CInstructionTable::GetEntry(int iType, const char *pStringData, bool *pRetFail)
{
	map_type &mt = iType == 0 ? m_InstructionMap : m_UserFunctionMap;

	auto entry = mt.Find(pStringData);
	if (!entry)
	{
		if (pRetFail)
			*pRetFail = true;

		return nullptr;
	}

	if (pRetFail)
		*pRetFail = false;

	return entry->P;
}
CInstructionTableEntry *CInstructionTable::ResolveEntry(CInstructionTableEntry *pHead, int iWithAnyReturnValue, bool bCommandWhiteSpace)
{
	CInstructionTableEntry *pIter;

	for(pIter=pHead; pIter; pIter=pIter->GetNext())
	{
		bool bRetParmOK = false;
		if (bCommandWhiteSpace==true)
		{
			DWORD dwRetParm = pIter->GetReturnParam();

			if (iWithAnyReturnValue==0)
				bRetParmOK = dwRetParm==0 || (dwRetParm>=11 && dwRetParm<=19) || !!pIter->GetSpecialArrayParam();
			else if(iWithAnyReturnValue==1)
				bRetParmOK = dwRetParm>0;
		} else
			bRetParmOK = true;

		if (!bRetParmOK)
			continue;

		return pIter;
	}

	return nullptr;
}
bool CInstructionTable::FindEntryDirect(int iType, bool bCommandWhiteSpace, LPSTR pStringData, int iWithReturnValue, DWORD* pdwData, DWORD* pdwParamMax, DWORD* pdwLength, CInstructionTableEntry** ppRef)
{
	CInstructionTableEntry *pItem, *pRslv;

	pItem = GetEntry(iType, pStringData, nullptr);
	if (!pItem)
		return false;

	pRslv = ResolveEntry(pItem, iWithReturnValue, bCommandWhiteSpace);
	if (!pRslv)
		return false;

	*pdwData = pRslv->GetInternalID();
	*pdwParamMax = pRslv->GetParamMax();
	*pdwLength = pRslv->GetName()->Length();
	if (ppRef)
		*ppRef = pRslv;

	return true;
}

bool CInstructionTable::FindEntry(int iType, bool bCommandWhiteSpace, CInstructionTableEntry* pBaseEntry, LPSTR pStringData, int iWithAnyReturnValue, DWORD* pdwData, DWORD* pdwParamMax, DWORD* pdwLength, CInstructionTableEntry** ppRef)
{
#ifdef __AARON_INSTRPERF__
# if 0
	map_type &mt = iType == 0 ? m_InstructionMap     : m_UserFunctionMap;
	// locking may not be necessary because we're only doing a look up...
	//lock_type &l = iType == 0 ? m_InstructionMapLock : m_UserFunctionMapLock;
	//autolock_type autolock(l);

	CInstructionTableEntry *largestMatch = nullptr, *lastMatch = nullptr;
	const char *check;
	db3::uint len;
	char buf[256];
	//bool possibleMatch;

	check = pStringData;
	while(*check!='\0')
	{
		check = AdvanceIdentifier(check + 1, bCommandWhiteSpace);

		len = static_cast<db3::uint>(check - pStringData);
		strncpy_s(buf, sizeof(buf), pStringData, len);

		for(char *x=strchr(buf, '\0') - 1; *x<=' '; x--)
			*x = '\0';

		auto entry = mt.Find(buf);
		if (!entry) //if this entry isn't here, larger entries won't occur, break from the loop
			break;

		if (entry->P)
			largestMatch = entry->P;

		if ((*check<=' ' && !bCommandWhiteSpace) || *check=='(' || *check==':')
			break;
	}

	lastMatch = largestMatch;

	for(; largestMatch; largestMatch=largestMatch->GetNext())
	{
		bool bRetParmOK = false;
		if (bCommandWhiteSpace==true)
		{
			DWORD dwRetParm = largestMatch->GetReturnParam();

			if (iWithAnyReturnValue==0)
				bRetParmOK = dwRetParm==0 || (dwRetParm>=11 && dwRetParm<=19) || !!largestMatch->GetSpecialArrayParam();
			else if(iWithAnyReturnValue==1)
				bRetParmOK = dwRetParm>0;
		} else
			bRetParmOK = true;

		if (!bRetParmOK)
			continue;

		*pdwData = largestMatch->GetInternalID();
		*pdwParamMax = largestMatch->GetParamMax();
		*pdwLength = largestMatch->GetName()->Length();
		if (ppRef)
			*ppRef = largestMatch;

		return true;
	}

	return false;
# else
	CInstructionTableEntry *pTest=nullptr, *pBest=nullptr, *pRslv=nullptr;
	const char *check;
	db3::uint len;
	char buf[256];
	bool bRetFail;

	bRetFail = false;
	check = pStringData;
	while(*check!='\0')
	{
		check = AdvanceIdentifier(check + 1, bCommandWhiteSpace);

		len = static_cast<db3::uint>(check - pStringData);
		while(len > 0 && pStringData[len - 1]<=' ')
			len--;

		strncpy_s(buf, sizeof(buf), pStringData, len);

		pTest = GetEntry(iType, buf, &bRetFail);
		if (!pTest && bRetFail==true)
			break;

		if (pTest)
			pBest = pTest;

		if ((*check<=' ' && !bCommandWhiteSpace) || *check=='(' || *check==':')
			break;
	}

	if (!pBest)
		return false;

	pRslv = ResolveEntry(pBest, iWithAnyReturnValue, bCommandWhiteSpace);
	if (!pRslv)
		return false;

	*pdwData = pRslv->GetInternalID();
	*pdwParamMax = pRslv->GetParamMax();
	*pdwLength = pRslv->GetName()->Length();
	if (ppRef)
		*ppRef = pRslv;

	return true;
# endif
#else
	// Default result
	bool bResult=false;

	// Biggest instruction goes
	DWORD dwBiggestInstruction=0;
	DWORD dwBiggestInstructionLength=0;

	// Scan entire instruction database
	char pTry[256];
	CInstructionTableEntry* pEntry = pBaseEntry;
	if(pEntry)
	{
		while(pEntry)
		{
			// Copy next one in to test against
			snprintf(pTry, sizeof(pTry), "%s", pEntry->GetName()->GetStr());

			// Check with current parse item
			DWORD length=strlen(pTry);
			bool bFoundAPossible=false;
			if(_strnicmp(pStringData, pTry, length)==NULL)
			{
				if(bCommandWhiteSpace==false)
				{
					if( pStringData[length]=='(' || pStringData[length]==0 )
					{
						bFoundAPossible=true;
					}
					else
					{
						// Inform user
						if(iType==1)
						{
							// Scan on - look for more
							bool bNothingElse=true;
							LPSTR pPtr = pStringData+length;
							while(*(pPtr)!=0 && *(pPtr)!=10 && *(pPtr)!=':' && bNothingElse==true)
							{
								if(*pPtr>32) bNothingElse=false;
								pPtr++;
							}

							// all user functions now need brackets
							if(bNothingElse==true)
							{
								DWORD StatementLineNumber = g_pStatementList->GetTokenLineNumber();
								g_pErrorReport->SetError(StatementLineNumber, ERR_SYNTAX+41);
							}
						}
					}
				}
				if((bCommandWhiteSpace==true && (pStringData[length]<=32 || pStringData[length]==':')) )
					bFoundAPossible=true;
			}

			// Match, test further
			if(bFoundAPossible==true)
			{
				bool bRetParamOk=false;
				if(iWithAnyReturnValue==0 && pEntry->GetReturnParam()==0) bRetParamOk=true;
				if(iWithAnyReturnValue==1 && pEntry->GetReturnParam()>0) bRetParamOk=true;
				if(iWithAnyReturnValue==0 && pEntry->GetReturnParam()>=11 && pEntry->GetReturnParam()<=19) bRetParamOk=true;
				if(iWithAnyReturnValue==0 && pEntry->GetSpecialArrayParam()) bRetParamOk=true;
				if(bCommandWhiteSpace==false) bRetParamOk=true;
				if(bRetParamOk==true)
				{
					if(length>dwBiggestInstructionLength)
					{
						dwBiggestInstructionLength=length;
						dwBiggestInstruction=(DWORD)pEntry->GetInternalID();
						*pdwData=dwBiggestInstruction;
						*pdwParamMax=pEntry->GetParamMax();
						*pdwLength=length;
						if(ppRef) *ppRef=pEntry;
						bResult=true;
					}
				}
			}
			pEntry=pEntry->GetNext();
		}
	}

	return bResult;
#endif
}

bool CInstructionTable::FindInstruction(bool bCommandWhiteSpace, LPSTR pStringData, int iWithAnyReturnValue, DWORD* pdwData, DWORD* pdwParamMax, DWORD* pdwLength, CInstructionTableEntry** ppRef)
{
#ifdef __AARON_INSTRPERF__
	CInstructionTableEntry *m_pFirstInstructionEntry = nullptr;
#endif
	if (!bCommandWhiteSpace)
		return FindEntryDirect(0, bCommandWhiteSpace, pStringData, iWithAnyReturnValue, pdwData, pdwParamMax, pdwLength, ppRef);

	return FindEntry(0, bCommandWhiteSpace, m_pFirstInstructionEntry, pStringData, iWithAnyReturnValue, pdwData, pdwParamMax, pdwLength, ppRef);
}

bool CInstructionTable::FindUserFunction(LPSTR pStringData, int iWithAnyReturnValue, DWORD* pdwData, DWORD* pdwParamMax, DWORD* pdwLength)
{
#ifdef __AARON_INSTRPERF__
	CInstructionTableEntry *m_pFirstUserFunctionEntry = nullptr;
#endif
	return FindEntry(1, false, m_pFirstUserFunctionEntry, pStringData, iWithAnyReturnValue, pdwData, pdwParamMax, pdwLength, NULL);
}

bool CInstructionTable::FindInstructionParams(DWORD dwInstructionValue, DWORD dwParamMax, DWORD* pdwData, DWORD* pdwParamMax, CStr** pValidParamTypes, CInstructionTableEntry** pRefEntry)
{
#ifdef __AARON_INSTRPERF__
	if (static_cast<db3::uint>(dwInstructionValue) >= m_EntryArray.Size())
		return false;

	auto primary = m_EntryArray[dwInstructionValue];
	if (!primary)
		return false;

	*pdwData = primary->GetInternalID();
	*pdwParamMax = primary->GetParamMax();
	*pValidParamTypes = primary->GetParamTypes();
	*pRefEntry = primary;

	return true;
#else
	CInstructionTableEntry* pEntry = m_pFirstInstructionEntry;
	if(pEntry)
	{
		while(pEntry)
		{
			if(pEntry->GetInternalID()==dwInstructionValue)
			{
				// Store first instruction of this name (sorted)
				CInstructionTableEntry* pPrimary = pEntry;

				// Found Instruction - now find params that match
				while(stricmp(pPrimary->GetName()->GetStr(),pEntry->GetName()->GetStr())==NULL)
				{
					// Match Param to exit with valid instruction
					if(pPrimary->GetParamMax()==pEntry->GetParamMax())
					{
						*pdwData=pEntry->GetInternalID();
						*pdwParamMax=pEntry->GetParamMax();
						*pValidParamTypes=pEntry->GetParamTypes();
						*pRefEntry=pEntry;
						return true;
					}
					pEntry=pEntry->GetNext();
				}
			}
			pEntry=pEntry->GetNext();
		}
	}

	// Could not match param soft fail
	return false;
#endif
}

bool CInstructionTable::FindUserFunctionParams(DWORD dwInstructionValue, DWORD dwParamMax, DWORD* pdwData, DWORD* pdwParamMax, CStr** pValidParamTypes, CInstructionTableEntry** pRefEntry)
{
#ifdef __AARON_INSTRPERF__
	if (static_cast<db3::uint>(dwInstructionValue) >= m_EntryArray.Size())
		return false;

	auto primary = m_EntryArray[dwInstructionValue];
	if (!primary)
		return false;

	*pdwData = primary->GetInternalID();
	*pdwParamMax = primary->GetParamMax();
	*pValidParamTypes = primary->GetParamTypes();
	*pRefEntry = primary;

	return true;
#else
	CInstructionTableEntry* pEntry = m_pFirstUserFunctionEntry;
	if(pEntry)
	{
		while(pEntry)
		{
			if(pEntry->GetInternalID()==dwInstructionValue)
			{
				// Store first instruction of this name (sorted)
				CInstructionTableEntry* pPrimary = pEntry;

				// Found Instruction - now find params that match
				while(stricmp(pPrimary->GetName()->GetStr(),pEntry->GetName()->GetStr())==NULL)
				{
					// Match Param to exit with valid instruction
					if(pPrimary->GetParamMax()==pEntry->GetParamMax())
					{
						*pdwData=pEntry->GetInternalID();
						*pdwParamMax=pEntry->GetParamMax();
						*pValidParamTypes=pEntry->GetParamTypes();
						*pRefEntry=pEntry;
						return true;
					}
					pEntry=pEntry->GetNext();
				}
			}
			pEntry=pEntry->GetNext();
		}
	}

	// Could not match param soft fail
	return false;
#endif
}

bool CInstructionTable::CompareInstructionNames(CInstructionTableEntry* pRefEntryA, CInstructionTableEntry* pRefEntryB)
{
	// Compare Entry names, success if match
	if(stricmp(pRefEntryA->GetName()->GetStr(), pRefEntryB->GetName()->GetStr())==NULL)
		return true;

	// Could not match names soft fail
	return false;
}

CInstructionTableEntry* CInstructionTable::FindUserFunction(LPSTR pUserFunctionName)
{
#ifdef __AARON_INSTRPERF__
	auto entry = m_UserFunctionMap.Find(pUserFunctionName);
	if (!entry)
		return nullptr;

	return entry->P;
#else
	// Search for user function
	CInstructionTableEntry* pEntry = m_pFirstUserFunctionEntry;
	while(pEntry)
	{
		if(stricmp(pEntry->GetName()->GetStr(), pUserFunctionName)==NULL)
			return pEntry;

		pEntry=pEntry->GetNext();
	}

	// Could not find
	return NULL;
#endif
}

CInstructionTableEntry* CInstructionTable::FindReservedFunction(LPSTR pFunctionName)
{
#ifdef __AARON_INSTRPERF__
	auto entry = m_InstructionMap.Find(pFunctionName);
	if (!entry)
		return nullptr;

	return entry->P;
#else
	// Search for reserved function
	CInstructionTableEntry* pEntry = m_pFirstInstructionEntry;
	while(pEntry)
	{
		if(stricmp(pEntry->GetName()->GetStr(), pFunctionName)==NULL)
			return pEntry;

		pEntry=pEntry->GetNext();
	}

	// Could not find
	return NULL;
#endif
}

bool CInstructionTable::FindInstructionWithNameAndParams(LPSTR pName, LPSTR pParams)
{
#ifdef __AARON_INSTRPERF__
	auto entry = m_InstructionMap.Find(pName);
	if (!entry)
		return nullptr;

	auto item = entry->P;

	while(item)
	{
		if (stricmp(item->GetParamTypes()->GetStr(), pParams)==NULL)
			return true;

		item = item->GetNext();
	}

	return false;
#else
	// Search for entry with matching name and params
	CInstructionTableEntry* pEntry = m_pFirstInstructionEntry;
	while(pEntry)
	{
		if(stricmp(pEntry->GetName()->GetStr(), pName)==NULL
		&& stricmp(pEntry->GetParamTypes()->GetStr(), pParams)==NULL)
		{
			// Found same instruction
			return true;
		}

		pEntry=pEntry->GetNext();
	}

	// Could not fund soft fail
	return false;
#endif
}

CInstructionTableEntry* CInstructionTable::FindLastFriendOfName(LPSTR pFriendName)
{
#ifdef __AARON_INSTRPERF__
	auto entry = m_InstructionMap.Find(pFriendName);
	if (!entry)
		return nullptr;

	auto item = entry->P;
	if (!item)
		return nullptr;

	while(item->GetNext())
		item = item->GetNext();

	return item;
#else
	// Latest match
	CInstructionTableEntry* pMatch = NULL;

	// Search for last entry with the friend name
	CInstructionTableEntry* pEntry = m_pFirstInstructionEntry;
	while(pEntry)
	{
		if(stricmp(pEntry->GetName()->GetStr(), pFriendName)==NULL) pMatch=pEntry;
		pEntry=pEntry->GetNext();
	}

	// Pass last match if any
	return pMatch;
#endif
}

// DLL Scanning and Database Building

std::unique_ptr<char[]> CInstructionTable::ReadRawStringTable ( LPSTR pFilenameEXE, DWORD* pdwDataSize )
{
	// raw string table data retutned
	std::unique_ptr<char[]> pReturnData;

	// Simply scans the EXE and locates the pattern in the data, and replaces it
	DWORD dwSizeOfEXECode = 0;	
	DWORD dwOverallDataSize = 0;
	HMODULE hEXE = LoadLibraryExW(TextConvert::UTF8ToUTF16(pFilenameEXE).c_str(), NULL, LOAD_LIBRARY_AS_DATAFILE);
	if ( hEXE )
	{
		// look for string table data in packed data format
		HRSRC hRes = NULL;
		int iFirstStringIndexFound = -1;
		for ( int iIndex=0; iIndex<255; iIndex++ )
		{
			/* old system did not account for gaps in the string table sequence (styx)
			hRes = FindResource(hEXE, (LPCTSTR)iIndex, RT_STRING);
			if ( hRes )
			{
				dwOverallDataSize += SizeofResource(hEXE, hRes);
				if ( iFirstStringIndexFound==-1 )
					iFirstStringIndexFound = iIndex;
			}
			if ( iFirstStringIndexFound!=-1 && hRes==NULL )
				break;
			*/
			hRes = FindResource(hEXE, (LPCTSTR)iIndex, RT_STRING);
			if ( hRes )
			{
				DWORD dwSizeOStringData = SizeofResource(hEXE, hRes);
				if ( dwSizeOStringData>0 )
				{
					dwOverallDataSize += dwSizeOStringData;
					if ( iFirstStringIndexFound==-1 )
						iFirstStringIndexFound = iIndex;
				}
			}
		}
		if ( iFirstStringIndexFound!=-1 )
		{
			// get first data ptr again
			hRes = FindResource(hEXE, (LPCTSTR)iFirstStringIndexFound, RT_STRING);
			HGLOBAL hGlobal = LoadResource(hEXE, hRes);
			LPVOID lpResReal = LockResource(hGlobal);

			// get string table data
			pReturnData = std::make_unique<char[]>(dwOverallDataSize);
			memcpy(pReturnData.get(), (LPSTR)lpResReal, dwOverallDataSize);
		}

		// free usages
		if ( hEXE ) FreeLibrary ( hEXE );
	}

	// erase again if first character is NOT a space(32)
	if ( pReturnData )
		if ( pReturnData [ 0 ]!=32 )
			pReturnData.reset();

	// return data size
	if ( pdwDataSize ) *pdwDataSize = dwOverallDataSize;

	// return string
	return pReturnData;
}

bool CInstructionTable::VerifyCertificateForPlugin ( LPSTR pDLLName, LPSTR pProductCode )
{
	// Certificate system removed - open-source project, all plugins allowed
	return true;
}
//#define __AARON_DEBUG__ 1
#define __AARON_COMMANDS__ 1

#ifdef __AARON_DEBUG__
static void PrintToFile(const char *text, ...)
{
	va_list args;
	FILE *f;

	f = fopen("D:\\Log\\Commands.txt", "a+");
	if (!f)
		return;

	va_start(args, text);
	vfprintf(f, text, args);
	va_end(args);
	fprintf(f, "\n");
	fclose(f);
}
#else
# define PrintToFile(text,...)
#endif
bool CInstructionTable::LoadCommandsFromDLL(LPSTR pCategory, LPSTR pFilename)
{
#ifdef __AARON_COMMANDS__
	// Added for simplicity
	static char libname[512];
	FILE *f;

	sprintf_s(libname, "%s.commands", pFilename);

	f = fopen(libname, "r");
	if (f != nullptr)
	{
		static char tempstr[4096];

		while(fgets(tempstr, sizeof(tempstr), f) != nullptr)
		{
			char *p;

			p = strchr(tempstr, '\n');
			if (p)
				*p = '\0';

			p = &tempstr[0];
			while(*p<=' ' && *p!='\0')
				p++;

			if (*p=='\0')
				break;
				
			PrintToFile( "%s:\"%s\"", libname, p );
			if(TurnStringIntoCommand(pCategory, pFilename, p)==false)
			{
				char err[512];
				snprintf(err, sizeof(err), "Command in '%s' command-table unrecognised (%s)", libname, p);
				g_pErrorReport->AddErrorString(err);
				fclose(f);
				return false;
			}
		}

		fclose(f);
		return true;
	}
#endif

	// First check if DLL is PROTECTED or not
	DWORD dwDataSize = 0;
	HMODULE hModule = NULL;
	DWORD dwCountProtectedCommands=0;
	std::unique_ptr<char[]> pProtectedData = ReadRawStringTable ( pFilename, &dwDataSize );
	if ( pProtectedData )
	{
		// if a protected plugin, obtain string data in sequential stream
		LPSTR pPtr=pProtectedData.get();
		LPSTR pPtrLineStart=pProtectedData.get();
		LPSTR pPtrEnd=pProtectedData.get()+dwDataSize;
		while ( pPtr<pPtrEnd )
		{
			// find end of line
			if ( (unsigned char)*(pPtr)==0 )
			{
				// get line
				char pTempStr [ 300 ];
				strcpy ( pTempStr, pPtrLineStart );
				pPtrLineStart=pPtr+1;

				// first entry is protected plugin header
				if ( dwCountProtectedCommands == 0 )
				{
					// protected plugin must be verified before we include commands
					LPSTR pProductCode = pTempStr;
					if ( VerifyCertificateForPlugin ( pFilename, pProductCode )==false )
					{
						// failed to verify - simply ignore plugin silently
						char pDirINI [ _MAX_PATH ];
						getcwd ( pDirINI, _MAX_PATH);
						strcat ( pDirINI, "\\report.ini");
						
						// make silent INI report
						WritePrivateProfileString("VERIFICATION REPORT", pFilename, "NOT VALID", pDirINI);

						// free usages (pProtectedData released automatically)
						return true;
					}

					// and advance to first character of data (string table starts)
					while ( (unsigned char)*(pPtr+0)==0 && pPtr+2<pPtrEnd )
						pPtr++;

					// add to count
					dwCountProtectedCommands=1;
					pPtrLineStart=pPtr;
				}
				else
				{
					// add each as found
					PrintToFile( "[protected]%s:\"%s\"", pFilename, pTempStr );
					if(TurnStringIntoCommand(pCategory, pFilename, pTempStr)==false)
					{
						char err[512];
						snprintf(err, sizeof(err), "Command in '%s' command-table unrecognised (%s)", pFilename, pTempStr);
						g_pErrorReport->AddErrorString(err);
						return false;
					}

					// add to count
					dwCountProtectedCommands++;

					// two nulls or more, advance to end or next string char
					if ( (unsigned char)*(pPtr+1)==0 )
					{
						// and advance to first character of data (string table starts)
						while ( (unsigned char)*(pPtr+0)==0 && pPtr+2<pPtrEnd )
							pPtr++;

						// continue or end
						if ( pPtr+2<pPtrEnd )
							pPtrLineStart=pPtr;
						else
							break;
					}
				}
			}

			// next character
			pPtr++;
		}

		// Free usages (pProtectedData released automatically)
	}
	else
	{
		// if not a protected plugin, open for direct string reading
		hModule = LoadLibraryExW ( TextConvert::UTF8ToUTF16(pFilename).c_str(), NULL, LOAD_LIBRARY_AS_DATAFILE );
		if ( hModule  )
		{
			// Load DLL Decorated Names via unprotected resource
			DWORD dwTry=0;
			DWORD dwMax=0;
			DWORD dwLength = 255;
			char pTempStr[256];
			while(dwTry<1000)
			{
				int iStrQty = LoadStringA(hModule, 1+dwTry, pTempStr, 2);
				if(iStrQty!=0) dwMax=1+dwTry;
				dwTry++;
			}

			// For each string..
			for(DWORD n=0; n<dwMax; n++)
			{
				// Get String
				int iChar = LoadStringA(hModule, 1+n, pTempStr, dwLength);

				// Parse it for info
				if(iChar>0)
				{
					PrintToFile( "%s:\"%s\"", pFilename, pTempStr );
					if(TurnStringIntoCommand(pCategory, pFilename, pTempStr)==false)
					{
						char err[512];
						snprintf(err, sizeof(err), "Command in '%s' command-table unrecognised (%s)", pFilename, pTempStr);
						g_pErrorReport->AddErrorString(err);
						FreeLibrary(hModule);
						return false;
					}
				}
			}
		}

		// free module
		if ( hModule ) FreeLibrary ( hModule );
	}

	// Complete
	return true;
}

bool CInstructionTable::TurnStringIntoCommand(LPSTR pCategory, LPSTR pDLLName, LPSTR pRawCommandString)
{
	// Parse Sections of string
	CStr strStorage(pRawCommandString);
	CStr* pStr = &strStorage;
	DWORD dwParamPos = pStr->FindFirstChar('%');
	DWORD dwDecPos;
	{
		CStr strPart2(pRawCommandString+dwParamPos+1);
		dwDecPos = strPart2.FindFirstChar('%') + dwParamPos + 2;
	}

	// Ensure we have TWO of them
	if(dwParamPos>0)
	{
		CStr checkForTwo(pStr->GetStr()+dwParamPos+1);
		DWORD dwSecondPos = checkForTwo.FindFirstChar('%');
		if(dwSecondPos==0)
		{
			return false;
		}
	}
	else
	{
		return false;
	}

	// Validate String Parts
	if(dwParamPos>0 && dwDecPos>0 && dwDecPos>dwParamPos)
	{
		// Command Name Part
		CStr nameStorage(pRawCommandString);
		CStr* pName = &nameStorage;
		pName->SetChar(dwParamPos, 0);

		// Command Params Part
		CStr paramStorage(pRawCommandString+dwParamPos+1);
		CStr* pParam = &paramStorage;
		pParam->SetChar(dwDecPos-dwParamPos-2, 0);
		unsigned char cFirstParamChar=0;
		if(pName->GetChar(pName->Length()-1)=='[')
		{
			// Command Is Expression
			pName->SetChar(pName->Length()-1,0);
			cFirstParamChar = pParam->GetChar(0);
			pParam->SetText(pParam->GetStr()+1);
		}

		// If * character inside param description
		DWORD dwStarPos = 0;
		if(pParam->Length()>0)
		{
			// param to its left is a return variable
			dwStarPos = pParam->FindFirstChar('*');
			if(dwStarPos>0)
			{
				cFirstParamChar = pParam->GetChar(dwStarPos-1);
			}
		}

		bool bPassArrayPtrAsInput=false;
		DWORD dwReturnParam=0;
		if(cFirstParamChar>0)
		{
			switch(cFirstParamChar)
			{
				case 'L' : dwReturnParam=1;	break;
				case 'F' : dwReturnParam=2;	break;
				case 'S' : dwReturnParam=3;	break;
				case 'B' : dwReturnParam=4;	break;
				case 'Y' : dwReturnParam=5;	break;
				case 'W' : dwReturnParam=6;	break;
				case 'D' : dwReturnParam=7;	break;
				case 'O' : dwReturnParam=8;	break;
				case 'R' : dwReturnParam=9;	break;
				case 'H' : dwReturnParam=7; bPassArrayPtrAsInput=true; break;
				case 'X' : dwReturnParam=501;break;//any-type (no casting)
				default: dwReturnParam=1;	break;
			}
		}

		// If param is zero, no params required
		if(pParam->CheckChar(0,'0')) pParam->SetText("");

		// If star part of param string, remove it and make return-var a ptr..
		if(dwStarPos>0)
		{
			// Shuffle to overwrite * character
			for(DWORD n=dwStarPos; n<pParam->Length(); n++)
				pParam->SetChar(n, pParam->GetChar(n+1));

			if(bPassArrayPtrAsInput)
			{
				// Keep pass in and return out as DWORDs for correct array-ptr handling
//				pParam->SetChar(0, 'D'); //LEEFIX-300902-Keep Array Handling on the H
				dwReturnParam=7;
			}
			else
			{
				dwReturnParam=dwReturnParam+10;
			}
		}

		// Decorated Name Part
		CStr decoratedStorage(pRawCommandString+dwDecPos);
		CStr* pDecorated = &decoratedStorage;

		// Get Param Desc string out of decorated name (ownership passes to AddCommandCore2)
		std::unique_ptr<CStr> pParamDesc = std::make_unique<CStr>();
		pParamDesc->SetText(pDecorated->GetStr());
		DWORD dwPDPos = pDecorated->FindFirstChar('%');
		if(dwPDPos>0)
		{
			pDecorated->SetChar(dwPDPos,0);
			pParamDesc->SetText(pDecorated->GetStr()+dwPDPos+1);
		}
		else
			pParamDesc->SetText("");

		// released locally
		CStr paramDescForHelpStorage("");
		CStr* pParamDescForHelp = &paramDescForHelpStorage;
		pParamDescForHelp->SetText(pParamDesc->GetStr());

		// Add Command To Database
		if(strcmp(pDecorated->GetStr(), "??")!=NULL)
		{
			LPSTR lpConflictingDLLName = NULL;
			if(AddCommandCore2(pName->GetStr(), pDLLName, pDecorated->GetStr(), pParam->GetStr(), dwReturnParam, pParam->Length(), 0, 0, dwStarPos, bPassArrayPtrAsInput, pParamDesc.release(), &lpConflictingDLLName)==false)
			{
				// Could not add command as it was identically duplicated!
				char err[512];
//				snprintf(err, sizeof(err), "Command in DLL command-table duplicated (%s:%s)", pDLLName, pName->GetStr());
				snprintf(err, sizeof(err), "Duplicate %s in %s and %s!", pName->GetStr(), pDLLName, lpConflictingDLLName);
				g_pErrorReport->AddErrorString(err);
				SAFE_DELETE(lpConflictingDLLName);
				return false;
			}
		}

		// If active, add Command To HelpText Folder
		if(g_pDBPCompiler && g_pDBPCompiler->GetGenerateHelpTxtMode())
		{
			if(dwStarPos>0) dwReturnParam=0; //so visible sign of return value in help!
			AddCommandToHelpTxt(pCategory, pName->GetStr(), pParam->GetStr(), dwReturnParam, pParam->Length(), pParamDescForHelp->GetStr());
		}
	}
	else
	{
		return false;
	}

	// Complete
	return true;
}

void CInstructionTable::AddCommandToHelpTxt(LPSTR pCategory, LPSTR pCommandName, LPSTR pParamStr, DWORD dwReturnParam, DWORD dwParamCount, LPSTR pParamDesc)
{
	// Usage strings
	char lpFilename[_MAX_PATH];
	char lpSubMenuFilename[_MAX_PATH];
	char lpMenuFilename[_MAX_PATH];
	char lpTXTTitle[_MAX_PATH];
	char lpTXTSyntax[_MAX_PATH];
	std::string sTXTThisSyntax;
	char lpTXTCommand[_MAX_PATH];
	char lpTXTThisCommand[_MAX_PATH];

	// Make folder and enter it
	mkdir("helptxt");
	chdir("helptxt");
	char lpIndexFilename[_MAX_PATH];
	_getcwd(lpIndexFilename, _MAX_PATH);
	snprintf(lpIndexFilename, sizeof(lpIndexFilename), "%s\\index.txt", lpIndexFilename);
	mkdir("commands");
	chdir("commands");
	mkdir(pCategory);
	chdir(pCategory);

	// PREPARE HELP PAGE CONTENTS TXT

	// Filename
	snprintf(lpFilename, sizeof(lpFilename), "..\\%s\\%s.txt", pCategory, pCommandName);

	// Title of Command Page
	snprintf(lpTXTTitle, sizeof(lpTXTTitle), "%s", pCommandName);

	// Check if a trouble command
	sTXTThisSyntax.clear();
	if(CheckTroubleCommandSyntax(sTXTThisSyntax, pCommandName))
	{
		// Syntax Of Standard Command
		if(dwReturnParam>0 && dwReturnParam<=10)
		{
			if(dwReturnParam>0 && pParamStr[0]!='*')
			{
				sTXTThisSyntax += "Return ";
				switch(dwReturnParam)
				{
					case 1 : sTXTThisSyntax += "Integer";			break;
					case 2 : sTXTThisSyntax += "Float";				break;
					case 3 : sTXTThisSyntax += "String";				break;
					case 4 : sTXTThisSyntax += "Flag";				break;
					case 5 : sTXTThisSyntax += "BYTE";				break;
					case 6 : sTXTThisSyntax += "WORD";				break;
					case 7 : sTXTThisSyntax += "DWORD";				break;
					case 8 : sTXTThisSyntax += "Double Float";		break;
					case 9 : sTXTThisSyntax += "Double Integer";		break;
				}
				sTXTThisSyntax += "=";
			}
			sTXTThisSyntax += pCommandName;
			if(dwReturnParam>0)
				sTXTThisSyntax += "(";
			else
				sTXTThisSyntax += " ";
		}
		else
		{
			// No return value command
			sTXTThisSyntax += pCommandName;
			sTXTThisSyntax += " ";
		}

		if(dwParamCount>0)
		{
			if(strcmp(pParamDesc, "")==NULL)
			{
				for(DWORD p=0; p<=dwParamCount; p++)
				{
					if(pParamStr[p]>0)
					{
						switch(pParamStr[p])
						{
							case 'L' : sTXTThisSyntax += "Integer";			break;
							case 'F' : sTXTThisSyntax += "Float";			break;
							case 'S' : sTXTThisSyntax += "String";			break;
							case 'B' : sTXTThisSyntax += "Flag";				break;
							case 'Y' : sTXTThisSyntax += "BYTE";				break;
							case 'W' : sTXTThisSyntax += "WORD";				break;
							case 'D' : sTXTThisSyntax += "DWORD";			break;
							case 'O' : sTXTThisSyntax += "Double Float";		break;
							case 'R' : sTXTThisSyntax += "Double Integer";	break;
						}
						sTXTThisSyntax += " Value";
						if(p<dwParamCount-1) sTXTThisSyntax += ", ";
					}
				}
			}
			else
			{
				// Param Desc stored in DLL
				sTXTThisSyntax += pParamDesc;
			}
		}
		if(dwReturnParam>0) sTXTThisSyntax += ")";
	}

	// WRITE HELP TXT FILE

	// Create help txt file for command
	WritePrivateProfileString("COMMAND", "TITLE", lpTXTTitle, lpFilename);

	// Add Unique Command To Unique Syntax Line
	DWORD dwField=1;
	char lpField[256];
	while(dwField<998)
	{
		snprintf(lpField, sizeof(lpField), "SYNTAX%d", dwField);
		GetPrivateProfileString("COMMAND", lpField, "", lpTXTSyntax, 256, lpFilename);
		if(strcmp(lpTXTSyntax,"")==NULL)
		{
			// Not there, so add it
			WritePrivateProfileString("COMMAND", lpField, sTXTThisSyntax.c_str(), lpFilename);
			break;
		}
		if(strcmp(lpTXTSyntax,sTXTThisSyntax.c_str())==NULL)
		{
			// Already got it
			break;
		}
		dwField++;
	}

	// Add parameter types to command-data
	snprintf(lpField, sizeof(lpField), "%s", "PARAM");
	char lpThisParamStr [ 256 ];
	GetPrivateProfileString("COMMAND", lpField, "", lpThisParamStr, 256, lpFilename);
	std::string sNewParamStr;
	if ( dwReturnParam > 0 ) sNewParamStr += "(";
	switch(dwReturnParam)
	{
		case 1 : sNewParamStr += "L";		break;
		case 2 : sNewParamStr += "F";		break;
		case 3 : sNewParamStr += "S";		break;
		case 4 : sNewParamStr += "B";		break;
		case 5 : sNewParamStr += "Y";		break;
		case 6 : sNewParamStr += "W";		break;
		case 7 : sNewParamStr += "D";		break;
		case 8 : sNewParamStr += "O";		break;
		case 9 : sNewParamStr += "R";		break;
	}
	if ( pParamStr ) sNewParamStr += pParamStr;
	if ( sNewParamStr.length() > strlen(lpThisParamStr) )
		WritePrivateProfileString("COMMAND", lpField, sNewParamStr.c_str(), lpFilename);

	// Add Unique Command To The IndexTXT (for the index.htm)
	DWORD dwIndexField=1;
	char lpIndexField[_MAX_PATH];
	char lpIndexFieldData[_MAX_PATH];
	char lpFilenameToCommand[_MAX_PATH];
	snprintf(lpFilenameToCommand, sizeof(lpFilenameToCommand), "commands\\%s\\%s.htm", pCategory, pCommandName);
	while(1)
	{
		snprintf(lpIndexField, sizeof(lpIndexField), "ENTRY%d", dwIndexField);
		GetPrivateProfileString("INDEX", lpIndexField, "", lpIndexFieldData, _MAX_PATH, lpIndexFilename);
		if(strcmp(lpIndexFieldData,"")==NULL)
		{
			WritePrivateProfileString("INDEX", lpIndexField, pCommandName, lpIndexFilename);
			snprintf(lpIndexField, sizeof(lpIndexField), "FILE%d", dwIndexField);
			WritePrivateProfileString("INDEX", lpIndexField, lpFilenameToCommand, lpIndexFilename);
			break;
		}
		if(strcmp(lpIndexFieldData, pCommandName)==NULL)
		{
			// Already got it
			break;
		}
		dwIndexField++;
	}

	// Leave category folder
	chdir("..");

	// Update category sub-menu file
	dwField=1;
	bool bAlreadyExists=false;
	snprintf(lpTXTThisCommand, sizeof(lpTXTThisCommand), "%s", pCommandName);
	snprintf(lpSubMenuFilename, sizeof(lpSubMenuFilename), "..\\commands\\%s.txt", pCategory);
	while(dwField<2000)
	{
		snprintf(lpField, sizeof(lpField), "MENU%d", dwField);
		GetPrivateProfileString("SUBMENU", lpField, "", lpTXTCommand, 256, lpSubMenuFilename);
		if(strcmp(lpTXTCommand,lpTXTThisCommand)==NULL)
		{
			// Already got it
			bAlreadyExists=true;
			break;
		}
		if(lpTXTSyntax[0]=='@')
		{
			// End of list
			dwField++;
			break;
		}
		dwField++;
	}
	if(bAlreadyExists==false)
	{
		// New commands temporarily between 2000-3000 for reordering manually.
		while(dwField<3000)
		{
			char pAnything[256];
			snprintf(lpField, sizeof(lpField), "MENU%d", dwField);
			GetPrivateProfileString("SUBMENU", lpField, "", pAnything, 256, lpSubMenuFilename);
			if(strcmp(pAnything,"")==NULL)
			{
				// Empty - Can put it here
				snprintf(lpField, sizeof(lpField), "MENU%d", dwField);
				WritePrivateProfileString("SUBMENU", lpField, lpTXTThisCommand, lpSubMenuFilename);
				break;
			}
			dwField++;
		}
	}

	// Leave commands folder
	chdir("..");

	// Update Commands Menu
	dwField=1;
	snprintf(lpTXTThisCommand, sizeof(lpTXTThisCommand), "%s", pCategory);
	snprintf(lpMenuFilename, sizeof(lpMenuFilename), "..\\helptxt\\commands.txt");
	while(1)
	{
		snprintf(lpField, sizeof(lpField), "MENU%d", dwField);
		GetPrivateProfileString("COMMANDSMENU", lpField, "", lpTXTCommand, 256, lpMenuFilename);
		if(strcmp(lpTXTCommand,"")==NULL)
		{
			WritePrivateProfileString("COMMANDSMENU", lpField, lpTXTThisCommand, lpMenuFilename);
			break;
		}
		if(strcmp(lpTXTCommand,lpTXTThisCommand)==NULL)
		{
			// Already got it
			break;
		}
		dwField++;
	}

	// Leave helptxt folder
	chdir("..");
}

bool CInstructionTable::CheckTroubleCommandSyntax(std::string& sTXTThisSyntax, LPSTR pCommandName)
{
	bool bNotAnyTrouble=true;

	if(stricmp(pCommandName,"print")==NULL)
	{
		sTXTThisSyntax = "PRINT Print Statements";
		bNotAnyTrouble=false;
	}
	if(stricmp(pCommandName,"input")==NULL)
	{
		sTXTThisSyntax = "INPUT Print Statements, Input Variable";
		bNotAnyTrouble=false;
	}

	return bNotAnyTrouble;
}

DWORD CInstructionTable::DetermineInternalCommandCode(DWORD dwMathSymbol, DWORD dwTypeValue)
{
	switch(dwTypeValue)
	{
		case 1 : // INTEGER
		case 101 : // INTEGER POINTER
		switch(dwMathSymbol)
		{
			case 1 :	return static_cast<DWORD>(InternalInstruction::PowerLLL);
			case 2 :	return static_cast<DWORD>(InternalInstruction::MulLLL);
			case 3 :	return static_cast<DWORD>(InternalInstruction::DivLLL);
			case 4 :	return static_cast<DWORD>(InternalInstruction::AddLLL);
			case 5 :	return static_cast<DWORD>(InternalInstruction::SubLLL);
			case 6 :	return static_cast<DWORD>(InternalInstruction::ModLLL);
			case 27 :	return static_cast<DWORD>(InternalInstruction::EqualLLL);
			case 25 :	return static_cast<DWORD>(InternalInstruction::GreaterLLL);
			case 26 :	return static_cast<DWORD>(InternalInstruction::LessLLL);
			case 22 :	return static_cast<DWORD>(InternalInstruction::NotEqualLLL);
			case 23 :	return static_cast<DWORD>(InternalInstruction::GreaterEqualLLL);
			case 24 :	return static_cast<DWORD>(InternalInstruction::LessEqualLLL);
		}
		break;

		case 2 : // FLOAT
		case 102 : // FLOAT POINTER
		switch(dwMathSymbol)
		{
			case 1 :	return static_cast<DWORD>(InternalInstruction::PowerFFF);
			case 2 :	return static_cast<DWORD>(InternalInstruction::MulFFF);
			case 3 :	return static_cast<DWORD>(InternalInstruction::DivFFF);
			case 4 :	return static_cast<DWORD>(InternalInstruction::AddFFF);
			case 5 :	return static_cast<DWORD>(InternalInstruction::SubFFF);
			case 6 :	return static_cast<DWORD>(InternalInstruction::ModFFF);
			case 27 :	return static_cast<DWORD>(InternalInstruction::EqualFF);
			case 25 :	return static_cast<DWORD>(InternalInstruction::GreaterLFF);
			case 26 :	return static_cast<DWORD>(InternalInstruction::LessLFF);
			case 22 :	return static_cast<DWORD>(InternalInstruction::NotEqualLFF);
			case 23 :	return static_cast<DWORD>(InternalInstruction::GreaterEqualLFF);
			case 24 :	return static_cast<DWORD>(InternalInstruction::LessEqualLFF);
		}
		break;

		case 3 : // STRING
		case 103 : // STRING POINTER
		switch(dwMathSymbol)
		{
//			case 1 :	return static_cast<DWORD>(InternalInstruction::PowerSSS);
//			case 2 :	return static_cast<DWORD>(InternalInstruction::MulSSS);
//			case 3 :	return static_cast<DWORD>(InternalInstruction::DivSSS);
//			case 5 :	return static_cast<DWORD>(InternalInstruction::SubSSS);
			case 4 :	return static_cast<DWORD>(InternalInstruction::AddSSS);
			case 27 :	return static_cast<DWORD>(InternalInstruction::EqualSS);
			case 25 :	return static_cast<DWORD>(InternalInstruction::GreaterLSS);
			case 26 :	return static_cast<DWORD>(InternalInstruction::LessLSS);
			case 22 :	return static_cast<DWORD>(InternalInstruction::NotEqualLSS);
			case 23 :	return static_cast<DWORD>(InternalInstruction::GreaterEqualLSS);
			case 24 :	return static_cast<DWORD>(InternalInstruction::LessEqualSS);
		}
		break;

		case 4 : // BOOL
		case 104 : // BOOL POINTER
		switch(dwMathSymbol)
		{
			case 1 :	return static_cast<DWORD>(InternalInstruction::PowerBBB);
			case 2 :	return static_cast<DWORD>(InternalInstruction::MulBBB);
			case 3 :	return static_cast<DWORD>(InternalInstruction::DivBBB);
			case 4 :	return static_cast<DWORD>(InternalInstruction::AddBBB);
			case 5 :	return static_cast<DWORD>(InternalInstruction::SubBBB);
			case 6 :	return static_cast<DWORD>(InternalInstruction::ModBBB);
			case 27 :	return static_cast<DWORD>(InternalInstruction::EqualLLL);
			case 25 :	return static_cast<DWORD>(InternalInstruction::GreaterLLL);
			case 26 :	return static_cast<DWORD>(InternalInstruction::LessLLL);
			case 22 :	return static_cast<DWORD>(InternalInstruction::NotEqualLLL);
			case 23 :	return static_cast<DWORD>(InternalInstruction::GreaterEqualLLL);
			case 24 :	return static_cast<DWORD>(InternalInstruction::LessEqualLLL);
		}
		break;

		case 5 : // BYTE
		case 105 : // BYTE POINTER
		switch(dwMathSymbol)
		{
			case 1 :	return static_cast<DWORD>(InternalInstruction::PowerYYY);
			case 2 :	return static_cast<DWORD>(InternalInstruction::MulYYY);
			case 3 :	return static_cast<DWORD>(InternalInstruction::DivYYY);
			case 4 :	return static_cast<DWORD>(InternalInstruction::AddYYY);
			case 5 :	return static_cast<DWORD>(InternalInstruction::SubYYY);
			case 6 :	return static_cast<DWORD>(InternalInstruction::ModYYY);
			case 27 :	return static_cast<DWORD>(InternalInstruction::EqualLLL);
			case 25 :	return static_cast<DWORD>(InternalInstruction::GreaterLLL);
			case 26 :	return static_cast<DWORD>(InternalInstruction::LessLLL);
			case 22 :	return static_cast<DWORD>(InternalInstruction::NotEqualLLL);
			case 23 :	return static_cast<DWORD>(InternalInstruction::GreaterEqualLLL);
			case 24 :	return static_cast<DWORD>(InternalInstruction::LessEqualLLL);
		}
		break;

		case 6 : // WORD
		case 106 : // WORD POINTER
		switch(dwMathSymbol)
		{
			case 1 :	return static_cast<DWORD>(InternalInstruction::PowerWWW);
			case 2 :	return static_cast<DWORD>(InternalInstruction::MulWWW);
			case 3 :	return static_cast<DWORD>(InternalInstruction::DivWWW);
			case 4 :	return static_cast<DWORD>(InternalInstruction::AddWWW);
			case 5 :	return static_cast<DWORD>(InternalInstruction::SubWWW);
			case 6 :	return static_cast<DWORD>(InternalInstruction::ModWWW);
			case 27 :	return static_cast<DWORD>(InternalInstruction::EqualLLL);
			case 25 :	return static_cast<DWORD>(InternalInstruction::GreaterLLL);
			case 26 :	return static_cast<DWORD>(InternalInstruction::LessLLL);
			case 22 :	return static_cast<DWORD>(InternalInstruction::NotEqualLLL);
			case 23 :	return static_cast<DWORD>(InternalInstruction::GreaterEqualLLL);
			case 24 :	return static_cast<DWORD>(InternalInstruction::LessEqualLLL);
		}
		break;

		case 7 : // DWORD
		case 107 : // DWORD POINTER
		case 1001 : // USERDEFINED STRUCTURE VAR
		case 1101 : // USERDEFINED STRUCTURE PTR
		switch(dwMathSymbol)
		{
			case 1 :	return static_cast<DWORD>(InternalInstruction::PowerDDD);
			case 2 :	return static_cast<DWORD>(InternalInstruction::MulDDD);
			case 3 :	return static_cast<DWORD>(InternalInstruction::DivDDD);
			case 4 :	return static_cast<DWORD>(InternalInstruction::AddDDD);
			case 5 :	return static_cast<DWORD>(InternalInstruction::SubDDD);
			case 6 :	return static_cast<DWORD>(InternalInstruction::ModDDD); // leeadd - 300305 - forgot this?!?
// lee - 240306 - u6b4 - definate differences in INT compare and DWORD compare (ie 0xFF < 0x00 = false)
//			case 27 :	return static_cast<DWORD>(InternalInstruction::EqualLLL);
//			case 25 :	return static_cast<DWORD>(InternalInstruction::GreaterLLL);
//			case 26 :	return static_cast<DWORD>(InternalInstruction::LessLLL);
//			case 22 :	return static_cast<DWORD>(InternalInstruction::NotEqualLLL);
//			case 23 :	return static_cast<DWORD>(InternalInstruction::GreaterEqualLLL);
//			case 24 :	return static_cast<DWORD>(InternalInstruction::LessEqualLLL);
			case 27 :	return static_cast<DWORD>(InternalInstruction::EqualDDD);
			case 25 :	return static_cast<DWORD>(InternalInstruction::GreaterDDD);
			case 26 :	return static_cast<DWORD>(InternalInstruction::LessDDD);
			case 22 :	return static_cast<DWORD>(InternalInstruction::NotEqualDDD);
			case 23 :	return static_cast<DWORD>(InternalInstruction::GreaterEqualDDD);
			case 24 :	return static_cast<DWORD>(InternalInstruction::LessEqualDDD);
		}
		break;

		case 8 : // DOUBLE FLOAT
		case 108 : // DOUBLE FLOAT POINTER
		switch(dwMathSymbol)
		{
			case 1 :	return static_cast<DWORD>(InternalInstruction::PowerOOO);
			case 2 :	return static_cast<DWORD>(InternalInstruction::MulOOO);
			case 3 :	return static_cast<DWORD>(InternalInstruction::DivOOO);
			case 4 :	return static_cast<DWORD>(InternalInstruction::AddOOO);
			case 5 :	return static_cast<DWORD>(InternalInstruction::SubOOO);
			case 27 :	return static_cast<DWORD>(InternalInstruction::EqualLOO);
			case 25 :	return static_cast<DWORD>(InternalInstruction::GreaterLOO);
			case 26 :	return static_cast<DWORD>(InternalInstruction::LessLOO);
			case 22 :	return static_cast<DWORD>(InternalInstruction::NotEqualLOO);
			case 23 :	return static_cast<DWORD>(InternalInstruction::GreaterEqualLOO);
			case 24 :	return static_cast<DWORD>(InternalInstruction::LessEqualOO);
		}
		break;

		case 9 : // DOUBLE INTEGER
		case 109 : // DOUBLE INTEGER POINTER
		switch(dwMathSymbol)
		{
			case 1 :	return static_cast<DWORD>(InternalInstruction::PowerRRR);
			case 2 :	return static_cast<DWORD>(InternalInstruction::MulRRR);
			case 3 :	return static_cast<DWORD>(InternalInstruction::DivRRR);
			case 4 :	return static_cast<DWORD>(InternalInstruction::AddRRR);
			case 5 :	return static_cast<DWORD>(InternalInstruction::SubRRR);
			case 6 :	return static_cast<DWORD>(InternalInstruction::ModRRR);
			case 27 :	return static_cast<DWORD>(InternalInstruction::EqualLRR);
			case 25 :	return static_cast<DWORD>(InternalInstruction::GreaterLRR);
			case 26 :	return static_cast<DWORD>(InternalInstruction::LessLRR);
			case 22 :	return static_cast<DWORD>(InternalInstruction::NotEqualLRR);
			case 23 :	return static_cast<DWORD>(InternalInstruction::GreaterEqualLRR);
			case 24 :	return static_cast<DWORD>(InternalInstruction::LessEqualRR);
		}
		break;
	}

	// Not Type Based
	switch(dwMathSymbol)
	{
		case 11 :	return static_cast<DWORD>(InternalInstruction::ShiftRLLL);
		case 12 :	return static_cast<DWORD>(InternalInstruction::ShiftLLLL);
		case 31 :	return static_cast<DWORD>(InternalInstruction::BitAndLLL);
		case 32 :	return static_cast<DWORD>(InternalInstruction::BitOrLLL);
		case 33 :	return static_cast<DWORD>(InternalInstruction::BitXorLLL);
		case 34 :	return static_cast<DWORD>(InternalInstruction::BitNotLLL);

		case 41 :	return static_cast<DWORD>(InternalInstruction::AndLLL);
		case 42 :	return static_cast<DWORD>(InternalInstruction::OrLLL);
		case 43 :	return static_cast<DWORD>(InternalInstruction::NotLLL);

		case 101 :	return 0;//IT_INTERNAL_CASTLTOL;
		case 102 :	return static_cast<DWORD>(InternalInstruction::CastLToF);
		case 103 :	return 0;//IT_INTERNAL_CASTLTOS;
		case 104 :	return static_cast<DWORD>(InternalInstruction::CastLToB);
		case 105 :	return static_cast<DWORD>(InternalInstruction::CastLToY);
		case 106 :	return static_cast<DWORD>(InternalInstruction::CastLToW);
		case 107 :	return static_cast<DWORD>(InternalInstruction::CastLToD);
		case 108 :	return static_cast<DWORD>(InternalInstruction::CastLToO);
		case 109 :	return static_cast<DWORD>(InternalInstruction::CastLToR);

		case 111 :	return static_cast<DWORD>(InternalInstruction::CastFTOL);
		case 112 :	return 0;//IT_INTERNAL_CASTFTOF;
		case 114 :	return static_cast<DWORD>(InternalInstruction::CastFTOB);
		case 115 :	return static_cast<DWORD>(InternalInstruction::CastFTOY);
		case 116 :	return static_cast<DWORD>(InternalInstruction::CastFTOW);
		case 117 :	return static_cast<DWORD>(InternalInstruction::CastFTOD);
		case 118 :	return static_cast<DWORD>(InternalInstruction::CastFTOO);
		case 119 :	return static_cast<DWORD>(InternalInstruction::CastFTOR);

		case 131 :	return static_cast<DWORD>(InternalInstruction::CastBTOL);
		case 132 :	return static_cast<DWORD>(InternalInstruction::CastBTOF);
		case 134 :	return 0;//IT_INTERNAL_CASTBTOB;
		case 135 :	return 0;//static_cast<DWORD>(InternalInstruction::CastBTOY);
		case 136 :	return static_cast<DWORD>(InternalInstruction::CastBTOW);
		case 137 :	return static_cast<DWORD>(InternalInstruction::CastBTOD);
		case 138 :	return static_cast<DWORD>(InternalInstruction::CastBTOO);
		case 139 :	return static_cast<DWORD>(InternalInstruction::CastBTOR);

		case 141 :	return static_cast<DWORD>(InternalInstruction::CastYTOL);
		case 142 :	return static_cast<DWORD>(InternalInstruction::CastYTOF);
		case 144 :	return 0;//static_cast<DWORD>(InternalInstruction::CastYTOB);
		case 145 :	return 0;//IT_INTERNAL_CASTYTOY;
		case 146 :	return static_cast<DWORD>(InternalInstruction::CastYTOW);
		case 147 :	return static_cast<DWORD>(InternalInstruction::CastYTOD);
		case 148 :	return static_cast<DWORD>(InternalInstruction::CastYTOO);
		case 149 :	return static_cast<DWORD>(InternalInstruction::CastYTOR);

		case 151 :	return static_cast<DWORD>(InternalInstruction::CastWTOL);
		case 152 :	return static_cast<DWORD>(InternalInstruction::CastWTOF);
		case 154 :	return static_cast<DWORD>(InternalInstruction::CastWTOB);
		case 155 :	return static_cast<DWORD>(InternalInstruction::CastWTOY);
		case 156 :	return 0;//IT_INTERNAL_CASTWTOW;
		case 157 :	return static_cast<DWORD>(InternalInstruction::CastWTOD);
		case 158 :	return static_cast<DWORD>(InternalInstruction::CastWTOO);
		case 159 :	return static_cast<DWORD>(InternalInstruction::CastWTOR);

		case 161 :	return static_cast<DWORD>(InternalInstruction::CastDTOL);
		case 162 :	return static_cast<DWORD>(InternalInstruction::CastDTOF);
		case 164 :	return static_cast<DWORD>(InternalInstruction::CastDTOB);
		case 165 :	return static_cast<DWORD>(InternalInstruction::CastDTOY);
		case 166 :	return static_cast<DWORD>(InternalInstruction::CastDTOW);
		case 167 :	return 0;//IT_INTERNAL_CASTDTOD;
		case 168 :	return static_cast<DWORD>(InternalInstruction::CastDTOO);
		case 169 :	return static_cast<DWORD>(InternalInstruction::CastDTOR);

		case 171 :	return static_cast<DWORD>(InternalInstruction::CastOTOL);
		case 172 :	return static_cast<DWORD>(InternalInstruction::CastOTOF);
		case 174 :	return static_cast<DWORD>(InternalInstruction::CastOTOB);
		case 175 :	return static_cast<DWORD>(InternalInstruction::CastOTOY);
		case 176 :	return static_cast<DWORD>(InternalInstruction::CastOTOW);
		case 177 :	return static_cast<DWORD>(InternalInstruction::CastOTOD);
		case 178 :	return 0;//IT_INTERNAL_CASTOTOO;
		case 179 :	return static_cast<DWORD>(InternalInstruction::CastOTOR);

		case 181 :	return static_cast<DWORD>(InternalInstruction::CastRTOL);
		case 182 :	return static_cast<DWORD>(InternalInstruction::CastRTOF);
		case 184 :	return static_cast<DWORD>(InternalInstruction::CastRTOB);
		case 185 :	return static_cast<DWORD>(InternalInstruction::CastRTOY);
		case 186 :	return static_cast<DWORD>(InternalInstruction::CastRTOW);
		case 187 :	return static_cast<DWORD>(InternalInstruction::CastRTOD);
		case 188 :	return static_cast<DWORD>(InternalInstruction::CastRTOO);
		case 189 :	return 0;//IT_INTERNAL_CASTRTOR;
	}
	return 0;
}

bool CInstructionTable::EnsureWordIsNotPartOfACommand ( LPSTR pConstantName )
{
#ifdef __AARON_INSTRPERF__
	//
	//	TODO: Implement this.
	//
	return false;
#else
	// go through entire command database and make sure the submitted word does NOT occur
	if ( m_pFirstInstructionEntry && pConstantName )
	{
		LPSTR pLastCommand = NULL;
		LPSTR pPtr, pPtrBegin, pPtrEnd;
		DWORD dwCompareSize = strlen( pConstantName );
		CInstructionTableEntry* pEntry = m_pFirstInstructionEntry;
		while(pEntry)
		{
			// get command name
			LPSTR pCommandName = pEntry->GetName()->GetStr();

			// quick rejects
			if ( pCommandName )
			{
				if ( pCommandName[0]==0 )
					goto _next;
				if ( pCommandName[0]=='+' )
					goto _next;
			}

			// skip if this same as last
			if ( pLastCommand )
				if ( strcmp ( pLastCommand, pCommandName )==NULL )
					goto _next;

			// compare command string with passed in constant-name
			pPtr = pCommandName;
			pPtrEnd = pPtr + strlen(pCommandName);
			pPtrBegin = pPtr;
			while ( pPtr<=pPtrEnd )
			{
				// space or end of string
				if ( *pPtr==32 || *pPtr==0 )
				{
					// this one word (leefix - 260604 - u54-must be greater than one letter - deal with JOYSTICK FIRE A( by detecting bracket)
					DWORD dwOneWordLength = pPtr - pPtrBegin;
					if ( dwOneWordLength > 1 )
					{
						// leefix - 260604 - u54 - largest word rules
						DWORD dwThisCompareSize = dwCompareSize;
						if ( dwThisCompareSize<dwOneWordLength ) dwThisCompareSize=dwOneWordLength;
						if ( _strnicmp ( pPtrBegin, pConstantName, dwThisCompareSize )==NULL )
						{
							// this word matches the one word from the command, leave!
							return true;
						}
					}

					// new word, begin
					pPtrBegin = pPtr+1;
				}

				// next char
				pPtr++;
			}
			pLastCommand = pCommandName;

			// get next command
			_next: pEntry=pEntry->GetNext();
		}
	}

	// complete
	return false;
#endif
}
