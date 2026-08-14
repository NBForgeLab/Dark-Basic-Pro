// InstructionTable.h: interface for the CInstructionTable class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_INSTRUCTIONTABLE_H__2C54A6BC_B075_484B_9706_9BBCE0C8F37F__INCLUDED_)
#define AFX_INSTRUCTIONTABLE_H__2C54A6BC_B075_484B_9706_9BBCE0C8F37F__INCLUDED_

#define __AARON_INSTRPERF__ 1

// Includes
#include <memory>
#include "ParserHeader.h"
#include "InstructionTableEntry.h"
#include "Task.h"

#include "PerfMacros.h"

#ifdef __AARON_INSTRPERF__
# include <DB3Dict.h>
# include <DB3Array.h>
#endif

// Internal instruction codes (converted from #define constants)
constexpr int IT_INTERNAL_MAXCOUNT = 1000;

enum class InternalInstruction : int {
	// Control
	Alloc             = 1,
	Free              = 2,
	AssignLL          = 3,
	AssignFF          = 4,
	AssignSS          = 5,
	AssignBB          = 6,
	AssignYY          = 7,
	AssignWW          = 8,
	AssignDD          = 9,
	AssignOO          = 10,
	AssignRR          = 11,
	AssignPP          = 12,
	RelAssignLL       = 13,
	RelAssignFF       = 14,
	RelAssignSS       = 15,
	RelAssignBB       = 16,
	RelAssignYY       = 17,
	RelAssignWW       = 18,
	RelAssignDD       = 19,
	RelAssignOO       = 20,
	RelAssignRR       = 21,
	StrFree           = 22,
	UserFunctionExit  = 24,
	AssignUdt         = 25,

	// Integer math
	PowerLLL          = 51,
	MulLLL            = 52,
	DivLLL            = 53,
	AddLLL            = 54,
	SubLLL            = 55,
	ModLLL            = 56,
	EqualLLL          = 57,
	GreaterLLL        = 58,
	LessLLL           = 59,
	NotEqualLLL       = 60,
	GreaterEqualLLL   = 61,
	LessEqualLLL      = 62,

	// Float math
	PowerFFF          = 71,
	MulFFF            = 72,
	DivFFF            = 73,
	AddFFF            = 74,
	SubFFF            = 75,
	EqualFF           = 76,
	GreaterLFF        = 77,
	LessLFF           = 78,
	NotEqualLFF       = 79,
	GreaterEqualLFF   = 80,
	LessEqualLFF      = 81,
	ModFFF            = 82,

	// Double math
	PowerOOO          = 91,
	MulOOO            = 92,
	DivOOO            = 93,
	AddOOO            = 94,
	SubOOO            = 95,
	EqualLOO          = 96,
	GreaterLOO        = 97,
	LessLOO           = 98,
	NotEqualLOO       = 99,
	GreaterEqualLOO   = 100,
	LessEqualOO       = 101,

	// String math
	PowerSSS          = 111,
	MulSSS            = 112,
	DivSSS            = 113,
	AddSSS            = 114,
	SubSSS            = 115,
	EqualSS           = 121,
	GreaterLSS        = 122,
	LessLSS           = 123,
	NotEqualLSS       = 124,
	GreaterEqualLSS   = 125,
	LessEqualSS       = 126,

	// Double integer math
	PowerRRR          = 31,
	MulRRR            = 32,
	DivRRR            = 33,
	AddRRR            = 34,
	SubRRR            = 35,
	EqualLRR          = 36,
	GreaterLRR        = 37,
	LessLRR           = 38,
	NotEqualLRR       = 39,
	GreaterEqualLRR   = 40,
	LessEqualRR       = 41,
	ModRRR            = 42,

	// Bitwise math
	ShiftLLLL         = 141,
	ShiftRLLL         = 142,
	BitOrLLL          = 143,
	BitAndLLL         = 144,
	BitXorLLL         = 145,
	BitNotLLL         = 149,

	// Comparison math
	OrLLL             = 146,
	AndLLL            = 147,
	NotLLL            = 148,

