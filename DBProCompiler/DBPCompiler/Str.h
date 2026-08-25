#pragma once

#include "ParserHeader.h"
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
		CStr(std::string_view text);
		CStr(const std::string& text);
		CStr(DWORD dwTextSize);
		void		Enlarge(DWORD length);
		LPSTR		GetStr(void) const { return const_cast<char*>(m_buffer.data()); }
		double		GetValue(void) const;
		void		SetText(LPSTR pStr);
		void		SetText(LPCSTR pStr);
		void		SetText(std::string_view text);
		void		SetText(CStr* pStrText);
		void		AddText(LPSTR pStr);
		void		AddText(LPCSTR pStr);
		void		AddText(std::string_view text);
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
		[[nodiscard]] size_t size(void) const noexcept { return m_dwLen; }
		[[nodiscard]] bool empty(void) const noexcept { return m_dwLen == 0; }
		[[nodiscard]] const char* c_str(void) const noexcept { return m_buffer.data(); }
		[[nodiscard]] const char* data(void) const noexcept { return m_buffer.data(); }
		[[nodiscard]] std::string str(void) const { return std::string(m_buffer.data(), m_dwLen); }
		[[nodiscard]] operator std::string_view() const noexcept { return View(); }
		[[nodiscard]] std::unique_ptr<char[]>	GetLeftOfPosition(DWORD Position) const;
		[[nodiscard]] std::unique_ptr<char[]>	GetRightOfPosition(DWORD Position) const;
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
		uint32_t	GetDWORDRepresentation(uint32_t dwTypeValue, uint32_t* pdwExtra) const {
			DWORD extra = pdwExtra ? static_cast<DWORD>(*pdwExtra) : 0;
			DWORD res = GetDWORDRepresentation(static_cast<DWORD>(dwTypeValue), &extra);
			if (pdwExtra) *pdwExtra = static_cast<uint32_t>(extra);
			return static_cast<uint32_t>(res);
		}
		DWORD		GetDWORDRepresentation(int dwTypeValue, std::nullptr_t) const {
			return GetDWORDRepresentation(static_cast<DWORD>(dwTypeValue), static_cast<DWORD*>(nullptr));
		}
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