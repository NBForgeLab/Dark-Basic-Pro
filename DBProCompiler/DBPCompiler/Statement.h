// Statement.h: interface for the CStatement class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_STATEMENT_H__2A1543E2_9870_4E5E_B056_C09192997E8D__INCLUDED_)
#define AFX_STATEMENT_H__2A1543E2_9870_4E5E_B056_C09192997E8D__INCLUDED_

// Common Includes
#include "windows.h"
#include <memory>
#include <optional>
#include <string>
#include <variant>

// Custom Includes
#include "InstructionTableEntry.h"
#include "Declaration.h"
#include "Str.h"
#include "ParserResultData.h"
#include "Tokenizer.h"
#include "ExpressionParser.h"

// Token enum class (converted from #define constants)
enum class Token : int {
	// General
	Comma           = 10001,
	Crt             = 10002,

	// End / Exit
	End             = 10010,
	Exit            = 10011,

	// Loop
	Do              = 10021,
	Loop            = 10022,
	While           = 10023,
	EndWhile        = 10024,
	Repeat          = 10025,
	Until           = 10026,
	For             = 10031,
	Next            = 10032,

	// User function
	UserFunction      = 10101,
	ExitUserFunction  = 10102,
	EndUserFunction   = 10103,
	UserFunctionCall  = 10104,

	// If / Else
	If              = 10151,
	Else            = 10152,
	EndIf           = 10153,
	ElseEndIf       = 10154,
	ElseCrt         = 10155,

	// Jump
	Goto            = 10201,
	Gosub           = 10202,

	// Select / Case
	Select          = 10211,
	EndSelect       = 10212,
	Case            = 10213,
	EndCase         = 10214,
	CaseDefault     = 10215,

	// Type / Declaration
	Type            = 10301,
	EndType         = 10302,
	Global          = 10303,
	Local           = 10304,
	Dim             = 10305,
	Undim           = 10306,
	Asterisk        = 10307,

	// Data types
	Boolean         = 10311,
	Byte            = 10312,
	Word            = 10313,
	Dword           = 10314,
	Integer         = 10315,
	Float           = 10316,
	String          = 10317,
	Double          = 10318,

	// Remarks
	RemLine         = 10501,
	RemStart        = 10502,
	RemEnd          = 10503,

	// Label / Data
	Label           = 10701,
	Data            = 10702,

	// Statement types
	Assignment      = 11001,
	Instruction     = 11004,
};

// Class Prototypes
class CParameter;
class CDatatype;
class CMathOp;
class CParseLoop;
class CParseType;
class CParseInit;
class CParseUserFunction;
class CParseJump;
class CParseInstruction;
class CParseFunction;
class CASTAssignment;

// Type-safe tagged union for statement objects (replaces void* m_pObjectClass)
using StatementObject = std::variant<
	std::monostate,          // empty (legacy type 0, 999)
	CParseLoop*,             // legacy type 1
	CParseType*,             // legacy type 2
	CParseInit*,             // legacy type 3
	CParseUserFunction*,     // legacy type 6
	CParseJump*,             // legacy type 8
	CParseInstruction*,      // legacy type 11
	CParseFunction*,         // legacy type 12
	CASTAssignment*          // legacy type 20
>;

// Class Def
class CStatement  
{
	public:
		CStatement();
		virtual ~CStatement();
		void FreeObjects(void);
		void Free(void);

	private:
		CTokenizer		m_tokenizer;

	public:
		// Tokenizer (extracted subsystem)
		[[nodiscard]] CTokenizer& GetTokenizer() noexcept { return m_tokenizer; }
		[[nodiscard]] const CTokenizer& GetTokenizer() const noexcept { return m_tokenizer; }

		void			Add(CStatement *pNext);
		CStatement*		GetNext(void) { return m_pNext; }
		CStatement*		FindLastStatement(void);
		void			SetData(DWORD, DWORD, void*) = delete; // Use SetData<T> or SetObject instead
		void			SetNext(CStatement* pNext) { m_pNext=pNext; }

		// Type-safe object setters (takes ownership)
		void			SetObject(CParseLoop* p);
		void			SetObject(CParseType* p);
		void			SetObject(CParseInit* p);
		void			SetObject(CParseUserFunction* p);
		void			SetObject(CParseJump* p);
		void			SetObject(CParseInstruction* p);
		void			SetObject(CParseFunction* p);
		void			SetObject(CASTAssignment* p);

		// Type-safe object access
		template<typename T>
		T* GetObject() const {
			if (auto* ptr = std::get_if<T*>(&m_object))
				return *ptr;
			return nullptr;
		}
		bool			HasObject() const { return !std::holds_alternative<std::monostate>(m_object); }
		void			ClearObject();

