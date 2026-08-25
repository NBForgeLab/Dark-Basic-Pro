// MathOp.cpp: implementation of the CMathOp class.
//
//////////////////////////////////////////////////////////////////////
#include "ParserHeader.h"
#include "StringUtils.h"

// Custom Includes
#include "VarTable.h"
#include "DataTable.h"
#include "Statement.h"
#include "LabelTable.h"
#include "StructTable.h"
#include "ParseFunction.h"
#include "ParseInstruction.h"
#include "InstructionTable.h"
#include "DBPCompiler.h"

// for the POW function
#include "math.h"

// External Class Pointers
extern CVarTable *g_pVarTable;
extern CDataTable *g_pDataTable;
extern CDataTable *g_pStringTable;
extern CStructTable *g_pStructTable;
extern CInstructionTable *g_pInstructionTable;
extern CLabelTable* g_pLabelTable;
extern CDBPCompiler* g_pDBPCompiler;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CMathOp::CMathOp()
{
	m_dwLineNumber=0;

	m_Result.m_pStringToken.reset();
	m_Result.m_pAdditionalOffset.reset();
	m_Result.m_dwType=0;
	m_Result.m_dwDataOffset=0;

	m_dwOffsetLValueTypeValue=0;
	m_bConcatFlagUsed=false;

	m_dwMathSymbol=0;
}

CMathOp::~CMathOp()
{
	// m_Result cleanup handled by unique_ptr members automatically
}

void CMathOp::Add(CMathOp* pNext)
{
	if(!m_pNext)
		m_pNext.reset(pNext);
	else
		m_pNext->Add(pNext);
}

void CMathOp::SetResult(std::string_view stringToken, DWORD dwType, DWORD dwDataOffset)
{
	m_Result.m_pStringToken = std::make_unique<CStr>(stringToken);
	m_Result.m_dwType = dwType;
	m_Result.m_dwDataOffset = dwDataOffset;
}

void CMathOp::SetResultData(CResultData ResultData)
{
	m_Result = ResultData; // deep copy via CResultData copy assignment
}

void CMathOp::SetArrayOffsetResult(std::string_view stringToken)
{
	m_Result.m_pAdditionalOffset = std::make_unique<CStr>(stringToken);
}

CStr* CMathOp::FindResultStringTokenForDBM(void)
{
	if(m_pNext==nullptr && m_Result.m_pStringToken)
	{
		// Translate result value
		if(TranslateStringTokenForDBM()==false)
		{
			return nullptr;
		}
		return m_Result.m_pStringToken.get();
	}
	else
	{
		if(m_pNext)
			return m_pNext->FindResultStringTokenForDBM();
		else
			return nullptr;
	}
}

CResultData* CMathOp::FindResultDataForDBM(void)
{
	if(m_pNext==nullptr && m_Result.m_pStringToken)
	{
		// Translate result value
		if(TranslateStringTokenForDBM()==false)
		{
			return nullptr;
		}
		return &m_Result;
	}
	else
	{
		if(m_pNext)
			return m_pNext->FindResultDataForDBM();
		else
			return nullptr;
	}
}

DWORD CMathOp::FindResultTypeValueForDBM(void)
{
	if(m_pNext==nullptr && m_Result.m_pStringToken)
	{
		// Result value
		return m_Result.m_dwType;
	}
	else
	{
		if(m_pNext)
			return m_pNext->FindResultTypeValueForDBM();
		else
			return 0;
	}
}

CResultData* CMathOp::GetResultData(void)
{
	return &m_Result;
}

CResultData* CMathOp::FindResultData(void)
{
	if(m_pNext==nullptr && m_Result.m_pStringToken)
	{
		// Translate result value
		if(TranslateStringTokenForDBM()==false)
		{
			return nullptr;
		}
		// Result
		return &m_Result;
	}
	else
	{
		if(m_pNext)
			return m_pNext->FindResultData();
		else
			return nullptr;
	}
}

bool CMathOp::TranslateStringTokenForDBM(void)
{
	// Ensure string present..
	if(m_Result.m_pStringToken==nullptr)
	{
		// Nothing to translate
		return true;
	}

	// Translate and resolve item described by string
	CResultData* pResultPtr = GetResultData();
	m_Result.m_pStringToken->TranslateForDBM(pResultPtr);
	if(m_Result.m_pAdditionalOffset) m_Result.m_pAdditionalOffset->TranslateForDBM(pResultPtr);

	// Complete
	return true;
}

