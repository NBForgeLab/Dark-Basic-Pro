// Str.cpp: implementation of the CStr class.
//
//////////////////////////////////////////////////////////////////////

#include "macros.h"
#include "stdio.h"
#include "Str.h"
#include "float.h"
#include <math.h>
#include <cstring>
#include <cstdlib>
#include <algorithm>

#include "StructTable.h"
#include "InstructionTable.h"
#include "DataTable.h"

// External Class Pointers
extern CStructTable *g_pStructTable;
extern CInstructionTable *g_pInstructionTable;
extern CDataTable *g_pStringTable;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CStr::CStr() : m_buffer(1, '\0'), m_dwLen(0)
{
}

CStr::CStr(LPSTR pText) : m_dwLen(0)
{
	if(pText)
	{
		DWORD length = static_cast<DWORD>(strlen(pText));
		m_buffer.assign(pText, pText + length + 1);
		m_dwLen = length;
	}
	else
	{
		m_buffer.assign(1, '\0');
	}
}

CStr::CStr(DWORD dwTextSize) : m_dwLen(0)
{
	m_buffer.assign(dwTextSize + 1, '\0');
}

CStr::CStr(CStr&& other) noexcept
	: m_buffer(std::move(other.m_buffer)), m_dwLen(other.m_dwLen)
{
	other.m_buffer.assign(1, '\0');
	other.m_dwLen = 0;
}

CStr& CStr::operator=(CStr&& other) noexcept
{
	if (this != &other)
	{
		m_buffer = std::move(other.m_buffer);
		m_dwLen = other.m_dwLen;
		other.m_buffer.assign(1, '\0');
		other.m_dwLen = 0;
	}
	return *this;
}

void CStr::Enlarge(DWORD length)
{
	if (length + 1 > m_buffer.size())
	{
		m_buffer.resize(length + 1, '\0');
	}
	UpdateLen();
}

void CStr::UpdateLen(void)
{
	m_dwLen = static_cast<DWORD>(strlen(m_buffer.data()));
}

CStr::CStr(LPCSTR pText)
{
	SetText(pText);
}

void CStr::SetText(LPCSTR pText)
{
	if(pText)
	{
		const DWORD length = static_cast<DWORD>(strlen(pText));
		if(length + 1 > m_buffer.size()) Enlarge(length);
		memmove(m_buffer.data(), pText, length + 1);
		UpdateLen();
	}
}

void CStr::AddText(LPCSTR pText)
{
	if(pText)
	{
		const DWORD srcLen = static_cast<DWORD>(strlen(pText));
		const DWORD length = srcLen + m_dwLen;
		if(length + 1 > m_buffer.size()) Enlarge(length);
		memcpy(m_buffer.data() + m_dwLen, pText, srcLen + 1);
		UpdateLen();
	}
}

void CStr::InsertText(LPCSTR pStr)
{
	if(pStr)
	{
		const DWORD insertLen = static_cast<DWORD>(strlen(pStr));
		const DWORD totalLen = insertLen + m_dwLen;
		if(totalLen + 1 > m_buffer.size()) Enlarge(totalLen);

		// Use a temporary buffer to avoid overlap issues
		std::vector<char> temp(m_buffer.data(), m_buffer.data() + m_dwLen + 1);
		memcpy(m_buffer.data(), pStr, insertLen + 1);
		memcpy(m_buffer.data() + insertLen, temp.data(), m_dwLen + 1);
		UpdateLen();
	}
}

void CStr::SetText(LPSTR pText)
{
	if(pText)
	{
		DWORD length = static_cast<DWORD>(strlen(pText));
		if(length + 1 > m_buffer.size()) Enlarge(length);
		memmove(m_buffer.data(), pText, length + 1);
		UpdateLen();
	}
}

void CStr::SetText(CStr* pStrText)
{
	if(pStrText)
	{
		DWORD length = pStrText->Length();
		if(length + 1 > m_buffer.size()) Enlarge(length);
		memcpy(m_buffer.data(), pStrText->GetStr(), length + 1);
		UpdateLen();
	}
}

void CStr::AddText(LPSTR pText)
{
	if(pText)
	{
		const DWORD srcLen = static_cast<DWORD>(strlen(pText));
		const DWORD length = srcLen + m_dwLen;
		if(length + 1 > m_buffer.size()) Enlarge(length);
		memcpy(m_buffer.data() + m_dwLen, pText, srcLen + 1);
		UpdateLen();
	}
}

void CStr::AddText(CStr* pStrText)
{
	if(pStrText)
	{
		const DWORD srcLen = pStrText->Length();
		const DWORD length = srcLen + m_dwLen;
		if(length + 1 > m_buffer.size()) Enlarge(length);
		memcpy(m_buffer.data() + m_dwLen, pStrText->GetStr(), srcLen + 1);
		UpdateLen();
	}
}

void CStr::InsertText(LPSTR pStr)
{
	if(pStr)
	{
		DWORD insertLen = static_cast<DWORD>(strlen(pStr));
		DWORD totalLen = insertLen + m_dwLen;
		if(totalLen + 1 > m_buffer.size()) Enlarge(totalLen);

		// Use a temporary buffer to avoid overlap issues
		std::vector<char> temp(m_buffer.data(), m_buffer.data() + m_dwLen + 1);
		memcpy(m_buffer.data(), pStr, insertLen + 1);
		memcpy(m_buffer.data() + insertLen, temp.data(), m_dwLen + 1);
		UpdateLen();
	}
}