		// Type-safe SetData overloads
		template<typename T>
		void SetData(DWORD LineNumber, std::unique_ptr<T> pObj) {
			m_dwLineNumber = LineNumber;
			m_dwStartChar = 0;
			m_dwEndChar = 0;
			SetObject(pObj.release());
		}

		void			SetLineNumber(DWORD line) { m_dwLineNumber=line; }
		void			SetObjectClass(void* pPtr) = delete; // Use SetObject<T> instead
		void			SetParameter(CParameter* pParam) { m_pParameters.reset(pParam); }
		void			SetLine(DWORD dwLine);
		DWORD			GetLineNumber(void) { return m_dwLineNumber; }

		void			SetLineAndCharPos(DWORD dwLine);
		void			SetLineAndCharPos(DWORD dwLine, int iFlag);

		void			SurpressJumpChecks(void) { m_bPerformJumpChecks=false; }

	public:
		bool			DoPreScanBlock(DWORD RequiredTerminator);
		bool			DoLocalScanBlock(DWORD RequiredTerminator);
		bool			DoBlock(DWORD RequiredTerminator, DWORD* dwLastToken);
		bool			DoInitCode(void);
		bool			DoEndCode(void);
		CStatement*		AddInternalStatement(DWORD dwCodeIndex, DWORD dwInternalCode);
		bool			DoStatement(DWORD TokenID);
		bool			DoLoop(DWORD StatementLineNumber, DWORD TokenID);
		bool			DoJump(DWORD StatementLineNumber, DWORD TokenID);
		bool			DoType(DWORD StatementLineNumber, DWORD TokenID);
		bool			DoLabel(DWORD StatementLineNumber, DWORD TokenID);
		bool			DoDataStatement(DWORD StatementLineNumber, DWORD TokenID);
		bool			DoDeclaration(bool bVariableDeclaration, DWORD dwTerminatorType, CDeclaration** pDec, bool bDoneDim, bool bAutoInitialiseData, bool bIsGlobal, bool bDefineOnly);
		bool			DoInstruction(DWORD StatementLineNumber, DWORD TokenID);
		bool			DoAssignment(DWORD StatementLineNumber, DWORD TokenID);
		bool			DoAllocation(DWORD StatementLineNumber, LPSTR pVarName, LPSTR pValue);
		bool			DoDeAllocation(DWORD StatementLineNumber);
		bool			DoUserFunction(DWORD StatementLineNumber, DWORD TokenID);
		bool			DoUserFunctionCall(DWORD StatementLineNumber, DWORD TokenID);
		bool			DoUserFunctionExit(DWORD StatementLineNumber, DWORD TokenID);
		bool			DoParameterListString(CStr* pParamString, CParameter** ppFirstParameter);
		bool			DoExpressionList(CParameter** ppParameter, bool* bNoMoreParams);
		bool			DoExpressionListString(CParameter** ppParameter, CStr* pOptionalString, DWORD* pdwReturnDistance, bool* bTerminator);
		bool			DoExpression(CStr* pStr, CParameter* pParameter);
		bool			FindHighestPres(CStr* pStr, DWORD *dwPosition, DWORD *dwType);

		DWORD			GetMainToken(void);
		DWORD			GetToken(void);
		LPSTR			SkipAllComments ( LPSTR pPointer, LPSTR pPtrEnd );
		void			SkipToCR(void);
		LPSTR			SeekToSeperator(LPSTR pPointer, bool bAdvanceLine, bool bStopAtComment);
		LPSTR			SeekToCRReadOnly(LPSTR pPointer);
		LPSTR			SeekToRemEnd(LPSTR pPointer);
		DWORD			GetTokenToSeperator(void);
		DWORD			FindToken(LPSTR pPointer, bool bIncrementLineNumber);
		DWORD			DetermineNameToken(LPSTR pToken);
		DWORD			DetermineToken(LPSTR pToken);
		bool			DetermineIfReservedWord(LPSTR pToken);
		bool			DetermineIfFunctionName(LPSTR pWord, bool bIncludeUserFunctions);
		LPSTR			ProduceNextTokenEx(LPSTR* pString, bool bIncrementLineNumber, bool bProduceCRTK, bool bIncludeCommas, bool bIgnoreSpacesAsSeperators);
		LPSTR			ProduceNextToken(LPSTR* pString, bool bIncrementLineNumber, bool bProduceCRTK, bool bIncludeCommas);
		LPSTR			ProduceNextArrayToken(LPSTR* pOrigPointer);
		LPSTR			ProduceFullSegment(LPSTR* pString);
		bool			SeekCharAsPrevChar(unsigned char c, int* piBacktrak);
		bool			SeekCharAsNextChar(unsigned char c, DWORD* pdwAdvance);
		LPSTR			GetStringToEndOfLine(void);
		bool			RemoveEdgeBracketFromSegment(LPSTR pPointer, DWORD *pdwSPos, DWORD *pdwEPos);
		bool			ExtractDetailsFromForNext(CStr* pVar, CStr* pInit, CStr* pEnd, CStr* pStep);
		bool			ReplaceTHENandELSEwithSep(void);