bool CMathOp::DoValue(CStr* pExpression)
{
	DWORD dwType=0;
	DWORD dwExpValueType=0;

	// Before processing, crop spaces, tabs and equal brackets
	do { pExpression->EatEdgeSpacesandTabs(nullptr);
	} while(pExpression->CropEqualEdgeBrackets(nullptr)==true);

	// Upper-case comparisons
	CStr UpperExpression = CStr(pExpression->GetStr());
	UpperExpression.MakeUpper();

	// If starts with "*", it can only be a pointer assignment
	DWORD dwPosition=0, dwMathSymbol=0, dwMathSymbolWidth=0, dwSciNot=0;
	if(UpperExpression.GetChar(0)!='*')
	{
		// Find Best Math Symbol
		if(FindHighestPres(&UpperExpression, &dwPosition, &dwMathSymbol, &dwMathSymbolWidth, &dwSciNot)==false)
		{
			g_pErrorReport->AddErrorString("Failed to 'CMathOp::DoValue::FindHighestPres'");
			return false;
		}

		// LEEFIX - 171002 - If negative symbol used as first char (ie -99 or -a#), only literals are direct values
		if(UpperExpression.GetChar(0)=='-')
		{
			DWORD dwLiteralType=0;
			if(IsLiteral(&UpperExpression, &dwLiteralType)==false)
			{
				dwPosition=0;
				dwMathSymbol=5;
				dwMathSymbolWidth=1;
			}
		}
	}

	// Record Line Number at math operation
	DWORD StatementLineNumber = g_pStatementList->GetTokenLineNumber();
	SetLineNumber(StatementLineNumber);

	// Direct Value or another MathOp split
	if(dwMathSymbol==0)
	{
		// No math symbol, only one value here
		m_dwMathSymbol=0;

		// Determine if function, array, variable or value
		std::unique_ptr<CStr> pExpressionValue;
		if(pExpression)
		{
			// Create the exp value
			pExpressionValue = std::make_unique<CStr>(1);
			pExpressionValue->SetText(pExpression);

			// leeadd - U71 - 111008 - if scientific notation
			if ( dwSciNot>0 )
			{
				// find seperation between first and second values (XX e [+/-] YY)
				pExpressionValue->ResolveSciNot();
			}

			// Check what kind of value it is
			dwType=0;
			dwExpValueType=0;

			if(g_pStatementList->GetAllowLabelAsValue()==true)
			{
				if(dwExpValueType==0 && IsLabel(pExpressionValue.get()))			dwExpValueType=5;
			}
			if(dwExpValueType==0 && IsFunction(pExpressionValue.get()))			dwExpValueType=1;
			if(dwExpValueType==0 && IsReserved(pExpressionValue.get()))			dwExpValueType=6;
			if(dwExpValueType==0 && IsLiteral(pExpressionValue.get(), &dwType))	dwExpValueType=2;
			if(dwExpValueType==0 && IsSingleVariable(pExpressionValue.get()))		dwExpValueType=3;
			if(dwExpValueType==0 && IsComplexVariable(pExpressionValue.get()))	dwExpValueType=4;
			if(dwExpValueType>0)
			{
				if(dwExpValueType==1)
				{
					if(DoValueFunction(pExpressionValue.get())==false)
					{
						return false;
					}
				}
				if(dwExpValueType==2)
				{
					if(DoValueLiteral(pExpressionValue.get(), dwType)==false)
					{
						g_pErrorReport->AddErrorString("Failed to DoValueLiteral");
						return false;
					}
				}
				if(dwExpValueType==3)
				{
					if(DoValueSingleVariable(pExpressionValue.get())==false)
					{
						g_pErrorReport->AddErrorString("Failed to DoValueSingleVariable");
						return false;
					}
				}
				if(dwExpValueType==4)
				{
					if(DoValueComplexVariable(pExpressionValue.get())==false)
					{
						return false;
					}
				}
				if(dwExpValueType==5)
				{
					if(DoValueLabel(pExpressionValue.get())==false)
					{
						g_pErrorReport->AddErrorString("Failed to DoValueLabel");
						return false;
					}
				}
				if(dwExpValueType==6)
				{
					DWORD dwLine = g_pStatementList->GetTokenLineNumber();
					g_pErrorReport->SetError(dwLine, ERR_SYNTAX+60, pExpressionValue->GetStr());
					return false;
				}
				pExpressionValue.reset();
			}
		}

		// If still have this expression, report error
		if(pExpressionValue)
		{
			DWORD dwLine = g_pStatementList->GetTokenLineNumber();
			g_pErrorReport->SetError(dwLine, ERR_SYNTAX+1, pExpressionValue->GetStr());
			return false;
		}
	}
	else
	{
		// Get Both Sides as strings
		std::unique_ptr<char[]> pLeftText(pExpression->GetLeftOfPosition(dwPosition));
		std::unique_ptr<char[]> pRightText(pExpression->GetRightOfPosition(dwPosition+dwMathSymbolWidth));
		CStr StrLeft = CStr(pLeftText.get());
		CStr StrRight = CStr(pRightText.get());
		// LEEFIX - 201102 - Added prechop space eat as a string such as " -1" would actually chop before the minus
		// LEEFIX - 211102 - Put dwLeftEaten/dwRightEaten as the rightmostpos was being calculated incorrectly for later
		DWORD dwLeftEaten=0, dwRightEaten=0;
		StrLeft.EatEdgeSpacesandTabs(&dwLeftEaten);
		StrRight.EatEdgeSpacesandTabs(&dwRightEaten);
		DWORD dwLeftMostPos = ChopOffOneItemFromRight(&StrLeft);
		DWORD dwRightMostPos = ChopOffOneItemFromLeft(&StrRight);
		dwLeftMostPos+=dwLeftEaten;
		dwRightMostPos+=dwRightEaten;
		StrLeft.EatEdgeSpacesandTabs(nullptr);
		StrRight.EatEdgeSpacesandTabs(nullptr);
		dwRightMostPos=dwPosition+dwMathSymbolWidth+dwRightMostPos;

// LEEFIX - 230604 - IS IT NEEDED ANY MORE? AS -X IS DONE ELSEWHEER (I THINK)
//		// DOES THIS WORK FOR ALL UNARY OPERATORS?
//		// A blank literal is translated to a numeric zero (so -99 becomes 0-99)
//		if(strcmp(StrLeft.GetStr(),"") == 0)
//			StrLeft.SetText("0");

		// for NOT cases, copy string to left side
		if(dwMathSymbol==43)
		{
			// A NOT B [B-this part ignored in compiler]
			StrLeft.SetText(StrRight.GetStr());

			// leefix - 250604 - u54 - wiped this out as user functions could be called twice!
			StrRight.SetText("");
		}
		else
		{
			// leefix - 230604 - u54 - in all other on-uniary cases, a missing operand is an error
			// except dwMathSymbol==5 (negative value)
			if ( dwMathSymbol==5 )
			{
				// A blank literal is translated to a numeric zero (so -99 becomes 0-99)
				if(strcmp(StrLeft.GetStr(),"")==0)
					StrLeft.SetText("0");
			}
			else
			{
				if(strcmp(StrLeft.GetStr(),"")==0
				|| strcmp(StrRight.GetStr(),"")==0 )
				{
					g_pErrorReport->SetError(StatementLineNumber, ERR_SYNTAX+59);
					m_pRightMathOp.reset();
					m_pLeftMathOp.reset();
					return false;
				}
			}
		}

		// Found a math symbol (xxx * xxx)
		m_dwMathSymbol=dwMathSymbol;

		// Traverse each side for final values
		m_pLeftMathOp = std::make_unique<CMathOp>();
		if(m_pLeftMathOp)
		{
			if(m_pLeftMathOp->DoValue(&StrLeft)==false)
			{
// lee - 150306 - u60b3 - surfaced at end user level, replaced with line number error
//				g_pErrorReport->AddErrorString("Failed to 'DoValue::m_pLeftMathOp->DoValue'");
				g_pErrorReport->SetError(StatementLineNumber, ERR_SYNTAX+1, StrLeft.GetStr() );
				m_pLeftMathOp.reset();
				return false;
			}
		}
		m_pRightMathOp = std::make_unique<CMathOp>();
		if(m_pRightMathOp)
		{
			if(m_pRightMathOp->DoValue(&StrRight)==false)
			{
// lee - 150306 - u60b3 - surfaced at end user level, replaced with line number error
//				g_pErrorReport->AddErrorString("Failed to 'DoValue::m_pRightMathOp->DoValue'");
				g_pErrorReport->SetError(StatementLineNumber, ERR_SYNTAX+1, StrRight.GetStr() );
				m_pLeftMathOp.reset();
				m_pRightMathOp.reset();
				return false;
			}
		}

		// Conversion Table A + B = C type result handling
		DWORD dwLType=m_pLeftMathOp->FindResultTypeValueForDBM();
		DWORD dwRType=m_pRightMathOp->FindResultTypeValueForDBM();

		// Check if right param has offset-lvalue-typevalue
		if(m_pRightMathOp->GetResultOffsetLValueTypeValue()>0)
			m_dwOffsetLValueTypeValue=m_pRightMathOp->GetResultOffsetLValueTypeValue();

		// If L-Value is non-specified userdatatype, assign LValue gathered from offset field
		if(dwLType==1001 && m_dwOffsetLValueTypeValue>0)
		{
			dwLType=100+m_dwOffsetLValueTypeValue;
		}
		if(dwLType==1101 && m_dwOffsetLValueTypeValue>0)
		{
			dwLType=100+m_dwOffsetLValueTypeValue;
		}

		// Determine Result Type
		bool bSkipMathInputCasting=false;
// LEEFIX - 281102 - Type Mode can only be a number from 0-99
//		DWORD dwTypeMode=dwLType%100;
		DWORD dwTypeMode=dwLType%100;

//lee, why does 'not equal' add three params to stack????

		// Determine if cast required
		if(dwLType==1 && dwRType==1) dwTypeMode=1;
		if(dwLType==2 && dwRType==2) dwTypeMode=2;
		if(dwLType==4 && dwRType==4) dwTypeMode=4;
		if(dwLType==5 && dwRType==5) dwTypeMode=5;
		if(dwLType==6 && dwRType==6) dwTypeMode=6;
		if(dwLType==7 && dwRType==7) dwTypeMode=7;
		if(dwLType==9 || dwRType==9) dwTypeMode=9;
		if(dwLType==2 || dwRType==2) dwTypeMode=2;
		if(dwLType==8 || dwRType==8) dwTypeMode=8;
		// LEEFIX - 281102 - Added additional checks to arrive at correct result type
		if(dwLType==102 || dwRType==102)
			dwTypeMode=2;
		if(dwLType==109 || dwRType==109) dwTypeMode=9;
		if(dwLType==108 || dwRType==108) dwTypeMode=8;
		if((dwLType==3 || dwLType==103) && (dwRType==3 || dwRType==103))
		{
			// Comparisons return Integer, not type-specific value
			bSkipMathInputCasting=true;
			if(m_dwMathSymbol>=22 && m_dwMathSymbol<=27)
				dwTypeMode=1;
			else
			{
				// lee - 290306 - u6rc3 - only ADDITION is supported by strings
				if ( m_dwMathSymbol==4 )
				{
					dwTypeMode=3;
				}
				else
				{
					// tried to subtract or power two strings
					g_pErrorReport->SetError(StatementLineNumber, ERR_SYNTAX+11);
					m_pRightMathOp.reset();
					m_pLeftMathOp.reset();
					return false;
				}
			}
		}
		if((dwLType%100>=4 && dwLType%100<=7) || (dwLType%100>=4 && dwLType%100<=7))
		{
			// Comparisons against BOOL,BYTE,WORD,DWORD should use 4 bytes
			//if(m_dwMathSymbol>=22 && m_dwMathSymbol<=27)
			// Plus any sort of maths needs to hold a value greater than its datatype
			// leefix - 040803 - except where floats or double floats are concerned..
			if ( dwLType%100!=2 && dwLType%100!=8 && dwRType%100!=2 && dwRType%100!=8 )
				dwTypeMode=7;
		}
		if(dwLType==10 || dwRType==10) dwTypeMode=0;
		if(dwLType==20 || dwRType==20) dwTypeMode=0;

		// Maybe a range of 'unique' type values..
		if(dwLType>=1001 && dwLType==dwRType) dwTypeMode=dwLType;

		// leefix - 050803 - Attempts to assign or compare a type and non-type results in failure
		if(dwLType>=1001 && dwRType<1000) dwTypeMode=0;
		if(dwRType>=1001 && dwLType<1000) dwTypeMode=0;

		// Types are in-compatable
		if(dwTypeMode==0)
		{
			LPSTR pL = m_pLeftMathOp->FindResultStringTokenForDBM()->GetStr();
			LPSTR pR = m_pRightMathOp->FindResultStringTokenForDBM()->GetStr();
			g_pErrorReport->SetError(StatementLineNumber, ERR_SYNTAX+2, pL, pR);
			m_pRightMathOp.reset();
			m_pLeftMathOp.reset();
			return false;
		}

		// Only cast if regular values, not pointers..
// LEEFIX - 281102 - This would miss something like ( 0 - array#(x) )
//		if(dwLType<100 && dwRType<100 && bSkipMathInputCasting==false)
		if((dwLType<100 || dwRType<100) && bSkipMathInputCasting==false)
		{
			// First Cast Left Or Right 'not' matching produced type
// LEEFIX - 281102 - As the values can be var or array, ensure this does not affect check
			DWORD dwRealLValue = dwLType%100;
			DWORD dwRealRValue = dwRType%100;
			if(dwRealLValue!=dwTypeMode || dwRealRValue!=dwTypeMode)
			{
				if(dwRealLValue!=dwTypeMode)
				{
					if(DoCastOnMathOp(m_pLeftMathOp, dwTypeMode)==false)
					{
						LPSTR pL = m_pLeftMathOp->FindResultStringTokenForDBM()->GetStr();
						LPSTR pR = m_pRightMathOp->FindResultStringTokenForDBM()->GetStr();
						g_pErrorReport->SetError(StatementLineNumber, ERR_SYNTAX+2, pL, pR);
						m_pRightMathOp.reset();
						m_pLeftMathOp.reset();
						return false;
					}
				}
				if(dwRealRValue!=dwTypeMode)
				{
					if(DoCastOnMathOp(m_pRightMathOp, dwTypeMode)==false)
					{
						LPSTR pL = m_pLeftMathOp->FindResultStringTokenForDBM()->GetStr();
						LPSTR pR = m_pRightMathOp->FindResultStringTokenForDBM()->GetStr();
						g_pErrorReport->SetError(StatementLineNumber, ERR_SYNTAX+2, pL, pR);
						m_pRightMathOp.reset();
						m_pLeftMathOp.reset();
						return false;
					}
				}
			}
		}
		else
		{
			// If L-Value is array, must cast to its actual datatype
			if(dwLType>100 && dwRType!=7 && dwRType!=3)
			{
				// leefix - U71 - 081208 - in the case of T(1).int * T(1).float, the float is cast to INT (making 400 * 0.5 = 0 wrong)
				// so we make a check to see if either is FLOAT vs INT and cast the integers to floats to make above work
				DWORD dwActualLType = dwLType-100;
				if ( dwLType==101 && dwRType==102 )
				{
					dwActualLType = 2;
					if(DoCastOnMathOp(m_pLeftMathOp, dwActualLType)==false)
					{
						LPSTR pL = m_pLeftMathOp->FindResultStringTokenForDBM()->GetStr();
						LPSTR pR = m_pRightMathOp->FindResultStringTokenForDBM()->GetStr();
						g_pErrorReport->SetError(StatementLineNumber, ERR_SYNTAX+3, pL, pR);
						m_pRightMathOp.reset();
						m_pLeftMathOp.reset();
						return false;
					}
				}
				else
				{
					if(DoCastOnMathOp(m_pRightMathOp, dwActualLType)==false)
					{
						LPSTR pL = m_pLeftMathOp->FindResultStringTokenForDBM()->GetStr();
						LPSTR pR = m_pRightMathOp->FindResultStringTokenForDBM()->GetStr();
						g_pErrorReport->SetError(StatementLineNumber, ERR_SYNTAX+3, pL, pR);
						m_pRightMathOp.reset();
						m_pLeftMathOp.reset();
						return false;
					}
				}

				// Get Actual Type Value
				if(m_dwMathSymbol>=22 && m_dwMathSymbol<=27)
				{
					// No need to adjust result type for comparisons!
				}
				else
				{
					// If Array succeeds in recasting right, then output maybe changed
					dwTypeMode=dwActualLType;
				}
			}

			// LEEFIX - 101102 - Ensure R-Value is cast to Desired Type (if comparison) ie a#>b(0)
			if(m_dwMathSymbol>=22 && m_dwMathSymbol<=27 && bSkipMathInputCasting==false)
			{
				if(dwRType%100 != dwTypeMode%100)
				{
					if(DoCastOnMathOp(m_pRightMathOp, dwTypeMode)==false)
					{
						LPSTR pL = m_pLeftMathOp->FindResultStringTokenForDBM()->GetStr();
						LPSTR pR = m_pRightMathOp->FindResultStringTokenForDBM()->GetStr();
						g_pErrorReport->SetError(StatementLineNumber, ERR_SYNTAX+3, pL, pR);
						m_pRightMathOp.reset();
						m_pLeftMathOp.reset();
						return false;
					}
				}
			}

			// LEEFIX - 040803 - Ensure L-Value is ALSO cast to Desired Type (if comparison) ie byte(10)>byte(10)
			if(m_dwMathSymbol>=22 && m_dwMathSymbol<=27 && bSkipMathInputCasting==false)
			{
				if(dwLType%100 != dwTypeMode%100)
				{
					if(DoCastOnMathOp(m_pLeftMathOp, dwTypeMode)==false)
					{
						LPSTR pL = m_pLeftMathOp->FindResultStringTokenForDBM()->GetStr();
						LPSTR pR = m_pRightMathOp->FindResultStringTokenForDBM()->GetStr();
						g_pErrorReport->SetError(StatementLineNumber, ERR_SYNTAX+3, pL, pR);
						m_pRightMathOp.reset();
						m_pLeftMathOp.reset();
						return false;
					}
				}
			}
		}
		
		// Once params are properly cast, comparisons ALWAYS return integer
		if(m_dwMathSymbol>=22 && m_dwMathSymbol<=27)
		{
			// Comparison always returns an integer result
			dwTypeMode=1;
		}

		// Ensure temp return type holds direct value
		if(dwTypeMode>100 && dwTypeMode<1000) dwTypeMode-=100;

		// Return value to a Temporary Produced Item
		CStr TempVarToken(1);
		ProduceNewTempToken(&TempVarToken, dwTypeMode);

		// Take Remainder of String and combine with temp var token from math result
		std::unique_ptr<char[]> pRemainderLeftText(pExpression->GetLeftOfPosition(dwLeftMostPos));
		std::unique_ptr<char[]> pRemainderRightText(pExpression->GetRightOfPosition(dwRightMostPos));
		CStr StrRemainderLeft = CStr(pRemainderLeftText.get());
		CStr StrRemainderRight = CStr(pRemainderRightText.get());
		CStr strNewString(1);
		strNewString.SetText(&StrRemainderLeft);
		strNewString.AddText(&TempVarToken);
		strNewString.AddText(&StrRemainderRight);

		// Store Result String Token
		SetResult(TempVarToken.GetStr(), dwTypeMode, 0);
		SetResultStruct(nullptr);

		// When Right Operand L-Value found, record and send it back...
		DWORD dwDiscoveredOffsetLValueTypeValue = GetResultOffsetLValueTypeValue();
		if(m_pRightMathOp->GetResultOffsetLValueTypeValue()>0) dwDiscoveredOffsetLValueTypeValue = m_pRightMathOp->GetResultOffsetLValueTypeValue();

		// Traverse until no more
		auto pAnotherMathOp = std::make_unique<CMathOp>();
		{
			// Travserse remaining maths
			pAnotherMathOp->m_dwOffsetLValueTypeValue = dwDiscoveredOffsetLValueTypeValue;
			if(pAnotherMathOp->DoValue(&strNewString)==false)
			{
				g_pErrorReport->AddErrorString("Failed to 'DoValue::pAnotherMathOp->DoValue'");
				m_pRightMathOp.reset();
				m_pLeftMathOp.reset();
				return false;
			}

			// If new math item redundant (it doesn't do maths, delete it)
			if(pAnotherMathOp->GetMathSymbol()==0)
			{
				pAnotherMathOp.reset();
			}

			// Ensure Offset L-Value is not forgotten
			if(pAnotherMathOp && dwDiscoveredOffsetLValueTypeValue>0)
			{
				pAnotherMathOp->m_dwOffsetLValueTypeValue = dwDiscoveredOffsetLValueTypeValue;
			}

			// Add onto chain if still valid
			CMathOp* pAddedMathOp = pAnotherMathOp.get();
			if(pAnotherMathOp) Add(pAnotherMathOp.release());

			// Carry back Offset LValue Type Value
			if(pAddedMathOp)
				m_dwOffsetLValueTypeValue = pAddedMathOp->GetResultOffsetLValueTypeValue();
			else
				m_dwOffsetLValueTypeValue = dwDiscoveredOffsetLValueTypeValue;
		}
	}

	// Complete
	return true;
}

