// InstructionTable.cpp: implementation of the CInstructionTable class.
//
//////////////////////////////////////////////////////////////////////


// Includes
#include "macros.h"
#include "ParserHeader.h"
#include "StatementList.h"
#include "DBMWriter.h"
#include "Error.h"
#include "StringUtils.h"
#include "InstructionTable.h"
#include "VarTable.h"
#include "direct.h"
#include "DBPCompiler.h"
#include "io.h"
#include "TextConvert.h"
#include <string>
#include <filesystem>

// External Class Pointers
extern CVarTable* g_pVarTable;
extern CDBPCompiler* g_pDBPCompiler;

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
	m_pFirstInstructionEntry=nullptr;
	m_pFirstUserFunctionEntry=nullptr;
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
	AddCommandCore("+math", "dbprocore.dll", "?PowerLLL@@YAKHH@Z", "LL", 1, 2, static_cast<DWORD>(InternalInstruction::PowerLLL), 0);
	AddCommandCore("+math", "dbprocore.dll", "?PowerBBB@@YAKKK@Z", "BB", 4, 2, static_cast<DWORD>(InternalInstruction::PowerBBB), 0);
	AddCommandCore("+math", "dbprocore.dll", "?PowerBBB@@YAKKK@Z", "YY", 5, 2, static_cast<DWORD>(InternalInstruction::PowerYYY), 0);
	AddCommandCore("+math", "dbprocore.dll", "?PowerWWW@@YAKKK@Z", "WW", 6, 2, static_cast<DWORD>(InternalInstruction::PowerWWW), 0);
	AddCommandCore("+math", "dbprocore.dll", "?PowerDDD@@YAKKK@Z", "DD", 7, 2, static_cast<DWORD>(InternalInstruction::PowerDDD), 0);

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
	AddCommandCore("+allocate", "dbprocore.dll", "?DimDDD@@YA_K_KKKKKKKKKKK@Z", "RDDDDDDDDDD", static_cast<DWORD>(DBPType::DwordArray), 11, static_cast<DWORD>(InternalInstruction::Alloc), 0);
	AddCommandCore("+deallocate", "dbprocore.dll", "?UnDimDD@@YA_K_K@Z", "R", static_cast<DWORD>(DBPType::DwordArray), 1, static_cast<DWORD>(InternalInstruction::Free), 0);
	AddCommandCore("+assign", "", "MOVLL", "LL", 0, 0, static_cast<DWORD>(InternalInstruction::AssignLL), 0);
	AddCommandCore("+assign", "", "MOVFF", "FF", 0, 0, static_cast<DWORD>(InternalInstruction::AssignFF), 0);
	AddCommandCore("+assign", "dbprocore.dll", "?EquateSS@@YA_K_K0@Z", "S", 3, 2, static_cast<DWORD>(InternalInstruction::AssignSS), 0);
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
	AddCommandCore("+mathfloat", "dbprocore.dll", "?PowerFFF@@YAKMM@Z", "FF", 1, 2, static_cast<DWORD>(InternalInstruction::PowerFFF), 0);
	AddCommandCore("+mathfloat", "dbprocore.dll", "?MulFFF@@YAKMM@Z", "FF", 1, 2, static_cast<DWORD>(InternalInstruction::MulFFF), 0);
	AddCommandCore("+mathfloat", "dbprocore.dll", "?DivFFF@@YAKMM@Z", "FF", 1, 2, static_cast<DWORD>(InternalInstruction::DivFFF), 0);
	AddCommandCore("+mathfloat", "dbprocore.dll", "?AddFFF@@YAKMM@Z", "FF", 1, 2, static_cast<DWORD>(InternalInstruction::AddFFF), 0);
	AddCommandCore("+mathfloat", "dbprocore.dll", "?SubFFF@@YAKMM@Z", "FF", 1, 2, static_cast<DWORD>(InternalInstruction::SubFFF), 0);
	AddCommandCore("+mathfloat", "dbprocore.dll", "?ModFFF@@YAKMM@Z", "FF", 1, 2, static_cast<DWORD>(InternalInstruction::ModFFF), 0);

	// Internal Comparison Commands
	AddCommandCore("+mathfloat", "dbprocore.dll", "?EqualLFF@@YAKMM@Z", "FF", 1, 2, static_cast<DWORD>(InternalInstruction::EqualFF), 0);
	AddCommandCore("+mathfloat", "dbprocore.dll", "?GreaterLFF@@YAKMM@Z", "FF", 1, 2, static_cast<DWORD>(InternalInstruction::GreaterLFF), 0);
	AddCommandCore("+mathfloat", "dbprocore.dll", "?LessLFF@@YAKMM@Z", "FF", 1, 2, static_cast<DWORD>(InternalInstruction::LessLFF), 0);
	AddCommandCore("+mathfloat", "dbprocore.dll", "?NotEqualLFF@@YAKMM@Z", "FF", 1, 2, static_cast<DWORD>(InternalInstruction::NotEqualLFF), 0);
	AddCommandCore("+mathfloat", "dbprocore.dll", "?GreaterEqualLFF@@YAKMM@Z", "FF", 1, 2, static_cast<DWORD>(InternalInstruction::GreaterEqualLFF), 0);
	AddCommandCore("+mathfloat", "dbprocore.dll", "?LessEqualLFF@@YAKMM@Z", "FF", 1, 2, static_cast<DWORD>(InternalInstruction::LessEqualLFF), 0);

	// Internal Math Commands
