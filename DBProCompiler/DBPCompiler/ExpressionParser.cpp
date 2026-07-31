#include "ExpressionParser.h"
#include <cctype>

bool CExpressionParser::IsNumericLiteral(std::string_view svText) const noexcept
{
    if (svText.empty()) return false;
    for (char c : svText)
    {
        if (!std::isdigit(static_cast<unsigned char>(c)))
        {
            return false;
        }
    }
    return true;
}

bool CExpressionParser::CheckForSymbol(std::string_view svText, DWORD dwSP, DWORD* pdwMathType, DWORD* pdwPriority, DWORD* pdwSymbolWidth) const noexcept
{
    if (svText.empty() || dwSP >= svText.length()) return false;
    
    DWORD dwMathType = 0;
    DWORD dwPriority = 0;
    DWORD dwSymbolWidth = 0;
    
    std::string_view sub = svText.substr(dwSP);

    // Single character symbols
    char c = sub[0];
    if (c == '^') { dwMathType = 1; dwPriority = 1; dwSymbolWidth = 1; }
    else if (c == '/') { dwMathType = 3; dwPriority = 2; dwSymbolWidth = 1; }
    else if (c == '*') { dwMathType = 2; dwPriority = 3; dwSymbolWidth = 1; }
    else if (c == '+') { dwMathType = 4; dwPriority = 5; dwSymbolWidth = 1; }
    else if (c == '-') { dwMathType = 5; dwPriority = 4; dwSymbolWidth = 1; }
    else if (c == '=') { dwMathType = 27; dwPriority = 27; dwSymbolWidth = 1; }
    else if (c == '>') { dwMathType = 25; dwPriority = 25; dwSymbolWidth = 1; }
    else if (c == '<') { dwMathType = 26; dwPriority = 26; dwSymbolWidth = 1; }

    // Two character symbols
    if (sub.length() >= 2)
    {
        std::string_view sub2 = sub.substr(0, 2);
        if (sub2 == "<>") { dwMathType = 22; dwPriority = 22; dwSymbolWidth = 2; }
        else if (sub2 == ">=") { dwMathType = 23; dwPriority = 23; dwSymbolWidth = 2; }
        else if (sub2 == "=>") { dwMathType = 23; dwPriority = 23; dwSymbolWidth = 2; }
        else if (sub2 == "<=") { dwMathType = 24; dwPriority = 24; dwSymbolWidth = 2; }
        else if (sub2 == "=<") { dwMathType = 24; dwPriority = 24; dwSymbolWidth = 2; }
        else if (sub2 == "%%") { dwMathType = 6; dwPriority = 3; dwSymbolWidth = 2; }
        else if (sub2 == "&&") { dwMathType = 31; dwPriority = 31; dwSymbolWidth = 2; }
        else if (sub2 == "||") { dwMathType = 32; dwPriority = 32; dwSymbolWidth = 2; }
        else if (sub2 == "~~") { dwMathType = 33; dwPriority = 33; dwSymbolWidth = 2; }
        else if (sub2 == "..") { dwMathType = 34; dwPriority = 34; dwSymbolWidth = 2; }
        else if (sub2 == ">>") { dwMathType = 11; dwPriority = 11; dwSymbolWidth = 2; }
        else if (sub2 == "<<") { dwMathType = 12; dwPriority = 12; dwSymbolWidth = 2; }
    }

    // Word symbols
    if (sub.length() >= 4 && sub.substr(0, 4) == " OR ") { dwMathType = 42; dwPriority = 42; dwSymbolWidth = 4; }
    if (sub.length() >= 4 && sub.substr(0, 4) == "NOT ") { dwMathType = 43; dwPriority = 43; dwSymbolWidth = 4; }
    if (sub.length() >= 5 && sub.substr(0, 5) == " AND ") { dwMathType = 41; dwPriority = 41; dwSymbolWidth = 5; }
    if (sub.length() >= 5 && sub.substr(0, 5) == " XOR ") { dwMathType = 33; dwPriority = 44; dwSymbolWidth = 5; }
    if (sub.length() >= 5 && sub.substr(0, 5) == " DIV ") { dwMathType = 3; dwPriority = 3; dwSymbolWidth = 5; }
    if (sub.length() >= 5 && sub.substr(0, 5) == " MOD ") { dwMathType = 6; dwPriority = 3; dwSymbolWidth = 5; }

    if (dwMathType > 0)
    {
        if (pdwMathType) *pdwMathType = dwMathType;
        if (pdwPriority) *pdwPriority = dwPriority;
        if (pdwSymbolWidth) *pdwSymbolWidth = dwSymbolWidth;
        return true;
    }
    return false;
}