bool CMathOp::DoCastOnMathOp(std::unique_ptr<CMathOp>& pMathOp, DWORD dwTypeWant)
{
	// Produce Temp to hold cast
	CStr pTempVarToken("");
	ProduceNewTempToken(&pTempVarToken, dwTypeWant);

	// Produce Code to specify new type (in math instruction)
	CStr pTypeCodeStr("");
	pTypeCodeStr.SetNumericText(dwTypeWant);

	// Value as it is originally
	CMathOp* pValueToCast = pMathOp.get();

	// Determine what I am ultimately casting into
	DWORD dwTypeHave = pValueToCast->FindResultTypeValueForDBM();

	if(dwTypeWant==501)
	{
		return true;
	}

	// Some types simply cannot be cast
	if(((dwTypeWant%100)==3 && (dwTypeHave%100)!=3) || ((dwTypeHave%100)==3 && (dwTypeWant%100)!=3))
	{
		return false;
	}

	// Some types are different, but dont need casting
	if((dwTypeWant==4 && dwTypeHave==5) || (dwTypeHave==4 && dwTypeWant==5))
	{
		return true;
	}

	// Ensure relative pointers to data merely revert to the datatype value it ultimately uses 
	if(dwTypeHave>100 && dwTypeHave<1000)
	{
		// indirect addressing 200-299
		if(dwTypeHave>200)
			dwTypeHave-=200;
		else
			dwTypeHave-=100;
	}
	if(dwTypeHave==1001) dwTypeHave=7;
	if(dwTypeHave==1101) dwTypeHave=7;

	// If no cast required, skip new cast task
	if(dwTypeHave==dwTypeWant)
	{
		return true;
	}

	// Create a Cast Math Instruction
	auto pNewMath = std::make_unique<CMathOp>();
	pNewMath->m_dwLineNumber=GetLineNumber();
	pNewMath->m_pLeftMathOp=std::move(pMathOp);
	pNewMath->m_pRightMathOp = std::make_unique<CMathOp>();
	pNewMath->m_pRightMathOp->DoValue(&pTypeCodeStr);

	// Produce result token as var
	pNewMath->SetResult(pTempVarToken.GetStr(), dwTypeWant, 0);
	pNewMath->SetResultStruct(nullptr);

	// Types that are ptrs, can only be cast to actual datatypes
	if(dwTypeWant>=101 && dwTypeWant<=109) dwTypeWant-=100;

	// Work out Casting Math Symbol (ie 101-110 L to ??? 111-120 F to ???)
	pNewMath->m_dwMathSymbol=100+dwTypeWant+((dwTypeHave-1)*10);

	// Assign to original math chain
	pMathOp = std::move(pNewMath);

	// Complete
	return true;
}

bool CMathOp::DoValueFunction(CStr* pExpressionValue)
{
	//
	//	TODO: This looks a lot like certain other methods and seems to duplicate some functionality. This method should
	//	      be reviewed and refactored.
	//

	// Process Value as Function
	auto pFunctionNameString = std::make_unique<CStr>(1);
	auto pFunctionDataString = std::make_unique<CStr>(1);
	pFunctionNameString->SetText(pExpressionValue);
	pFunctionDataString->SetText(pExpressionValue);
	DWORD dwPos = pFunctionDataString->FindFirstChar('(');
	{
		std::unique_ptr<char[]> pLeft(pFunctionNameString->GetLeftOfPosition(dwPos));
		std::unique_ptr<char[]> pRight(pFunctionDataString->GetRightOfPosition(dwPos));
		pFunctionNameString->SetText(pLeft.get());
		pFunctionDataString->SetText(pRight.get());
	}

	// Get Details of instruction
	CInstructionTableEntry* pRef = g_pStatementList->GetInstructionRef();
	DWORD dwInstructionType = g_pStatementList->GetInstructionType();
	DWORD dwInstructionValue = g_pStatementList->GetInstructionValue();
	DWORD dwInstructionParamMax = g_pStatementList->GetInstructionParamMax();

	// Create param chain from param string
	m_pStatement = std::make_unique<CStatement>();
	CParameter* pFirstParameterRaw = nullptr;
	if(m_pStatement->DoParameterListString(pFunctionDataString.get(), &pFirstParameterRaw)==false)
	{
		g_pErrorReport->SetError ( g_pStatementList->GetLineNumber(), ERR_SYNTAX+55, pFunctionNameString->GetStr() );
		delete pFirstParameterRaw;
		m_pStatement.reset();
		return false;
	}
	std::unique_ptr<CParameter> pFirstParameter(pFirstParameterRaw);

	// Work out if right number of parameterd parsed
	DWORD dwParamCount=0;
	CParameter* pCurrent = pFirstParameter.get();
	while(pCurrent)
	{
		dwParamCount++;
		pCurrent = pCurrent->GetNext();
	}

	// pTrack kicks in on 'second' interation (search list bby entry, not by index)
	CInstructionTableEntry* pTrack = nullptr;

	// Special invalid param reason
	int iInvalidParamReason=0;
	std::unique_ptr<char[]> pValidEntryStr;
	std::unique_ptr<char[]> pFunctionTypeStr;

	// Scan Each Matching Instruction and find match with parameters used, else error
	DWORD dwScoreBestMatch=0;
	DWORD dwBestScoreSoFar=0;
	DWORD StatementLineNumber = g_pStatementList->GetLineNumber();
	DWORD dwValidInstructionToUse=0;
	CInstructionTableEntry* pFirstEntryRef=nullptr; 
	CInstructionTableEntry* pNextEntryRef=nullptr; 
	DWORD dwValidInstructionValue=0, dwValidParamMax=0;
	DWORD dwTryingIndex=0;

	// LEEFIX - 201102 - CALL DLL has more than 16 instances, so increased to 32 instances..
	DWORD dwTryInstruction;
	for(dwTryingIndex=0; dwTryingIndex<32; dwTryingIndex++)
	{
		// Try This Instruction
		dwTryInstruction = dwInstructionValue + dwTryingIndex;
		if(pTrack) dwTryInstruction = pTrack->GetInternalID();

		// Check for parameter-mismatch
		bool bInValidParams=false;
		CInstructionTableEntry* pValidEntryRef=nullptr; 
		CStr* pValidParamTypes = nullptr;
		if(dwInstructionType==1)
		{
			dwValidInstructionValue=dwInstructionValue;
			dwValidParamMax=dwInstructionParamMax;
		}
		if(dwInstructionType==2)
			if(g_pInstructionTable->FindInstructionParams(dwTryInstruction, dwParamCount, &dwValidInstructionValue, &dwValidParamMax, &pValidParamTypes, &pValidEntryRef)==false)
				bInValidParams=true;
		if(dwInstructionType==3)
		{
			// leenote - 230306 - u6b4 - odd that there are two areas to parse and validate a user function call ( based on a=userfunc() and userfunc() )
			// if(g_pInstructionTable->FindUserFunctionParams(dwTryInstruction, dwParamCount, &dwValidInstructionValue, &dwValidParamMax, &pValidParamTypes, &pValidEntryRef)==false)
			if(g_pInstructionTable->FindUserFunctionParams(dwTryInstruction, dwParamCount, &dwValidInstructionValue, &dwValidParamMax, &pValidParamTypes, &pValidEntryRef)==true)
			{
				// lee - 230306 - u6b4 - copied from Statement.cpp line 5641 (handle UDT missue)
				// bInValidParams=true;
				if ( bInValidParams==false )
				{
					CDeclaration* pDecChain = pValidEntryRef->GetDecChain();
					if ( pDecChain )
					{
						CDeclaration* pDec = pDecChain->GetNext();
						CParameter* pParamWalk = pFirstParameter.get();
						while ( pParamWalk && pDec )
						{
							DWORD dwDataType = pParamWalk->GetMathItem()->FindResultTypeValueForDBM();
							if ( dwDataType>=1001 )
							{
								// UDT vars passed in is ok
								if ( dwDataType==1001 )
								{
									CStr* pFunctionTypeName = pDec->GetType();
									CResultData* pParamData = pParamWalk->GetMathItem()->FindResultData();
									if ( pParamData )
									{
										CStr* pParamTypeName = pParamData->m_pStruct->GetTypeName();
										if ( !dbp::iequals( pFunctionTypeName->GetStr(), pParamTypeName->GetStr() ) )
										{
											iInvalidParamReason=2;
											pValidEntryStr.reset(new char [ strlen(pValidEntryRef->GetName()->GetStr())+1 ]);
											snprintf(pValidEntryStr.get(), strlen(pValidEntryRef->GetName()->GetStr())+1, "%s", pValidEntryRef->GetName()->GetStr());
											pFunctionTypeStr.reset(new char [ strlen(pFunctionTypeName->GetStr())+1 ]);
											snprintf(pFunctionTypeStr.get(), strlen(pFunctionTypeName->GetStr())+1, "%s", pFunctionTypeName->GetStr());
											//g_pErrorReport->SetError(g_pStatementList->GetTokenLineNumber(), ERR_SYNTAX+8, pValidEntryRef->GetName()->GetStr(), pFunctionTypeName->GetStr());
											bInValidParams=true;
											break;
										}
									}
								}

								// UDT arrays not supported
								if ( dwDataType==1101 )
								{
									iInvalidParamReason=3;
									//g_pErrorReport->SetError(g_pStatementList->GetTokenLineNumber(), ERR_SYNTAX+64);
									bInValidParams=true;
									break;
								}
							}
							pParamWalk = pParamWalk->GetNext();
							pDec = pDec->GetNext();
						}
					}
				}
			}
			else
			{
				// lee - 180406 - u6rc10 - as originally done, if not find function, also invalid
				bInValidParams=true;
			}
		}

		// Ensure first instruction entry ref is recorded
		if(pFirstEntryRef==nullptr)
			pFirstEntryRef=pValidEntryRef;
		else
			pNextEntryRef=pValidEntryRef;

		// If instruction name matches original, continue
		if(pFirstEntryRef && pNextEntryRef)
			if(g_pInstructionTable->CompareInstructionNames(pFirstEntryRef, pNextEntryRef)==false)
				break;

		// Validate type of parameters to call
		if(pFirstParameter && pValidParamTypes)
		{
			dwScoreBestMatch=0;
			if(pFirstParameter->ValidateWithCorrectCall(pValidParamTypes, &dwScoreBestMatch, 0)==false)
			{
				bInValidParams=true;
			}
		}
		else
		{
			// if parse-has no params and function HAS params, no dice
			if(pFirstParameter==nullptr)
			{
				if(pValidEntryRef && pValidEntryRef->GetParamMax()>0)
				{
					bInValidParams=true;
				}
			}
		}

		// Must have a return type, or ref function cannot be a function
		if(pValidEntryRef)
		{
			if(pValidEntryRef->GetReturnParam()==0)
			{
				iInvalidParamReason=1;
				bInValidParams=true;
			}
		}

		// Store instruction that passes all checks
		if(bInValidParams==false)
		{
			if(dwScoreBestMatch>=dwBestScoreSoFar)
			{
				dwBestScoreSoFar=dwScoreBestMatch;
				dwValidInstructionToUse=dwValidInstructionValue;
				if(pValidEntryRef) pRef=pValidEntryRef;
				dwScoreBestMatch=0;
			}
		}

		// Try again with next matching instruction
		if(pTrack==nullptr) pTrack = pValidEntryRef;
		if(pTrack)
		{
			pTrack = pTrack->GetNext();
			if(pTrack==nullptr) break;
		}

	}
	if(dwValidInstructionToUse>0)
	{
		// Use This Instruction (values retained as left immediately)
		dwValidInstructionValue=dwValidInstructionValue;
		dwValidParamMax=dwValidParamMax;
	}
	else
	{
		// Parameter-mismatch
		if(iInvalidParamReason==0)
		{
			CStr* pParamDesc=nullptr;
			if(pRef) pParamDesc=pRef->GetFullParamDesc();
			if(pParamDesc)
				g_pErrorReport->SetError(StatementLineNumber, ERR_SYNTAX+8, pFunctionNameString->GetStr(), pParamDesc->GetStr());
			else
				g_pErrorReport->SetError(StatementLineNumber, ERR_SYNTAX+4, pFunctionNameString->GetStr());
			
		}
		else
		{
			switch(iInvalidParamReason)
			{
				case 1 :	// Function does not return a value and it should as this is a function!
							g_pErrorReport->SetError(StatementLineNumber, ERR_SYNTAX+46, pFunctionNameString->GetStr());
							break;
				case 2 :	// Function dec error
							g_pErrorReport->SetError(g_pStatementList->GetTokenLineNumber(), ERR_SYNTAX+8, pValidEntryStr.get(), pFunctionTypeStr.get());
							break;

				case 3 :	// Function dec error
							g_pErrorReport->SetError(g_pStatementList->GetTokenLineNumber(), ERR_SYNTAX+64);
							break;
			}
		}

		pFirstParameter.reset();
		m_pStatement.reset();
		return false;
	}

	// Run through parameters of validated user function and make any required casts
	if(pRef && pFirstParameter)
	{
		if(pFirstParameter->CastAllParametersToInstruction(pRef)==false)
		{
			g_pErrorReport->AddErrorString("Failed to 'DoValueFunction::CastAllParametersToInstruction'");
			return false;
		}
	}

	// Type Of Result
	DWORD dwTypeValue=7;
	if(pRef) dwTypeValue=pRef->GetReturnParam();

	// Create temp symbol and use as return data store for function
	auto pResultStr = std::make_unique<CStr>();
	if(dwTypeValue>0)
	{
		CStr TempVarToken(1);
		ProduceNewTempToken(&TempVarToken, dwTypeValue);
		pResultStr->SetText("");
		pResultStr->AddText(&TempVarToken);
	}

	// Also write result param to this math object
	SetResult(pResultStr->GetStr(), dwTypeValue, 0);
	SetResultStruct(nullptr);

	// Construct full label name for function
	auto pFullLabelName = std::make_unique<CStr>();
	pFullLabelName->SetText("$label ");
	pFullLabelName->AddText(pFunctionNameString->GetStr());

	// Complete Object Data
	auto pInstruction = std::make_unique<CParseInstruction>();
	pInstruction->SetType(dwInstructionType);
	pInstruction->SetValue(dwInstructionValue);
	pInstruction->SetParamMax(dwInstructionParamMax);
	pInstruction->SetParameter(pFirstParameter.release());
	pInstruction->SetLineNumber(StatementLineNumber);
	pInstruction->SetReturnParameter(pResultStr.release());
	pInstruction->SetLabelParam(pFullLabelName.release());
	pInstruction->SetInstructionRef(pRef);
	m_pStatement->SetData(StatementLineNumber, std::move(pInstruction));

	// Clear memory usage
	pFunctionNameString.reset();
	pFunctionDataString.reset();

	// Complete
	return true;
}