void CStr::AddChar(char cChar)
{
	DWORD length = m_dwLen;
	if(length + 2 > m_buffer.size()) Enlarge(length + 1);
	m_buffer[length] = cChar;
	m_buffer[length + 1] = '\0';
	UpdateLen();
}

void CStr::SetNumericText(DWORD dwNumText)
{
	char buf[32];
	_itoa_s(static_cast<int>(dwNumText), buf, sizeof(buf), 10);
	SetText(buf);
}

void CStr::SetUnsignedNumericText(DWORD dwNumText)
{
	char buf[32];
	_ultoa_s(dwNumText, buf, sizeof(buf), 10);
	SetText(buf);
}

void CStr::SetDWORDNumericText(DWORD dwNumText)
{
	char buf[256];
	sprintf_s(buf, sizeof(buf), "%u", dwNumText);
	SetText(buf);
}

void CStr::AddNumericText(DWORD dwNumText)
{
	char buf[32];
	_itoa_s(static_cast<int>(dwNumText), buf, sizeof(buf), 10);
	AddText(buf);
}

void CStr::AddDoubleText(double dNumText)
{
	char buf[128];
	sprintf_s(buf, sizeof(buf), "%f", dNumText);
	AddText(buf);
}

double CStr::GetValue(void) const
{
	if(!m_buffer.empty() && m_dwLen > 0)
	{
		return atof(m_buffer.data());
	}
	return 0;
}

LPSTR CStr::GetLeftOfPosition(DWORD Position) const
{
	LPSTR pText = new char[Position + 1];
	if(pText)
	{
		memcpy(pText, m_buffer.data(), Position);
		pText[Position] = '\0';
	}
	return pText;
}

LPSTR CStr::GetRightOfPosition(DWORD Position) const
{
	DWORD len = m_dwLen - Position;
	LPSTR pText = new char[len + 1];
	if(pText)
	{
		memcpy(pText, m_buffer.data() + Position, len);
		pText[len] = '\0';
	}
	return pText;
}

void CStr::CopyToPtr(LPSTR pPointer) const
{
	if(m_dwLen > 0)
	{
		memcpy(pPointer, m_buffer.data(), m_dwLen);
	}
}

void CStr::CopyFromPtr(LPSTR pPointer, LPSTR pPointerEnd, DWORD length)
{
	if(length + 1 > m_buffer.size()) Enlarge(length + 1);
	if(pPointer + length > pPointerEnd) length = static_cast<DWORD>(length - (pPointerEnd - pPointer));
	memcpy(m_buffer.data(), pPointer, length);
	m_buffer[length] = '\0';
	UpdateLen();
}

bool CStr::MakeUpper(void)
{
	if(m_dwLen > 0)
	{
		char* p = m_buffer.data();
		for(; *p != '\0'; ++p)
		{
			if(*p >= 'a' && *p <= 'z')
				*p = *p - 'a' + 'A';
		}
		return true;
	}
	return false;
}

bool CStr::ReplaceSemicolons(void)
{
	bool bConcatFlag = false;
	if(m_dwLen > 0)
	{
		if(GetChar(m_dwLen - 1) == ';')
		{
			SetChar(m_dwLen - 1, ' ');
			bConcatFlag = true;
		}

		DWORD dwSpeechMark = 0;
		for(DWORD n = 0; n < m_dwLen - 1; n++)
		{
			if(GetChar(n) == '"') dwSpeechMark = 1 - dwSpeechMark;
			if(dwSpeechMark == 0)
			{
				if(GetChar(n) == ';')
				{
					SetChar(n, ',');
				}
			}
		}
	}
	return bConcatFlag;
}

bool CStr::CheckChar(DWORD dwPos, unsigned char cChar) const
{
	if(m_dwLen > 0)
	{
		const int diff = 'a' - 'A';
		unsigned char schar = static_cast<unsigned char>(m_buffer[dwPos]);
		if(schar >= 'a' && schar <= 'z') schar = static_cast<unsigned char>(schar - diff);
		if(schar == cChar)
			return true;
	}
	return false;
}

bool CStr::CheckChars(DWORD dwPos, DWORD num, LPCSTR pText) const
{
	if(m_dwLen > 0 && pText)
	{
		const int diff = 'a' - 'A';
		for(DWORD n = 0; n < num; n++)
		{
			unsigned char schar = static_cast<unsigned char>(m_buffer[dwPos + n]);
			if(schar >= 'a' && schar <= 'z') schar = static_cast<unsigned char>(schar - diff);
			if(schar != static_cast<unsigned char>(pText[n]))
			{
				return false;
			}
		}
		return true;
	}
	return false;
}

bool CStr::SetChar(DWORD dwPos, DWORD value)
{
	if(m_dwLen > 0 || dwPos < m_buffer.size())
	{
		m_buffer[dwPos] = static_cast<unsigned char>(value);
		if(!value)
		{
			if(dwPos < m_dwLen)
				m_dwLen = dwPos;
		}
		return true;
	}
	return false;
}

unsigned char CStr::GetChar(DWORD dwPos) const
{
	if(dwPos < m_buffer.size())
		return static_cast<unsigned char>(m_buffer[dwPos]);
	else
		return 0;
}

DWORD CStr::FindFirstChar(char cChar) const
{
	if(m_dwLen > 0)
	{
		for(DWORD n = 0; n < m_dwLen; n++)
			if(m_buffer[n] == cChar)
				return n;
	}
	return 0;
}