	// DWORD pointer math
	PowerDDD          = 151,
	MulDDD            = 152,
	DivDDD            = 153,
	AddDDD            = 154,
	SubDDD            = 155,
	ModDDD            = 156,
	GreaterDDD        = 157,
	LessDDD           = 158,
	NotEqualDDD       = 159,
	GreaterEqualDDD   = 160,
	LessEqualDDD      = 161,
	EqualDDD          = 162,

	// Boolean math
	PowerBBB          = 171,
	MulBBB            = 172,
	DivBBB            = 173,
	AddBBB            = 174,
	SubBBB            = 175,
	ModBBB            = 176,

	// BYTE math
	PowerYYY          = 181,
	MulYYY            = 182,
	DivYYY            = 183,
	AddYYY            = 184,
	SubYYY            = 185,
	ModYYY            = 186,

	// WORD math
	PowerWWW          = 191,
	MulWWW            = 192,
	DivWWW            = 193,
	AddWWW            = 194,
	SubWWW            = 195,
	ModWWW            = 196,

	// Casting math
	CastLToF          = 201,
	CastLToB          = 202,
	CastLToY          = 203,
	CastLToW          = 204,
	CastLToD          = 205,
	CastLToO          = 206,
	CastLToR          = 207,
	CastFTOL          = 211,
	CastFTOB          = 212,
	CastFTOY          = 213,
	CastFTOW          = 214,
	CastFTOD          = 215,
	CastFTOO          = 216,
	CastFTOR          = 217,
	CastBTOL          = 221,
	CastBTOF          = 222,
	CastBTOY          = 223,
	CastBTOW          = 224,
	CastBTOD          = 225,
	CastBTOO          = 226,
	CastBTOR          = 227,
	CastYTOL          = 231,
	CastYTOF          = 232,
	CastYTOB          = 233,
	CastYTOW          = 234,
	CastYTOD          = 235,
	CastYTOO          = 236,
	CastYTOR          = 237,
	CastWTOL          = 241,
	CastWTOF          = 242,
	CastWTOB          = 243,
	CastWTOY          = 244,
	CastWTOD          = 245,
	CastWTOO          = 246,
	CastWTOR          = 247,
	CastDTOL          = 251,
	CastDTOF          = 252,
	CastDTOB          = 253,
	CastDTOY          = 254,
	CastDTOW          = 255,
	CastDTOO          = 256,
	CastDTOR          = 257,
	CastOTOL          = 261,
	CastOTOF          = 262,
	CastOTOB          = 263,
	CastOTOY          = 264,
	CastOTOW          = 265,
	CastOTOD          = 266,
	CastOTOR          = 267,
	CastRTOL          = 271,
	CastRTOF          = 272,
	CastRTOB          = 273,
	CastRTOY          = 274,
	CastRTOW          = 275,
	CastRTOD          = 276,
	CastRTOO          = 277,

	// Internal commands
	Return            = 301,
	End               = 302,
	Sync              = 303,
	StartProgram      = 304,
	EndProgram        = 305,
	IncVar            = 306,
	DecVar            = 307,
	PureReturn        = 308,
	EndError          = 309,

	// Wave 11: runtime list API — ArrayInsert / ArrayDelete / Queue / Stack.
	// These register in the internal DB with the widened uintptr_t decorated
	// names (the .rc resource strings still carry 32-bit names).
	ArrayInsertTop      = 401,
	ArrayInsertBottom   = 402,
	ArrayInsertElement  = 403,
	ArrayDeleteElement  = 404,
	EmptyArray          = 405,
	AddToQueue          = 406,
	RemoveFromQueue     = 407,
	PushStack           = 408,
	PopStack            = 409,
};

// Build-in-instruction task codes (converted from #define constants)
enum class BuildTask : int {
	Ret               = 1,
	End               = 2,
	Sync              = 3,
	StartProgram      = 4,
	EndProgramAndQuit = 5,
	UserFunctionExit  = 6,
	PureRet           = 7,
	CopyUdt           = 8,
	EndError          = 9,

	Power             = 101,
	Mul               = 102,
	Div               = 103,
	Add               = 104,
	Sub               = 105,
	Mod               = 106,

	Shr               = 151,
	Shl               = 152,
	BitAnd            = 153,
	BitOr             = 154,
	BitXor            = 155,
	BitNot            = 156,

	And               = 161,
	Or                = 162,
	Not               = 163,

