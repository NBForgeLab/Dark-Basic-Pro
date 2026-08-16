// Str.h: interface for the CStr class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_STR_H__95C8EB95_D88D_48CF_9F08_36248C3E570E__INCLUDED_)
#define AFX_STR_H__95C8EB95_D88D_48CF_9F08_36248C3E570E__INCLUDED_

#include "windows.h"
#include <memory>
#include <vector>
#include <string_view>

#include <DB3.h>
#include <DB3Factory.h>

class CResultData;

class CStr: public db3::TObject<CStr>
{
	public:
		CStr();
		virtual ~CStr() = default;
		// Disable copy to prevent double-free bugs
		CStr(const CStr&) = delete;
		CStr& operator=(const CStr&) = delete;
		// Enable move semantics for modern ownership transfer
		CStr(CStr&& other) noexcept;
		CStr& operator=(CStr&& other) noexcept;
	public:
		CStr(LPSTR pText);
		CStr(LPCSTR pText);
		CStr(DWORD dwTextSize);
		void		Enlarge(DWORD length);
		LPSTR		GetStr(void) const { return const_cast<char*>(m_buffer.data()); }
		double		GetValue(void) const;
		void		SetText(LPSTR pStr);
		void		SetText(LPCSTR pStr);
		void		SetText(CStr* pStrText);
		void		AddText(LPSTR pStr);
		void		AddText(LPCSTR pStr);
		void		AddText(CStr* pStrText);
		void		AddChar(char cChar);
		void		InsertText(LPSTR pStr);
		void		InsertText(LPCSTR pStr);
		void		SetNumericText(DWORD dwNumText);
		void		SetUnsignedNumericText(DWORD dwNumText);
		void		SetDWORDNumericText(DWORD dwNumText);
		void		AddNumericText(DWORD dwNumText);
		void		AddDoubleText(double dNumText);
		DWORD		Length(void) const { return m_dwLen; }
		LPSTR		GetLeftOfPosition(DWORD Position) const;
		LPSTR		GetRightOfPosition(DWORD Position) const;
		void		CopyToPtr(LPSTR pPointer) const;
		void		CopyFromPtr(LPSTR pPointer, LPSTR pPointerEnd, DWORD length);

		bool		MakeUpper(void);
		bool		ReplaceSemicolons(void);
		bool		CheckChar(DWORD dwPos, unsigned char cChar) const;
		bool		CheckChars(DWORD dwPos, DWORD num, LPCSTR pText) const;
		bool		SetChar(DWORD dwPos, DWORD value);
		DWORD		FindFirstChar(char cChar) const;
		DWORD		FindFirstCharAtBracketLevel(char cChar) const;
		DWORD		FindLastChar(char cChar) const;
		unsigned char GetChar(DWORD dwPos) const;

		bool		IsAlpha(DWORD dwPos) const;
		bool		IsAlphaNumericLabel(DWORD dwPos) const;
		bool		IsMathCharLabel(DWORD dwPos) const;
		bool		IsSpaceCharacter(DWORD dwPos) const;
		bool		ContainsASOperator(void) const;

		bool		IsSciNot(void) const;
		void		ResolveSciNot(void);

		void		EatTrailingEdgeSpacesandTabs(void);
		void		EatEdgeSpacesandTabs(DWORD* pdwHowMany);
		void		EatSpeechMarks(void);
		bool		EatLeadingChars(void);
		void		EatNonImportantChars(void);
		bool		CropEqualEdgeBrackets(DWORD* pdwHowMany);
		bool		CropAll(DWORD *pHowManyCroppedTotal);
		void		Shorten(DWORD dwNewLength);
		void		Reverse(void);
		void		EatChar(DWORD dwPos);

		bool		ConvertDataListToMathList(CStr* pDimensionString);

		bool		IsTextALabel(void) const;
		bool		IsTextASingleVariable(void) const;
		bool		IsTextAComplexVariable(void) const;
		bool		IsTextAnAlphaNumericValue(void) const;
		bool		IsTextInBrackets(void) const;
		bool		IsTextSpeechMarked(void) const;
		bool		IsTextArrayDimensionDef(void) const;
		bool		IsTextNumericValue(void) const;
		bool		IsTextIntegerOnlyValue(void) const;
		bool		IsTextHexValue(void) const;
		bool		IsTextBinaryValue(void) const;
		bool		IsTextOctalValue(void) const;
		bool		IsTextLValue(void) const;
		bool		IsTextRValue(void) const;

		DWORD		GetDWORDRepresentation(DWORD dwTypeValue, DWORD* dwExtraDWORD) const;
		bool		IsIntegerBiggerThanDWORD(void) const;
		bool		IsFloatBiggerThanDWORD(void) const;

		void		TrimToPathOnly(void);

		bool		TranslateForDBM(CResultData* pResultPtr);

		bool		IsConstant(void) const;

		// Modern accessor - zero-copy view into the buffer
		std::string_view View() const { return {m_buffer.data(), m_dwLen}; }

	private:
		std::vector<char>       m_buffer;  // Null-terminated string buffer (RAII managed)
		DWORD                   m_dwLen;   // Cached string length (excludes null terminator)

		void                    UpdateLen(void);
};

#endif // !defined(AFX_STR_H__95C8EB95_D88D_48CF_9F08_36248C3E570E__INCLUDED_)