DWORD CStr::FindFirstCharAtBracketLevel(char cChar) const
{
	int iBracketCount = 0;
	int iSpeech = 0;
	if(m_dwLen > 0)
	{
		for(DWORD n = 0; n < m_dwLen; n++)
		{
			if(m_buffer[n] == '"') iSpeech = 1 - iSpeech;
			if(iSpeech == 0)
			{
				if(m_buffer[n] == '(') iBracketCount++;
				if(m_buffer[n] == ')') iBracketCount--;
				if(m_buffer[n] == cChar && iBracketCount == 0)
					return n;
			}
		}
	}
	return 0;
}

DWORD CStr::FindLastChar(char cChar) const
{
	if(m_dwLen > 0)
	{
		for(int n = static_cast<int>(m_dwLen) - 1; n >= 0; n--)
			if(m_buffer[n] == cChar)
				return static_cast<DWORD>(n);
	}
	return 0;
}

bool CStr::IsAlpha(DWORD dwPos) const
{
	if(m_dwLen > 0)
	{
		if((m_buffer[dwPos] == '_')
		|| (m_buffer[dwPos] >= 'a' && m_buffer[dwPos] <= 'z')
		|| (m_buffer[dwPos] >= 'A' && m_buffer[dwPos] <= 'Z'))
		{
			return true;
		}
	}
	return false;
}

bool CStr::IsAlphaNumericLabel(DWORD dwPos) const
{
	if(m_dwLen > 0)
	{
		if((m_buffer[dwPos] == '_')
		|| (m_buffer[dwPos] >= 'a' && m_buffer[dwPos] <= 'z')
		|| (m_buffer[dwPos] >= 'A' && m_buffer[dwPos] <= 'Z')
		|| (m_buffer[dwPos] >= '0' && m_buffer[dwPos] <= '9'))
		{
			return true;
		}
	}
	return false;
}

bool CStr::IsMathCharLabel(DWORD dwSP) const
{
	if(m_dwLen > 0)
	{
		if(CheckChar(dwSP, '^'))             return true;
		if(CheckChar(dwSP, '*'))             return true;
		if(CheckChar(dwSP, '/'))             return true;
		if(CheckChar(dwSP, '+'))             return true;
		if(CheckChar(dwSP, '-'))             return true;
		if(CheckChar(dwSP, '='))             return true;
		if(CheckChar(dwSP, '>'))             return true;
		if(CheckChar(dwSP, '<'))             return true;
		if(CheckChars(dwSP, 2, ">>"))        return true;
		if(CheckChars(dwSP, 2, "<<"))        return true;
		if(CheckChars(dwSP, 2, "<>"))        return true;
		if(CheckChars(dwSP, 2, ">="))        return true;
		if(CheckChars(dwSP, 2, "=>"))        return true;
		if(CheckChars(dwSP, 2, "<="))        return true;
		if(CheckChars(dwSP, 2, "=<"))        return true;
		if(CheckChars(dwSP, 4, " OR "))      return true;
		if(CheckChars(dwSP, 5, " AND "))     return true;
		if(CheckChars(dwSP, 5, " NOT "))     return true;
		if(CheckChars(dwSP, 5, " XOR "))     return true;
	}
	return false;
}

bool CStr::IsSpaceCharacter(DWORD dwPos) const
{
	if(m_dwLen > 0)
	{
		if(m_buffer[dwPos] == ' ')
			return true;
	}
	return false;
}

bool CStr::ContainsASOperator(void) const
{
	DWORD dwOnlySpacesNow = 0;
	DWORD dwInSpeechMarks = 0;
	if(m_dwLen > 0)
	{
		for(DWORD n = 0; n < m_dwLen; n++)
		{
			if(dwOnlySpacesNow == 0 && IsAlphaNumericLabel(n) == false)
				dwOnlySpacesNow = 1;

			if(dwOnlySpacesNow == 1 && IsAlphaNumericLabel(n) == true)
			{
				return false;
			}

			if(m_buffer[n] == '"') dwInSpeechMarks = 1 - dwInSpeechMarks;
			if(dwInSpeechMarks == 0)
			{
				if(_strnicmp((m_buffer.data() + n), " as ", 4) == 0)
					return true;
			}
		}
	}
	return false;
}

bool CStr::IsSciNot(void) const
{
	bool bStillValid = true;
	DWORD dwN = 0;
	if(m_dwLen > 0)
	{
		for(DWORD dwPos = 0; dwPos < m_dwLen; dwPos++)
		{
			if((m_buffer[dwPos] == '+')
			|| (m_buffer[dwPos] == '-')
			|| (m_buffer[dwPos] == '.')
			|| (m_buffer[dwPos] == 'e')
			|| (m_buffer[dwPos] == 'E')
			|| (m_buffer[dwPos] >= '0' && m_buffer[dwPos] <= '9'))
			{
				if(m_buffer[dwPos] == 'e') dwN = dwPos;
				if(m_buffer[dwPos] == 'E') dwN = dwPos;
			}
			else
			{
				bStillValid = false;
				break;
			}
		}
	}

	if(bStillValid && dwN > 0)
	{
		dwN--;
		bool bANonNumericInStringCanOnlyBeVariable = false;
		int iDetectNotation = 1;
		int iN = static_cast<int>(dwN);
		while(iN >= 0)
		{
			if(iDetectNotation == 1 && iN >= 0)
			{
				unsigned char num = m_buffer[iN];
				if(num >= '0' && num <= '9')
				{
					iDetectNotation = 2;
					iN--;
				}
				else
				{
					if(m_buffer[iN] == ' ')
					{ /* allow spaces */ }
					else
						break;
				}
			}
			if(iDetectNotation == 2 && iN >= 0)
			{
				unsigned char num = m_buffer[iN];
				if(num == '.' || (num >= '0' && num <= '9'))
				{ /* numerics allowed */ }
				else
				{
					if(m_buffer[iN] == ' ')
					{ /* allow spaces */ }
					else
					{
						bANonNumericInStringCanOnlyBeVariable = true;
						break;
					}
				}
			}
			iN--;
		}
		if(bANonNumericInStringCanOnlyBeVariable) iDetectNotation = 0;
		if(iDetectNotation != 2) bStillValid = false;
	}
	else
	{
		bStillValid = false;
	}

	return bStillValid;
}