//	AddCommandCore("+mathstr", "dbprocore.dll", "?AddSSS@@YA_K_K00@Z", "SS", 3, 2, static_cast<DWORD>(InternalInstruction::AddSSS), 0);
//	AddCommandCore("+mathstr", "dbprocore.dll", "?EqualLSS@@YAK_K0@Z", "SS", 3, 2, static_cast<DWORD>(InternalInstruction::EqualSS), 0);
//	AddCommandCore("+mathstr", "dbprocore.dll", "?GreaterLSS@@YAK_K0@Z", "SS", 3, 2, static_cast<DWORD>(InternalInstruction::GreaterLSS), 0);
//	AddCommandCore("+mathstr", "dbprocore.dll", "?LessLSS@@YAK_K0@Z", "SS", 3, 2, static_cast<DWORD>(InternalInstruction::LessLSS), 0);
//	AddCommandCore("+mathstr", "dbprocore.dll", "?NotEqualLSS@@YAK_K0@Z", "SS", 3, 2, static_cast<DWORD>(InternalInstruction::NotEqualLSS), 0);
//	AddCommandCore("+mathstr", "dbprocore.dll", "?GreaterEqualLSS@@YAK_K0@Z", "SS", 3, 2, static_cast<DWORD>(InternalInstruction::GreaterEqualLSS), 0);
//	AddCommandCore("+mathstr", "dbprocore.dll", "?LessEqualLSS@@YAK_K0@Z", "SS", 3, 2, static_cast<DWORD>(InternalInstruction::LessEqualSS), 0);
	AddCommandCore("+mathstr", "dbprocore.dll", "?AddSSS@@YA_K_K00@Z", "SS", 3, 2, static_cast<DWORD>(InternalInstruction::AddSSS), 0);
	AddCommandCore("+mathstr", "dbprocore.dll", "?EqualLSS@@YAK_K0@Z", "SS", 1, 2, static_cast<DWORD>(InternalInstruction::EqualSS), 0);
	AddCommandCore("+mathstr", "dbprocore.dll", "?GreaterLSS@@YAK_K0@Z", "SS", 1, 2, static_cast<DWORD>(InternalInstruction::GreaterLSS), 0);
	AddCommandCore("+mathstr", "dbprocore.dll", "?LessLSS@@YAK_K0@Z", "SS", 1, 2, static_cast<DWORD>(InternalInstruction::LessLSS), 0);
	AddCommandCore("+mathstr", "dbprocore.dll", "?NotEqualLSS@@YAK_K0@Z", "SS", 1, 2, static_cast<DWORD>(InternalInstruction::NotEqualLSS), 0);
	AddCommandCore("+mathstr", "dbprocore.dll", "?GreaterEqualLSS@@YAK_K0@Z", "SS", 1, 2, static_cast<DWORD>(InternalInstruction::GreaterEqualLSS), 0);
	AddCommandCore("+mathstr", "dbprocore.dll", "?LessEqualLSS@@YAK_K0@Z", "SS", 1, 2, static_cast<DWORD>(InternalInstruction::LessEqualSS), 0);

	// Internal Math Commands
	AddCommandCore("+mathdoublef", "dbprocore.dll", "?PowerOOO@@YANNN@Z", "OO", 8, 2, static_cast<DWORD>(InternalInstruction::PowerOOO), 0);
	AddCommandCore("+mathdoublef", "dbprocore.dll", "?MulOOO@@YANNN@Z", "OO", 8, 2, static_cast<DWORD>(InternalInstruction::MulOOO), 0);
	AddCommandCore("+mathdoublef", "dbprocore.dll", "?DivOOO@@YANNN@Z", "OO", 8, 2, static_cast<DWORD>(InternalInstruction::DivOOO), 0);
	AddCommandCore("+mathdoublef", "dbprocore.dll", "?AddOOO@@YANNN@Z", "OO", 8, 2, static_cast<DWORD>(InternalInstruction::AddOOO), 0);
	AddCommandCore("+mathdoublef", "dbprocore.dll", "?SubOOO@@YANNN@Z", "OO", 8, 2, static_cast<DWORD>(InternalInstruction::SubOOO), 0);

	// Internal Comparison Commands
	AddCommandCore("+mathdoublef", "dbprocore.dll", "?EqualLOO@@YAKNN@Z", "OO", 1, 8, static_cast<DWORD>(InternalInstruction::EqualLOO), 0);
	AddCommandCore("+mathdoublef", "dbprocore.dll", "?GreaterLOO@@YAKNN@Z", "OO", 1, 8, static_cast<DWORD>(InternalInstruction::GreaterLOO), 0);
	AddCommandCore("+mathdoublef", "dbprocore.dll", "?LessLOO@@YAKNN@Z", "OO", 1, 8, static_cast<DWORD>(InternalInstruction::LessLOO), 0);
	AddCommandCore("+mathdoublef", "dbprocore.dll", "?NotEqualLOO@@YAKNN@Z", "OO", 1, 8, static_cast<DWORD>(InternalInstruction::NotEqualLOO), 0);
	AddCommandCore("+mathdoublef", "dbprocore.dll", "?GreaterEqualLOO@@YAKNN@Z", "OO", 1, 8, static_cast<DWORD>(InternalInstruction::GreaterEqualLOO), 0);
	AddCommandCore("+mathdoublef", "dbprocore.dll", "?LessEqualLOO@@YAKNN@Z", "OO", 1, 8, static_cast<DWORD>(InternalInstruction::LessEqualOO), 0);

	// Internal Math Commands
	AddCommandCore("+mathdoublei", "dbprocore.dll", "?PowerRRR@@YA_J_J0@Z", "RR", 9, 2, static_cast<DWORD>(InternalInstruction::PowerRRR), 0);
	AddCommandCore("+mathdoublei", "dbprocore.dll", "?MulRRR@@YA_J_J0@Z", "RR", 9, 2, static_cast<DWORD>(InternalInstruction::MulRRR), 0);
	AddCommandCore("+mathdoublei", "dbprocore.dll", "?DivRRR@@YA_J_J0@Z", "RR", 9, 2, static_cast<DWORD>(InternalInstruction::DivRRR), 0);
	AddCommandCore("+mathdoublei", "dbprocore.dll", "?AddRRR@@YA_J_J0@Z", "RR", 9, 2, static_cast<DWORD>(InternalInstruction::AddRRR), 0);
	AddCommandCore("+mathdoublei", "dbprocore.dll", "?SubRRR@@YA_J_J0@Z", "RR", 9, 2, static_cast<DWORD>(InternalInstruction::SubRRR), 0);

	// Internal Comparison Commands
	AddCommandCore("+mathdoublei", "dbprocore.dll", "?EqualLRR@@YAK_J0@Z", "RR", 1, 9, static_cast<DWORD>(InternalInstruction::EqualLRR), 0);
	AddCommandCore("+mathdoublei", "dbprocore.dll", "?GreaterLRR@@YAK_J0@Z", "RR", 1, 9, static_cast<DWORD>(InternalInstruction::GreaterLRR), 0);
	AddCommandCore("+mathdoublei", "dbprocore.dll", "?LessLRR@@YAK_J0@Z", "RR", 1, 9, static_cast<DWORD>(InternalInstruction::LessLRR), 0);
	AddCommandCore("+mathdoublei", "dbprocore.dll", "?NotEqualLRR@@YAK_J0@Z", "RR", 1, 9, static_cast<DWORD>(InternalInstruction::NotEqualLRR), 0);
	AddCommandCore("+mathdoublei", "dbprocore.dll", "?GreaterEqualLRR@@YAK_J0@Z", "RR", 1, 9, static_cast<DWORD>(InternalInstruction::GreaterEqualLRR), 0);
	AddCommandCore("+mathdoublei", "dbprocore.dll", "?LessEqualLRR@@YAK_J0@Z", "RR", 1, 9, static_cast<DWORD>(InternalInstruction::LessEqualRR), 0);

	// Cast Instructions
	AddCommandCore("+cast", "dbprocore.dll", "?CastLtoF@@YAKH@Z", "L", 1, 1, static_cast<DWORD>(InternalInstruction::CastLToF), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastLtoB@@YAKH@Z", "L", 1, 1, static_cast<DWORD>(InternalInstruction::CastLToB), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastLtoB@@YAKH@Z", "L", 1, 1, static_cast<DWORD>(InternalInstruction::CastLToY), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastLtoW@@YAKH@Z", "L", 1, 1, static_cast<DWORD>(InternalInstruction::CastLToW), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastLtoD@@YAKH@Z", "L", 1, 1, static_cast<DWORD>(InternalInstruction::CastLToD), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastLtoO@@YANH@Z", "L", 1, 1, static_cast<DWORD>(InternalInstruction::CastLToO), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastLtoR@@YA_JH@Z", "L", 1, 1, static_cast<DWORD>(InternalInstruction::CastLToR), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastFtoL@@YAKM@Z", "F", 1, 1, static_cast<DWORD>(InternalInstruction::CastFTOL), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastFtoB@@YAKM@Z", "F", 1, 1, static_cast<DWORD>(InternalInstruction::CastFTOB), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastFtoB@@YAKM@Z", "F", 1, 1, static_cast<DWORD>(InternalInstruction::CastFTOY), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastFtoW@@YAKM@Z", "F", 1, 1, static_cast<DWORD>(InternalInstruction::CastFTOW), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastFtoD@@YAKM@Z", "F", 1, 1, static_cast<DWORD>(InternalInstruction::CastFTOD), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastFtoO@@YANM@Z", "F", 1, 1, static_cast<DWORD>(InternalInstruction::CastFTOO), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastFtoR@@YA_JM@Z", "F", 1, 1, static_cast<DWORD>(InternalInstruction::CastFTOR), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastBtoL@@YAKE@Z", "B", 1, 1, static_cast<DWORD>(InternalInstruction::CastBTOL), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastBtoF@@YAKE@Z", "B", 1, 1, static_cast<DWORD>(InternalInstruction::CastBTOF), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastBtoW@@YAKE@Z", "B", 1, 1, static_cast<DWORD>(InternalInstruction::CastBTOW), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastBtoD@@YAKE@Z", "B", 1, 1, static_cast<DWORD>(InternalInstruction::CastBTOD), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastBtoO@@YANE@Z", "B", 1, 1, static_cast<DWORD>(InternalInstruction::CastBTOO), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastBtoR@@YA_JE@Z", "B", 1, 1, static_cast<DWORD>(InternalInstruction::CastBTOR), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastBtoL@@YAKE@Z", "Y", 1, 1, static_cast<DWORD>(InternalInstruction::CastYTOL), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastBtoF@@YAKE@Z", "Y", 1, 1, static_cast<DWORD>(InternalInstruction::CastYTOF), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastBtoW@@YAKE@Z", "Y", 1, 1, static_cast<DWORD>(InternalInstruction::CastYTOW), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastBtoD@@YAKE@Z", "Y", 1, 1, static_cast<DWORD>(InternalInstruction::CastYTOD), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastBtoO@@YANE@Z", "Y", 1, 1, static_cast<DWORD>(InternalInstruction::CastYTOO), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastBtoR@@YA_JE@Z", "Y", 1, 1, static_cast<DWORD>(InternalInstruction::CastYTOR), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastWtoL@@YAKG@Z", "W", 1, 1, static_cast<DWORD>(InternalInstruction::CastWTOL), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastWtoF@@YAKG@Z", "W", 1, 1, static_cast<DWORD>(InternalInstruction::CastWTOF), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastWtoB@@YAKG@Z", "W", 1, 1, static_cast<DWORD>(InternalInstruction::CastWTOB), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastWtoB@@YAKG@Z", "W", 1, 1, static_cast<DWORD>(InternalInstruction::CastWTOY), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastWtoD@@YAKG@Z", "W", 1, 1, static_cast<DWORD>(InternalInstruction::CastWTOD), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastWtoO@@YANG@Z", "W", 1, 1, static_cast<DWORD>(InternalInstruction::CastWTOO), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastWtoR@@YA_JG@Z", "W", 1, 1, static_cast<DWORD>(InternalInstruction::CastWTOR), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastDtoL@@YAKK@Z", "D", 1, 1, static_cast<DWORD>(InternalInstruction::CastDTOL), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastDtoF@@YAKK@Z", "D", 1, 1, static_cast<DWORD>(InternalInstruction::CastDTOF), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastDtoB@@YAKK@Z", "D", 1, 1, static_cast<DWORD>(InternalInstruction::CastDTOB), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastDtoB@@YAKK@Z", "D", 1, 1, static_cast<DWORD>(InternalInstruction::CastDTOY), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastDtoW@@YAKK@Z", "D", 1, 1, static_cast<DWORD>(InternalInstruction::CastDTOW), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastDtoO@@YANK@Z", "D", 1, 1, static_cast<DWORD>(InternalInstruction::CastDTOO), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastDtoR@@YA_JK@Z", "D", 1, 1, static_cast<DWORD>(InternalInstruction::CastDTOR), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastOtoL@@YAKN@Z", "O", 1, 1, static_cast<DWORD>(InternalInstruction::CastOTOL), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastOtoF@@YAKN@Z", "O", 1, 1, static_cast<DWORD>(InternalInstruction::CastOTOF), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastOtoB@@YAKN@Z", "O", 1, 1, static_cast<DWORD>(InternalInstruction::CastOTOB), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastOtoB@@YAKN@Z", "O", 1, 1, static_cast<DWORD>(InternalInstruction::CastOTOY), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastOtoW@@YAKN@Z", "O", 1, 1, static_cast<DWORD>(InternalInstruction::CastOTOW), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastOtoD@@YAKN@Z", "O", 1, 1, static_cast<DWORD>(InternalInstruction::CastOTOD), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastOtoR@@YA_JN@Z", "O", 1, 1, static_cast<DWORD>(InternalInstruction::CastOTOR), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastRtoL@@YAK_J@Z", "R", 1, 1, static_cast<DWORD>(InternalInstruction::CastRTOL), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastRtoF@@YAK_J@Z", "R", 1, 1, static_cast<DWORD>(InternalInstruction::CastRTOF), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastRtoB@@YAK_J@Z", "R", 1, 1, static_cast<DWORD>(InternalInstruction::CastRTOB), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastRtoB@@YAK_J@Z", "R", 1, 1, static_cast<DWORD>(InternalInstruction::CastRTOY), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastRtoW@@YAK_J@Z", "R", 1, 1, static_cast<DWORD>(InternalInstruction::CastRTOW), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastRtoD@@YAK_J@Z", "R", 1, 1, static_cast<DWORD>(InternalInstruction::CastRTOD), 0);
	AddCommandCore("+cast", "dbprocore.dll", "?CastRtoO@@YAN_J@Z", "R", 1, 1, static_cast<DWORD>(InternalInstruction::CastRTOO), 0);

	return true;
}