	Equal             = 201,
	Greater           = 202,
	Less              = 203,
	NotEqual          = 204,
	GreaterEqual      = 205,
	LessEqual         = 206,

	Inc               = 1001,
	Dec               = 1002,
	IncAdd            = 1003,
	DecAdd            = 1004,

	// Wave 8: emitter-side int<->float conversions (SSE2 CVT* instructions).
	CastIntToFloat    = 1101,
	CastIntToDouble   = 1102,
	CastFloatToInt    = 1103,
	CastFloatToDouble = 1104,
	CastDoubleToInt   = 1105,
	CastDoubleToFloat = 1106,

	// Wave 15: integer-family widening to int64 (REG64, no DLL).
	CastIntToInt64    = 1107,
	CastDwordToInt64  = 1108,

	// Wave 16: int64 <-> float/double conversions (SSE2 CVT*, REX.W).
	CastFloatToInt64  = 1109,
	CastDoubleToInt64 = 1110,
	CastInt64ToLower  = 1111, // int64 -> int/dword/byte/word (truncating store)
	CastInt64ToFloat  = 1112,
	CastInt64ToDouble = 1113,

	// Wave 18: narrowing casts to byte/word/dword (store-width truncation).
	CastToNarrow      = 1114, // L/D -> B/Y/W/D
	CastFloatToNarrow = 1115, // F/O -> B/Y/W

	// Wave 19: widening casts from byte/word (MOVZX then width store/CVT*).
	CastWiden         = 1116, // B/Y/W -> L/W/D
	CastWidenToFloat  = 1117, // B/Y/W -> F/O

	// Wave 17: Power (x^y = exp(y*log(x)), emitter-built; legacy 101 entry).
};


// Class Definition
class CInstructionTable  
{
	public:
		CInstructionTable();
		virtual ~CInstructionTable();

		bool		ScanPluginsForCommands(void);
		bool		LoadInstructionDatabase(void);
		bool		DefineHardCodedCommand(void);
		bool		SetInternalInstructionDatabase(void);

		void		SetRef(DWORD dwRefIndex, CInstructionTableEntry* pRef) { m_InternalInstructRef[dwRefIndex]=pRef; }
		CInstructionTableEntry* GetRef(DWORD dwRefIndex) { return m_InternalInstructRef[dwRefIndex]; }
		void		SetIIValue(DWORD dwRefIndex, DWORD dwValue) { m_InternalInstructions[dwRefIndex]=dwValue; }
		DWORD		GetIIValue(DWORD dwRefIndex) { return m_InternalInstructions[dwRefIndex]; }

		bool		AddCommand(LPSTR pName, LPSTR pDLL, LPSTR pDecoratedName, LPSTR pParamTypesString, DWORD resultp, DWORD pmax);
		bool		AddBuildCommand(LPSTR pName, LPSTR pDesc, LPSTR pParamTypesString, DWORD resultp, DWORD pmax, DWORD dwInternal, DWORD dwBuildID);
		bool		AddCommandCore(LPSTR pName, LPSTR pDLL, LPSTR pDecoratedName, LPSTR pParamTypesString, DWORD resultp, DWORD pmax, DWORD dwInternalValueIndex, DWORD dwBuildID);
		bool		AddCommandCore2(LPSTR pName, LPSTR pDLL, LPSTR pDecoratedName, LPSTR pParamTypesString, DWORD resultp, DWORD pmax, DWORD dwInternalValueIndex, DWORD dwBuildID, DWORD dwPlace, bool bPassArrayAsInput, CStr* pParamFullDesc, LPSTR* plpretStr);
		bool		AddUserFunction(LPSTR pName, DWORD resultp, LPSTR pParamTypesString, DWORD pmax, CDeclaration* pDecChain);
		bool		AddUniqueCommand(LPSTR pName, LPSTR pDLL, LPSTR pDecoratedName, LPSTR pParamTypesString, DWORD resultp, DWORD pmax);