void CStr::ResolveSciNot(void)
{
	EatNonImportantChars();
	DWORD dwE = FindFirstChar('e');
	if(dwE == 0) dwE = FindFirstChar('E');
	if(dwE > 0)
	{
		CStr pExpressionValueL((DWORD)1), pExpressionValueR((DWORD)1);
		char str[512];
		pExpressionValueL.SetText(this->GetStr());
		pExpressionValueL.Shorten(dwE);
		pExpressionValueR.SetText(this->GetStr() + dwE + 1);
		double dLeftValue = atof(pExpressionValueL.GetStr());
		double dRightValue = atof(pExpressionValueR.GetStr());
		double dPowerValue = pow(10, dRightValue);
		dLeftValue = dLeftValue * dPowerValue;
		sprintf_s(str, sizeof(str), "%.23f", dLeftValue);
		SetText(str);
	}
}

void CStr::EatEdgeSpacesandTabs(DWORD* pdwHowMany)
{
	if(m_dwLen > 0)
	{
		char* p = m_buffer.data();
		for(int twice = 0; twice < 2; twice++)
		{
			bool bOnlyStart = true;
			DWORD w = 0;
			DWORD length = Length();
			for(DWORD n = 0; n < length; n++)
			{
				if(static_cast<unsigned char>(p[n]) != 32
				&& static_cast<unsigned char>(p[n]) != 9
				&& static_cast<unsigned char>(p[n]) != 13
				&& static_cast<unsigned char>(p[n]) != 10)
				{
					if(pdwHowMany && twice == 0 && bOnlyStart == true)
						if(*pdwHowMany == 0)
							*pdwHowMany = n;

					bOnlyStart = false;
				}

				if(bOnlyStart == false)
					p[w++] = p[n];
			}
			p[w++] = 0;

			_strrev(p);
		}
		UpdateLen();
	}
}

void CStr::EatTrailingEdgeSpacesandTabs(void)
{
	if(m_dwLen > 0)
	{
		char* p = m_buffer.data();
		// Reverse string
		_strrev(p);

		// Eat spaces at start
		bool bOnlyStart = true;
		DWORD w = 0;
		DWORD length = Length();
		for(DWORD n = 0; n < length; n++)
		{
			if(static_cast<unsigned char>(p[n]) != 32
			&& static_cast<unsigned char>(p[n]) != 9
			&& static_cast<unsigned char>(p[n]) != 13
			&& static_cast<unsigned char>(p[n]) != 10)
				if(bOnlyStart == true)
					bOnlyStart = false;

			if(bOnlyStart == false)
				p[w++] = p[n];
		}
		p[w++] = 0;

		// Reverse string
		_strrev(p);
		UpdateLen();
	}
}

void CStr::EatChar(DWORD dwPos)
{
	if(m_dwLen > 0)
	{
		char* p = m_buffer.data();
		DWORD dwLen = m_dwLen;
		for(DWORD n = dwPos; n < dwLen; n++)
		{
			p[n] = p[n + 1];
		}
		m_dwLen--;
		UpdateLen();
	}
}

void CStr::EatSpeechMarks(void)
{
	if(m_dwLen > 0)
	{
		if(IsTextSpeechMarked())
		{
			int length = static_cast<int>(Length()) - 2;
			if(length >= 0)
			{
				char* p = m_buffer.data();
				int n;
				for(n = 0; n < length; n++)
				{
					p[n] = p[n + 1];
				}
				p[n] = 0;
				UpdateLen();
			}
		}
	}
}

bool CStr::EatLeadingChars(void)
{
	if(m_dwLen > 0)
	{
		char* p = m_buffer.data();
		for(int twice = 0; twice < 2; twice++)
		{
			bool bOnlyStart = true;
			DWORD w = 0;
			DWORD length = Length();
			for(DWORD n = 0; n < length; n++)
			{
				if(static_cast<unsigned char>(p[n]) != 32
				&& static_cast<unsigned char>(p[n]) != 9)
					bOnlyStart = false;

				if(bOnlyStart == false)
					p[w++] = p[n];
			}
			p[w++] = 0;

			_strrev(p);
		}
		UpdateLen();
	}
	return true;
}

void CStr::EatNonImportantChars(void)
{
	if(m_dwLen > 0)
	{
		DWORD dwInSpeechMarks = 0;
		CStr pNewStr("");
		DWORD length = Length();
		for(DWORD n = 0; n < length; n++)
		{
			bool bValid = true;
			if(m_buffer[n] == '"') dwInSpeechMarks = 1 - dwInSpeechMarks;
			if(dwInSpeechMarks == 0)
				if(IsSpaceCharacter(n)) bValid = false;

			if(bValid)
				pNewStr.AddChar(m_buffer[n]);
		}
		SetText(pNewStr.GetStr());
	}
}