void CInstructionTable::ScanPluginDirectory(const std::filesystem::path& dirPath)
{
	std::error_code ec;
	if (!std::filesystem::exists(dirPath, ec) || !std::filesystem::is_directory(dirPath, ec))
		return;

	for (const auto& entry : std::filesystem::directory_iterator(dirPath, ec))
	{
		if (ec) break;
		if (entry.is_regular_file(ec))
		{
			auto ext = entry.path().extension().string();
			if (dbp::iequals(ext.c_str(), ".dll"))
			{
				std::string name = entry.path().stem().string();
				std::string filename = entry.path().filename().string();
				LoadCommandsFromDLL(name.c_str(), filename.c_str());
			}
		}
	}
}

bool CInstructionTable::ScanPluginsForCommands(void)
{
	// Store current folder
	std::error_code ec;
	const auto originalPath = std::filesystem::current_path(ec);
	g_pDBPCompiler->SetInternalFile(PATH_CURRENTFOLDER, const_cast<LPSTR>(originalPath.string().c_str()));

	// Switch to PLUGINS Folder
	std::filesystem::current_path(g_pDBPCompiler->GetInternalFile(PATH_PLUGINSFOLDER), ec);

	// Read core commands from the selected core runtime. This is the same image
	// that packaging embeds, so decorated command names cannot drift.
	const auto* runtimeBundle = g_pDBPCompiler->GetResolvedRuntimeBundle();
	if(runtimeBundle == nullptr)
	{
		std::filesystem::current_path(originalPath, ec);
		return false;
	}

	std::filesystem::current_path(runtimeBundle->pluginsDirectory, ec);
	if(!LoadCommandsFromDLL("core","dbprocore.dll"))
	{
		std::filesystem::current_path(originalPath, ec);
		return false;
	}
	std::filesystem::current_path(g_pDBPCompiler->GetInternalFile(PATH_PLUGINSFOLDER), ec);

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
	LoadCommandsFromDLL("gamefx","DBProGameFX.dll");
	LoadCommandsFromDLL("gamefx","DBProGameFXDebug.dll");
	LoadCommandsFromDLL("advancedmatrix","DBProAdvancedMatrixDebug.dll");
	LoadCommandsFromDLL("q2bsp","DBProQ2BSPDebug.dll");
	LoadCommandsFromDLL("q3bsp","DBProQ3BSPDebug.dll");
	LoadCommandsFromDLL("custombsp","DBProCustomBSPDebug.dll");
	LoadCommandsFromDLL("ode","DBProODEDebug.dll");

	// Switch to PLUGINS-USER Folder
	std::filesystem::current_path(g_pDBPCompiler->GetInternalFile(PATH_PLUGINSUSERFOLDER), ec);
	ScanPluginDirectory(g_pDBPCompiler->GetInternalFile(PATH_PLUGINSUSERFOLDER));

	// Switch to PLUGINS-LICENSED Folder
	std::filesystem::current_path(g_pDBPCompiler->GetInternalFile(PATH_PLUGINSLICENSEDFOLDER), ec);
	ScanPluginDirectory(g_pDBPCompiler->GetInternalFile(PATH_PLUGINSLICENSEDFOLDER));

	// Switch back to current folder
	std::filesystem::current_path(originalPath, ec);
	return true;
}