bool CMathOp::CalculateDataOffsetAndTypeFromFieldString(CStr* pVarName, DWORD dwArrayType, CStr* pFieldData, DWORD* pdwSize, DWORD* pdwLType, DWORD* pdwSizeOfWholeType, CStructTable** ppStruct)
{
	// Get Statement Line
	DWORD StatementLineNumber = g_pStatementList->GetTokenLineNumber();

	// Create name to begin structure search
	std::unique_ptr<CStr> pFirstName;
	if(dwArrayType==1)
	{
		pFirstName = std::make_unique<CStr>(const_cast<LPSTR>("&"));
		pFirstName->AddText(pVarName->GetStr());
	}
	else
		pFirstName = std::make_unique<CStr>(pVarName->GetStr());
	
	if(pFirstName->Length()==0)
	{
		g_pErrorReport->AddErrorString("Failed to 'Calculate DataOffsetAndTypeFromFieldString::pFirstName->Length()==0'");
		return false;
	}

	// Eat spaces and tabs
	pFirstName->EatEdgeSpacesandTabs(nullptr);

	// Determine type of this variable
	LPSTR pTypeName=nullptr;
	std::unique_ptr<char[]> pTypeNameOwner;
	if(g_pVarTable->FindVariableExist(pFirstName->GetStr(), dwArrayType)==true)
	{
		if(g_pVarTable->FindTypeOfVariable(pFirstName->GetStr(), dwArrayType, &pTypeName)==false)
		{
			g_pErrorReport->SetError(StatementLineNumber, ERR_SYNTAX+5, pFirstName->GetStr());
			return false;
		}
		pTypeNameOwner.reset(pTypeName);
	}
	else
	{
		g_pErrorReport->SetError(StatementLineNumber, ERR_SYNTAX+6, pFirstName->GetStr());
		return false;
	}

	// Store Size of overall type
	*pdwSizeOfWholeType = g_pStructTable->GetSizeOfType(pTypeName);
	
	// If simple array, take array element type
	DWORD dwTrackLastValidLValueFieldType=g_pVarTable->GetBasicTypeValue(pTypeName);

	// Proceed to find all field names
	CStructTable* pLastStruct = nullptr;
	DWORD dwTrackOffsetToActualData=0;
	int iStartNewName=0;
	DWORD dwEndNewName=0;
	DWORD length=pFieldData->Length();
	for(DWORD n=0; n<length; n++)
	{
		if(iStartNewName==-1)
		{
			if(pFieldData->GetChar(n)=='.')
			{
				// Record Start Of New Name
				iStartNewName=n+1;
			}
			else
			{
				char pErrorDetail[512];
				sprintf_s(pErrorDetail, 512, "Failed to 'Calculate DataOffsetAndTypeFromFieldString' : %s %s %s %s", pVarName->GetStr(), pFieldData->GetStr(), pFirstName->GetStr(), pTypeName );
				g_pErrorReport->AddErrorString(pErrorDetail);
				return false;
			}
		}
		else
		{
			if((pFieldData->IsAlphaNumericLabel(n)==false
			&& pFieldData->IsSpaceCharacter(n)==false)
			|| n==length-1)
			{
				// Determine real end
				if(n==length-1)
					dwEndNewName=n+1;
				else
					dwEndNewName=n;

				// Get fieldname
				auto pFieldname = std::make_unique<CStr>(1);
				for(DWORD o=iStartNewName; o<dwEndNewName; o++)
					pFieldname->AddChar(pFieldData->GetChar(o));

				// Eat spaces and tabs form field name
				pFieldname->EatEdgeSpacesandTabs(nullptr);

				// Eat spaces between name and symbol
				for(; n<length; n++)
					if(pFieldData->GetChar(n)!=' ')
						break;

				// Get name of new type
				DWORD dwArrFlag=0;
				DWORD dwFieldOffset=0;
				LPSTR pNewTypeName=nullptr;
				CDeclaration* pLastDec = g_pStructTable->FindFieldInType(pTypeName, pFieldname->GetStr(), &pNewTypeName, &dwArrFlag, &dwFieldOffset);
				if ( pLastDec )
				{
					CStr* pLastDecTypeName = pLastDec->GetType();
					pLastStruct = g_pStructTable->DoesTypeEvenExist(pLastDecTypeName->GetStr());
				}
				if ( pLastDec==nullptr )
				{
					g_pErrorReport->SetError(StatementLineNumber, ERR_SYNTAX+38, pFieldname->GetStr(), pTypeName);
					return false;
				}
				
				// Retain newly found type
				pTypeNameOwner.reset(pNewTypeName);
				pTypeName=pNewTypeName;

				// Store field type for L-Value process
				dwTrackLastValidLValueFieldType=g_pVarTable->GetBasicTypeValue(pTypeName);

				// If field is still traversing type-nest, apply offset
				dwTrackOffsetToActualData+=dwFieldOffset;

				// Restart and add non-ltr char too (ony if not at end of string)
				if(n<length-1)
				{
					iStartNewName=-1;
					if(pFieldData->GetChar(n)=='.')	iStartNewName=n+1;
				}
			}
		}
	}

	// pTypeNameOwner auto-cleanup

	// Return Data gathered from traversal
	*pdwLType=dwTrackLastValidLValueFieldType;
	*pdwSize=dwTrackOffsetToActualData;
	*ppStruct=pLastStruct;
	
	// Complete
	return true;
}