bool CStr::CropEqualEdgeBrackets(DWORD* pdwHowMany)
{
	if(m_dwLen > 0)
	{
		char* p = m_buffer.data();
		int iSpeechMarks = 0;
		DWORD dwOpenPos = 0;
		int iBracketCount = 0;
		DWORD length = Length();
		for(DWORD n = 0; n < length; n++)
		{
			if(p[n] == '"') iSpeechMarks = 1 - iSpeechMarks;
			if(iSpeechMarks == 0)
			{
				if(p[n] == '(')
				{
					if(iBracketCount == 0 && dwOpenPos == 0 && n == 0)
					{
						dwOpenPos = 1 + n;
					}
					iBracketCount++;
				}
				if(p[n] == ')')
				{
					iBracketCount--;

					bool bBracketIsAtEnd = false;
					if(p[n + 1] == 0) bBracketIsAtEnd = true;

					if(iBracketCount == 0 && dwOpenPos > 0)
					{
						if(bBracketIsAtEnd)
						{
							p[dwOpenPos - 1] = 32;
							p[n] = 32;
						}
						break;
					}
				}
			}
		}
		DWORD oldLen = Length();
		EatEdgeSpacesandTabs(nullptr);
		DWORD iNestCount = oldLen - Length();
		if(pdwHowMany) *pdwHowMany = iNestCount;
		if(iNestCount > 0) return true;
	}
	return false;
}

bool CStr::CropAll(DWORD *pHowManyCroppedTotal)
{
	bool bStay = true;
	while(bStay)
	{
		DWORD HowManyStartSpacesCropped = 0;
		EatEdgeSpacesandTabs(&HowManyStartSpacesCropped);
		*pHowManyCroppedTotal += HowManyStartSpacesCropped;

		DWORD HowManyStartBracketsCropped = 0;
		if(CropEqualEdgeBrackets(&HowManyStartBracketsCropped) == false) bStay = false;
		*pHowManyCroppedTotal += HowManyStartBracketsCropped;
	}
	return true;
}

void CStr::Shorten(DWORD dwNewLength)
{
	if(m_dwLen > 0)
	{
		m_buffer[dwNewLength] = 0;
		UpdateLen();
	}
}

void CStr::Reverse(void)
{
	if(m_dwLen > 0) _strrev(m_buffer.data());
}

bool CStr::ConvertDataListToMathList(CStr* pDimensionString)
{
	CStr pNewStr(pDimensionString ? "(" : "");

	CStr pDimStr(pDimensionString ? pDimensionString->GetStr() : nullptr);
	CStr pDimItem("");

	int iBracketCount = 0;
	if(m_dwLen > 0)
	{
		DWORD length = Length();
		for(DWORD n = 0; n < length; n++)
		{
			if(m_buffer[n] == '(') iBracketCount++;
			if(m_buffer[n] == ')') iBracketCount--;
			if(iBracketCount == 0 && m_buffer[n] == ',')
			{
				if(pDimensionString == nullptr)
				{
					pNewStr.AddText("*");
				}
				else
				{
					if(pDimItem.Length() == 0)
					{
						pNewStr.AddText(")+(");
					}
					else
					{
						pNewStr.AddText(")");
						pNewStr.AddText("*");
						pNewStr.AddText(pDimItem.GetStr());
						pNewStr.AddText("+(");
					}

					DWORD dwSep = pDimStr.FindFirstChar(',');
					if(dwSep == 0) dwSep = pDimStr.Length();
					char *pRest = pDimStr.m_buffer.data() + dwSep;
					pDimStr.SetChar(dwSep, 0);
					pDimItem.SetText(pDimStr.GetStr());
					if(pRest)
					{
						if(pDimStr.Length() > 0 && static_cast<DWORD>(strlen(pRest)) > 1)
						{
							pDimStr.SetText(pRest + 1);
						}
					}
					else
						pDimStr.SetText("");
				}
				n++;
			}

			pNewStr.AddChar(m_buffer[n]);
		}
		if(pDimensionString)
		{
			if(Length() == 0)
			{
				pNewStr.SetText("");
			}
			else
			{
				pNewStr.AddText(")");
				pNewStr.AddText("*");
				pNewStr.AddText(pDimItem.GetStr());
			}
		}
	}

	SetText(pNewStr.GetStr());
	return true;
}

bool CStr::IsTextALabel(void) const
{
	bool bStillValid = false;
	if(m_dwLen > 0)
	{
		for(DWORD n = 0; n < m_dwLen; n++)
		{
			bStillValid = false;
			if(IsAlphaNumericLabel(n))           bStillValid = true;
			if(m_buffer[n] == '_')               bStillValid = true;
			if(n == m_dwLen - 1 && m_buffer[n] == ':') bStillValid = true;
			if(bStillValid == false)
				break;
		}
		if(m_buffer[m_dwLen - 1] != ':')
			bStillValid = false;
	}
	return bStillValid;
}

bool CStr::IsTextASingleVariable(void) const
{
	bool bStillValid = false;
	if(m_dwLen > 0)
	{
		for(DWORD n = 0; n < m_dwLen; n++)
		{
			bStillValid = false;
			if(IsAlpha(n) && n == 0)                        bStillValid = true;
			if(IsAlphaNumericLabel(n) && n > 0)             bStillValid = true;
			if(m_buffer[n] == '*' && n == 0)                bStillValid = true;
			if(m_buffer[n] == '#' && n == (m_dwLen - 1))    bStillValid = true;
			if(m_buffer[n] == '$')                          bStillValid = true;
			if(m_buffer[n] == '@' && n == 0 && (m_buffer[n+1] == '$' || m_buffer[n+1] == ':')) bStillValid = true;
			if(bStillValid == false)
				break;
		}
	}
	return bStillValid;
}