bool CInstructionTable::LoadInstructionDatabase(void)
{
	// Scan Available DLLs for Command Names (this will be a plugin traversal)
	return ScanPluginsForCommands();
}

bool CInstructionTable::AddCommand(std::string_view name, std::string_view dll, std::string_view decoratedName, std::string_view paramTypesString, DWORD resultp, DWORD pmax)
{
	return AddCommandCore2(name, dll, decoratedName, paramTypesString, resultp, pmax, 0, 0, 0, false, nullptr, nullptr);
}

bool CInstructionTable::AddUniqueCommand(std::string_view name, std::string_view dll, std::string_view decoratedName, std::string_view paramTypesString, DWORD resultp, DWORD pmax)
{
	// Only add if completely unique
	if(FindInstructionWithNameAndParams(name, paramTypesString)==false)
		return AddCommandCore2(name, dll, decoratedName, paramTypesString, resultp, pmax, 0, 0, 0, false, nullptr, nullptr);
	else
		return true;
}

bool CInstructionTable::AddBuildCommand(std::string_view name, std::string_view desc, std::string_view paramTypesString, DWORD resultp, DWORD pmax, DWORD dwInternalValueIndex, DWORD dwBuildID)
{
	return AddCommandCore2(name, "", desc, paramTypesString, resultp, pmax, dwInternalValueIndex, dwBuildID, 0, false, nullptr, nullptr);
}

bool CInstructionTable::AddCommandCore(std::string_view name, std::string_view dll, std::string_view decoratedName, std::string_view paramTypesString, DWORD resultp, DWORD pmax, DWORD dwInternalValueIndex, DWORD dwBuildID)
{
	return AddCommandCore2(name, dll, decoratedName, paramTypesString, resultp, pmax, dwInternalValueIndex, dwBuildID, 0, false, nullptr, nullptr);
}

bool CInstructionTable::AddCommandCore2(std::string_view name, std::string_view dll, std::string_view decoratedName, std::string_view paramTypesString, DWORD resultp, DWORD pmax, DWORD dwInternalValueIndex, DWORD dwBuildID, DWORD dwPlace, bool bPassArrayAsInput, CStr* pParamFullDesc, LPSTR* plpretStr )
{
	// Make Entry (RAII-owned until inserted into the table)
	auto pEntryOwner = std::make_unique<CInstructionTableEntry>();
	CInstructionTableEntry* pEntry = pEntryOwner.get();
	auto pStr = std::make_unique<CStr>(name);
	auto pStrDLL = std::make_unique<CStr>(dll);
	auto pStrDecName = std::make_unique<CStr>(decoratedName);
	auto pStrParamTypes = std::make_unique<CStr>(paramTypesString);

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
			SafeDelete(pParamFullDesc);
		}
		else
		{
			// Add to instruction as a value full param desc
			pEntry->SetFullParamDesc(pParamFullDesc);
		}
	}