bool CMathOp::DoValueComplexVariable(CStr* pExpressionValue)
{
	// Statement Line
	DWORD StatementLineNumber = g_pStatementList->GetTokenLineNumber();

	// Assign Value as Array or Variable(with/without type)
	CStr nameStorage(pExpressionValue->GetStr());
	CStr* pName = &nameStorage;

	// Before processing DataType, crop spaces, tabs and equal brackets
	do { pName->EatEdgeSpacesandTabs(nullptr);
	} while(pName->CropEqualEdgeBrackets(nullptr)==true);
	
	// Just eat all non-important spacea
	// removed due to: "sample(array count(sample(0))).value" occurance - space would be deleted!
//	pName->EatNonImportantChars();

	// Complex variables are arrays or types
	DWORD dwArrayType=0;

	// Determine seperation between var and field specifiers
	DWORD dwTypeFieldSep = pName->FindFirstCharAtBracketLevel('.');
	DWORD dwSepPos = dwTypeFieldSep;
	CStr fixedDataOffsetStorage;
	CStr* pFixedDataOffset = &fixedDataOffsetStorage;
	if(dwSepPos==0)
	{
		// No datatype field part
		dwSepPos = pName->Length();
		pFixedDataOffset->SetText("");
	}
	else
	{
		// Seperate Dynamic and Fixed Components
		pFixedDataOffset->SetText(pName->GetStr()+dwSepPos+1);
	}

	// Check if array-subscript provided for var
	std::unique_ptr<CStr> pSubscriptStringOwner;
	CStr* pSubscriptString = nullptr;
	int iGrabSubscriptString=-1;
	DWORD n = 0;
	for(n=0; n<dwSepPos; n++)
	{
		if(pName->CheckChar(n, '(')) { iGrabSubscriptString=n; break; }
	}
	if(iGrabSubscriptString!=-1)
	{
		for(n=dwSepPos; n>0; n--)
		{
			if(pName->CheckChar(n, ')')) break;
		}
		if(n==dwSepPos-1)
		{
			// Subscript String Value
			pSubscriptStringOwner = std::make_unique<CStr>();
			pSubscriptString = pSubscriptStringOwner.get();
			pSubscriptString->SetText(pName->GetStr()+iGrabSubscriptString+1);
			pSubscriptString->SetChar((dwSepPos-iGrabSubscriptString)-2,0);

			// Value is array
			dwArrayType=1;
		}
	}

	// Take only name part
	if(iGrabSubscriptString==-1)
		pName->SetChar(dwSepPos, 0);
	else
		pName->SetChar(iGrabSubscriptString, 0);

	// Verify name is accurate for a variable
	if(pName->IsTextASingleVariable()==false)
	{
		if ( pName->Length() > 0 )
		{
			// ensure error responds only if something for user to see, else use a higher error report
			g_pErrorReport->SetError(StatementLineNumber, ERR_SYNTAX+7, pName->GetStr());
		}
		return false;
	}

	// Calculate actual size of data offset (from type data)
	DWORD dwDataOffset = 0;
	CStructTable* pStruct = nullptr; // used for inner-UDT type determination
	DWORD dwLValueType = g_pVarTable->MakeDefaultVarTypeValue(pName->GetStr());
	DWORD dwSizeOfWholeType = 0;
	if(pFixedDataOffset->Length()>0)
	{
		if(CalculateDataOffsetAndTypeFromFieldString(pName, dwArrayType, pFixedDataOffset, &dwDataOffset, &dwLValueType, &dwSizeOfWholeType, &pStruct)==false)
		{
			g_pErrorReport->AddErrorString("Failed to 'Calculate DataOffsetAndTypeFromFieldString'");
			return false;
		}
	}
	else
	{
		// LEEFIX - 171102 - Dot, but no data offset field..error
		if(dwTypeFieldSep>0)
		{
			g_pErrorReport->SetError(StatementLineNumber, ERR_SYNTAX+38, "", pName->GetStr());
			return false;
		}
	}

	// Obtain user-fucntion-scope if any..
	LPSTR pScope=nullptr;
	if(g_pStatementList->GetUserFunctionDecChain())
		if(g_pStatementList->GetUserFunctionName())
			pScope = g_pStatementList->GetUserFunctionName();

	// Check if name already exists as a global
	bool bNameAlreadyExistsAsAGlobal=false;
	CStr findNameStorage("");
	CStr* pFindName = &findNameStorage;
	if(dwArrayType==1) pFindName->SetText("&");
	pFindName->AddText(pName->GetStr());
	CVarTable* pVarTest;
	pVarTest = g_pVarTable->FindVariable(pScope, pFindName->GetStr(), dwArrayType);
	if(pVarTest==nullptr)
	{
		pVarTest = g_pVarTable->FindVariable("", pFindName->GetStr(), dwArrayType);
		if(pVarTest)
		{
			// LEEFIX - 271102 - Fix so arrays and types in functions
			DWORD dwBasicType = pVarTest->GetVarTypeValue();
			if(dwBasicType<1000)
			{
				dwBasicType = dwBasicType % 100;
				if(dwBasicType>=1 && dwBasicType<=9)
				{
					// LEEFIX - 171102 - If find as global, get true type
					dwLValueType = dwBasicType;
				}
			}
			else
			{
				// leefix-240603-if datatype with no field part, dest value is UDT
				if ( dwTypeFieldSep==0 )
				{
					dwLValueType = dwBasicType;
				}
			}
			bNameAlreadyExistsAsAGlobal=true;
		}
	}
	else
	{
		// LEEFIX - 171002 - Update array access type (if a basic type)
		DWORD dwBasicType = pVarTest->GetVarTypeValue();
		if(dwBasicType<1000)
		{
			dwBasicType = dwBasicType % 100;
			if(dwBasicType>=1 && dwBasicType<=9)
			{
				// IN case where string
				dwLValueType = dwBasicType;
			}
		}
		else
		{
			// leefix-240603-if datatype with no field part, dest value is UDT
			if ( dwTypeFieldSep==0 )
			{
				dwLValueType = dwBasicType;
			}
		}
	}

	// Get UDT of var if not got
	if ( pStruct==nullptr && pVarTest )
		pStruct = pVarTest->GetVarStruct();

	// Produce result name
	CStr resultNameStorage("");
	CStr* pResultName = &resultNameStorage;
	CDeclaration* pGlobalDecChain = g_pStatementList->GetUserFunctionDecChain();
	if(pGlobalDecChain && bNameAlreadyExistsAsAGlobal==false)
	{
		// Local Name
		pResultName->SetText("FS@");
		pResultName->AddText(g_pStatementList->GetUserFunctionName());
		pResultName->AddText("@");
		if(dwArrayType==1) pResultName->AddText("&");
		pResultName->AddText(pName);
	}
	else
	{
		// Global Name
		if(pName->CheckChar(0, '@')==false)
		{
			pResultName->SetText("@");
			if(dwArrayType==1) pResultName->AddText("&");
			pResultName->AddText(pName);
		}
		else
			pResultName->SetText(pName);
	}

	// Begin with math for dynamic array offset
	if(pSubscriptString==nullptr)
	{
		// VAR.VIA TYPEDEF
		SetLineNumber(StatementLineNumber);
		SetResult(pResultName->GetStr(), dwLValueType, dwDataOffset);
		SetResultStruct ( pStruct );
	}
	else
	{
		// Add all subscripts to stack in reverse order
		int iBracketCount=0;
		DWORD dwSubscriptCount=0;
		int iIndex = pSubscriptString->Length()-1;
		while(iIndex>=0)
		{
			// Get result of subscript calc
			CStr oneSubscriptStorage("");
			CStr* pOneSubscript = &oneSubscriptStorage;

			// Find subscript
			int iSpeechmark=0;
			while((pSubscriptString->GetChar(iIndex)!=',' || iSpeechmark==1 || iBracketCount!=0) && iIndex>=0)
			{
				if(pSubscriptString->GetChar(iIndex)=='(') iBracketCount++;
				if(pSubscriptString->GetChar(iIndex)==')') iBracketCount--;
				if(pSubscriptString->GetChar(iIndex)=='"') iSpeechmark=1-iSpeechmark;
				pOneSubscript->AddChar(pSubscriptString->GetChar(iIndex));
				iIndex--;
				if(iIndex<0) break;
			}
			pOneSubscript->Reverse();
			dwSubscriptCount++;
			iIndex--;

			// calculate result subscript
			auto pSubscriptResultOwner = std::make_unique<CMathOp>();
			CMathOp* pSubscriptResult = pSubscriptResultOwner.get();
			if(pSubscriptResult->DoValue(pOneSubscript)==false)
			{
				return false;
			}
			Add(pSubscriptResultOwner.release());

			// Subscript must be INTEGER, DWORD, INT64, BYTE or WORD
			DWORD dwSubscriptType = pSubscriptResult->FindResultTypeValueForDBM() % 100;
			if ( dwSubscriptType != 1 && dwSubscriptType != 7 && dwSubscriptType != 9 && dwSubscriptType != 4 && dwSubscriptType != 5 )
			{
				// otherwise compiler error
				g_pErrorReport->SetError(StatementLineNumber, ERR_SYNTAX+63);
				return false;
			}

			// Create stack adder for subscript
			if(pSubscriptResult->GetMathSymbol()!=0 || pSubscriptResult->GetNext())
			{
				// Need new math for stack instruction
				auto pAddSubscript = std::make_unique<CMathOp>();
				CResultData* pResultFromSubscript = pSubscriptResult->FindResultData();
				pAddSubscript->SetResult(pResultFromSubscript->m_pStringToken->GetStr(), pResultFromSubscript->m_dwType, pResultFromSubscript->m_dwDataOffset );
				pAddSubscript->SetResultStruct(nullptr);
				if(pResultFromSubscript->m_pAdditionalOffset)
				{
					pAddSubscript->SetArrayOffsetResult(pResultFromSubscript->m_pAdditionalOffset->GetStr());
				}
				pAddSubscript->SetMathSymbol(10002);
				Add(pAddSubscript.release());
			}
			else
			{
				// As no chain can make this single item stack instruction
				pSubscriptResult->SetMathSymbol(10002);
			}

			// Prev subscript (reverse order)
		}

		// If subscript count is zero, array must use internal index
		if(dwSubscriptCount==0)
		{
			// Extracts internal index and pushes it to stack (similar to passing calculated params to stack)
			auto pSubscriptResult = std::make_unique<CMathOp>();
			pSubscriptResult->SetLineNumber(StatementLineNumber);
			pSubscriptResult->SetResult(pResultName->GetStr(), 107, 0);
			pSubscriptResult->SetResultStruct(nullptr);
			pSubscriptResult->SetMathSymbol(10004);
			Add(pSubscriptResult.release());
		}

		// Create temp-token to hold final offset
		CStr tempTokenOffsetStorage("");
		CStr* pTempTokenOffset = &tempTokenOffsetStorage;
		ProduceNewTempToken(pTempTokenOffset, 7);

		// Array-passed-in-for-offset-calculation
		auto pPassArrayForOffsetCalc = std::make_unique<CMathOp>();
		pPassArrayForOffsetCalc->SetLineNumber(StatementLineNumber);
		pPassArrayForOffsetCalc->SetResult(pResultName->GetStr(), 107, 0);
		pPassArrayForOffsetCalc->SetResultStruct(nullptr);

		// Math to calculate offset for array
		auto pCalculateArrayOffset = std::make_unique<CMathOp>();
		pCalculateArrayOffset->SetLineNumber(StatementLineNumber);
		pCalculateArrayOffset->SetResult(pTempTokenOffset->GetStr(), 7, dwSubscriptCount);
		pCalculateArrayOffset->SetResultStruct(nullptr);
		pCalculateArrayOffset->SetMathSymbol(10003);
		pCalculateArrayOffset->m_pLeftMathOp = std::move(pPassArrayForOffsetCalc);
		Add(pCalculateArrayOffset.release());

		// Make math for [Array Name, Dynamic Offset and Data Offset]
		auto pArrayAccess = std::make_unique<CMathOp>();
		pArrayAccess->SetLineNumber(StatementLineNumber);
		pArrayAccess->SetResult(pResultName->GetStr(), 100+dwLValueType, dwDataOffset);
		pArrayAccess->SetResultStruct(pStruct);
		pArrayAccess->SetArrayOffsetResult(pTempTokenOffset->GetStr());
		Add(pArrayAccess.release());
	}

	// Complete
	return true;
}

bool CMathOp::ResolveStructValue(CStr* pExpressionValue)
{
	// Resolve STRUCT and FUNCTIONSTRUCT values to a result
	bool bIsStructItem=false;
	bool bIsStructFunction=false;
	CStr structValue(pExpressionValue->GetStr());

	// Is it a struct from a userfunction
	if(structValue.CheckChars(0,3,"FS@")==true)
	{
		std::unique_ptr<char[]> pRest(structValue.GetRightOfPosition(3));
		structValue.SetText(pRest.get());
		bIsStructItem=true;
		bIsStructFunction=true;
	}

	// Yes, proceed..
	if(bIsStructItem)
	{
		// Is it a type or field speciier
		DWORD dwPos = structValue.FindFirstChar('@');
		if(dwPos>0)
		{
			// Find subtype name
			std::unique_ptr<char[]> pSubtypename(structValue.GetLeftOfPosition(dwPos));

			// Determine if field is array
			DWORD dwArrayType = 0;
			if(structValue.GetChar(dwPos+1)=='&') dwArrayType=1;
			std::unique_ptr<char[]> pFieldname(structValue.GetRightOfPosition(dwPos+1));

			// Use correct L-Value for later resolution to reading/writing
			DWORD dwLValueType = 0;
			CStructTable* pStructRef = nullptr;
			CDeclaration* pDec=g_pStructTable->FindDecInType(pSubtypename.get(), pFieldname.get());
			if(pDec)
			{
				// Use Structure To Get LValue Type
				dwLValueType=g_pVarTable->GetBasicTypeValue(pDec->GetType()->GetStr());
				pStructRef = g_pVarTable->GetStruct(pDec->GetType()->GetStr());
			}
			else
			{
				// Struct not found, so try looking in var table (instantly updated)
				CVarTable* pVar = g_pVarTable->FindVariable(pSubtypename.get(), pFieldname.get(), dwArrayType);
				if(pVar)
				{
					dwLValueType = pVar->GetVarTypeValue();
					pStructRef = pVar->GetVarStruct();
				}
			}

			// This value is a simple offset (part of address calculation)		
			m_dwOffsetLValueTypeValue=dwLValueType;

			// Set Result Data
			SetResult(pExpressionValue->GetStr(), dwLValueType, GetResultDataOffset());
			SetResultStruct(pStructRef);
		}
		else
		{
			// Type Specifier - Use Size of Type Structure
			DWORD dwTypeSize=g_pStructTable->GetSizeOfType(structValue.GetStr());
			CStr sizeStr(1);
			sizeStr.SetNumericText(dwTypeSize);
			SetResult(sizeStr.GetStr(), 7, 0);
			SetResultStruct(nullptr);
		}
	}

	// Complete
	return true;
}