bool CStr::IsTextAComplexVariable(void) const
{
	int iBracketCount = 0;
	DWORD dwInSpeechMarks = 0;
	bool bStillValid = false;
	if(m_dwLen > 0)
	{
		for(DWORD n = 0; n < m_dwLen; n++)
		{
			if(dwInSpeechMarks == 0)
			{
				if(m_buffer[n] == '(') iBracketCount++;
				if(m_buffer[n] == ')') iBracketCount--;
			}
			if(m_buffer[n] == '"') dwInSpeechMarks = 1 - dwInSpeechMarks;
			bStillValid = false;
			if(iBracketCount > 0)
			{
				if(IsSpaceCharacter(n)) bStillValid = true;
			}
			if(dwInSpeechMarks == 0 && iBracketCount == 0)
			{
				if(IsAlphaNumericLabel(n))    bStillValid = true;
				if(IsMathCharLabel(n))        bStillValid = true;
				if(m_buffer[n] == '#')        bStillValid = true;
				if(m_buffer[n] == '$')        bStillValid = true;
				if(m_buffer[n] == '@')        bStillValid = true;
				if(m_buffer[n] == '(')        bStillValid = true;
				if(m_buffer[n] == ')')        bStillValid = true;
				if(m_buffer[n] == '.')        bStillValid = true;
				if(m_buffer[n] == ',')        bStillValid = true;
			}
			else
				bStillValid = true;
			if(bStillValid == false)
				break;
		}
	}
	if(dwInSpeechMarks == 1)
		bStillValid = false;
	return bStillValid;
}

bool CStr::IsTextAnAlphaNumericValue(void) const
{
	bool bStillValid = true;
	if(m_dwLen > 0)
	{
		for(DWORD n = 0; n < m_dwLen; n++)
		{
			if(IsAlphaNumericLabel(n) == false)
			{
				bStillValid = false;
				break;
			}
		}
	}
	return bStillValid;
}

bool CStr::IsTextInBrackets(void) const
{
	if(m_dwLen > 0)
	{
		if(m_buffer[0] == '(' && m_buffer[m_dwLen - 1] == ')')
			return true;
	}
	return false;
}

bool CStr::IsTextSpeechMarked(void) const
{
	if(m_dwLen > 0)
	{
		if(m_buffer[0] == '"' && m_buffer[m_dwLen - 1] == '"')
			return true;
	}
	return false;
}

bool CStr::IsTextArrayDimensionDef(void) const
{
	bool bStillValid = true;
	if(m_dwLen > 0)
	{
		for(DWORD n = 0; n < m_dwLen; n++)
		{
			bool bThisCharValid = false;
			if(m_buffer[n] >= '0' && m_buffer[n] <= '9') bThisCharValid = true;
			if(m_buffer[n] == ',')                       bThisCharValid = true;
			if(bThisCharValid == false) { bStillValid = false; break; }
		}
	}
	return bStillValid;
}

bool CStr::IsTextNumericValue(void) const
{
	bool bStillValid = true;
	if(m_dwLen > 0)
	{
		for(DWORD n = 0; n < m_dwLen; n++)
		{
			bool bThisCharValid = false;
			if(n == 0 && (m_buffer[n] == '-' || m_buffer[n] == '+')) bThisCharValid = true;
			if(m_buffer[n] >= '0' && m_buffer[n] <= '9')            bThisCharValid = true;
			if(m_buffer[n] == '.')                                   bThisCharValid = true;
			if(bThisCharValid == false) { bStillValid = false; break; }
		}
	}
	return bStillValid;
}

bool CStr::IsTextIntegerOnlyValue(void) const
{
	bool bStillValid = true;
	if(m_dwLen > 0)
	{
		for(DWORD n = 0; n < m_dwLen; n++)
		{
			bool bThisCharValid = false;
			if(n == 0 && (m_buffer[n] == '-' || m_buffer[n] == '+')) bThisCharValid = true;
			if(m_buffer[n] >= '0' && m_buffer[n] <= '9')            bThisCharValid = true;
			if(bThisCharValid == false) { bStillValid = false; break; }
		}
	}
	return bStillValid;
}

bool CStr::IsTextHexValue(void) const
{
	bool bStillValid = true;
	if(m_dwLen > 0)
	{
		for(DWORD n = 0; n < m_dwLen; n++)
		{
			bool bThisCharValid = false;
			if(n == 0 && m_buffer[n] == '0')                               bThisCharValid = true;
			if(n == 1 && (m_buffer[n] == 'x' || m_buffer[n] == 'X'))       bThisCharValid = true;
			if(n >= 2 && m_buffer[n] >= '0' && m_buffer[n] <= '9')         bThisCharValid = true;
			if(n >= 2 && m_buffer[n] >= 'a' && m_buffer[n] <= 'f')         bThisCharValid = true;
			if(n >= 2 && m_buffer[n] >= 'A' && m_buffer[n] <= 'F')         bThisCharValid = true;
			if(bThisCharValid == false) { bStillValid = false; break; }
		}
	}
	return bStillValid;
}

bool CStr::IsTextBinaryValue(void) const
{
	bool bStillValid = true;
	if(m_dwLen > 0)
	{
		for(DWORD n = 0; n < m_dwLen; n++)
		{
			bool bThisCharValid = false;
			if(n == 0 && m_buffer[n] == '%')                               bThisCharValid = true;
			if(n >= 1 && (m_buffer[n] == '0' || m_buffer[n] == '1'))       bThisCharValid = true;
			if(bThisCharValid == false) { bStillValid = false; break; }
		}
	}
	return bStillValid;
}

