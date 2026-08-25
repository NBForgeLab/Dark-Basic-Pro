#pragma once
#include "ParserHeader.h"
#include <string_view>

class CStatement;
class CStr;

/**
 * @file ExpressionParser.h
 * @brief Mathematical expression parsing and operator symbol classification.
 *
 * CExpressionParser identifies numeric literals, classifies arithmetic and
 * logical operator symbols (with their precedence and width), and delegates
 * full expression parsing to the statement layer.  Operator symbols include
 * single-character (`+`, `-`, `*`, `/`, `^`, `=`, `<`, `>`), two-character
 * (`<>`, `>=`, `<=`, `%%`, `&&`, `||`, `~~`, `..`, `>>`, `<<`), and keyword
 * forms (`AND`, `OR`, `NOT`, `XOR`, `DIV`, `MOD`).
 *
 * Extracted from CStatement / CMathOp to isolate expression analysis from
 * statement control flow.
 *
 * @see CStatement  Owns an instance and calls ParseExpression during compilation.
 * @see CMathOp     Delegates CheckForSymbol here for symbol identification.
 */
class CExpressionParser
{
public:
    CExpressionParser() noexcept = default;
    ~CExpressionParser() = default;

    CExpressionParser(const CExpressionParser&) = delete;
    CExpressionParser& operator=(const CExpressionParser&) = delete;
    CExpressionParser(CExpressionParser&&) noexcept = default;
    CExpressionParser& operator=(CExpressionParser&&) noexcept = default;

    /** @brief Returns true if every character in the text is a decimal digit. */
    [[nodiscard]] bool IsNumericLiteral(std::string_view svText) const noexcept;

    /**
     * @brief Identifies an operator symbol at position @p dwSP and returns its properties.
     * @param[in]  svText          Source text being scanned.
     * @param[in]  dwSP            Byte offset to begin symbol matching.
     * @param[out] pdwMathType     Operator type code (e.g. 1 = power, 4 = add, 41 = AND).
     * @param[out] pdwPriority     Operator precedence value (lower = tighter binding).
     * @param[out] pdwSymbolWidth  Character width of the matched symbol (1, 2, 4, or 5).
     * @return true if a symbol was recognized, false otherwise.
     */
    [[nodiscard]] bool CheckForSymbol(std::string_view svText, DWORD dwSP, DWORD* pdwMathType, DWORD* pdwPriority, DWORD* pdwSymbolWidth) const noexcept;

    /**
     * @brief Parses a mathematical expression within a statement context.
     * @param[in] pStatement  The owning statement providing compilation context.
     * @param[in] pExpression The expression string to parse.
     * @return true on success, false if inputs are null.
     */
    [[nodiscard]] bool ParseExpression(CStatement* pStatement, CStr* pExpression) const;
};