bool CMathOp::DoValueSingleVariable(CStr* pExpressionValue)
{
	// Struct is an absolute value (parsed later)
	if(pExpressionValue->CheckChars(0, 3, "FS@"))
	{
		// Determine type value from details
		if(ResolveStructValue(pExpressionValue)==false)
		{
			g_pErrorReport->AddErrorString("Failed to 'DoValueSingleVariable::ResolveStructValue'");
			return false;
		}
	}
	else
	{
		// Strip expression of @ symbol if prev. added
		if(pExpressionValue->CheckChar(0, '@'))
		{
			std::unique_ptr<char[]> pRight(pExpressionValue->GetRightOfPosition(1));
			pExpressionValue->SetText(pRight.get());
		}

		// Determine Array Marker from name
		DWORD dwArrFlag=0;
		if(pExpressionValue->CheckChar(0, '&'))
			dwArrFlag=1;

		// Determine If Pointing Into Variable
		DWORD dwPointingIntoVar=0;
		if(pExpressionValue->CheckChar(0, '*'))
		{
			pExpressionValue->EatChar(0);
			dwPointingIntoVar=1;
		}

		// Add Variable to variable table
		DWORD dwAction=0;
		LPSTR pDecName = pExpressionValue->GetStr();
		std::string decType = g_pVarTable->MakeDefaultVarType(pDecName);
		if(g_pVarTable->AddVariable(pDecName, decType.data(), dwArrFlag, 0, true, &dwAction, false)==false)
		{
			g_pErrorReport->AddErrorString("Failed to 'DoValueSingleVariable::AddVariable'");
			return false;
		}

		// Is variable a userfunction local variabale
		bool bVariableIsLocal=false;
		CDeclaration* pGlobalDecChain = g_pStatementList->GetUserFunctionDecChain();
		if(pGlobalDecChain && pExpressionValue->GetChar(0)!='$')	bVariableIsLocal=true;
		if(dwAction==2)												bVariableIsLocal=false;

		// If local, produce indirect access to local data block		
		if(bVariableIsLocal)
		{
			// LOCAL VARIABLE
			CStr localVar;
			localVar.SetText("FS@");
			localVar.AddText(g_pStatementList->GetUserFunctionName());
			localVar.AddText("@");
			localVar.AddText(pExpressionValue);

			// Parse function immediate
			if(DoValue(&localVar)==false)
			{
				g_pErrorReport->AddErrorString("Failed to 'DoValueSingleVariable::DoValue'");
				return false;
			}

			// LEEFIX - 111002 - Placed array (+=100) code and not in DBMtranslator - messes up array dimming in functions

			// Ensure arrays handled
			DWORD dwArr=0;
			if(pExpressionValue->CheckChar(0,'&')) dwArr=1;

			// Find Type Of Variable
			LPSTR pTypeNameRaw=nullptr;
			if(g_pVarTable->FindTypeOfVariable(pExpressionValue->GetStr(), dwArr, &pTypeNameRaw)==false)
			{
				g_pErrorReport->AddErrorString("Failed to 'DoValueSingleVariable::FindTypeOfVariable'");
				return false;
			}
			std::unique_ptr<char[]> pTypeName(pTypeNameRaw);

			// Amend result for whether local array or not
			DWORD dwType = g_pVarTable->GetBasicTypeValue(pTypeName.get());
			if(dwArr==1) dwType+=100;

			// is variable 'indirect' specified by * symbol
			if(dwPointingIntoVar==1)
			{
				// only for variables (not arrays)
				if(dwType<100) dwType+=200;
			}

			// finally set result
			SetResultType(dwType);
			SetResultStruct(g_pVarTable->GetStruct(pTypeName.get()));
		}
		else
		{
			// GLOBAL OR ABSOLUTE VARIABLE
			DWORD dwArr=0;
			if(pExpressionValue->CheckChar(0,'&')) dwArr=1;

			// Find Type Of Variable
			LPSTR pTypeNameRaw=nullptr;
			if(g_pVarTable->FindTypeOfVariable(pExpressionValue->GetStr(), dwArr, &pTypeNameRaw)==false)
			{
				g_pErrorReport->AddErrorString("Failed to 'DoValueSingleVariable::FindTypeOfVariable'");
				return false;
			}
			std::unique_ptr<char[]> pTypeName(pTypeNameRaw);

			// Create result (array or not)
			CStr str("@");
			str.AddText(pExpressionValue);
			DWORD dwType = g_pVarTable->GetBasicTypeValue(pTypeName.get());
			if(dwArr==1) dwType+=100;
			SetResult(str.GetStr(), dwType, 0);
			SetResultStruct(g_pVarTable->GetStruct(pTypeName.get()));
		}

		// If pointer to this var, make direct ref indirect
		if(dwPointingIntoVar==1)
		{
			// only for variables (not arrays)
			if(GetResultType()<100)
			{
				SetResultType(GetResultType()+200);
			}
		}
	}

	// Complete
	return true;
}

bool CMathOp::DoValueLabel(CStr* pExpressionValue)
{
	// Assign Value as Label
	CStr str(pExpressionValue->GetStr());
	SetResult(str.GetStr(), 10, 0);
	SetResultStruct(nullptr);

	// Complete
	return true;
}

bool CMathOp::DoValueLiteral(CStr* pExpressionValue, DWORD dwTypeValue)
{
	// Assign Value as Literal
	CStr str(pExpressionValue->GetStr());

	// leeadd - handle scientific notation
	if ( str.IsSciNot() ) { str.ResolveSciNot(); dwTypeValue=8; }

	// Ensure literal has no leading zero (hex and oct and bin are type=7)
	if ( dwTypeValue!=7 && dwTypeValue!=3 )
	{
		// non DWORD types can remove the leading zeros
		LPSTR pString = str.GetStr();
		DWORD dwLen = static_cast<DWORD>(strlen(pString));
		if ( pString[0]=='0' )
		{
			for ( DWORD c=0; c<dwLen; c++ )
			{
				// leefix - 300305 - except where followed by a dot (float/double float)
				if ( pString[c]!='0' && c>0 )
				{
					// leefix - 310305 - ensure only eliminate EXTRA zeros, and only handle '.' if not at start!
					if ( pString[c-1]=='0' )
					{
						if ( c>0 && pString[c]=='.' ) c-=1;
						str.SetText ( pString+c );
						break;
					}
				}
			}
		}
	}

	// Confirm data result
	SetResult(str.GetStr(), dwTypeValue, 0);
	SetResultStruct(nullptr);

	// Complete
	return true;
}

bool CMathOp::FindHighestPres(CStr* pString, DWORD *dwPosition, DWORD *dwType, DWORD *dwReturnSymbolWidth, DWORD *dwIsSciNot)
{
	// Bracket Vars
	int iBracketLevel=0;
	DWORD dwSpeechDisable=0;

	// Best Result Vars
	DWORD dwBestMathType=99;
	DWORD dwBestPriority=99;
	DWORD dwBestMathPosition=0;
	DWORD dwBestSymbolWidth=0;

	// usually zero
	*dwIsSciNot=0;

	// Find Math Symbols
	DWORD dwSP=0;
	DWORD dwLength = pString->Length();
	while(dwSP<dwLength)
	{
		// Detect Bracket Levels and Speech Mark Disable
		if(dwSpeechDisable==0)
		{
			if(pString->CheckChar(dwSP,'('))		iBracketLevel++;
			if(pString->CheckChar(dwSP,')'))		iBracketLevel--;
		}
		if(pString->CheckChar(dwSP,'"'))		dwSpeechDisable=1-dwSpeechDisable;

		// Detect Math Symbol
		if(iBracketLevel==0 && dwSpeechDisable==0)
		{
			// Math Symbols Table (in precedence order)
			DWORD dwMathType=0, dwSymbolWidth=0, dwPriority=0;
			CheckForSymbol(pString, dwSP, &dwMathType, &dwPriority, &dwSymbolWidth);

			// Ensure its not a negaive value (-XXX)
			if(dwMathType==5 && dwSP==0) dwMathType=0;

			// leeadd - U71 - 111008 - scientific notation (1.0E+38 or 1.0E-38)
			if( (dwMathType==4 || dwMathType==5) && dwSP>=2 )
			{
				// symbol found was +, make sure it is not part of a scientific notation
				int iDetectNotation = 0;
				bool bANonNumericInStringCanOnlyBeVariable = false;
				for ( DWORD dwN=dwSP-1; dwN>0; dwN-- )
				{
					if ( iDetectNotation==0 )
					{
						if ( pString->CheckChar(dwN,'e') || pString->CheckChar(dwN,'E') )
						{
							// found an E - check next stage
							iDetectNotation = 1;
							dwN--;
						}
						else
						{
							if ( pString->CheckChar(dwN,' ') )
							{
								// allow spaces to pass
							}
							else
							{
								// anything else and this is no scientific notation!
								break;
							}
						}
					}
					if ( iDetectNotation==1 )
					{
						unsigned char num = pString->GetChar(dwN);
						if ( num>='0' && num<='9' )
						{
							// found first numeric before E, this may be scientific notation!
							iDetectNotation=2;
							dwN--;
						}
						else
						{
							if ( pString->CheckChar(dwN,' ') )
							{
								// allow spaces to pass
							}
							else
							{
								// anything else and this is no scientific notation!
								break;
							}
						}
					}
					if ( iDetectNotation==2 )
					{
						unsigned char num = pString->GetChar(dwN);
						if ( num=='.' || (num>='0' && num<='9') )
						{
							// numerics are allowed
						}
						else
						{
							if ( pString->CheckChar(dwN,' ') )
							{
								// allow spaces to pass
							}
							else
							{
								// anything else and this is no scientific notation!
								bANonNumericInStringCanOnlyBeVariable = true;
								break;
							}
						}
					}
				}
				if ( bANonNumericInStringCanOnlyBeVariable==true ) iDetectNotation=0;
				if ( iDetectNotation==2 )
				{
					// not a maths operation, replace with actual number 
					// represented by notation..
					*dwIsSciNot=dwMathType;
					dwMathType=0;
				}
			}

			// Only the highest..
			if(dwMathType>0 && dwPriority<=dwBestPriority)
			{
				// Precedence by Occurance Order (0-no pref/1-righttoleft/2-leftoright)
				DWORD dwOrder=2;
// leefix - 240604 - u54 - LEFT TO RIGHT PRECIDENCE 100/10/2 and 100-10-2
//				if(dwMathType==1) dwOrder=1;
//				if(dwMathType==5) dwOrder=2;

				// Remove other best vars if better symb found
				if(dwPriority<dwBestPriority)
					dwBestMathPosition=0;

				// Record best so far
				if((dwOrder==0)
				|| (dwOrder==1 && dwSP>dwBestMathPosition)
				|| (dwOrder==2 && dwBestMathPosition==0) )
				{
					dwBestMathPosition=dwSP;
					dwBestMathType=dwMathType;
					dwBestPriority=dwPriority;
					dwBestSymbolWidth=dwSymbolWidth;
				}
			}
		}
		dwSP++;
	}

	// If bracket count irregular, then error
	if(iBracketLevel!=0)
	{
		DWORD StatementLineNumber = g_pStatementList->GetTokenLineNumber();
		g_pErrorReport->SetError(StatementLineNumber, ERR_SYNTAX+9);
		return false;
	}

	// If Found Math Symbol
	if(dwBestMathType!=99)
	{
		*dwPosition=dwBestMathPosition;
		*dwType=dwBestMathType;
		*dwReturnSymbolWidth=dwBestSymbolWidth;
	}
	else
	{
		*dwPosition=0;
		*dwType=0;
		*dwReturnSymbolWidth=0;
	}

	// Complete
	return true;
}