#ifndef __AARON_INSTRPERF__
	// See if command has friends of same name (as they go together)
	CInstructionTableEntry* pLastFriendEntry = FindLastFriendOfName(name);

	// Has a Friend - Insert Into Database
	if(pLastFriendEntry)
	{
		// If command is identical to friend, cannot add it
		if(pEntry->GetHardcoreInternalValue()==0)
		{
			if(pEntry->GetReturnParam()==pLastFriendEntry->GetReturnParam())
			{
				if(pEntry->GetDLL() && pLastFriendEntry->GetDLL() && pEntry->GetDLL()->Length()>0 && pLastFriendEntry->GetDLL()->Length()>0)
				{
					if(pEntry->GetDecoratedName() && pLastFriendEntry->GetDecoratedName() && pEntry->GetDecoratedName()->Length()>0 && pLastFriendEntry->GetDecoratedName()->Length()>0)
					{
						if(pLastFriendEntry->GetName() && pEntry->GetName() && dbp::iequals(pLastFriendEntry->GetName()->GetStr(), pEntry->GetName()->GetStr()))
						{
							if(pLastFriendEntry->GetParamTypes() && pEntry->GetParamTypes() && dbp::iequals(pLastFriendEntry->GetParamTypes()->GetStr(), pEntry->GetParamTypes()->GetStr()))
							{
								// if return string valid, fill with conflicting DLL/name
								if ( plpretStr )
								{
									// create and return in ptr
									*plpretStr = new char[512];
									snprintf ( *plpretStr, 512, "%s", pLastFriendEntry->GetDLL()->GetStr() );
								}
								// identical command, cannot have two the same
								pEntryOwner.reset();
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
				pNextAfterLast->Insert(pEntryOwner.release());
			else
				pLastFriendEntry->Add(pEntryOwner.release());
		}
	}
	else
	{
		db3::CAutolock autolock(m_Lock);

		// Newy - Add to Database
		if(m_pFirstInstructionEntry==nullptr)
			m_pFirstInstructionEntry=pEntryOwner.release();
		else
			m_pFirstInstructionEntry->Add(pEntryOwner.release());
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

#ifdef __AARON_INSTRPERF__
	// NOTE: Storing this doesn't look like it's necessary, but it might as well be done anyway
	// NOTE[20121123]: Storing this is somewhat necessary.
	if (!m_EntryArray.CheckSlot(static_cast<db3::uint>(dwCurrentID)))
		return false;

	m_EntryArray[dwCurrentID] = pEntry;
#endif

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

bool CInstructionTable::AddUserFunction(std::string_view name, DWORD resultp, std::string_view paramTypesString, DWORD pmax, CDeclaration* pDecChain)
{
	// leefix-040803-Before confirm, check if name is a reserved word or function name
	std::string nameStr(name);
	if ( g_pStatementList->GetProgramStatements()->DetermineIfReservedWord ( nameStr.c_str() ) ) return false;
	if ( g_pStatementList->GetProgramStatements()->DetermineIfFunctionName ( nameStr.c_str(), false ) ) return false;

	// Increment ID
	DWORD dwCurrentID = static_cast<DWORD>(db3::atomic_inc(reinterpret_cast<db3::u32 *>(&m_dwCurrentInternalID)));

	// Make Entry (RAII-owned until inserted into the table)
	auto pEntryOwner = std::make_unique<CInstructionTableEntry>();
	CInstructionTableEntry* pEntry = pEntryOwner.get();
	auto pStr = std::make_unique<CStr>(name);
	auto pStrID = std::make_unique<CStr>((DWORD)1);
	auto pStrParamTypes = std::make_unique<CStr>(paramTypesString);

	pStrID->SetNumericText(dwCurrentID);
	// Ownership of the three CStr buffers transfers into the entry.
	pEntry->SetData(dwCurrentID, pStr.release(), nullptr, pStrID.release(), pStrParamTypes.release(), resultp, pmax, 0, 0);

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
	if(m_pFirstUserFunctionEntry==nullptr)
		m_pFirstUserFunctionEntry=pEntryOwner.release();
	else
		m_pFirstUserFunctionEntry->Add(pEntryOwner.release());
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

#ifdef __AARON_INSTRPERF__
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

		for(p="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_#$% \t"; *p!='\0'; p++)
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
#endif

#ifdef __AARON_INSTRPERF__
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
bool CInstructionTable::FindEntryDirect(int iType, bool bCommandWhiteSpace, LPCSTR pStringData, int iWithReturnValue, DWORD* pdwData, DWORD* pdwParamMax, DWORD* pdwLength, CInstructionTableEntry** ppRef)
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
#endif

bool CInstructionTable::FindEntry(int iType, bool bCommandWhiteSpace, CInstructionTableEntry* pBaseEntry, LPCSTR pStringData, int iWithAnyReturnValue, DWORD* pdwData, DWORD* pdwParamMax, DWORD* pdwLength, CInstructionTableEntry** ppRef)
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
			DWORD length = static_cast<DWORD>(strlen(pTry));
			bool bFoundAPossible=false;
			if(_strnicmp(pStringData, pTry, length)==0)
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
							LPCSTR pPtr = pStringData+length;
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

bool CInstructionTable::FindInstruction(bool bCommandWhiteSpace, LPCSTR pStringData, int iWithAnyReturnValue, DWORD* pdwData, DWORD* pdwParamMax, DWORD* pdwLength, CInstructionTableEntry** ppRef)
{
#ifdef __AARON_INSTRPERF__
	CInstructionTableEntry *m_pFirstInstructionEntry = nullptr;
	if (!bCommandWhiteSpace)
		return FindEntryDirect(0, bCommandWhiteSpace, pStringData, iWithAnyReturnValue, pdwData, pdwParamMax, pdwLength, ppRef);
#endif

	return FindEntry(0, bCommandWhiteSpace, m_pFirstInstructionEntry, pStringData, iWithAnyReturnValue, pdwData, pdwParamMax, pdwLength, ppRef);
}

bool CInstructionTable::FindUserFunction(LPCSTR pStringData, int iWithAnyReturnValue, DWORD* pdwData, DWORD* pdwParamMax, DWORD* pdwLength)
{
#ifdef __AARON_INSTRPERF__
	CInstructionTableEntry *m_pFirstUserFunctionEntry = nullptr;
#endif
	return FindEntry(1, false, m_pFirstUserFunctionEntry, pStringData, iWithAnyReturnValue, pdwData, pdwParamMax, pdwLength, nullptr);
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
				while(pEntry && pPrimary->GetName() && pEntry->GetName() && dbp::iequals(pPrimary->GetName()->GetStr(), pEntry->GetName()->GetStr()))
				{
					// Match Param to exit with valid instruction (or Type A repeated instruction like PRINT/INPUT/READ)
					if(pEntry->GetParamMax()==dwParamMax || (pEntry->GetParamTypes() && pEntry->GetParamTypes()->CheckChars(0, 1, "A")))
					{
						*pdwData=pEntry->GetInternalID();
						*pdwParamMax=pEntry->GetParamMax();
						*pValidParamTypes=pEntry->GetParamTypes();
						*pRefEntry=pEntry;
						return true;
					}
					pEntry=pEntry->GetNext();
				}
				break;
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
				while(pEntry && pPrimary->GetName() && pEntry->GetName() && dbp::iequals(pPrimary->GetName()->GetStr(), pEntry->GetName()->GetStr()))
				{
					// Match Param to exit with valid instruction
					if(pEntry->GetParamMax()==dwParamMax)
					{
						*pdwData=pEntry->GetInternalID();
						*pdwParamMax=pEntry->GetParamMax();
						*pValidParamTypes=pEntry->GetParamTypes();
						*pRefEntry=pEntry;
						return true;
					}
					pEntry=pEntry->GetNext();
				}
				break;
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
	if(pRefEntryA && pRefEntryB && pRefEntryA->GetName() && pRefEntryB->GetName()
	&& pRefEntryA->GetName()->GetStr() && pRefEntryB->GetName()->GetStr())
	{
		if(dbp::iequals(pRefEntryA->GetName()->GetStr(), pRefEntryB->GetName()->GetStr()))
			return true;
	}

	// Could not match names soft fail
	return false;
}

CInstructionTableEntry* CInstructionTable::FindUserFunction(std::string_view userFunctionName)
{
	if (userFunctionName.empty())
		return nullptr;

#ifdef __AARON_INSTRPERF__
	std::string nameStr(userFunctionName);
	auto entry = m_UserFunctionMap.Find(nameStr.c_str());
	if (!entry)
		return nullptr;

	return entry->P;
#else
	// Search for user function
	CInstructionTableEntry* pEntry = m_pFirstUserFunctionEntry;
	while(pEntry)
	{
		if(dbp::iequals(pEntry->GetNameView(), userFunctionName))
			return pEntry;

		pEntry=pEntry->GetNext();
	}

	// Could not find
	return nullptr;
#endif
}

CInstructionTableEntry* CInstructionTable::FindReservedFunction(std::string_view functionName)
{
	if (functionName.empty())
		return nullptr;

#ifdef __AARON_INSTRPERF__
	std::string nameStr(functionName);
	auto entry = m_InstructionMap.Find(nameStr.c_str());
	if (!entry)
		return nullptr;

	return entry->P;
#else
	// Search for reserved function
	CInstructionTableEntry* pEntry = m_pFirstInstructionEntry;
	while(pEntry)
	{
		if(dbp::iequals(pEntry->GetNameView(), functionName))
			return pEntry;

		pEntry=pEntry->GetNext();
	}

	// Could not find
	return nullptr;
#endif
}

bool CInstructionTable::FindInstructionWithNameAndParams(std::string_view pName, std::string_view pParams)
{
	if (pName.empty())
		return false;

#ifdef __AARON_INSTRPERF__
	std::string nameStr(pName);
	auto entry = m_InstructionMap.Find(nameStr.c_str());
	if (!entry)
		return nullptr;

	auto item = entry->P;

	while(item)
	{
		if (dbp::iequals(item->GetParamTypesView(), pParams))
			return true;

		item = item->GetNext();
	}

	return false;
#else
	// Search for entry with matching name and params
	CInstructionTableEntry* pEntry = m_pFirstInstructionEntry;
	while(pEntry)
	{
		if(dbp::iequals(pEntry->GetNameView(), pName) && dbp::iequals(pEntry->GetParamTypesView(), pParams))
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

CInstructionTableEntry* CInstructionTable::FindLastFriendOfName(std::string_view pFriendName)
{
	if (pFriendName.empty())
		return nullptr;

#ifdef __AARON_INSTRPERF__
	std::string nameStr(pFriendName);
	auto entry = m_InstructionMap.Find(nameStr.c_str());
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
	CInstructionTableEntry* pMatch = nullptr;

	// Search for last entry with the friend name
	CInstructionTableEntry* pEntry = m_pFirstInstructionEntry;
	while(pEntry)
	{
		if(dbp::iequals(pEntry->GetNameView(), pFriendName))
			pMatch=pEntry;

		pEntry=pEntry->GetNext();
	}

	// Pass last match if any
	return pMatch;
#endif
}

// DLL Scanning and Database Building

std::unique_ptr<char[]> CInstructionTable::ReadRawStringTable ( LPCSTR pFilenameEXE, DWORD* pdwDataSize )
{
	if ( pdwDataSize ) *pdwDataSize = 0;
	std::unique_ptr<char[]> pReturnData;
	DWORD dwOverallDataSize = 0;

	if (!pFilenameEXE || pFilenameEXE[0] == '\0')
		return pReturnData;

	std::filesystem::path dllPath(pFilenameEXE);
	if (dllPath.is_relative())
	{
		std::error_code ec;
		dllPath = std::filesystem::absolute(dllPath, ec);
	}

	HMODULE hEXE = LoadLibraryExW(dllPath.c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE);
	if ( hEXE )
	{
		// Check first string segment to see if this is a protected resource table
		HRSRC hFirst = FindResource(hEXE, MAKEINTRESOURCE(1), RT_STRING);
		if ( hFirst )
		{
			DWORD dwFirstSize = SizeofResource(hEXE, hFirst);
			HGLOBAL hGlobFirst = LoadResource(hEXE, hFirst);
			if ( hGlobFirst )
			{
				LPVOID lpFirst = LockResource(hGlobFirst);
				if ( lpFirst && dwFirstSize > 0 && ((const char*)lpFirst)[0] == 32 )
				{
					// Protected plugin detected (first char is 32/space). Accumulate segments safely.
					std::vector<char> buffer;
					for ( int iIndex = 1; iIndex <= 255; iIndex++ )
					{
						HRSRC hRes = FindResource(hEXE, MAKEINTRESOURCE(iIndex), RT_STRING);
						if ( hRes )
						{
							DWORD dwSize = SizeofResource(hEXE, hRes);
							HGLOBAL hGlobal = LoadResource(hEXE, hRes);
							if ( hGlobal )
							{
								LPVOID lpRes = LockResource(hGlobal);
								if ( lpRes && dwSize > 0 )
								{
									const char* src = (const char*)lpRes;
									buffer.insert(buffer.end(), src, src + dwSize);
								}
							}
						}
					}
					if ( !buffer.empty() )
					{
						dwOverallDataSize = static_cast<DWORD>(buffer.size());
						pReturnData = std::make_unique<char[]>(dwOverallDataSize);
						memcpy(pReturnData.get(), buffer.data(), dwOverallDataSize);
					}
				}
			}
		}

		// Free library
		FreeLibrary(hEXE);
	}

	// return data size
	if ( pdwDataSize ) *pdwDataSize = dwOverallDataSize;

	// return string
	return pReturnData;
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
bool CInstructionTable::LoadCommandsFromDLL(LPCSTR pCategory, LPCSTR pFilename)
{
#ifdef __AARON_COMMANDS__
	// Added for simplicity
	static char libname[512];
	FILE *f = nullptr;

	sprintf_s(libname, "%s.commands", pFilename);

	fopen_s(&f, libname, "r");
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

	// Plugins may declare their command list through the PE string table.
	// The first entry is a legacy product-code header and is skipped; every
	// following NUL-separated string is one command signature.
	DWORD dwDataSize = 0;
	HMODULE hModule = nullptr;
	DWORD dwCountProtectedCommands=0;
	std::unique_ptr<char[]> pProtectedData = ReadRawStringTable ( pFilename, &dwDataSize );
	if ( pProtectedData )
	{
		// walk the string data as a sequential stream of NUL-separated strings
		LPCSTR pPtr=pProtectedData.get();
		LPCSTR pPtrLineStart=pProtectedData.get();
		LPCSTR pPtrEnd=pProtectedData.get()+dwDataSize;
		while ( pPtr<pPtrEnd )
		{
			// find end of line
			if ( (unsigned char)*(pPtr)==0 )
			{
				// each string is NUL-terminated inside the walked buffer
				LPCSTR pTempStr = pPtrLineStart;
				pPtrLineStart=pPtr+1;

				// first entry is the legacy product-code header
				if ( dwCountProtectedCommands == 0 )
				{
					// advance to the first character of the command data
					while ( (unsigned char)*(pPtr+0)==0 && pPtr+2<pPtrEnd )
						pPtr++;

					// add to count
					dwCountProtectedCommands=1;
					pPtrLineStart=pPtr;
				}
				else
				{
					// add each as found
					PrintToFile( "[stringtable]%s:\"%s\"", pFilename, pTempStr );
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
		// without a string-table command list, open for direct string reading
		std::filesystem::path dllPath(pFilename);
		if (dllPath.is_relative())
		{
			std::error_code ec;
			dllPath = std::filesystem::absolute(dllPath, ec);
		}
		hModule = LoadLibraryExW ( dllPath.c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE );
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

bool CInstructionTable::TurnStringIntoCommand(LPCSTR pCategory, LPCSTR pDLLName, LPCSTR pRawCommandString)
{
	if (!pRawCommandString || pRawCommandString[0] == '\0')
		return false;

	// Parse Sections of string
	CStr strStorage(pRawCommandString);
	CStr* pStr = &strStorage;
	DWORD dwParamPos = pStr->FindFirstChar('%');
	if (dwParamPos == 0)
		return false;

	CStr checkForTwo(pStr->GetStr() + dwParamPos + 1);
	DWORD dwSecondPos = checkForTwo.FindFirstChar('%');
	if (dwSecondPos == 0)
		return false;

	DWORD dwDecPos = dwSecondPos + dwParamPos + 2;

	// Validate String Parts
	if(dwParamPos>0 && dwDecPos>0 && dwDecPos>dwParamPos)
	{
		// Command Name Part
		CStr nameStorage(pRawCommandString);
		CStr* pName = &nameStorage;
		pName->SetChar(dwParamPos, 0);

		if (pName->Length() == 0)
			return false;

		// Command Params Part
		CStr paramStorage(pRawCommandString+dwParamPos+1);
		CStr* pParam = &paramStorage;
		pParam->SetChar(dwDecPos-dwParamPos-2, 0);
		unsigned char cFirstParamChar=0;
		if(pName->Length() > 0 && pName->GetChar(pName->Length()-1)=='[')
		{
			// Command Is Expression
			pName->SetChar(pName->Length()-1,0);
			if (pParam->Length() > 0)
			{
				cFirstParamChar = pParam->GetChar(0);
				pParam->SetText(pParam->GetStr()+1);
			}
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
		if(strcmp(pDecorated->GetStr(), "??")!=0)
		{
			LPSTR lpConflictingDLLName = nullptr;
			if(AddCommandCore2(pName->GetStr(), pDLLName, pDecorated->GetStr(), pParam->GetStr(), dwReturnParam, pParam->Length(), 0, 0, dwStarPos, bPassArrayPtrAsInput, pParamDesc.release(), &lpConflictingDLLName)==false)
			{
				// Command already exists (either in same DLL or another DLL); ignore and continue loading remaining commands
				SafeDelete(lpConflictingDLLName);
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

void CInstructionTable::AddCommandToHelpTxt(LPCSTR pCategory, LPCSTR pCommandName, LPCSTR pParamStr, DWORD dwReturnParam, DWORD dwParamCount, LPCSTR pParamDesc)
{
	// Paths with std::filesystem
	const std::filesystem::path helpDir = std::filesystem::current_path() / "helptxt";
	const std::filesystem::path commandsDir = helpDir / "commands";
	const std::filesystem::path categoryDir = commandsDir / pCategory;

	std::error_code ec;
	std::filesystem::create_directories(categoryDir, ec);

	const std::string indexFilenameStr = (helpDir / "index.txt").string();
	const std::string commandFilenameStr = (categoryDir / (std::string(pCommandName) + ".txt")).string();
	const std::string subMenuFilenameStr = (commandsDir / (std::string(pCategory) + ".txt")).string();
	const std::string menuFilenameStr = (helpDir / "commands.txt").string();

	// Title of Command Page
	const std::string sTXTTitle = pCommandName;

	// Check if a trouble command
	std::string sTXTThisSyntax;
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
			if(strcmp(pParamDesc, "")==0)
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
	WritePrivateProfileString("COMMAND", "TITLE", sTXTTitle.c_str(), commandFilenameStr.c_str());

	// Add Unique Command To Unique Syntax Line
	DWORD dwField=1;
	char lpField[256];
	char lpTXTSyntax[256];
	while(dwField<998)
	{
		snprintf(lpField, sizeof(lpField), "SYNTAX%d", dwField);
		GetPrivateProfileString("COMMAND", lpField, "", lpTXTSyntax, 256, commandFilenameStr.c_str());
		if(strcmp(lpTXTSyntax,"")==0)
		{
			// Not there, so add it
			WritePrivateProfileString("COMMAND", lpField, sTXTThisSyntax.c_str(), commandFilenameStr.c_str());
			break;
		}
		if(strcmp(lpTXTSyntax,sTXTThisSyntax.c_str())==0)
		{
			// Already got it
			break;
		}
		dwField++;
	}

	// Add parameter types to command-data
	snprintf(lpField, sizeof(lpField), "%s", "PARAM");
	char lpThisParamStr [ 256 ];
	GetPrivateProfileString("COMMAND", lpField, "", lpThisParamStr, 256, commandFilenameStr.c_str());
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
		WritePrivateProfileString("COMMAND", lpField, sNewParamStr.c_str(), commandFilenameStr.c_str());

	// Add Unique Command To The IndexTXT (for the index.htm)
	DWORD dwIndexField=1;
	char lpIndexField[_MAX_PATH];
	char lpIndexFieldData[_MAX_PATH];
	char lpFilenameToCommand[_MAX_PATH];
	snprintf(lpFilenameToCommand, sizeof(lpFilenameToCommand), "commands\\%s\\%s.htm", pCategory, pCommandName);
	while(1)
	{
		snprintf(lpIndexField, sizeof(lpIndexField), "ENTRY%d", dwIndexField);
		GetPrivateProfileString("INDEX", lpIndexField, "", lpIndexFieldData, _MAX_PATH, indexFilenameStr.c_str());
		if(strcmp(lpIndexFieldData,"")==0)
		{
			WritePrivateProfileString("INDEX", lpIndexField, pCommandName, indexFilenameStr.c_str());
			snprintf(lpIndexField, sizeof(lpIndexField), "FILE%d", dwIndexField);
			WritePrivateProfileString("INDEX", lpIndexField, lpFilenameToCommand, indexFilenameStr.c_str());
			break;
		}
		if(strcmp(lpIndexFieldData, pCommandName)==0)
		{
			// Already got it
			break;
		}
		dwIndexField++;
	}

	// Update category sub-menu file
	dwField=1;
	bool bAlreadyExists=false;
	char lpTXTCommand[256];
	while(dwField<2000)
	{
		snprintf(lpField, sizeof(lpField), "MENU%d", dwField);
		GetPrivateProfileString("SUBMENU", lpField, "", lpTXTCommand, 256, subMenuFilenameStr.c_str());
		if(strcmp(lpTXTCommand,pCommandName)==0)
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
			GetPrivateProfileString("SUBMENU", lpField, "", pAnything, 256, subMenuFilenameStr.c_str());
			if(strcmp(pAnything,"")==0)
			{
				// Empty - Can put it here
				snprintf(lpField, sizeof(lpField), "MENU%d", dwField);
				WritePrivateProfileString("SUBMENU", lpField, pCommandName, subMenuFilenameStr.c_str());
				break;
			}
			dwField++;
		}
	}

	// Update Commands Menu
	dwField=1;
	while(1)
	{
		snprintf(lpField, sizeof(lpField), "MENU%d", dwField);
		GetPrivateProfileString("COMMANDSMENU", lpField, "", lpTXTCommand, 256, menuFilenameStr.c_str());
		if(strcmp(lpTXTCommand,"")==0)
		{
			WritePrivateProfileString("COMMANDSMENU", lpField, pCategory, menuFilenameStr.c_str());
			break;
		}
		if(strcmp(lpTXTCommand,pCategory)==0)
		{
			// Already got it
			break;
		}
		dwField++;
	}
}

bool CInstructionTable::CheckTroubleCommandSyntax(std::string& sTXTThisSyntax, LPCSTR pCommandName)
{
	bool bNotAnyTrouble=true;

	if(dbp::iequals(pCommandName,"print"))
	{
		sTXTThisSyntax = "PRINT Print Statements";
		bNotAnyTrouble=false;
	}
	if(dbp::iequals(pCommandName,"input"))
	{
		sTXTThisSyntax = "INPUT Print Statements, Input Variable";
		bNotAnyTrouble=false;
	}

	return bNotAnyTrouble;
}

DWORD CInstructionTable::DetermineInternalCommandCode(DWORD dwMathSymbol, DWORD dwTypeValue)
{
	const auto type = static_cast<DBPType>(dwTypeValue);
	switch(type)
	{
		case DBPType::Integer:
		case DBPType::IntegerArray:
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

		case DBPType::Float:
		case DBPType::FloatArray:
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

		case DBPType::String:
		case DBPType::StringArray:
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

		case DBPType::Boolean:
		case DBPType::BooleanArray:
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

		case DBPType::Byte:
		case DBPType::ByteArray:
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

		case DBPType::Word:
		case DBPType::WordArray:
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

		case DBPType::Dword:
		case DBPType::DwordArray:
		case DBPType::UserDefinedPtr:
		case DBPType::UserDefinedArrayPtr:
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

		case DBPType::DoubleFloat:
		case DBPType::DoubleFloatArray:
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

		case DBPType::DoubleInteger:
		case DBPType::DoubleIntegerArray:
		switch(dwMathSymbol)
		{
			case 1 :	return static_cast<DWORD>(InternalInstruction::PowerRRR);
			case 2 :	return static_cast<DWORD>(InternalInstruction::MulRRR);
			case 3 :	return static_cast<DWORD>(InternalInstruction::DivRRR);
			case 4 :	return static_cast<DWORD>(InternalInstruction::AddRRR);
			case 5 :	return static_cast<DWORD>(InternalInstruction::SubRRR);
			case 27 :	return static_cast<DWORD>(InternalInstruction::EqualLRR);
			case 25 :	return static_cast<DWORD>(InternalInstruction::GreaterLRR);
			case 26 :	return static_cast<DWORD>(InternalInstruction::LessLRR);
			case 22 :	return static_cast<DWORD>(InternalInstruction::NotEqualLRR);
			case 23 :	return static_cast<DWORD>(InternalInstruction::GreaterEqualLRR);
			case 24 :	return static_cast<DWORD>(InternalInstruction::LessEqualRR);
		}
		break;
		default:
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

bool CInstructionTable::EnsureWordIsNotPartOfACommand ( std::string_view constantName )
{
#ifdef __AARON_INSTRPERF__
	//
	//	TODO: Implement this.
	//
	return false;
#else
	// go through entire command database and make sure the submitted word does NOT occur
	if ( m_pFirstInstructionEntry && !constantName.empty() )
	{
		std::string_view lastCommand;
		CInstructionTableEntry* pEntry = m_pFirstInstructionEntry;
		while(pEntry)
		{
			// get command name
			std::string_view commandName = pEntry->GetNameView();

			// quick rejects
			if ( commandName.empty() || commandName.front() == '+' )
				goto _next;

			// skip if this same as last
			if ( !lastCommand.empty() && lastCommand == commandName )
				goto _next;

			// compare command string with passed in constant-name
			{
				size_t pos = 0;
				while ( pos < commandName.size() )
				{
					size_t nextSpace = commandName.find(' ', pos);
					if ( nextSpace == std::string_view::npos )
						nextSpace = commandName.size();

					std::string_view word = commandName.substr(pos, nextSpace - pos);
					if ( word.size() > 1 )
					{
						if ( dbp::iequals(word, constantName) )
							return true;
					}

					pos = nextSpace + 1;
				}
			}
			lastCommand = commandName;

			// get next command
			_next: pEntry=pEntry->GetNext();
		}
	}

	// complete
	return false;
#endif
}