		DWORD			PeekToken(LPSTR pPointer);
		DWORD			PeekLabel(LPSTR pPointer);
		std::string		GetLabel(LPSTR* pPointer);
		void			AdvancePastCRandSPACES(LPSTR* pPointerPtr);

		std::optional<std::string>	AddInternalLabel(void);
		bool			FindCorrectInstruction(CInstructionTableEntry** pRef, CParameter* pFirstParameter, DWORD dwOrigValue, DWORD dwOrigType, DWORD dwOrigParamMax, DWORD* pdwValidInstructionToUse, bool* pbIfFindTypeA);
	
		DWORD			GetObjectLineNumber(void) { return m_dwLineNumber; }
		CParameter*		GetParameter(void) { return m_pParameters.get(); }

		bool			WriteDBM(void);
		bool			WriteDBMBit(DWORD dwLineNumber, LPSTR pText, LPSTR pResult);

	private:
		bool			WriteDBMNode(void);

		// Debug Data
		DWORD			m_dwLineNumber;
		DWORD			m_dwStartChar;
		DWORD			m_dwEndChar;
		bool			m_bPerformJumpChecks;

		// Object Data
		StatementObject	m_object;
		std::unique_ptr<CParameter>	m_pParameters;

		// Hierarchy Data
		CStatement		*m_pNext;
};

class CParameter  
{
	public:
		CParameter();
		virtual ~CParameter();

		CMathOp* GetMathItem(void) { return m_pMainMathOp.get(); }
		CParameter* GetNext(void) { return m_pNext.get(); }
		CParameter* GetPrev(void) { return m_pPrev; }
		void SetNext(CParameter* pPtr) { m_pNext.reset(pPtr); }
		CParameter* GetLast(void);

		void Add(CParameter* pParameter);
		void SetMathItem(CMathOp* pItem) { m_pMainMathOp.reset(pItem); }
		CMathOp* ReleaseMathItem(void) { return m_pMainMathOp.release(); }
		bool MakeParamList(CStr* pStrList);
		bool ValidateWithCorrectCall(CStr* pValidParamTypes, DWORD* pdwScore, DWORD dwInternalCode);
		bool CastAllParametersToInstruction(CInstructionTableEntry* pRef);

		bool SetParamAsLabel(std::string labelName);
		DWORD Count(void);

		bool WriteDBM(void);

	private:

		// Uses math object to store value
		std::unique_ptr<CMathOp>	m_pMainMathOp;

		// Hierarchy Data
		std::unique_ptr<CParameter>	m_pNext;
		CParameter*			m_pPrev;
};

class CMathOp
{
	public:
		CMathOp();
		virtual ~CMathOp();
		CMathOp* GetNext(void) { return m_pNext.get(); }

		void SetResult(LPSTR pString, DWORD dwType, DWORD dwDataOffset);
		void SetResultData(CResultData ResultData);
		void SetResultType(DWORD dwType) { m_Result.m_dwType=dwType; }
		void SetResultStruct(CStructTable* pStruct) { m_Result.m_pStruct=pStruct; }
		void SetArrayOffsetResult(LPSTR pString);
		CStr* FindResultStringTokenForDBM(void);
		CResultData* FindResultDataForDBM(void);
		DWORD FindResultTypeValueForDBM(void);

		CResultData* GetResultData(void);
		CResultData* FindResultData(void);
		CStr* GetResultStringToken(void) { return m_Result.m_pStringToken.get(); }
		CStr* GetResultArrayOffset(void) { return m_Result.m_pAdditionalOffset.get(); }
		DWORD GetResultType(void) { return m_Result.m_dwType; }
		DWORD GetResultDataOffset(void) { return m_Result.m_dwDataOffset; }
		
		DWORD GetResultOffsetLValueTypeValue(void) { return m_dwOffsetLValueTypeValue; }
		bool GetConcatFlag(void) { return m_bConcatFlagUsed; }
		void SetConcatFlag(bool bFlag) { m_bConcatFlagUsed=bFlag; }