bool CMathOp::CheckForSymbol(CStr* pString, DWORD dwSP, DWORD *dwMathType, DWORD *dwPriority, DWORD *dwSymbolWidth)
{
	if (!pString || !pString->GetStr()) return false;
	static const CExpressionParser exprParser;
	return exprParser.CheckForSymbol(pString->GetStr(), dwSP, dwMathType, dwPriority, dwSymbolWidth);
}

bool CMathOp::ProduceNewTempToken(CStr* pTempVarToken, DWORD dwTypeMode)
{
	// Create temp name
	CStr tempName("$");
	tempName.AddChar(g_pVarTable->GetCharOfType(dwTypeMode));
	tempName.AddNumericText(g_pStatementList->GetTempVarIndex());

	// Produce temp var based on parse location
	CDeclaration* pGlobalDecChain = g_pStatementList->GetUserFunctionDecChain();
	if(pGlobalDecChain)
	{
		// Inside a function
		pTempVarToken->SetText("FS@");
		pTempVarToken->AddText(g_pStatementList->GetUserFunctionName());
		pTempVarToken->AddText("@");
		pTempVarToken->AddText(&tempName);
	}
	else
	{
		// Main program temp var
		pTempVarToken->SetText("@");
		pTempVarToken->AddText(&tempName);
	}
	g_pStatementList->IncTempVarIndex();

	// Any temporary variables created must be added to variable table space
	LPSTR pDecName = tempName.GetStr();
	std::unique_ptr<char[]> pDecType(g_pVarTable->MakeTypeNameOfTypeValue(dwTypeMode));
	if(g_pVarTable->AddVariable(pDecName, pDecType.get(), 0, 0, true, nullptr, false)==false)
	{
		g_pErrorReport->AddErrorString("Failed to 'Produce NewTempToken::AddVariable'");
		return false;
	}

	return true;
}

bool CMathOp::IsReserved ( CStr* pString )
{
	// If any of these match, trying to use a sole reserved word in the expression
	if(dbp::iequals(pString->GetStr(), "AND"))	return true;
	if(dbp::iequals(pString->GetStr(), "OR"))	return true;
	if(dbp::iequals(pString->GetStr(), "NOT"))	return true;
	if(dbp::iequals(pString->GetStr(), "DIV"))	return true;
	if(dbp::iequals(pString->GetStr(), "MOD"))	return true;

	// not matched
	return false;
}

bool CMathOp::IsItLabelFollowedByBracket(CStr* pExpressionValue, DWORD *pdwLabelLength)
{
	if(pExpressionValue->IsAlpha(0))
	{
		DWORD length=pExpressionValue->Length();
		if(length>1)
		{
			bool bHoweverNoMoreLetters=false;
			DWORD n = 1;
			for(n=1; n<length-1; n++)
			{
				bool bValid=false;

				// Alphanumeric and other valid charactersvalid
				if(bHoweverNoMoreLetters==false)
				{
					if(pExpressionValue->IsAlphaNumericLabel(n)==true) bValid=true;
					if(pExpressionValue->GetChar(n)=='$' || pExpressionValue->GetChar(n)=='#')
					{
						bHoweverNoMoreLetters=true;
						bValid=true;
					}
				}
				if(pExpressionValue->IsSpaceCharacter(n)==true) bValid=true;

				// If not valid, no more
				if(bValid==false) break;
			}
			
			if(pExpressionValue->GetChar(n)=='(')
			{
				// Record length of function
				if(pdwLabelLength) *pdwLabelLength=n;

				return true;
			}
		}
	}

	// soft fail
	return false;
}

bool CMathOp::IsFunction(CStr* pExpressionValue)
{
	bool bFound=false;
	DWORD dwLabelLength=0;
	if(IsItLabelFollowedByBracket(pExpressionValue, &dwLabelLength))
	{
		// If function exists
		CStr possibleName(pExpressionValue->GetStr());
		possibleName.Shorten(dwLabelLength);
		if(SearchForFunction(&possibleName))
			bFound=true;
	}
	return bFound;
}

bool CMathOp::IsLiteral(CStr* pExpressionValue, DWORD* pdwTypeValue)
{
	// Determine if this is a literal value (string, numeric, etc)
	if ( !pExpressionValue )
		return false;

	// Is it a string
	if(pExpressionValue->IsTextSpeechMarked())
	{
		*pdwTypeValue=3;
		return true;
	}

	// Is it an decimal value
	if(pExpressionValue->IsTextNumericValue())
	{
		if(pExpressionValue->IsTextIntegerOnlyValue())
		{
			if(pExpressionValue->IsIntegerBiggerThanDWORD())
				*pdwTypeValue=9;
			else
				*pdwTypeValue=1;
		}
		else
		{
			if(g_pDBPCompiler->m_bDoubleLiterals)
			{
				// Compiler may exclusively use doubles..
				*pdwTypeValue=8;
			}
			else
			{
				// By default, float..
				*pdwTypeValue=2;

				// LEEFIX - 141102 - Added back in as I have a better way to check
				if(pExpressionValue->IsFloatBiggerThanDWORD())
					*pdwTypeValue=8;
				else
					*pdwTypeValue=2;
			}
		}

		return true;
	}

	// Is it an hexidecimal value
	if(pExpressionValue->IsTextHexValue())
	{
		// leefix - 210703 - HEX, OCT and BINARY all report as DWORD values now
		*pdwTypeValue=7;
		return true;
	}

	// Is it an octal value
	if(pExpressionValue->IsTextOctalValue())
	{
		// leefix - 210703 - HEX, OCT and BINARY all report as DWORD values now
		*pdwTypeValue=7;
		return true;
	}

	// Is it an binary value
	if(pExpressionValue->IsTextBinaryValue())
	{
		// leefix - 210703 - HEX, OCT and BINARY all report as DWORD values now
		*pdwTypeValue=7;
		return true;
	}

	// leeadd - 121008 - u71 - Is scientific notation XX E [+/-]YY
	if(pExpressionValue->IsSciNot())
	{
		return true;
	}

	// Could not recognise soft fail
	return false;
}

bool CMathOp::IsSingleVariable(CStr* pExpressionValue)
{
	// Is it a temporary assigned variable
	if(pExpressionValue->CheckChars(0,1,"$"))
	{
		// lee - 130206 - u60 - can only be certain alpha/numeric symbols
		unsigned char cChar = pExpressionValue->GetChar(1);
		if ( cChar >= 48 && cChar <= 'z' )
			return true;
	}

	// Is it an array variable
	if(pExpressionValue->CheckChars(0,1,"&"))
		return true;

	// Last, treat as variable
	if(pExpressionValue->IsTextASingleVariable())
		return true;

	// If structure constant for a user function
	if(pExpressionValue->CheckChars(0,3,"FS@")==true)
		return true;

	// Could not recognise soft fail
	return false;
}

bool CMathOp::IsLabel(CStr* pExpressionValue)
{
	// Make label
	CStr FullLabel;
	FullLabel.SetText("$label ");
	FullLabel.AddText(pExpressionValue);

	// Search for label..
	if(g_pLabelTable->FindLabel(FullLabel.GetStr())!=nullptr)
		return true;

	// Could not recognise soft fail
	return false;
}

bool CMathOp::IsComplexVariable(CStr* pExpressionValue)
{
	// Complex variable is one with fields and array specifiers ( () & .)
	if(pExpressionValue->IsTextAComplexVariable())
		return true;

	// Could not recognise soft fail
	return false;
}

bool CMathOp::IsAnything([[maybe_unused]] CStr* pExpressionValue)
{
	// Always try and resolve it
	return true;
}

bool CMathOp::SearchForFunction(CStr* pFunctionName)
{
	// Remove spaces before checking for existence
	pFunctionName->EatEdgeSpacesandTabs(nullptr);

	// Check if a recognised instruction
	CInstructionTableEntry* pRef = nullptr;
	DWORD dwTokenData=0, dwParamMax=0, dwLength=0;
	if(g_pInstructionTable->FindInstruction(false, pFunctionName->GetStr(), 1, &dwTokenData, &dwParamMax, &dwLength, &pRef))
	{
		// Record instruction for later parsing (doinstruction)
		g_pStatementList->SetInstructionRef(pRef);
		g_pStatementList->SetInstructionType(2);
		g_pStatementList->SetInstructionValue(dwTokenData);
		g_pStatementList->SetInstructionParamMax(dwParamMax);
		return true;
	}

	// Check if a user defined function
	dwTokenData=0, dwParamMax=0, dwLength=0;
	if(g_pInstructionTable->FindUserFunction(pFunctionName->GetStr(), 1, &dwTokenData, &dwParamMax, &dwLength))
	{
		// Record instruction for later parsing (doinstruction)
		g_pStatementList->SetInstructionRef(nullptr);
		g_pStatementList->SetInstructionType(3);
		g_pStatementList->SetInstructionValue(dwTokenData);
		g_pStatementList->SetInstructionParamMax(dwParamMax);
		return true;
	}

	// No function of this name soft fail
	//DB3_CRASH_MSG(pFunctionName->GetStr());
	return false;
}

DWORD CMathOp::ChopOffOneItemFromLeft(CStr* pString)
{
	int iBracketLevel=0;
	DWORD pos = 0;
	DWORD dwSpeechDisable=0;
	DWORD length = pString->Length();
	while(pos<length)
	{
		if ( dwSpeechDisable==0 )
		{
			if(pString->CheckChar(pos,'('))		iBracketLevel++;
			if(pString->CheckChar(pos,')'))		iBracketLevel--;
		}
		if(pString->CheckChar(pos,'"'))		dwSpeechDisable=1-dwSpeechDisable;
		if(iBracketLevel==0 && dwSpeechDisable==0)
		{
			DWORD dwMathType=0, dwSymbolWidth=0, dwPriority=0;
			if(CheckForSymbol(pString, pos, &dwMathType, &dwPriority, &dwSymbolWidth)==true)
			{
				if(pos==0)
				{
					if(!pString->CheckChar(0,'-') && !pString->CheckChar(0,'+'))
						break;
				}
				else
					break;
			}
		}
		pos++;
	}
	pString->SetChar(pos,0);
	return pos;
}

DWORD CMathOp::ChopOffOneItemFromRight(CStr* pString)
{
	int iBracketLevel=0;
	DWORD length = pString->Length();
	DWORD dwSpeechDisable=0;
	DWORD pos = length;
	DWORD dwMathType=0;
	DWORD dwPriority=0;
	DWORD dwSymbolWidth=0;
	while(pos>0)
	{
		// leefix - 270206 - bracket only valid outside speech marks
		if(pString->CheckChar(pos,'"'))		dwSpeechDisable=1-dwSpeechDisable;
		if ( dwSpeechDisable==0 )
		{
			if(pString->CheckChar(pos,'('))		iBracketLevel++;
			if(pString->CheckChar(pos,')'))		iBracketLevel--;
			if(iBracketLevel==0)
			{
				if(CheckForSymbol(pString, pos, &dwMathType, &dwPriority, &dwSymbolWidth)==true) break;
			}
		}
		pos--;
	}
	DWORD nread=pos+dwSymbolWidth;
	DWORD nwrite=0;
	while(nread<length)
	{
		pString->SetChar(nwrite, pString->GetChar(nread));
		nwrite++;
		nread++;
	}
	pString->SetChar(nwrite, 0);
	return pos+dwSymbolWidth;
}

bool CMathOp::DetermineIfPointerMaths(DWORD dwMathSymbol, DWORD dwTypeValue)
{
	if(dwTypeValue>=101 && dwTypeValue<=109)
		if(dwMathSymbol<100)
			return true;

	// soft faul
	return false;
}

//
// WriteDBM Segment
//

