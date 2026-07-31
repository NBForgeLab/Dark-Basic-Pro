#ifndef EXPRESSIONPARSER_H
#define EXPRESSIONPARSER_H

#include <windows.h>
#include <string_view>

class CExpressionParser
{
public:
    CExpressionParser() noexcept = default;
    ~CExpressionParser() = default;

    CExpressionParser(const CExpressionParser&) = delete;
    CExpressionParser& operator=(const CExpressionParser&) = delete;
    CExpressionParser(CExpressionParser&&) noexcept = default;
    CExpressionParser& operator=(CExpressionParser&&) noexcept = default;

    [[nodiscard]] bool IsNumericLiteral(std::string_view svText) const noexcept;
    [[nodiscard]] bool CheckForSymbol(std::string_view svText, DWORD dwSP, DWORD* pdwMathType, DWORD* pdwPriority, DWORD* pdwSymbolWidth) const noexcept;
};

#endif // EXPRESSIONPARSER_H