bool CStr::IsTextOctalValue(void) const
{
	bool bStillValid = false;
	if(m_dwLen > 2)
	{
		bStillValid = true;
		for(DWORD n = 0; n < m_dwLen; n++)
		{
			bool bThisCharValid = false;
			if(n == 0 && m_buffer[n] == '0')                               bThisCharValid = true;
			if(n == 1 && (m_buffer[n] == 'c' || m_buffer[n] == 'C'))       bThisCharValid = true;
			if(n >= 2 && m_buffer[n] >= '0' && m_buffer[n] <= '7')         bThisCharValid = true;
			if(bThisCharValid == false) { bStillValid = false; break; }
		}
	}
	return bStillValid;
}

bool CStr::IsTextLValue(void) const
{
	DWORD dwStage = 0;
	int iBracketCount = 0;
	DWORD dwInSpeechMarks = 0;
	bool bStillValid = false;
	if(m_dwLen > 0)
	{
		for(DWORD n = 0; n < m_dwLen; n++)
		{
			if(dwInSpeechMarks == 0)
			{
				if(m_buffer[n] == '(') iBracketCount++;
				if(m_buffer[n] == ')') iBracketCount--;
			}
			if(m_buffer[n] == '"') dwInSpeechMarks = 1 - dwInSpeechMarks;
			if(IsAlphaNumericLabel(n) && dwStage == 0) dwStage = 1;
			if(dwInSpeechMarks == 0 && iBracketCount == 0)
			{
				if(IsSpaceCharacter(n) && dwStage == 1) dwStage = 2;
				if(m_buffer[n] == '.' && dwStage > 0) dwStage = 0;
			}
			if(IsSpaceCharacter(n) == false && dwStage == 2) { bStillValid = false; break; }
			bStillValid = false;
			if(dwInSpeechMarks == 0 && iBracketCount == 0)
			{
				if(IsAlphaNumericLabel(n)) bStillValid = true;
				if(IsSpaceCharacter(n))    bStillValid = true;
				if(m_buffer[n] == '*')     bStillValid = true;
				if(m_buffer[n] == '#')     bStillValid = true;
				if(m_buffer[n] == '$')     bStillValid = true;
				if(m_buffer[n] == '@')     bStillValid = true;
				if(m_buffer[n] == '(')     bStillValid = true;
				if(m_buffer[n] == ')')     bStillValid = true;
				if(m_buffer[n] == '.')     bStillValid = true;
			}
			else
				bStillValid = true;
			if(bStillValid == false) break;
		}
	}
	if(dwInSpeechMarks == 1) bStillValid = false;
	return bStillValid;
}

bool CStr::IsTextRValue(void) const
{
	if(IsTextAComplexVariable()) return true;
	return false;
}

DWORD CStr::GetDWORDRepresentation(DWORD dwTypeValue, DWORD* dwExtraDWORD) const
{
	LPSTR EndString = nullptr;
	DWORD dwResult = 0;
	if(m_dwLen > 0)
	{
		unsigned long num = 0;
		CStr pIntStr("");
		LPSTR pNumericStr = const_cast<char*>(m_buffer.data());
		if(IsTextBinaryValue())
		{
			num = strtoul(m_buffer.data() + 1, &EndString, 2);
			pIntStr.SetUnsignedNumericText(num);
			pNumericStr = pIntStr.GetStr();
		}
		if(IsTextHexValue())
		{
			num = strtoul(m_buffer.data(), &EndString, 16);
			pIntStr.SetUnsignedNumericText(num);
			pNumericStr = pIntStr.GetStr();
		}
		if(IsTextOctalValue())
		{
			pIntStr.SetText("0");
			pIntStr.AddText(const_cast<char*>(m_buffer.data() + 2));
			num = strtoul(pIntStr.GetStr(), &EndString, 8);
			pIntStr.SetUnsignedNumericText(num);
			pNumericStr = pIntStr.GetStr();
		}
		int integervalue = 0;
		float floatvalue = 0.0f;
		unsigned char booleanvalue = 0, bytevalue = 0;
		WORD wordvalue = 0;
		DWORD dwordvalue = 0;
		double doublevalue = 0.0;
		LONGLONG longlongvalue = 0;
		sscanf_s(pNumericStr, "%i", &integervalue);
		sscanf_s(pNumericStr, "%f", &floatvalue);
		DWORD dwGetU = 0;
		sscanf_s(pNumericStr, "%u", &dwGetU); booleanvalue = static_cast<unsigned char>(dwGetU);
		sscanf_s(pNumericStr, "%u", &dwGetU); bytevalue = static_cast<unsigned char>(dwGetU);
		sscanf_s(pNumericStr, "%u", &dwGetU); wordvalue = static_cast<WORD>(dwGetU);
		sscanf_s(pNumericStr, "%u", &dwordvalue);
		sscanf_s(pNumericStr, "%lf", &doublevalue);
		sscanf_s(pNumericStr, "%I64d", &longlongvalue);
		switch(dwTypeValue)
		{
			case 1 : std::memcpy(&dwResult, &integervalue, sizeof(dwResult)); break;
			case 2 : std::memcpy(&dwResult, &floatvalue, sizeof(dwResult));   break;
			case 4 : dwResult = booleanvalue; break;
			case 5 : dwResult = bytevalue;    break;
			case 6 : dwResult = wordvalue;    break;
			case 7 : dwResult = dwordvalue;   break;
			case 8 :
				std::memcpy(&dwResult, &doublevalue, sizeof(DWORD));
				std::memcpy(dwExtraDWORD, reinterpret_cast<const char*>(&doublevalue) + sizeof(DWORD), sizeof(DWORD));
				break;
			case 9 :
				std::memcpy(&dwResult, &longlongvalue, sizeof(DWORD));
				std::memcpy(dwExtraDWORD, reinterpret_cast<const char*>(&longlongvalue) + sizeof(DWORD), sizeof(DWORD));
				break;
		}
	}
	return dwResult;
}

