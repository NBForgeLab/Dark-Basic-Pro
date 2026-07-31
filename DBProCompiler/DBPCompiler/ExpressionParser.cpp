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