bool CMathOp::WriteDBM(void)
{
	// If need to process extra statements
	if(m_pStatement)
	{
		m_pStatement->WriteDBM();
	}

	// Write out text
	if(m_dwMathSymbol==0)
	{
//		Moved to above: an 'array count()' expression in a stack calc had no DBM code!
//		// If need to process extra statements
//		if(m_pStatement)
//		{
//			m_pStatement->WriteDBM();
//		}
	}
	else
	{
		// instructions 10000 and above are special codes
		if(m_dwMathSymbol<10000)
		{
			// First Traverse Deeper
			if(m_pLeftMathOp) m_pLeftMathOp->WriteDBM();
			if(m_pRightMathOp) m_pRightMathOp->WriteDBM();

			// Write out math command
			if(WriteDBMBit(GetLineNumber())==false) return false;
		}
		else
		{
			// Special array-offset-calculation instructions
			// LEEFIX - 281102 - Added m_pAdditionalOffset to translateDBM
			switch(GetMathSymbol())
			{
				case 10002 :	// Add contents of 'this' result to stack (used for subscript passing for dynamic array)
								if(GetResultData()->m_pStringToken) GetResultData()->m_pStringToken->TranslateForDBM(GetResultData());
								if(GetResultData()->m_pAdditionalOffset) GetResultData()->m_pAdditionalOffset->TranslateForDBM(GetResultData());
								g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::Push), GetResultData());
								break;

				case 10003 :	// Process stack-items to calculate array-offset (array stored in left mathitem)
								if(GetResultData()->m_pStringToken) GetResultData()->m_pStringToken->TranslateForDBM(GetResultData());
								if(GetResultData()->m_pAdditionalOffset) GetResultData()->m_pAdditionalOffset->TranslateForDBM(GetResultData());
								if(m_pLeftMathOp) if(m_pLeftMathOp->GetResultData()->m_pStringToken) m_pLeftMathOp->GetResultData()->m_pStringToken->TranslateForDBM(GetResultData());
								if(m_pLeftMathOp) if(m_pLeftMathOp->GetResultData()->m_pAdditionalOffset) m_pLeftMathOp->GetResultData()->m_pAdditionalOffset->TranslateForDBM(GetResultData());
								g_pASMWriter->WriteASMTaskP2(m_dwLineNumber, static_cast<DWORD>(ASMTask::CalcArrayOffset), GetResultData(), m_pLeftMathOp->GetResultData());
								break;

				case 10004 :	// Adds internal index from array to stack
								if(GetResultData()->m_pStringToken) GetResultData()->m_pStringToken->TranslateForDBM(GetResultData());
								if(GetResultData()->m_pAdditionalOffset) GetResultData()->m_pAdditionalOffset->TranslateForDBM(GetResultData());
								g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::PushInternalArrayIndex), GetResultData());
								break;
			}
		}
	}

	// Then Do Next One
	if(m_pNext)
	{
		if(m_pNext->WriteDBM()==false) return false;
	}

	return true;
}


bool CMathOp::WriteDBMBit(DWORD dwLineNumber)
{
	/* moved to praseinstruction
	// Special math commands (10000>)
	if(m_dwMathSymbol>10000)
	{
		switch(m_dwMathSymbol)
		{
			case 10002 :	// Add contents of 'this' result to stack (used for subscript passing for dynamic array)
							g_pASMWriter->WriteASMTaskP1(dwLineNumber, static_cast<DWORD>(ASMTask::Push), GetResultData());
							break;
		}
		return true;
	}
	*/

	// Cast Maths have one param, regular maths have two
	DWORD dwNumberOfPopsToMake=2;
	if(m_dwMathSymbol>=101)
		dwNumberOfPopsToMake=1;

	// Data type to handle data (treat indirect addressing as regular values
	DWORD dwDataTypeA = m_pLeftMathOp->FindResultTypeValueForDBM();
	if ( dwDataTypeA>=200 && dwDataTypeA<=299 )
		dwDataTypeA-=200;

	// Internal Math Call to DLL
	DWORD dwUseNewInstruction=g_pInstructionTable->DetermineInternalCommandCode(m_dwMathSymbol, dwDataTypeA);
	if(dwUseNewInstruction==0)
	{
		// Command not yet implemented
		g_pASMWriter->WriteASMTaskP1(dwLineNumber, static_cast<DWORD>(ASMTask::Unknown), nullptr);
		return true;
	}

	// Work out more final DBM data (moved from after stack adds)
	TranslateStringTokenForDBM();

	// CALL Instruction with current stack state
	CInstructionTableEntry* pRef=g_pInstructionTable->GetRef(dwUseNewInstruction);
	if(pRef==nullptr)
	{
		// Command not yet implemented
		g_pASMWriter->WriteASMTaskP1(dwLineNumber, static_cast<DWORD>(ASMTask::Unknown), nullptr);
		return true;
	}

	// If math is hardcoded, build directly (no need for DLL call)
	bool bMathCommandIsHardCoded=false;
	if(pRef->GetBuildID()>0) bMathCommandIsHardCoded=true;
	if(bMathCommandIsHardCoded)
	{
		// Work out build
		DWORD dwASMToBuild = static_cast<DWORD>(ASMTask::Unknown);
		if(pRef->GetBuildID()==static_cast<DWORD>(BuildTask::Mul)) dwASMToBuild=static_cast<DWORD>(ASMTask::Mul);
		if(pRef->GetBuildID()==static_cast<DWORD>(BuildTask::Div)) dwASMToBuild=static_cast<DWORD>(ASMTask::Div);
		if(pRef->GetBuildID()==static_cast<DWORD>(BuildTask::Add)) dwASMToBuild=static_cast<DWORD>(ASMTask::Add);
		if(pRef->GetBuildID()==static_cast<DWORD>(BuildTask::Sub)) dwASMToBuild=static_cast<DWORD>(ASMTask::Sub);
		if(pRef->GetBuildID()==static_cast<DWORD>(BuildTask::Mod)) dwASMToBuild=static_cast<DWORD>(ASMTask::Mod);

		if(pRef->GetBuildID()==static_cast<DWORD>(BuildTask::Shr)) dwASMToBuild=static_cast<DWORD>(ASMTask::Shr);
		if(pRef->GetBuildID()==static_cast<DWORD>(BuildTask::Shl)) dwASMToBuild=static_cast<DWORD>(ASMTask::Shl);
		if(pRef->GetBuildID()==static_cast<DWORD>(BuildTask::BitAnd)) dwASMToBuild=static_cast<DWORD>(ASMTask::And);
		if(pRef->GetBuildID()==static_cast<DWORD>(BuildTask::BitOr)) dwASMToBuild=static_cast<DWORD>(ASMTask::Or);
		if(pRef->GetBuildID()==static_cast<DWORD>(BuildTask::BitXor)) dwASMToBuild=static_cast<DWORD>(ASMTask::Xor);
		if(pRef->GetBuildID()==static_cast<DWORD>(BuildTask::BitNot)) dwASMToBuild=static_cast<DWORD>(ASMTask::BitNot);

		if(pRef->GetBuildID()==static_cast<DWORD>(BuildTask::And)) dwASMToBuild=static_cast<DWORD>(ASMTask::And);
		if(pRef->GetBuildID()==static_cast<DWORD>(BuildTask::Or)) dwASMToBuild=static_cast<DWORD>(ASMTask::Or);
		if(pRef->GetBuildID()==static_cast<DWORD>(BuildTask::Not)) dwASMToBuild=static_cast<DWORD>(ASMTask::Not);

		if(pRef->GetBuildID()==static_cast<DWORD>(BuildTask::Equal)) dwASMToBuild=static_cast<DWORD>(ASMTask::Equal);
		if(pRef->GetBuildID()==static_cast<DWORD>(BuildTask::NotEqual)) dwASMToBuild=static_cast<DWORD>(ASMTask::NotEqual);
		if(pRef->GetBuildID()==static_cast<DWORD>(BuildTask::Greater)) dwASMToBuild=static_cast<DWORD>(ASMTask::Greater);
		if(pRef->GetBuildID()==static_cast<DWORD>(BuildTask::GreaterEqual)) dwASMToBuild=static_cast<DWORD>(ASMTask::GreaterEqual);
		if(pRef->GetBuildID()==static_cast<DWORD>(BuildTask::Less)) dwASMToBuild=static_cast<DWORD>(ASMTask::Less);
		if(pRef->GetBuildID()==static_cast<DWORD>(BuildTask::LessEqual)) dwASMToBuild=static_cast<DWORD>(ASMTask::LessEqual);

		// Call hard code builder
		CResultData* pA = m_pLeftMathOp->FindResultData();
		CResultData* pB = m_pRightMathOp->FindResultData();
		if(GetResultData()->m_pStringToken) GetResultData()->m_pStringToken->TranslateForDBM(GetResultData());
		if(GetResultData()->m_pAdditionalOffset) GetResultData()->m_pAdditionalOffset->TranslateForDBM(GetResultData());
		g_pASMWriter->WriteASMTaskCore(	m_dwLineNumber, dwASMToBuild,
										pA->m_pStringToken.get(), pA->m_pAdditionalOffset.get(), pA->m_dwType, pA->m_dwDataOffset,
										pB->m_pStringToken.get(), pB->m_pAdditionalOffset.get(), pB->m_dwType, pB->m_dwDataOffset,
										GetResultData()->m_pStringToken.get(), GetResultData()->m_pAdditionalOffset.get(), GetResultData()->m_dwType, GetResultData()->m_dwDataOffset);
	}
	else
	{
		// Push Parameters for Math Call to Stack (in reverse order)
		if(m_dwMathSymbol<101)
		{
			// Ignore second param of casting maths (redundant)
			DWORD dwDataTypeB = m_pRightMathOp->FindResultTypeValueForDBM();
			g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::Push), m_pRightMathOp->FindResultData());
			if(dwDataTypeB==8 || dwDataTypeB==9 || dwDataTypeB==108 || dwDataTypeB==109) dwNumberOfPopsToMake++;
		}
		g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::Push), m_pLeftMathOp->FindResultData());
		if(dwDataTypeA==8 || dwDataTypeA==9 || dwDataTypeA==108 || dwDataTypeA==109) dwNumberOfPopsToMake++;

		// Some Core Instructions require additional 'internal' params
		if(GetResultData()->m_dwType==3)
		{
			// Add Destination string (as it needs to be freed if not-nullptr)
			g_pASMWriter->WriteASMTaskP1(m_dwLineNumber, static_cast<DWORD>(ASMTask::Push), GetResultData());
			dwNumberOfPopsToMake++;
		}


		LPSTR pMathDLL=pRef->GetDLL()->GetStr();
		LPSTR pMathCommand=pRef->GetDecoratedName()->GetStr();
		g_pASMWriter->WriteASMCall(dwLineNumber, pMathDLL, pMathCommand);

		// Fundamental pop of all calls
		g_pASMWriter->WriteASMTaskP1(dwLineNumber, static_cast<DWORD>(ASMTask::PopRbx), nullptr);

		// Copy RAX (holding call result) to temp var used to hold return value
		CStr* pReturnData = GetResultStringToken();
		if(pReturnData)
		{
			[[maybe_unused]] DWORD dwReturnDataType = GetResultType();
			g_pASMWriter->WriteASMTaskP2(dwLineNumber, static_cast<DWORD>(ASMTask::Assign), GetResultData(), nullptr);
		}

		// Pop params off stack in same quantity as those added
		for(DWORD i=0; i<dwNumberOfPopsToMake-1; i++)
			g_pASMWriter->WriteASMTaskP1(dwLineNumber, static_cast<DWORD>(ASMTask::PopRax), nullptr);
	}

	return true;
}

bool CMathOp::WriteDBMLine(DWORD dwLineNumber, std::string_view text, std::string_view result)
{
	// Write out text
	CStr strDBMLine(256);
	strDBMLine.SetNumericText(dwLineNumber);
	strDBMLine.AddText(" ");
	strDBMLine.AddText(text);
	strDBMLine.AddText(" ");
	strDBMLine.AddText(result);
	if(g_pDBMWriter->OutputDBM(&strDBMLine)==false) return false;

	return true;
}