		bool CalculateDataOffsetAndTypeFromFieldString(CStr* pVarName, DWORD dwArrayType, CStr* pFieldData, DWORD* pdwSize, DWORD* pdwLType, DWORD* pdwSizeOfWholeType, CStructTable** ppStruct);
		bool ResolveStructValue(CStr* pExpressionValue);
		DWORD TranslateStructToOffsetForDBM(CStr* pString);
		bool TranslateStringTokenForDBM(void);

		void SetLineNumber(DWORD dwLine) { m_dwLineNumber=dwLine; }
		DWORD GetLineNumber(void) { return m_dwLineNumber; }

		void SetMathSymbol(DWORD dwM) { m_dwMathSymbol=dwM; }
		DWORD GetMathSymbol(void) { return m_dwMathSymbol; }

		void Add(CMathOp* pNext);

		bool DoValue(CStr* pStr);
		bool DoCastOnMathOp(std::unique_ptr<CMathOp>& pMathOp, DWORD dwTypeMode);
		bool DoValueFunction(CStr* pExpression);
		bool DoValueComplexVariable(CStr* pExpression);
		bool TokeniseStructuresOfDataString(CStr* pEntireData, DWORD* pdwLValueType);
		bool DoValueSingleVariable(CStr* pExpression);
		bool DoValueLiteral(CStr* pExpression, DWORD dwTypeValue);
		bool DoValueLabel(CStr* pExpressionValue);

		bool FindHighestPres(CStr* pStr, DWORD *dwPosition, DWORD *dwType, DWORD *dwSymbWidth, DWORD *dwIsSciNot);
		bool CheckForSymbol(CStr* pString, DWORD dwSP, DWORD *dwMathType, DWORD *dwPriority, DWORD *dwSymbolWidth);
		bool ProduceNewTempToken(CStr* pTempVarToken, DWORD dwTypeMode);

		bool IsReserved ( CStr* pExpressionValue );
		bool IsItLabelFollowedByBracket(CStr* pExpressionValue, DWORD *pdwLabelLength);
		bool IsFunction(CStr* pExpressionValue);
		bool IsLiteral(CStr* pExpressionValue, DWORD* pdwTypeValue);
		bool IsSingleVariable(CStr* pExpressionValue);
		bool IsLabel(CStr* pExpressionValue);
		bool IsComplexVariable(CStr* pExpressionValue);
		bool IsAnything(CStr* pExpressionValue);

		bool SearchForFunction(CStr* pExpressionValue);

		DWORD ChopOffOneItemFromLeft(CStr* pStr);
		DWORD ChopOffOneItemFromRight(CStr* pStr);

//		DWORD DetermineInternalCommandCode(DWORD dwMathSymbol, DWORD dwTypeValue);
		bool DetermineIfPointerMaths(DWORD dwMathSymbol, DWORD dwTypeValue);

		bool WriteDBM(void);
		bool WriteDBMBit(DWORD dwLineNumber);
		bool WriteDBMLine(DWORD dwLineNumber, LPSTR pText, LPSTR pResult);

		// Tokenizer (extracted subsystem)
		[[nodiscard]] CTokenizer& GetTokenizer() noexcept { return m_tokenizer; }
		[[nodiscard]] const CTokenizer& GetTokenizer() const noexcept { return m_tokenizer; }

		// ExpressionParser (extracted subsystem)
		[[nodiscard]] CExpressionParser& GetExpressionParser() noexcept { return m_expressionParser; }
		[[nodiscard]] const CExpressionParser& GetExpressionParser() const noexcept { return m_expressionParser; }

	private:
		CTokenizer m_tokenizer;
		CExpressionParser m_expressionParser;

		// Debug Data
		DWORD			m_dwLineNumber;

		// Result Data
		CResultData		m_Result;

		// Used when returnning type of L-value to original @address of structure
		DWORD			m_dwOffsetLValueTypeValue;

		// Used when need to determine if CONCAT Flag used in params (printc)
		bool			m_bConcatFlagUsed;

		// Further math operations
		DWORD			m_dwMathSymbol;
		std::unique_ptr<CMathOp>	m_pLeftMathOp;
		std::unique_ptr<CMathOp>	m_pRightMathOp;

		// Seen as a work class..
		std::unique_ptr<CStatement>	m_pStatement;

		// Hierarchy Data
		std::unique_ptr<CMathOp>	m_pNext;
};

#endif // !defined(AFX_STATEMENT_H__2A1543E2_9870_4E5E_B056_C09192997E8D__INCLUDED_)