		CInstructionTableEntry *GetEntry(int iType, const char *pStringData, bool *pRetFail=nullptr);
		CInstructionTableEntry *ResolveEntry(CInstructionTableEntry *pHead, int iWithAnyReturnValue, bool bCommandWhiteSpace=false);
		bool		FindEntryDirect(int iType, bool bCommandWhiteSpace, LPSTR pStringData, int iWithReturnValue, DWORD* pdwData, DWORD* pdwParamMax, DWORD* pdwLength, CInstructionTableEntry** ppRef);
		bool		FindEntry(int iType, bool bCommandWhiteSpace, CInstructionTableEntry* pEntry, LPSTR pStringData, int iWithReturnValue, DWORD* pdwData, DWORD* pdwParamMax, DWORD* pdwLength, CInstructionTableEntry** ppRef);

		bool		FindInstruction(bool bCommandWhiteSpace, LPSTR pStringData, int iWithReturnValue, DWORD* Data, DWORD* pdwParamMax, DWORD* Length, CInstructionTableEntry** ppRef);
		bool		FindUserFunction(LPSTR pStringData, int iWithReturnValue, DWORD* Data, DWORD* pdwParamMax, DWORD* Length);

		bool		FindInstructionParams(DWORD dwInstructionValue, DWORD dwParamMax, DWORD* pdwData, DWORD* pdwParamMax, CStr** pValidParamTypes, CInstructionTableEntry** pRefEntry);
		bool		FindUserFunctionParams(DWORD dwInstructionValue, DWORD dwParamMax, DWORD* pdwData, DWORD* pdwParamMax, CStr** pValidParamTypes, CInstructionTableEntry** pRefEntry);
		bool		CompareInstructionNames(CInstructionTableEntry* pRefEntryA, CInstructionTableEntry* pRefEntryB);
		bool		FindInstructionWithNameAndParams(LPSTR pFriendName, LPSTR pParams);

		CInstructionTableEntry* FindUserFunction(LPSTR pUserFunctionName);
		CInstructionTableEntry* FindReservedFunction(LPSTR pFunctionName);
		CInstructionTableEntry* FindLastFriendOfName(LPSTR pFriendName);

		void		ScanStart(void);
		void		ScanStep(void);
		void		ScanEnd(void);

		bool		VerifyCertificateForPlugin ( LPSTR pDLLName, LPSTR pProductName );
		std::unique_ptr<char[]>	ReadRawStringTable ( LPSTR pFilenameEXE, DWORD* pdwDataSize  );
		bool		LoadCommandsFromDLL(LPSTR pCategory, LPSTR pFilename);
		bool		TurnStringIntoCommand(LPSTR pCategory, LPSTR pDLLName, LPSTR pRawCommandString);
		void		AddCommandToHelpTxt(LPSTR pCategory, LPSTR pDLLName, LPSTR pParamStr, DWORD dwReturnParam, DWORD dwParamCount, LPSTR pParamDesc);
		bool		CheckTroubleCommandSyntax(std::string& sTXTThisSyntax, LPSTR pCommandName);
	
		DWORD		DetermineInternalCommandCode(DWORD dwMathSymbol, DWORD dwTypeValue);

		bool		EnsureWordIsNotPartOfACommand ( LPSTR pConstantName );

		inline CInstructionTableEntry *GetEntryByIndex(db3::uint index) { return m_EntryArray[index]; }

	private:
		// Types
		typedef db3::TDictionary<CInstructionTableEntry> map_type;
		typedef db3::CLock lock_type;
		typedef db3::CAutolock autolock_type;
		typedef db3::TArray<CInstructionTableEntry *> array_type;

		// Track Current Internal ID (Counter)
		DWORD						m_dwCurrentInternalID;

		// Instruction Maps
#ifdef __AARON_INSTRPERF__
		map_type					m_InstructionMap;
		lock_type					m_InstructionMapLock;

		map_type					m_UserFunctionMap;
		lock_type					m_UserFunctionMapLock;

		array_type					m_EntryArray;
#else
		CInstructionTableEntry*		m_pFirstInstructionEntry;
		CInstructionTableEntry*		m_pFirstUserFunctionEntry;

		lock_type					m_Lock;
#endif

		// Internal Instruction Value Database
		DWORD						m_InternalInstructions[IT_INTERNAL_MAXCOUNT];
		CInstructionTableEntry*		m_InternalInstructRef[IT_INTERNAL_MAXCOUNT];
};

#endif // !defined(AFX_INSTRUCTIONTABLE_H__2C54A6BC_B075_484B_9706_9BBCE0C8F37F__INCLUDED_)