bool CStr::IsIntegerBiggerThanDWORD(void) const
{
	if(m_dwLen > 0)
	{
		int intvalue = static_cast<int>(atol(m_buffer.data()));
		__int64 value = _atoi64(m_buffer.data());
		if(intvalue != value) return true;
	}
	return false;
}

bool CStr::IsFloatBiggerThanDWORD(void) const
{
	if(m_dwLen > 0)
	{
		bool bFoundDot = false;
		const char* lpNum = m_buffer.data();
		int iLatestCount = 0, iCount = 0;
		for(DWORD i = 0; i < m_dwLen; i++)
		{
			if(bFoundDot)
			{
				if(static_cast<unsigned char>(lpNum[i]) != '0') iLatestCount = iCount;
				iCount++;
			}
			if(lpNum[i] == '.') bFoundDot = true;
		}
		return (iLatestCount >= 5);
	}
	return false;
}

void CStr::TrimToPathOnly(void)
{
	if(m_dwLen > 0)
	{
		DWORD dwFileSepPos = FindLastChar('\\');
		if(dwFileSepPos > 0)
			SetChar(dwFileSepPos + 1, 0);
		else
			SetText("");
	}
}

bool CStr::TranslateForDBM(CResultData* pResultPtr)
{
	if(m_buffer.empty() || m_dwLen == 0)
		return true;

	bool bIsStructFunction = false;
	CStr pString(m_buffer.data());

	if(pString.CheckChars(0, 3, "FS@") == true)
	{
		LPSTR pRest = pString.GetRightOfPosition(3);
		pString.SetText(pRest);
		pString.EatEdgeSpacesandTabs(nullptr);
		delete[] pRest;
		bIsStructFunction = true;
	}

	if(bIsStructFunction)
	{
		DWORD dwPos = pString.FindFirstChar('@');
		if(dwPos > 0)
		{
			bool bHaltCompilation = true;
			LPSTR pSubtypename = pString.GetLeftOfPosition(dwPos);
			DWORD dwArrayType = 0;
			if(pString.GetChar(dwPos + 1) == '&') dwArrayType = 1;
			LPSTR pFieldname = pString.GetRightOfPosition(dwPos + 1);
			DWORD dwOffset = 0, dwSizeOfData = 0;
			if(g_pStructTable->FindOffsetFromField(pSubtypename, pFieldname, &dwOffset, &dwSizeOfData))
			{
				DWORD dwTokenData = 0, dwParamMax = 0, dwLength = 0;
				if(g_pInstructionTable->FindUserFunction(pSubtypename, 1, &dwTokenData, &dwParamMax, &dwLength))
				{
					CStructTable* pThisStruct = g_pStructTable->DoesTypeEvenExist(pSubtypename);
					CDeclaration* pLastParamDec = pThisStruct->GetDecChain();
					for(DWORD n = 0; n < dwParamMax; n++) if(pLastParamDec->GetNext()) pLastParamDec = pLastParamDec->GetNext();
					DWORD dOffsetToLastParamInStruct = pLastParamDec->GetOffset();
					int iDisplacement = 0;
					if(dwOffset == 0) { iDisplacement = -4; }
					else if(dwOffset <= dOffsetToLastParamInStruct) { iDisplacement = dwOffset + 4; }
					else { iDisplacement = ((dOffsetToLastParamInStruct) - dwOffset) - dwSizeOfData; }
					CStr pStr("@:");
					pStr.AddNumericText(iDisplacement);
					SetText(pStr.GetStr());
					bHaltCompilation = false;
				}
			}
			if(g_pStatementList->GetWriteStarted() == true)
			{
				if(bHaltCompilation == true)
				{
					DWORD dwLine = g_pStatementList->GetTokenLineNumber();
					if(g_pStatementList->GetRefStatement()) dwLine = g_pStatementList->GetRefStatement()->GetLineNumber();
					CStr pFullField(pSubtypename);
					pFullField.AddText(":");
					pFullField.AddText(pFieldname);
					g_pErrorReport->SetError(dwLine, ERR_SYNTAX+44, pFullField.GetStr());
					delete[] pSubtypename;
					delete[] pFieldname;
					return false;
				}
			}
			delete[] pSubtypename;
			delete[] pFieldname;
		}
	}

	if(pString.IsTextSpeechMarked() == true)
	{
		pString.EatSpeechMarks();
		DWORD dwIndex = g_pStatementList->GetStringIndexCounter() + 1;
		if(g_pStringTable->AddString(pString.GetStr(), dwIndex))
			g_pStatementList->IncStringIndexCounter(1);
		if(dwIndex == 0) return false;
		CStr pStr("$$");
		pStr.AddNumericText(dwIndex);
		SetText(pStr.GetStr());
		pResultPtr->m_dwType = 3;
		pResultPtr->m_dwDataOffset = 0;
	}

	return true;
}

bool CStr::IsConstant(void) const
{
	bool bStillValid = true;
	if(m_dwLen > 0)
	{
		for(DWORD n = 0; n < m_dwLen; n++)
		{
			if(IsAlphaNumericLabel(n) == false) bStillValid = false;
			if(bStillValid == false) break;
		}
	}
	return bStillValid;
}
